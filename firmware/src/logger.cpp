#include "logger.h"

static String logBuffer;
static const size_t MAX_LOG_BUFFER = 4096;

static String timestamp() {
  unsigned long seconds = millis() / 1000;
  unsigned long h = seconds / 3600;
  unsigned long m = (seconds % 3600) / 60;
  unsigned long s = seconds % 60;
  char buf[16];
  snprintf(buf, sizeof(buf), "%02lu:%02lu:%02lu", h, m, s);
  return String(buf);
}

static void appendLog(const String &level, const String &message) {
  String line = timestamp() + " [" + level + "] " + message;
  Serial.println(line);
  logBuffer += line + "\n";
  if (logBuffer.length() > MAX_LOG_BUFFER) {
    logBuffer.remove(0, logBuffer.length() - MAX_LOG_BUFFER);
  }
}

void logInfo(const String &message) {
  appendLog("INFO", message);
}

void logWarn(const String &message) {
  appendLog("WARN", message);
}

void logError(const String &message) {
  appendLog("ERROR", message);
}

String getLogBuffer() {
  return logBuffer;
}
