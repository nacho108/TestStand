#include "esc_telemetry.h"

#include "app_config.h"
#include "app_state.h"

void beginEscTelemetry() {
    escSerial.begin(115200, SERIAL_8N1, ESC_TLM_RX_PIN, -1);
}

uint8_t updateCrc8(uint8_t crc, uint8_t crcSeed) {
    uint8_t c = crc ^ crcSeed;
    for (int i = 0; i < 8; i++) {
        c = (c & 0x80) ? (uint8_t)(0x07 ^ (c << 1)) : (uint8_t)(c << 1);
    }
    return c;
}

uint8_t calculateCrc8(const uint8_t* buf, uint8_t len) {
    uint8_t crc = 0;
    for (uint8_t i = 0; i < len; i++) {
        crc = updateCrc8(buf[i], crc);
    }
    return crc;
}

bool tryReadTelemetryFrame(EscTelemetry& outTlm) {
    static uint8_t frame[10];

    while (escSerial.available() >= 10) {
        size_t bytesRead = escSerial.readBytes(frame, 10);
        if (bytesRead != 10) {
            return false;
        }

        uint8_t crc = calculateCrc8(frame, 9);
        if (crc != frame[9]) {
            continue;
        }

        outTlm.valid = true;
        outTlm.temperatureC = frame[0];
        outTlm.voltageRaw = (uint16_t(frame[1]) << 8) | frame[2];
        outTlm.currentRaw = (uint16_t(frame[3]) << 8) | frame[4];
        outTlm.consumptionRaw = (uint16_t(frame[5]) << 8) | frame[6];
        outTlm.rpmField = (uint16_t(frame[7]) << 8) | frame[8];
        outTlm.lastUpdateMs = millis();
        return true;
    }

    return false;
}

void pollEscTelemetry() {
    EscTelemetry tlm;
    if (tryReadTelemetryFrame(tlm)) {
        lastTlm = tlm;
    }
}

float estimateMechanicalRpm(uint16_t rpmField, int magnetCount) {
    if (magnetCount <= 0) return 0.0f;
    float polePairs = magnetCount / 2.0f;
    return (rpmField * 100.0f) / polePairs;
}
