#include "ir_manager.h"

#include <math.h>

#include "app_state.h"

bool beginIrManager() {
    irDetected = irSensor.begin();
    return irDetected;
}

bool readIrTemperatures(float& ambientC, float& objectC) {
    if (!irDetected) {
        return false;
    }

    ambientC = irSensor.readAmbientTempC();
    objectC = irSensor.readObjectTempC();

    if (isnan(ambientC) || isnan(objectC)) {
        return false;
    }

    lastIrAmbientC = ambientC;
    lastIrObjectC = objectC;
    lastIrReadMs = millis();
    return true;
}

void printIrStatus() {
    Serial.println("IR sensor:");

    Serial.print("  detected: ");
    Serial.println(irDetected ? "yes" : "no");

    if (!irDetected) {
        Serial.println("  error: MLX90614 not detected");
        return;
    }

    Serial.println("  type: MLX90614 / GY-906 / HW-691");
    Serial.println("  i2c address: 0x5A");

    if (!isnan(lastIrAmbientC) && !isnan(lastIrObjectC)) {
        Serial.print("  last ambient: ");
        Serial.print(lastIrAmbientC, 2);
        Serial.println(" C");

        Serial.print("  last object: ");
        Serial.print(lastIrObjectC, 2);
        Serial.println(" C");

        Serial.print("  last read age: ");
        Serial.print(millis() - lastIrReadMs);
        Serial.println(" ms");
    } else {
        Serial.println("  last reading: none yet");
    }
}

void printIrRead() {
    if (!irDetected) {
        Serial.println("IR sensor not detected");
        return;
    }

    float ambientC = NAN;
    float objectC = NAN;

    if (!readIrTemperatures(ambientC, objectC)) {
        Serial.println("IR read failed");
        return;
    }

    Serial.print("IR ambient=");
    Serial.print(ambientC, 2);
    Serial.print(" C  object=");
    Serial.print(objectC, 2);
    Serial.println(" C");
}
