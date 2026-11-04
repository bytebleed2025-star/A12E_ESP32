/**
 * @file license.cpp
 * @brief Powered-on trial + ECDSA P-256 signed activation keys
 *
 * Key format (URL-safe, copy/paste friendly):
 *
 *     base64url(payload) "." base64url(DER_signature)
 *
 * payload = "A12E1|<DEVICE-ID>|P|<serial>"
 *
 * The signature is made with tools/license_private.pem on your PC and
 * verified on the ESP32 with the public key in license_config.h. The ESP32
 * never contains the private key, so keys cannot be generated from the .bin.
 */

#include "license.h"

#include <Preferences.h>
#include <esp_mac.h>
#include <mbedtls/md.h>
#include <mbedtls/pk.h>
#include <string.h>

#include "license_config.h"
#include "log.h"

// ============================================================================
// State
// ============================================================================

static Preferences prefs;
static LicenseState state = LicenseState::Trial;
static String deviceId;
static String storedKey;

static uint32_t trialUsedSec = 0;
static unsigned long lastTickMs = 0;
static uint32_t unsavedSec = 0;

// ============================================================================
// Helpers
// ============================================================================

static int base64UrlValue(char c) {
  if (c >= 'A' && c <= 'Z') return c - 'A';
  if (c >= 'a' && c <= 'z') return c - 'a' + 26;
  if (c >= '0' && c <= '9') return c - '0' + 52;
  if (c == '-') return 62;
  if (c == '_') return 63;
  return -1;
}

static bool base64UrlDecode(const String &in, uint8_t *out, size_t outCapacity,
                            size_t &outLength) {
  uint32_t acc = 0;
  int bits = 0;
  outLength = 0;

  for (size_t i = 0; i < in.length(); ++i) {
    const char c = in[i];
    if (c == '=' || c == '\r' || c == '\n' || c == ' ') continue;

    const int value = base64UrlValue(c);
    if (value < 0) return false;

    acc = (acc << 6) | static_cast<uint32_t>(value);
    bits += 6;

    if (bits >= 8) {
      bits -= 8;
      if (outLength >= outCapacity) return false;
      out[outLength++] = static_cast<uint8_t>((acc >> bits) & 0xFF);
    }
  }

  return true;
}

/**
 * Verify an ECDSA P-256 / SHA-256 signature over the exact payload bytes.
 */
static bool verifySignature(const uint8_t *payload, size_t payloadLen,
                            const uint8_t *signature, size_t signatureLen) {
  mbedtls_pk_context pk;
  mbedtls_pk_init(&pk);

  bool ok = false;

  const int parseResult = mbedtls_pk_parse_public_key(
      &pk, reinterpret_cast<const unsigned char *>(LICENSE_PUBLIC_KEY_PEM),
      sizeof(LICENSE_PUBLIC_KEY_PEM));

  if (parseResult != 0) {
    LOG.printf("[LIC] Public key parse failed: -0x%04X\n", -parseResult);
  } else {
    const mbedtls_md_info_t *md =
        mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    uint8_t hash[32];

    if (md == nullptr ||
        mbedtls_md(md, payload, payloadLen, hash) != 0) {
      LOG.println("[LIC] SHA-256 failed");
    } else {
      const int verifyResult =
          mbedtls_pk_verify(&pk, MBEDTLS_MD_SHA256, hash, 0, signature,
                            signatureLen);
      if (verifyResult == 0) {
        ok = true;
      } else {
        LOG.printf("[LIC] Signature verify failed: -0x%04X\n",
                      -verifyResult);
      }
    }
  }

  mbedtls_pk_free(&pk);
  return ok;
}

/**
 * Validate a key for this device.
 * @param reason optional failure description
 */
static bool verifyKey(const String &rawKey, String *reason = nullptr) {
  const auto fail = [&](const char *message) {
    if (reason != nullptr) *reason = message;
    return false;
  };

  String key = rawKey;
  key.trim();

  if (key.length() < 32 || key.length() > 400) {
    return fail("Key length is not valid");
  }

  const int dot = key.indexOf('.');
  if (dot <= 0 || key.indexOf('.', dot + 1) >= 0) {
    return fail("Key format is not valid");
  }

  const String payloadPart = key.substring(0, dot);
  const String signaturePart = key.substring(dot + 1);

  uint8_t payload[160];
  uint8_t signature[96];
  size_t payloadLen = 0;
  size_t signatureLen = 0;

  if (!base64UrlDecode(payloadPart, payload, sizeof(payload), payloadLen) ||
      payloadLen == 0) {
    return fail("Key payload is corrupted");
  }
  if (!base64UrlDecode(signaturePart, signature, sizeof(signature),
                       signatureLen) ||
      signatureLen == 0) {
    return fail("Key signature is corrupted");
  }

  if (!verifySignature(payload, payloadLen, signature, signatureLen)) {
    return fail("Invalid key (signature mismatch)");
  }

  // Signature is valid -> the payload is authentic and can be trusted.
  char payloadText[sizeof(payload) + 1];
  memcpy(payloadText, payload, payloadLen);
  payloadText[payloadLen] = '\0';

  const String text(payloadText);

  const int p1 = text.indexOf('|');
  const int p2 = text.indexOf('|', p1 + 1);
  const int p3 = text.indexOf('|', p2 + 1);
  if (p1 <= 0 || p2 <= p1 || p3 <= p2) {
    return fail("Key payload is not valid");
  }

  const String version = text.substring(0, p1);
  const String keyDevice = text.substring(p1 + 1, p2);
  const String type = text.substring(p2 + 1, p3);
  const String serial = text.substring(p3 + 1);

  if (version != LICENSE_PRODUCT_CODE) {
    return fail("Key is for a different product");
  }
  if (keyDevice != deviceId) {
    return fail("Key is for a different device");
  }
  if (type != "P") {
    return fail("Unsupported license type");
  }
  if (serial.length() == 0) {
    return fail("Key serial is missing");
  }

  return true;
}

