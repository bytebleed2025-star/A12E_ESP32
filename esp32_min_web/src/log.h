/**
 * @file log.h
 * @brief Tiny in-RAM log buffer + Serial tee for the web debug page.
 *
 * Every line written through LOG is:
 *   1. written to the USB serial port (same as before), and
 *   2. stored in a fixed-size ring buffer readable at /api/log.
 *
 * RAM only: a reboot clears the buffer. Do not put secrets in log messages.
 */

#ifndef LOG_H
#define LOG_H

#include <Arduino.h>

#ifndef LOG_CAPACITY
#define LOG_CAPACITY 48
#endif

#ifndef LOG_LINE_LENGTH
#define LOG_LINE_LENGTH 160
#endif

struct LogEntry {
  uint32_t seq;
  uint32_t ms;
  char text[LOG_LINE_LENGTH];
};

class Logger : public Print {
 public:
  using Print::write;

  size_t write(uint8_t c) override;
  size_t write(const uint8_t *buffer, size_t size) override;

 private:
  void commitLine();

  char line_[LOG_LINE_LENGTH];
  size_t lineLen_ = 0;
};

extern Logger LOG;

/** Sequence number that will be assigned to the next committed line. */
uint32_t logNextSeq();

/** Lines lost because the ring buffer wrapped before the client read them. */
uint32_t logDropped();

/** Copy entries with seq > since, oldest first. Returns the number copied. */
size_t logCopySince(uint32_t since, LogEntry *out, size_t maxOut);

#endif // LOG_H
