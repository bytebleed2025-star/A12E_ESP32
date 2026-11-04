/**
 * @file sheets.h
 * @brief Google Sheets uploader (via a Google Apps Script Web App).
 */

#ifndef SHEETS_H
#define SHEETS_H

#include <Arduino.h>

void sheetsBegin();
void sheetsLoop(float weight, bool weightValid);

/** Reset the upload timer after settings change / reconnect. */
void sheetsResetSchedule();

/** Send an immediate test row (ignores trial/scale state). */
bool sheetsTest(String &message, int &httpCode);

long sheetsLastUploadAgeMs();
bool sheetsLastUploadOk();
uint32_t sheetsUploadOkCount();
uint32_t sheetsUploadFailCount();

#endif // SHEETS_H
