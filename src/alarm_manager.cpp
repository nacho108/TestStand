#include "alarm_manager.h"

#include <LittleFS.h>
#include <math.h>

#include "app_state.h"
#include "time_sync.h"

namespace {

constexpr const char* kAlarmPrefsNamespace = "alarms";
constexpr const char* kAlarmLastReadKey = "last_read";
constexpr const char* kAlarmLogDir = "/Log";
constexpr const char* kAlarmLogPath = "/Log/alarms.log";
constexpr size_t kRecentAlarmCapacity = 10;

AlarmEntry recentAlarms[kRecentAlarmCapacity];
size_t recentAlarmCount = 0;
unsigned long latestAlarmSequence = 0;
unsigned long lastReadAlarmSequence = 0;
bool alarmManagerReady = false;

String sanitizeAlarmField(const String& value) {
    String sanitized = value;
    sanitized.replace("\t", " ");
    sanitized.replace("\r", " ");
    sanitized.replace("\n", " ");
    return sanitized;
}

String makeSeverityLabel(SafetyLevel level) {
    return level == SafetyLevel::HiHi ? "HIHI" : "HI";
}

String makeSourceLabel(const char* sourceKey) {
    if (sourceKey == nullptr) {
        return "System";
    }

    if (strcmp(sourceKey, "current") == 0) {
        return "Current";
    }
    if (strcmp(sourceKey, "esc_temp") == 0) {
        return "ESC temp";
    }
    if (strcmp(sourceKey, "motor_temp") == 0) {
        return "Motor temp";
    }

    return String(sourceKey);
}

String makeAlarmMessage(const String& source, const String& severity, float value, float threshold, const char* unit) {
    String message = source;
    message += " ";
    message += severity;
    message += " threshold reached";

    if (isfinite(value) && isfinite(threshold) && unit != nullptr) {
        message += " (";
        message += String(value, 2);
        message += " ";
        message += unit;
        message += " >= ";
        message += String(threshold, 2);
        message += " ";
        message += unit;
        message += ")";
    }

    if (severity == "HIHI") {
        message += ". Motor stopped and active test stopped.";
    }

    return message;
}

void pushRecentAlarm(const AlarmEntry& entry) {
    const size_t cappedCount = recentAlarmCount < kRecentAlarmCapacity ? recentAlarmCount : kRecentAlarmCapacity - 1;
    for (size_t i = cappedCount; i > 0; --i) {
        recentAlarms[i] = recentAlarms[i - 1];
    }
    recentAlarms[0] = entry;
    if (recentAlarmCount < kRecentAlarmCapacity) {
        ++recentAlarmCount;
    }
}

bool ensureAlarmStorage() {
    if (!LittleFS.exists(kAlarmLogDir) && !LittleFS.mkdir(kAlarmLogDir)) {
        return false;
    }

    if (!LittleFS.exists(kAlarmLogPath)) {
        File file = LittleFS.open(kAlarmLogPath, FILE_WRITE);
        if (!file) {
            return false;
        }
        file.close();
    }

    return true;
}

float parseAlarmFloatField(const String& field) {
    if (field == "nan" || field.length() == 0) {
        return NAN;
    }
    return field.toFloat();
}

bool parseAlarmLine(const String& line, AlarmEntry& outEntry) {
    int separators[7];
    int separatorCount = 0;
    for (size_t i = 0; i < line.length() && separatorCount < 7; ++i) {
        if (line.charAt(i) == '\t') {
            separators[separatorCount++] = (int)i;
        }
    }

    if (separatorCount != 7) {
        return false;
    }

    outEntry.sequence = strtoul(line.substring(0, separators[0]).c_str(), nullptr, 10);
    outEntry.timestampMs = strtoull(
        line.substring(separators[0] + 1, separators[1]).c_str(),
        nullptr,
        10
    );
    outEntry.source = line.substring(separators[1] + 1, separators[2]);
    outEntry.severity = line.substring(separators[2] + 1, separators[3]);
    outEntry.value = parseAlarmFloatField(line.substring(separators[3] + 1, separators[4]));
    outEntry.threshold = parseAlarmFloatField(line.substring(separators[4] + 1, separators[5]));
    outEntry.unit = line.substring(separators[5] + 1, separators[6]);
    outEntry.message = line.substring(separators[6] + 1);
    return outEntry.sequence > 0;
}

String jsonEscape(const String& value) {
    String escaped;
    escaped.reserve(value.length() + 8);
    for (size_t i = 0; i < value.length(); ++i) {
        const char c = value.charAt(i);
        if (c == '\\' || c == '"') {
            escaped += "\\";
        }
        escaped += c;
    }
    return escaped;
}

void loadRecentAlarmsFromDisk() {
    recentAlarmCount = 0;
    latestAlarmSequence = 0;

    File file = LittleFS.open(kAlarmLogPath, FILE_READ);
    if (!file) {
        return;
    }

    while (file.available()) {
        const String line = file.readStringUntil('\n');
        AlarmEntry entry;
        if (!parseAlarmLine(line, entry)) {
            continue;
        }

        latestAlarmSequence = max(latestAlarmSequence, entry.sequence);
        pushRecentAlarm(entry);
    }

    file.close();
}

void persistLastReadAlarmSequence() {
    preferences.begin(kAlarmPrefsNamespace, false);
    preferences.putULong(kAlarmLastReadKey, lastReadAlarmSequence);
    preferences.end();
}

}

