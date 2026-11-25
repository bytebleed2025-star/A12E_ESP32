/**
 * @file license.cpp
 * @brief One-time trial token + ECDSA P-256 signed activation keys.
 *
 * Key payload:
 *   base64url("A12E1|A12E-XXXXXXXXXXXX|P|<serial>") "." base64url(DER sig)
 *
 * Trial payload:
 *   base64url("TRIAL1|A12E-XXXXXXXXXXXX|<seconds>|<serial>") "." base64url(DER sig)
 *
 * The private key lives only on the seller's PC / Cloudflare secret.
 */

#include "license.h"

#include <Preferences.h>
#include <esp_mac.h>
#include <mbedtls/md.h>
#include <mbedtls/pk.h>
#include <ctype.h>
#include <stdarg.h>
#include <string.h>

#include "license_config.h"
#include "log.h"

// Route license diagnostics into the existing A12E log ring + serial.
static void webLog(const char *format, ...) {
  char line[160];
  va_list args;
  va_start(args, format);
  vsnprintf(line, sizeof(line), format, args);
  va_end(args);
  LOG.println(line);
}

// ============================================================================
// State
// ============================================================================

static Preferences prefs;
static LicenseState state = LicenseState::Unclaimed;
static String deviceId;
static String storedKey;
static String storedTrialToken;

static uint32_t trialUsedSec = 0;
static uint32_t trialTotalSec = LICENSE_TRIAL_SECONDS;
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

static bool verifySignature(const uint8_t *payload, size_t payloadLen,
                            const uint8_t *signature, size_t signatureLen) {
  mbedtls_pk_context pk;
  mbedtls_pk_init(&pk);

  bool ok = false;

  const int parseResult = mbedtls_pk_parse_public_key(
      &pk, reinterpret_cast<const unsigned char *>(LICENSE_PUBLIC_KEY_PEM),
      sizeof(LICENSE_PUBLIC_KEY_PEM));

  if (parseResult != 0) {
    webLog("[LIC] Public key parse failed: -0x%04X", -parseResult);
  } else {
    const mbedtls_md_info_t *md = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    uint8_t hash[32];

    if (md == nullptr || mbedtls_md(md, payload, payloadLen, hash) != 0) {
      webLog("[LIC] SHA-256 failed");
    } else {
      const int verifyResult = mbedtls_pk_verify(
          &pk, MBEDTLS_MD_SHA256, hash, 0, signature, signatureLen);
      if (verifyResult == 0) {
        ok = true;
      } else {
        webLog("[LIC] Signature verify failed: -0x%04X", -verifyResult);
      }
    }
  }

  mbedtls_pk_free(&pk);
  return ok;
}

static bool decodeAndVerify(const String &rawToken, String &payloadText,
                            String *reason = nullptr) {
  const auto fail = [&](const char *message) {
    if (reason != nullptr) *reason = message;
    return false;
  };

  String token = rawToken;
  token.trim();

  if (token.length() < 32 || token.length() > 400) {
    return fail("Token length is not valid");
  }

  const int dot = token.indexOf('.');
  if (dot <= 0 || token.indexOf('.', dot + 1) >= 0) {
    return fail("Token format is not valid");
  }

  const String payloadPart = token.substring(0, dot);
  const String signaturePart = token.substring(dot + 1);

  uint8_t payload[160];
  uint8_t signature[96];
  size_t payloadLen = 0;
  size_t signatureLen = 0;

  if (!base64UrlDecode(payloadPart, payload, sizeof(payload), payloadLen) ||
      payloadLen == 0) {
    return fail("Token payload is corrupted");
  }
  if (!base64UrlDecode(signaturePart, signature, sizeof(signature),
                       signatureLen) ||
      signatureLen == 0) {
    return fail("Token signature is corrupted");
  }

  if (!verifySignature(payload, payloadLen, signature, signatureLen)) {
    return fail("Invalid token (signature mismatch)");
  }

  char text[sizeof(payload) + 1];
  memcpy(text, payload, payloadLen);
  text[payloadLen] = '\0';
  payloadText = text;
  return true;
}

