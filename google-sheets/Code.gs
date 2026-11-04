/**
 * A12E Scale Monitor -> Google Sheets
 *
 * Setup:
 *  1. Create a Google Sheet.
 *  2. Extensions -> Apps Script.
 *  3. Replace Code.gs with this file.
 *  4. Change TOKEN to a long random string (same value as the Config tab).
 *  5. Project Settings -> Time zone -> Asia/Jakarta.
 *  6. Deploy -> New deployment -> Web app:
 *       Execute as: Me
 *       Who has access: Anyone
 *     Authorize, then copy the /exec URL.
 *  7. Paste the URL and TOKEN into the monitor Config tab.
 */

const SHEET_NAME = 'Data';
const TOKEN = 'CHANGE_ME_TO_A_LONG_RANDOM_TOKEN';

function doGet(e) {
  const p = e.parameter || {};

  if (p.token !== TOKEN) {
    return json_({ ok: false, error: 'unauthorized' });
  }

  const ss = SpreadsheetApp.getActiveSpreadsheet();
  let sheet = ss.getSheetByName(SHEET_NAME);
  if (!sheet) sheet = ss.insertSheet(SHEET_NAME);

  if (sheet.getLastRow() === 0) {
    sheet.appendRow([
      'Received At',
      'Device ID',
      'Weight (kg)',
      'License',
      'RSSI',
      'Uptime (s)',
      'Device Time',
      'Test',
    ]);
    sheet.getRange(1, 1, 1, 8).setFontWeight('bold');
    sheet.setFrozenRows(1);
  }

  sheet.appendRow([
    new Date(),
    p.device || '',
    p.weight === undefined ? '' : Number(p.weight),
    p.license || '',
    p.rssi === undefined ? '' : Number(p.rssi),
    p.uptime === undefined ? '' : Number(p.uptime),
    p.timestamp || '',
    p.test ? 'TEST' : '',
  ]);

  return json_({ ok: true });
}

function doPost(e) {
  // Optional JSON POST support for future firmware.
  try {
    const body = JSON.parse((e && e.postData && e.postData.contents) || '{}');
    return doGet({ parameter: body });
  } catch (err) {
    return json_({ ok: false, error: String(err) });
  }
}

function json_(obj) {
  return ContentService
    .createTextOutput(JSON.stringify(obj))
    .setMimeType(ContentService.MimeType.JSON);
}