bool beginAlarmManager() {
    if (!ensureAlarmStorage()) {
        return false;
    }

    preferences.begin(kAlarmPrefsNamespace, true);
    lastReadAlarmSequence = preferences.getULong(kAlarmLastReadKey, 0);
    preferences.end();

    loadRecentAlarmsFromDisk();
    if (lastReadAlarmSequence > latestAlarmSequence) {
        lastReadAlarmSequence = latestAlarmSequence;
        persistLastReadAlarmSequence();
    }

    alarmManagerReady = true;
    return true;
}

void logSafetyAlarm(const char* sourceKey, SafetyLevel level, float value, float threshold, const char* unit) {
    if (!alarmManagerReady) {
        return;
    }

    if (level != SafetyLevel::Hi && level != SafetyLevel::HiHi) {
        return;
    }

    AlarmEntry entry;
    entry.sequence = latestAlarmSequence + 1;
    entry.timestampMs = getCurrentTimeMs();
    entry.source = makeSourceLabel(sourceKey);
    entry.severity = makeSeverityLabel(level);
    entry.message = makeAlarmMessage(entry.source, entry.severity, value, threshold, unit);
    entry.value = value;
    entry.threshold = threshold;
    entry.unit = unit == nullptr ? "" : String(unit);

    File file = LittleFS.open(kAlarmLogPath, FILE_APPEND);
    if (!file) {
        Serial.println("WARNING: Failed to append alarm log entry.");
        return;
    }

    String line;
    line.reserve(192);
    line += String(entry.sequence);
    line += "\t";
    line += String((unsigned long long)entry.timestampMs);
    line += "\t";
    line += sanitizeAlarmField(entry.source);
    line += "\t";
    line += sanitizeAlarmField(entry.severity);
    line += "\t";
    line += isfinite(entry.value) ? String(entry.value, 3) : "nan";
    line += "\t";
    line += isfinite(entry.threshold) ? String(entry.threshold, 3) : "nan";
    line += "\t";
    line += sanitizeAlarmField(entry.unit);
    line += "\t";
    line += sanitizeAlarmField(entry.message);
    line += "\n";

    const size_t written = file.print(line);
    file.close();
    if (written != line.length()) {
        Serial.println("WARNING: Failed to fully write alarm log entry.");
        return;
    }

    latestAlarmSequence = entry.sequence;
    pushRecentAlarm(entry);

    Serial.print("ALARM: ");
    Serial.println(entry.message);
}

size_t getUnreadAlarmCount() {
    if (latestAlarmSequence <= lastReadAlarmSequence) {
        return 0;
    }

    return (size_t)(latestAlarmSequence - lastReadAlarmSequence);
}

size_t getTotalAlarmCount() {
    return (size_t)latestAlarmSequence;
}

void markAllAlarmsRead() {
    if (lastReadAlarmSequence == latestAlarmSequence) {
        return;
    }

    lastReadAlarmSequence = latestAlarmSequence;
    persistLastReadAlarmSequence();
}

String buildRecentAlarmsJson(size_t limit) {
    String json = "[";
    const size_t count = min(limit, recentAlarmCount);
    for (size_t i = 0; i < count; ++i) {
        const AlarmEntry& entry = recentAlarms[i];
        json += "{";
        json += "\"sequence\":";
        json += String(entry.sequence);
        json += ",\"timestamp_ms\":";
        json += String((unsigned long long)entry.timestampMs);
        json += ",\"source\":\"";
        json += jsonEscape(entry.source);
        json += "\",\"severity\":\"";
        json += jsonEscape(entry.severity);
        json += "\",\"message\":\"";
        json += jsonEscape(entry.message);
        json += "\",\"value\":";
        json += isfinite(entry.value) ? String(entry.value, 3) : "null";
        json += ",\"threshold\":";
        json += isfinite(entry.threshold) ? String(entry.threshold, 3) : "null";
        json += ",\"unit\":\"";
        json += jsonEscape(entry.unit);
        json += "\"}";

        if (i + 1 < count) {
            json += ",";
        }
    }
    json += "]";
    return json;
}
