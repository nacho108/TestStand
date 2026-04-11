#include "time_sync.h"

namespace {

bool timeSynced = false;
uint64_t syncedEpochMs = 0;
unsigned long syncedAtMs = 0;

}

void setCurrentTimeFromEpochMs(uint64_t epochMs) {
    timeSynced = true;
    syncedEpochMs = epochMs;
    syncedAtMs = millis();
}

bool hasCurrentTimeSync() {
    return timeSynced;
}

uint64_t getCurrentTimeMs() {
    if (!timeSynced) {
        return 0;
    }

    return syncedEpochMs + (uint64_t)(millis() - syncedAtMs);
}