static uint32_t remainingTrialSec() {
  if (trialUsedSec >= LICENSE_TRIAL_SECONDS) return 0;
  return LICENSE_TRIAL_SECONDS - trialUsedSec;
}

static void refreshState() {
  if (state == LicenseState::Licensed) return;
  state = (trialUsedSec >= LICENSE_TRIAL_SECONDS) ? LicenseState::Expired
                                                  : LicenseState::Trial;
}

static void persistTrial(bool force) {
  if (state == LicenseState::Licensed) return;

  if (force || unsavedSec >= LICENSE_SAVE_INTERVAL_SEC) {
    prefs.putUInt("used", trialUsedSec);
    unsavedSec = 0;
  }
}

// ============================================================================
// Public API
// ============================================================================

void licenseBegin() {
  uint8_t mac[6] = {0};
  const esp_err_t macResult = esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP);
  if (macResult != ESP_OK) {
    LOG.println("[LIC] WARNING: could not read MAC address");
  }

  char id[24];
  snprintf(id, sizeof(id), "A12E-%02X%02X%02X%02X%02X%02X", mac[0], mac[1],
           mac[2], mac[3], mac[4], mac[5]);
  deviceId = id;

  if (!prefs.begin(LICENSE_NVS_NAMESPACE, false)) {
    LOG.println("[LIC] WARNING: NVS unavailable - trial will not persist");
  }

  trialUsedSec = prefs.getUInt("used", 0);
  if (trialUsedSec > LICENSE_TRIAL_SECONDS) {
    trialUsedSec = LICENSE_TRIAL_SECONDS;
  }

  storedKey = prefs.getString("key", "");
  state = LicenseState::Trial;

  if (storedKey.length() > 0) {
    if (verifyKey(storedKey)) {
      state = LicenseState::Licensed;
    } else {
      LOG.println("[LIC] Stored key is not valid for this device - ignored");
    }
  }

  refreshState();
  lastTickMs = millis();
  unsavedSec = 0;

  LOG.println("[LIC] Device ID: " + deviceId);
  if (state == LicenseState::Licensed) {
    LOG.println("[LIC] Status: LICENSED");
  } else if (state == LicenseState::Expired) {
    LOG.println("[LIC] Status: TRIAL EXPIRED - activation required");
  } else {
    LOG.println("[LIC] Status: TRIAL, " +
                   String(remainingTrialSec() / 3600) + "h " +
                   String((remainingTrialSec() % 3600) / 60) + "m remaining");
  }
}

void licenseLoop() {
  if (state == LicenseState::Licensed) return;

  const unsigned long now = millis();
  const unsigned long deltaMs = now - lastTickMs;
  if (deltaMs < 1000) return;

  const uint32_t elapsed = deltaMs / 1000;
  lastTickMs += elapsed * 1000UL;

  if (state == LicenseState::Expired) return;

  trialUsedSec += elapsed;
  unsavedSec += elapsed;

  if (trialUsedSec >= LICENSE_TRIAL_SECONDS) {
    trialUsedSec = LICENSE_TRIAL_SECONDS;
    state = LicenseState::Expired;
    persistTrial(true);
    LOG.println("[LIC] Trial expired - enter an activation key");
  } else {
    persistTrial(false);
  }
}

bool licenseActivate(const String &key, String &error) {
  if (!verifyKey(key, &error)) {
    LOG.println("[LIC] Activation rejected: " + error);
    return false;
  }

  String cleaned = key;
  cleaned.trim();

  if (prefs.putString("key", cleaned) == 0) {
    error = "Could not save key (NVS write failed)";
    LOG.println("[LIC] Activation failed: " + error);
    return false;
  }

  storedKey = cleaned;
  state = LicenseState::Licensed;

  LOG.println("[LIC] Activated successfully");
  return true;
}

LicenseState licenseGetState() { return state; }

bool licenseIsLocked() { return state == LicenseState::Expired; }

uint32_t licenseTrialRemainingSec() { return remainingTrialSec(); }

uint32_t licenseTrialTotalSec() { return LICENSE_TRIAL_SECONDS; }

const String &licenseDeviceId() { return deviceId; }

bool licenseHasStoredKey() { return storedKey.length() > 0; }

const char *licenseStateName() {
  switch (state) {
    case LicenseState::Licensed:
      return "licensed";
    case LicenseState::Expired:
      return "expired";
    default:
      return "trial";
  }
}
