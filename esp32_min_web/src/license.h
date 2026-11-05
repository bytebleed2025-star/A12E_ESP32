/**
 * @file license.h
 * @brief One-time trial token + offline activation keys for the A12E monitor.
 */

#ifndef LICENSE_H
#define LICENSE_H

#include <Arduino.h>

enum class LicenseState : uint8_t {
  Unclaimed,
  Trial,
  Licensed,
  Expired
};

void licenseBegin();
void licenseLoop();
bool licenseActivate(const String &key, String &error);
bool licenseClaimTrial(const String &token, String &error);

LicenseState licenseGetState();
bool licenseIsLocked();
uint32_t licenseTrialRemainingSec();
uint32_t licenseTrialTotalSec();
const String &licenseDeviceId();
bool licenseHasStoredKey();
bool licenseHasTrialToken();
const char *licenseStateName();

#endif // LICENSE_H
