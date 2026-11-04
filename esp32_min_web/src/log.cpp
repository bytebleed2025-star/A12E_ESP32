#include "log.h"

Logger LOG;

static LogEntry entries[LOG_CAPACITY];
static size_t head = 0;       // next slot to write
static size_t count = 0;      // valid entries (0..LOG_CAPACITY)
static uint32_t nextSeq = 1;  // first seq is 1
static uint32_t dropped = 0;  // overwritten before being read

// ============================================================================
// Logger
// ============================================================================

void Logger::commitLine() {
  if (lineLen_ == 0) return;

  LogEntry &entry = entries[head];
  entry.seq = nextSeq++;
  entry.ms = millis();

  size_t copyLen = lineLen_;
  if (copyLen >= sizeof(entry.text)) copyLen = sizeof(entry.text) - 1;
  memcpy(entry.text, line_, copyLen);
  entry.text[copyLen] = '\0';

  head = (head + 1) % LOG_CAPACITY;
  if (count < LOG_CAPACITY) {
    count++;
  } else {
    dropped++;
  }

  lineLen_ = 0;
}

size_t Logger::write(uint8_t c) {
  // Keep the USB serial output exactly as before.
  Serial.write(c);

  if (c == '\r') return 1;

  if (c == '\n') {
    commitLine();
    return 1;
  }

  if (lineLen_ >= sizeof(line_) - 1) {
    // Overlong line: commit what we have and start a new line.
    commitLine();
  }

  line_[lineLen_++] = static_cast<char>(c);
  return 1;
}

size_t Logger::write(const uint8_t *buffer, size_t size) {
  size_t written = 0;
  while (written < size) {
    written += write(buffer[written]);
  }
  return written;
}

// ============================================================================
// Buffer access
// ============================================================================

uint32_t logNextSeq() { return nextSeq; }

uint32_t logDropped() { return dropped; }

size_t logCopySince(uint32_t since, LogEntry *out, size_t maxOut) {
  if (out == nullptr || maxOut == 0) return 0;

  const size_t start = (head + LOG_CAPACITY - count) % LOG_CAPACITY;
  size_t copied = 0;

  for (size_t i = 0; i < count && copied < maxOut; i++) {
    const LogEntry &entry = entries[(start + i) % LOG_CAPACITY];
    if (entry.seq > since) {
      out[copied++] = entry;
    }
  }

  return copied;
}
