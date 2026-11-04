/**
 * @file net.h
 * @brief AP+STA WiFi management.
 */

#ifndef NET_H
#define NET_H

#include <Arduino.h>

void netBegin();
void netLoop();

void netRestartSta();
void netScheduleReboot(uint32_t delayMs);

bool netStaConnected();
bool netStaConfigured();
String netStaIP();
String netApIP();
String netStaStatus();
int netRssi();

/** Build a JSON object with nearby WiFi networks. */
String netScanJson();

#endif // NET_H