static bool verifyKey(const String &rawKey, String *reason = nullptr) {
  const auto fail = [&](const char *message) {
    if (reason != nullptr) *reason = message;
    return false;
  };

  String text;
  if (!decodeAndVerify(rawKey, text, reason)) return false;

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

static bool verifyTrialToken(const String &rawToken, uint32_t &seconds,
                             String *reason = nullptr) {
  const auto fail = [&](const char *message) {
    if (reason != nullptr) *reason = message;
    return false;
  };

  String text;
  if (!decodeAndVerify(rawToken, text, reason)) return false;

  const int p1 = text.indexOf('|');
  const int p2 = text.indexOf('|', p1 + 1);
  const int p3 = text.indexOf('|', p2 + 1);
  if (p1 <= 0 || p2 <= p1 || p3 <= p2 || text.indexOf('|', p3 + 1) >= 0) {
    return fail("Trial payload is not valid");
  }

  const String version = text.substring(0, p1);
  const String tokenDevice = text.substring(p1 + 1, p2);
  const String secondsText = text.substring(p2 + 1, p3);
  const String serial = text.substring(p3 + 1);

  if (version != LICENSE_TRIAL_CODE) {
    return fail("Token is for a different product");
  }
  if (tokenDevice != deviceId) {
    return fail("Token is for a different device");
  }
  if (serial.length() == 0) {
    return fail("Trial serial is missing");
  }
  if (secondsText.length() == 0 || secondsText.length() > 9) {
    return fail("Trial duration is not valid");
  }
  for (size_t i = 0; i < secondsText.length(); ++i) {
    if (!isdigit(static_cast<unsigned char>(secondsText[i]))) {
      return fail("Trial duration is not valid");
    }
  }

  const unsigned long parsed = strtoul(secondsText.c_str(), nullptr, 10);
  if (parsed < 60 || parsed > LICENSE_MAX_TRIAL_SECONDS) {
    return fail("Trial duration is not valid");
  }

  seconds = static_cast<uint32_t>(parsed);
  return true;
}

static uint32_t remainingTrialSec() {
  if (trialUsedSec >= trialTotalSec) return 0;
  return trialTotalSec - trialUsedSec;
}

static void refreshState() {
  if (state == LicenseState::Licensed) return;
  // Silent local trial: starts automatically on first boot, no token claim.
  state = (trialUsedSec >= trialTotalSec) ? LicenseState::Expired
                                          : LicenseState::Trial;
}

static void persistTrial(bool force) {
  if (state != LicenseState::Trial && state != LicenseState::Expired) return;
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
    webLog("[LIC] WARNING: could not read MAC address");
  }

  char id[24];
  snprintf(id, sizeof(id), "%s%02X%02X%02X%02X%02X%02X",
           LICENSE_DEVICE_PREFIX, mac[0], mac[1], mac[2], mac[3], mac[4],
           mac[5]);
  deviceId = id;

  if (!prefs.begin(LICENSE_NVS_NAMESPACE, false)) {
    webLog("[LIC] WARNING: NVS unavailable - trial will not persist");
  }

  trialUsedSec = prefs.getUInt("used", 0);
  storedKey = prefs.getString("key", "");
  storedTrialToken = prefs.getString("trial", "");
  trialTotalSec = LICENSE_TRIAL_SECONDS;
  state = LicenseState::Trial;

  if (storedKey.length() > 0) {
    if (verifyKey(storedKey)) {
      state = LicenseState::Licensed;
    } else {
      webLog("[LIC] Stored key is not valid for this device - ignored");
    }
  }

  refreshState();
  lastTickMs = millis();
  unsavedSec = 0;

  webLog("[LIC] Device ID: %s", deviceId.c_str());
  switch (state) {
    case LicenseState::Licensed:
      webLog("[LIC] Status: LICENSED");
      break;
    case LicenseState::Expired:
      webLog("[LIC] Status: TRIAL ENDED - activation required");
      break;
    case LicenseState::Trial:
      webLog("[LIC] Status: ACTIVE, %luh %lum remaining",
             remainingTrialSec() / 3600UL,
             (remainingTrialSec() % 3600UL) / 60UL);
      break;
    default:
      webLog("[LIC] Status: ACTIVE");
      break;
  }
}

