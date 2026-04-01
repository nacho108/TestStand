#pragma once

#include "app_types.h"

void beginEscTelemetry();
uint8_t updateCrc8(uint8_t crc, uint8_t crcSeed);
uint8_t calculateCrc8(const uint8_t* buf, uint8_t len);
bool tryReadTelemetryFrame(EscTelemetry& outTlm);
void pollEscTelemetry();
float estimateMechanicalRpm(uint16_t rpmField, int magnetCount);
