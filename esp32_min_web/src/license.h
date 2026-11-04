/**
 * @file license.h
 * @brief Trial counter + offline activation keys
 */

#ifndef LICENSE_H
#define LICENSE_H

#include <Arduino.h>

enum class LicenseState : uint8_t {
  Trial,     // trial running
  Licensed,  // valid activation key stored
  Expired    // trial over and no key
};

/** Load trial counter + stored key from NVS and verify the key. */
void licenseBegin();

/** Advance the powered-on trial timer (call from loop()). */
void licenseLoop();

/**
 * Verify and store an activation key.
 * @param key   key string from tools/make_license.js
 * @param error human readable reason on failure
 * @return true when the key is valid for THIS device and has been stored
 */
bool licenseActivate(const String &key, String &error);

LicenseState licenseGetState();
bool licenseIsLocked();
uint32_t licenseTrialRemainingSec();
uint32_t licenseTrialTotalSec();
const String &licenseDeviceId();
bool licenseHasStoredKey();

/** JSON-friendly name: "trial", "licensed", "expired". */
const char *licenseStateName();

#endif // LICENSE_H