void licenseLoop() {
  if (state != LicenseState::Trial) return;

  const unsigned long now = millis();
  const unsigned long deltaMs = now - lastTickMs;
  if (deltaMs < 1000) return;

  const uint32_t elapsed = deltaMs / 1000;
  lastTickMs += elapsed * 1000UL;

  trialUsedSec += elapsed;
  unsavedSec += elapsed;

  if (trialUsedSec >= trialTotalSec) {
    trialUsedSec = trialTotalSec;
    state = LicenseState::Expired;
    persistTrial(true);
    webLog("[LIC] TRIAL ENDED - monitor locked");
  } else {
    persistTrial(false);
  }
}

bool licenseActivate(const String &key, String &error) {
  if (!verifyKey(key, &error)) {
    webLog("[LIC] Activation rejected: %s", error.c_str());
    return false;
  }

  String cleaned = key;
  cleaned.trim();

  if (prefs.putString("key", cleaned) == 0) {
    error = "Could not save key (NVS write failed)";
    webLog("[LIC] Activation failed: %s", error.c_str());
    return false;
  }

  storedKey = cleaned;
  state = LicenseState::Licensed;
  webLog("[LIC] Activated successfully");
  return true;
}

bool licenseClaimTrial(const String &token, String &error) {
  // Tokens are no longer used: every device gets an automatic silent 6 h trial.
  (void)token;
  error = "Trial tokens are no longer used";
  return false;
}

#if 0
bool licenseClaimTrialLegacy(const String &token, String &error) {
  if (state == LicenseState::Licensed) {
    error = "Device is already licensed";
    return false;
  }
  if (storedTrialToken.length() > 0) {
    error = "Trial already claimed on this device";
    return false;
  }

  uint32_t seconds = 0;
  if (!verifyTrialToken(token, seconds, &error)) {
    webLog("[LIC] Trial claim rejected: %s", error.c_str());
    return false;
  }

  String cleaned = token;
  cleaned.trim();

  if (prefs.putString("trial", cleaned) == 0) {
    error = "Could not save trial token (NVS write failed)";
    webLog("[LIC] Trial claim failed: %s", error.c_str());
    return false;
  }

  storedTrialToken = cleaned;
  trialTotalSec = seconds;
  trialUsedSec = 0;
  unsavedSec = 0;
  prefs.putUInt("used", 0);
  state = LicenseState::Trial;
  lastTickMs = millis();

  webLog("[LIC] Trial claimed: %lu seconds", static_cast<unsigned long>(seconds));
  return true;
}
#endif

LicenseState licenseGetState() { return state; }

bool licenseIsLocked() {
  return state == LicenseState::Expired || state == LicenseState::Unclaimed;
}

uint32_t licenseTrialRemainingSec() { return remainingTrialSec(); }

uint32_t licenseTrialTotalSec() { return trialTotalSec; }

const String &licenseDeviceId() { return deviceId; }

bool licenseHasStoredKey() { return storedKey.length() > 0; }

bool licenseHasTrialToken() { return storedTrialToken.length() > 0; }

const char *licenseStateName() {
  switch (state) {
    case LicenseState::Licensed:
      return "licensed";
    case LicenseState::Expired:
      return "expired";
    case LicenseState::Trial:
      return "trial";
    default:
      return "unclaimed";
  }
}
