#include "motor_control.h"

#include <cstring>
#include <SoftwareSerial.h>

#include "app_config.h"
#include "app_state.h"
#include "esc_telemetry.h"

namespace {

constexpr unsigned long ESC_BOOTLOADER_BAUD = 19200;
const char* PASSTHROUGH_PREF_KEY = "pt_once";
const char* MOTOR_POLES_PREF_KEY = "motor_poles";

EspSoftwareSerial::UART escPassthroughSerial;
bool enable4Way = false;
uint8_t serialRx[300] = {0};
uint16_t escCrc = 0;

constexpr uint8_t MSP_API_VERSION = 1;
constexpr uint8_t MSP_FC_VARIANT = 2;
constexpr uint8_t MSP_FC_VERSION = 3;
constexpr uint8_t MSP_BOARD_INFO = 4;
constexpr uint8_t MSP_BUILD_INFO = 5;
constexpr uint8_t MSP_FEATURE_CONFIG = 36;
constexpr uint8_t MSP_ADVANCED_CONFIG = 90;
constexpr uint8_t MSP_STATUS = 101;
constexpr uint8_t MSP_MOTOR = 104;
constexpr uint8_t MSP_BOXIDS = 119;
constexpr uint8_t MSP_MOTOR_3D_CONFIG = 124;
constexpr uint8_t MSP_MOTOR_CONFIG = 131;
constexpr uint8_t MSP_UID = 160;
constexpr uint8_t MSP_SET_4WAY_IF = 245;

constexpr uint8_t MSP_PROTOCOL_VERSION = 0;
constexpr uint8_t API_VERSION_MAJOR = 1;
constexpr uint8_t API_VERSION_MINOR = 42;
constexpr uint8_t FC_VERSION_MAJOR = 4;
constexpr uint8_t FC_VERSION_MINOR = 1;
constexpr uint8_t FC_VERSION_PATCH_LEVEL = 0;

constexpr uint8_t IM_ARM_BLB = 4;
constexpr char SERIAL_4WAY_INTERFACE_NAME_STR[] = "m4wFCIntf";
constexpr uint8_t SERIAL_4WAY_PROTOCOL_VER = 108;
constexpr uint8_t SERIAL_4WAY_VERSION_HI = 200;
constexpr uint8_t SERIAL_4WAY_VERSION_LO = 5;

constexpr uint8_t CMD_REMOTE_ESCAPE = 0x2E;
constexpr uint8_t CMD_LOCAL_ESCAPE = 0x2F;
constexpr uint8_t CMD_INTERFACE_TEST_ALIVE = 0x30;
constexpr uint8_t CMD_PROTOCOL_GET_VERSION = 0x31;
constexpr uint8_t CMD_INTERFACE_GET_NAME = 0x32;
constexpr uint8_t CMD_INTERFACE_GET_VERSION = 0x33;
constexpr uint8_t CMD_INTERFACE_EXIT = 0x34;
constexpr uint8_t CMD_DEVICE_RESET = 0x35;
constexpr uint8_t CMD_DEVICE_INIT_FLASH = 0x37;
constexpr uint8_t CMD_DEVICE_PAGE_ERASE = 0x39;
constexpr uint8_t CMD_DEVICE_READ = 0x3A;
constexpr uint8_t CMD_DEVICE_WRITE = 0x3B;
constexpr uint8_t CMD_INTERFACE_SET_MODE = 0x3F;
constexpr uint8_t CMD_DEVICE_VERIFY = 0x40;

constexpr uint8_t ACK_OK = 0x00;
constexpr uint8_t ACK_I_INVALID_CRC = 0x03;
constexpr uint8_t ACK_I_INVALID_CHANNEL = 0x08;
constexpr uint8_t ACK_I_INVALID_PARAM = 0x09;
constexpr uint8_t ACK_D_GENERAL_ERROR = 0x0F;

constexpr uint8_t RESTART_BOOTLOADER = 0x00;
constexpr uint8_t CMD_PROG_FLASH = 0x01;
constexpr uint8_t CMD_ERASE_FLASH = 0x02;
constexpr uint8_t CMD_READ_FLASH_SIL = 0x03;
constexpr uint8_t CMD_KEEP_ALIVE = 0xFD;
constexpr uint8_t CMD_SET_BUFFER = 0xFE;
constexpr uint8_t CMD_SET_ADDRESS = 0xFF;
constexpr uint8_t BR_SUCCESS = 0x30;
constexpr uint8_t ESC_PARAM_READ_SIZE = 48 + 32;
constexpr uint16_t ESC_EEPROM_REGION_SIZE = 1024;
constexpr uint8_t ESC_EEPROM_DUMP_CHUNK_SIZE = 64;
constexpr uint8_t ESC_EEPROM_SUMMARY_SIZE = 48;
constexpr uint8_t ESC_LEGACY_EEPROM_WRITE_SIZE = 176;
constexpr uint8_t ESC_LEGACY_NAME_OFFSET = 5;
constexpr uint8_t ESC_LEGACY_NAME_LENGTH = 12;
constexpr uint8_t ESC_LEGACY_DIR_REVERSED_OFFSET = 17;
constexpr uint8_t ESC_LEGACY_MOTOR_KV_OFFSET = 26;
constexpr uint8_t ESC_LEGACY_MOTOR_POLES_OFFSET = 27;

struct EscBootInfo {
    uint8_t pinCode = 0;
    uint8_t flashSizeCode = 0;
    uint8_t infoByte5 = 0;
    uint8_t infoByte6 = 0;
    uint8_t protocolVersion = 0;
    uint8_t addressShift = 0;
    uint32_t absoluteEepromAddress = 0;
    uint16_t encodedEepromAddress = 0;
    const char* deviceName = "Unknown";
};

void initEscSerial();
void deinitEscSerial();
uint16_t sendEsc(uint8_t txBuf[], uint16_t bufSize, bool sendCrc);
uint16_t getEsc(uint8_t rxBuf[], uint16_t waitMs);
void prepareEscDirectSession();

bool isValidMotorPoleCount(int poleCount) {
    return poleCount >= 2 && poleCount <= 100;
}

bool isValidEscMotorKv(int motorKv) {
    return motorKv >= 20 && motorKv <= 10220;
}

uint8_t encodeLegacyEscMotorKv(int motorKv) {
    const int adjustedKv = motorKv - 20;
    const int raw = (adjustedKv + 20) / 40;
    return (uint8_t)constrain(raw, 0, 255);
}

int decodeLegacyEscMotorKv(uint8_t raw) {
    return (int)raw * 40 + 20;
}

void saveMotorPoleCount() {
    preferences.begin("am32cli", false);
    preferences.putInt(MOTOR_POLES_PREF_KEY, motorPoleCount);
    preferences.end();
}

uint16_t byteCrc(uint8_t data, uint16_t crc) {
    uint8_t xb = data;
    for (uint8_t i = 0; i < 8; i++) {
        if (((xb & 0x01) ^ (crc & 0x0001)) != 0) {
            crc = (crc >> 1) ^ 0xA001;
        } else {
            crc >>= 1;
        }
        xb >>= 1;
    }
    return crc;
}

uint16_t crcXmodemUpdate(uint16_t crc, uint8_t data) {
    crc ^= ((uint16_t)data << 8);
    for (int i = 0; i < 8; i++) {
        if (crc & 0x8000) {
            crc = (crc << 1) ^ 0x1021;
        } else {
            crc <<= 1;
        }
    }
    return crc;
}

void flushEscSerialRx() {
    while (escPassthroughSerial.available()) {
        escPassthroughSerial.read();
    }
}

void flushEscTelemetryRx() {
    while (escSerial.available()) {
        escSerial.read();
    }
}

void printHexByte(uint8_t value) {
    if (value < 0x10) {
        Serial.print('0');
    }
    Serial.print(value, HEX);
}

void printHexWord(uint16_t value) {
    if (value < 0x1000) Serial.print('0');
    if (value < 0x100) Serial.print('0');
    if (value < 0x10) Serial.print('0');
    Serial.print(value, HEX);
}

void printHexDword(uint32_t value) {
    for (int shift = 28; shift >= 0; shift -= 4) {
        Serial.print((value >> shift) & 0x0F, HEX);
    }
}

void printHexBuffer(const char* label, const uint8_t* data, uint16_t length) {
    Serial.print(label);
    Serial.print(" (");
    Serial.print(length);
    Serial.println(" bytes)");

    if (length == 0) {
        Serial.println("  <none>");
        return;
    }

    for (uint16_t i = 0; i < length; i++) {
        if ((i % 16) == 0) {
            Serial.print("  ");
        }

        printHexByte(data[i]);
        Serial.print(' ');

        if (((i % 16) == 15) || (i == length - 1)) {
            Serial.println();
        }
    }
}

bool isPrintableAsciiByte(uint8_t value) {
    return value >= 32 && value <= 126;
}

void printAsciiPreview(const char* label, const uint8_t* data, uint16_t length) {
    Serial.print(label);
    Serial.print(": ");
    for (uint16_t i = 0; i < length; i++) {
        Serial.print(isPrintableAsciiByte(data[i]) ? (char)data[i] : '.');
    }
    Serial.println();
}

uint16_t calculateEscResponseCrc(const uint8_t* data, uint16_t length) {
    uint16_t crc = 0;
    for (uint16_t i = 0; i < length; i++) {
        crc = byteCrc(data[i], crc);
    }
    return crc;
}

bool checkEscResponseCrc(const uint8_t* data, uint16_t payloadPlusCrcLength) {
    if (payloadPlusCrcLength < 2) {
        return false;
    }

    uint16_t calculated = calculateEscResponseCrc(data, payloadPlusCrcLength - 2);
    uint16_t received = (uint16_t)data[payloadPlusCrcLength - 2] |
                        ((uint16_t)data[payloadPlusCrcLength - 1] << 8);
    return calculated == received;
}

uint16_t trimReadResponsePrefix(uint8_t* rxBuf, uint16_t rxSize, uint16_t expectedPayloadLength) {
    const uint16_t expectedPacketSize = expectedPayloadLength + 3;
    if (rxSize <= expectedPacketSize) {
        return rxSize;
    }

    const uint16_t trimBytes = rxSize - expectedPacketSize;
    memmove(rxBuf, rxBuf + trimBytes, expectedPacketSize);
    return expectedPacketSize;
}

bool parseEscBootInfo(const uint8_t* rxBuf, uint16_t rxSize, EscBootInfo& info) {
    if (rxSize < 9 || rxBuf[rxSize - 1] != BR_SUCCESS) {
        return false;
    }

    info.pinCode = rxBuf[3];
    info.flashSizeCode = rxBuf[4];
    info.infoByte5 = rxBuf[5];
    info.infoByte6 = rxBuf[6];
    info.protocolVersion = rxBuf[7];

    switch (info.flashSizeCode) {
        case 0x1F:
            info.absoluteEepromAddress = 0x00007C00UL;
            info.addressShift = 0;
            info.deviceName = "F0 / 32 KB flash";
            break;
        case 0x35:
            info.absoluteEepromAddress = 0x0000F800UL;
            info.addressShift = 0;
            info.deviceName = "64 KB flash";
            break;
        case 0x2B:
            info.absoluteEepromAddress = 0x0001F800UL;
            info.addressShift = 2;
            info.deviceName = "G071 / 128 KB flash";
            break;
        default:
            return false;
    }

    info.encodedEepromAddress = (uint16_t)(info.absoluteEepromAddress >> info.addressShift);
    return true;
}

uint16_t getEncodedEscReadAddress(const EscBootInfo& info) {
    if (info.protocolVersion >= 2) {
        return 0x0021;  // ADDRESS_MAGIC_FILE_NAME on newer bootloaders
    }

    const uint32_t absoluteReadAddress = info.absoluteEepromAddress - 32UL;
    return (uint16_t)(absoluteReadAddress >> info.addressShift);
}

void printEscBootInfo(const EscBootInfo& bootInfo) {
    Serial.print("Detected ESC target: ");
    Serial.println(bootInfo.deviceName);
    Serial.print("  pinCode=0x");
    printHexByte(bootInfo.pinCode);
    Serial.print("  flashSizeCode=0x");
    printHexByte(bootInfo.flashSizeCode);
    Serial.print("  protocolVersion=");
    Serial.print(bootInfo.protocolVersion);
    Serial.print("  info5=0x");
    printHexByte(bootInfo.infoByte5);
    Serial.print("  info6=0x");
    printHexByte(bootInfo.infoByte6);
    Serial.println();
    Serial.print("Resolved EEPROM address: encoded=0x");
    Serial.print(bootInfo.encodedEepromAddress, HEX);
    Serial.print(" absolute=0x");
    Serial.println(bootInfo.absoluteEepromAddress, HEX);
}

void printStructuredHexDump(const uint8_t* data, uint16_t length, uint16_t baseOffset = 0) {
    for (uint16_t offset = 0; offset < length; offset += 16) {
        Serial.print("  +");
        printHexWord(baseOffset + offset);
        Serial.print("  ");

        for (uint8_t col = 0; col < 16; col++) {
            const uint16_t index = offset + col;
            if (index < length) {
                printHexByte(data[index]);
            } else {
                Serial.print("  ");
            }
            Serial.print(' ');
        }

        Serial.print(" |");
        for (uint8_t col = 0; col < 16; col++) {
            const uint16_t index = offset + col;
            if (index < length) {
                Serial.print(isPrintableAsciiByte(data[index]) ? (char)data[index] : '.');
            } else {
                Serial.print(' ');
            }
        }
        Serial.println('|');
    }
}

void copyEscAsciiField(char* out, size_t outSize, const uint8_t* data, size_t length) {
    if (outSize == 0) {
        return;
    }

    size_t outIndex = 0;
    for (size_t i = 0; i < length && (outIndex + 1) < outSize; i++) {
        const uint8_t value = data[i];
        if (value == 0x00 || value == 0xFF) {
            break;
        }
        out[outIndex++] = isPrintableAsciiByte(value) ? (char)value : '.';
    }
    out[outIndex] = '\0';
}

void printEscBoolField(const char* label, uint8_t raw) {
    Serial.print("  ");
    Serial.print(label);
    Serial.print('=');
    Serial.print(raw == 0x01 ? "true" : "false");
    if (raw > 1) {
        Serial.print(" (unexpected raw=");
        Serial.print(raw);
        Serial.print(')');
    }
    Serial.println();
}

const char* legacyInputTypeName(uint8_t raw) {
    switch (raw) {
        case 0:
            return "AUTO_IN";
        case 1:
            return "DSHOT_IN";
        case 2:
            return "SERVO_IN";
        case 3:
            return "SERIAL_IN";
        case 4:
            return "EDTARM";
        default:
            return nullptr;
    }
}

void printEscEepromSummary(const uint8_t* eeprom, uint16_t length) {
    if (length < ESC_EEPROM_SUMMARY_SIZE) {
        Serial.print("EEPROM summary unavailable: only ");
        Serial.print(length);
        Serial.println(" bytes captured.");
        return;
    }

    Serial.print("  byte0=0x");
    printHexByte(eeprom[0]);
    Serial.print("  eeprom_version=");
    Serial.print(eeprom[1]);
    Serial.print("  byte2=0x");
    printHexByte(eeprom[2]);
    Serial.print("  fw=");
    Serial.print(eeprom[3]);
    Serial.print('.');
    Serial.println(eeprom[4]);

    if (eeprom[1] >= 3) {
        Serial.println("Modern EEPROM summary:");
        Serial.print("  max_ramp raw=");
        Serial.print(eeprom[5]);
        Serial.print(" interpreted=");
        Serial.print(eeprom[5] / 10.0f, 1);
        Serial.println("%/ms");
        return;
    }

    char firmwareName[ESC_LEGACY_NAME_LENGTH + 1] = {0};
    copyEscAsciiField(firmwareName,
                      sizeof(firmwareName),
                      eeprom + ESC_LEGACY_NAME_OFFSET,
                      ESC_LEGACY_NAME_LENGTH);

    Serial.println("Legacy EEPROM summary (AM32 v2.16-era raw layout):");
    Serial.print("  firmware_name=\"");
    Serial.print(firmwareName[0] == '\0' ? "<empty>" : firmwareName);
    Serial.println("\"");
    printEscBoolField("dir_reversed", eeprom[17]);
    printEscBoolField("bi_direction", eeprom[18]);
    printEscBoolField("use_sin_start", eeprom[19]);
    printEscBoolField("comp_pwm", eeprom[20]);
    printEscBoolField("variable_pwm", eeprom[21]);
    printEscBoolField("stuck_rotor_protection", eeprom[22]);

    Serial.print("  advance_level=");
    Serial.print(eeprom[23]);
    if (eeprom[23] < 4) {
        Serial.print(" (");
        Serial.print(eeprom[23] * 7.5f, 1);
        Serial.println(" deg)");
    } else {
        Serial.println(" (loader falls back to level 2)");
    }

    Serial.print("  pwm_frequency_raw=");
    Serial.print(eeprom[24]);
    Serial.println(" (target-dependent legacy mapping)");
    Serial.print("  startup_power_raw=");
    Serial.println(eeprom[25]);
    Serial.print("  motor_kv_estimate=");
    Serial.print(decodeLegacyEscMotorKv(eeprom[ESC_LEGACY_MOTOR_KV_OFFSET]));
    Serial.print(" (raw=");
    Serial.print(eeprom[ESC_LEGACY_MOTOR_KV_OFFSET]);
    Serial.println(", v2.16 formula raw*40+20)");
    Serial.print("  motor_poles=");
    Serial.println(eeprom[27]);
    printEscBoolField("brake_on_stop", eeprom[28]);
    printEscBoolField("stall_protection", eeprom[29]);
    Serial.print("  beep_volume=");
    Serial.println(eeprom[30]);
    printEscBoolField("telemetry_on_interval", eeprom[31]);
    Serial.print("  servo_low_threshold=");
    Serial.print((uint16_t)eeprom[32] * 2U + 750U);
    Serial.println(" us");
    Serial.print("  servo_high_threshold=");
    Serial.print((uint16_t)eeprom[33] * 2U + 1750U);
    Serial.println(" us");
    Serial.print("  servo_neutral=");
    Serial.print((uint16_t)eeprom[34] + 1374U);
    Serial.println(" us");
    Serial.print("  servo_dead_band=");
    Serial.println(eeprom[35]);
    printEscBoolField("low_voltage_cutoff", eeprom[36]);
    Serial.print("  low_cell_cutoff=");
    Serial.print((eeprom[37] + 250) / 100.0f, 2);
    Serial.print(" V/cell (raw=");
    Serial.print(eeprom[37]);
    Serial.println(')');
    printEscBoolField("rc_car_reverse", eeprom[38]);
    printEscBoolField("use_hall_sensors", eeprom[39]);
    Serial.print("  sine_mode_changeover=");
    Serial.print(eeprom[40]);
    Serial.println("% throttle");
    Serial.print("  drag_brake_strength=");
    Serial.println(eeprom[41]);
    Serial.print("  driving_brake_strength_raw=");
    Serial.print(eeprom[42]);
    if (eeprom[42] > 0 && eeprom[42] < 10) {
        Serial.println(" (accepted by the v2.16-era loader)");
    } else {
        Serial.println(" (loader only applies 1..9)");
    }
    Serial.print("  temperature_limit_raw=");
    Serial.print(eeprom[43]);
    if (eeprom[43] >= 70 && eeprom[43] <= 140) {
        Serial.println(" C");
    } else {
        Serial.println(" (loader only applies 70..140 C)");
    }
    Serial.print("  current_limit_raw=");
    Serial.print(eeprom[44]);
    if (eeprom[44] > 0 && eeprom[44] < 100) {
        Serial.print(" (maps to ");
        Serial.print((uint16_t)eeprom[44] * 2U);
        Serial.println(" A)");
    } else {
        Serial.println(" (loader only applies 1..99, then doubles it)");
    }
    Serial.print("  sine_mode_power=");
    Serial.println(eeprom[45]);

    const char* inputTypeName = legacyInputTypeName(eeprom[46]);
    Serial.print("  input_type_raw=");
    Serial.print(eeprom[46]);
    if (inputTypeName != nullptr) {
        Serial.print(" (");
        Serial.print(inputTypeName);
        Serial.println(')');
    } else {
        Serial.println(" (not recognized by the v2.16-era loader)");
    }

    Serial.print("  auto_advance=");
    Serial.print(eeprom[47] == 0x01 ? "true" : "false");
    Serial.print(" (raw=");
    Serial.print(eeprom[47]);
    Serial.println(')');
}

bool beginEscDebugSession(EscBootInfo& bootInfo, bool& shouldResetEsc) {
    shouldResetEsc = false;

    Serial.println("Holding the signal line idle and waiting for the ESC to fall back into its bootloader window...");

    prepareEscDirectSession();

    uint8_t bootInit[] = {0, 0, 0, 0, 0, 0, 0, 0, 0x0D, 'B', 'L', 'H', 'e', 'l', 'i', 0xF4, 0x7D};
    uint8_t bootRx[64] = {0};
    uint16_t bootRxSize = 0;
    const unsigned long searchStartMs = millis();
    unsigned long nextAttemptMs = searchStartMs + 1800;
    uint8_t attempt = 0;

    while ((millis() - searchStartMs) < 4000) {
        const unsigned long now = millis();
        if ((long)(now - nextAttemptMs) < 0) {
            delay(20);
            continue;
        }

        attempt++;
        Serial.print("Bootloader probe attempt ");
        Serial.print(attempt);
        Serial.print(" at +");
        Serial.print(now - searchStartMs);
        Serial.println(" ms");
        printHexBuffer("TX boot init", bootInit, sizeof(bootInit));

        flushEscSerialRx();
        sendEsc(bootInit, sizeof(bootInit), false);
        delay(50);

        bootRxSize = getEsc(bootRx, 200);
        if (bootRxSize > 0) {
            printHexBuffer("RX boot init", bootRx, bootRxSize);
            break;
        }

        Serial.println("RX boot init: no bytes");
        nextAttemptMs = now + 150;
    }

    if (bootRxSize == 0) {
        Serial.println("ESC debug: no bootloader response after retry window.");
        Serial.println("Hint: this usually means the ESC never entered the AM32 bootloader.");
        Serial.println("Try powering the ESC first, then run the command, or rerun immediately after an ESC power-cycle.");
        return false;
    }

    if (bootRxSize >= 4) {
        printAsciiPreview("Boot header", bootRx, 4);
    }

    if (bootRx[bootRxSize - 1] == BR_SUCCESS) {
        shouldResetEsc = true;
    }

    if (!parseEscBootInfo(bootRx, bootRxSize, bootInfo)) {
        Serial.println("ESC debug: bootloader reply did not match a known AM32 device signature.");
        return false;
    }

    printEscBootInfo(bootInfo);
    return true;
}

bool readEscFlashBlock(uint16_t encodedAddress,
                       uint8_t requestedLength,
                       uint8_t* payloadOut,
                       uint16_t payloadCapacity,
                       uint16_t& payloadLength,
                       bool verbose) {
    payloadLength = 0;

    const uint16_t expectedPayloadLength = (requestedLength == 0) ? 256 : requestedLength;
    if (payloadCapacity < expectedPayloadLength) {
        Serial.println("ESC debug: output buffer too small for requested read.");
        return false;
    }

    uint8_t setAddressCmd[4] = {
        CMD_SET_ADDRESS,
        0x00,
        (uint8_t)(encodedAddress >> 8),
        (uint8_t)(encodedAddress & 0xFF)
    };
    uint8_t setAddressRx[32] = {0};
    uint8_t readCmd[2] = {CMD_READ_FLASH_SIL, requestedLength};
    uint8_t readRx[256 + 48] = {0};

    if (verbose) {
        printHexBuffer("TX set address", setAddressCmd, sizeof(setAddressCmd));
    }
    flushEscSerialRx();
    sendEsc(setAddressCmd, sizeof(setAddressCmd), true);
    delay(5);

    const uint16_t setAddressRxSize = getEsc(setAddressRx, 200);
    if (verbose) {
        printHexBuffer("RX set address", setAddressRx, setAddressRxSize);
    }
    if (setAddressRxSize == 0 || setAddressRx[0] != BR_SUCCESS) {
        Serial.println("ESC debug: set-address command failed.");
        return false;
    }

    if (verbose) {
        printHexBuffer("TX read block", readCmd, sizeof(readCmd));
    }
    flushEscSerialRx();
    sendEsc(readCmd, sizeof(readCmd), true);
    delay(expectedPayloadLength);

    const uint16_t readRxSize = getEsc(readRx, 500);
    if (verbose) {
        printHexBuffer("RX read block (raw)", readRx, readRxSize);
    }
    if (readRxSize == 0) {
        Serial.println("ESC debug: read command returned no data.");
        return false;
    }

    const uint16_t normalizedReadRxSize = trimReadResponsePrefix(readRx, readRxSize, expectedPayloadLength);
    if (verbose && normalizedReadRxSize != readRxSize) {
        Serial.print("Normalized read response by trimming ");
        Serial.print(readRxSize - normalizedReadRxSize);
        Serial.println(" leading bytes.");
        printHexBuffer("RX read block (normalized)", readRx, normalizedReadRxSize);
    }

    if (normalizedReadRxSize < (expectedPayloadLength + 3)) {
        Serial.println("ESC debug: read response was shorter than expected.");
        return false;
    }

    if (readRx[normalizedReadRxSize - 1] != BR_SUCCESS) {
        Serial.println("ESC debug: read response did not end with bootloader ACK 0x30.");
        return false;
    }

    const uint16_t payloadPlusCrcLength = normalizedReadRxSize - 1;
    const bool crcOk = checkEscResponseCrc(readRx, payloadPlusCrcLength);
    if (verbose) {
        Serial.print("Read CRC check: ");
        Serial.println(crcOk ? "OK" : "FAILED");
    }
    if (!crcOk) {
        Serial.println("ESC debug: CRC check failed.");
        return false;
    }

    payloadLength = payloadPlusCrcLength - 2;
    if (payloadLength < expectedPayloadLength) {
        Serial.println("ESC debug: payload shorter than requested.");
        return false;
    }

    memcpy(payloadOut, readRx, expectedPayloadLength);
    payloadLength = expectedPayloadLength;
    return true;
}

bool writeEscFlashBlock(uint16_t encodedAddress,
                        const uint8_t* payload,
                        uint16_t payloadLength,
                        bool verbose) {
    if (payloadLength == 0 || payloadLength > 256) {
        Serial.println("ESC debug: write payload length must be 1..256 bytes.");
        return false;
    }

    uint8_t setAddressCmd[4] = {
        CMD_SET_ADDRESS,
        0x00,
        (uint8_t)(encodedAddress >> 8),
        (uint8_t)(encodedAddress & 0xFF)
    };
    uint8_t setAddressRx[32] = {0};
    uint8_t setBufferCmd[4] = {
        CMD_SET_BUFFER,
        0x00,
        static_cast<uint8_t>((payloadLength == 256) ? 0x01 : 0x00),
        (uint8_t)(payloadLength & 0xFF)
    };
    uint8_t setBufferRx[32] = {0};
    uint8_t progCmd[2] = {CMD_PROG_FLASH, 0x01};
    uint8_t progRx[32] = {0};

    if (verbose) {
        printHexBuffer("TX set address", setAddressCmd, sizeof(setAddressCmd));
    }
    flushEscSerialRx();
    sendEsc(setAddressCmd, sizeof(setAddressCmd), true);
    delay(5);

    uint16_t setAddressRxSize = getEsc(setAddressRx, 200);
    if (verbose) {
        printHexBuffer("RX set address", setAddressRx, setAddressRxSize);
    }
    if (setAddressRxSize == 0 || setAddressRx[0] != BR_SUCCESS) {
        Serial.println("ESC debug: set-address command failed before write.");
        return false;
    }

    if (verbose) {
        printHexBuffer("TX set buffer", setBufferCmd, sizeof(setBufferCmd));
    }
    flushEscSerialRx();
    sendEsc(setBufferCmd, sizeof(setBufferCmd), true);
    delay(5);

    setBufferRx[0] = 0;
    uint16_t setBufferRxSize = getEsc(setBufferRx, 30);
    if (verbose && setBufferRxSize > 0) {
        printHexBuffer("RX set buffer", setBufferRx, setBufferRxSize);
    }
    if (setBufferRxSize > 0 && setBufferRx[0] != BR_SUCCESS) {
        Serial.println("ESC debug: unexpected response to set-buffer command.");
        return false;
    }

    if (verbose) {
        printHexBuffer("TX buffer payload", payload, payloadLength);
    }
    flushEscSerialRx();
    sendEsc(const_cast<uint8_t*>(payload), payloadLength, true);
    delay(10);

    uint8_t payloadAck[32] = {0};
    uint16_t payloadAckSize = getEsc(payloadAck, 200);
    if (verbose) {
        printHexBuffer("RX buffer payload ack", payloadAck, payloadAckSize);
    }
    if (payloadAckSize == 0 || payloadAck[0] != BR_SUCCESS) {
        Serial.println("ESC debug: payload upload was not acknowledged.");
        return false;
    }

    if (verbose) {
        printHexBuffer("TX program flash", progCmd, sizeof(progCmd));
    }
    flushEscSerialRx();
    sendEsc(progCmd, sizeof(progCmd), true);
    delay(80);

    uint16_t progRxSize = getEsc(progRx, 500);
    if (verbose) {
        printHexBuffer("RX program flash", progRx, progRxSize);
    }
    if (progRxSize == 0 || progRx[0] != BR_SUCCESS) {
        Serial.println("ESC debug: flash program command failed.");
        return false;
    }

    return true;
}

void prepareEscDirectSession() {
    cancelRamp();
    writeThrottlePercent(0.0f);
    flushEscTelemetryRx();
    escSerial.end();
    ledcDetachPin(ESC_PWM_PIN);
    pinMode(ESC_PWM_PIN, INPUT_PULLUP);
    delay(20);
    initEscSerial();
    flushEscSerialRx();
}

void restoreEscCliSession() {
    deinitEscSerial();
    beginMotorControl();
    beginEscTelemetry();
}

void resetEscAfterDebugRead() {
    uint8_t resetCmd[4] = {0x00, 0x00, 0x00, 0x00};
    Serial.println("Resetting ESC back out of bootloader...");
    printHexBuffer("TX reset", resetCmd, sizeof(resetCmd));
    sendEsc(resetCmd, sizeof(resetCmd), false);
    delay(50);

    uint8_t resetRx[16] = {0};
    uint16_t resetRxSize = getEsc(resetRx, 50);
    printHexBuffer("RX reset", resetRx, resetRxSize);
    delay(250);
}

void initEscSerial() {
    enable4Way = true;
    escPassthroughSerial.begin(ESC_BOOTLOADER_BAUD, SWSERIAL_8N1, ESC_PWM_PIN, ESC_PWM_PIN, false, 512);
    escPassthroughSerial.enableIntTx(false);
}

void deinitEscSerial() {
    escPassthroughSerial.end();
    enable4Way = false;
}

uint16_t sendEsc(uint8_t txBuf[], uint16_t bufSize, bool sendCrc = true) {
    escCrc = 0;
    if (bufSize == 0) {
        bufSize = 256;
    }

    escPassthroughSerial.enableTx(true);
    for (uint16_t i = 0; i < bufSize; i++) {
        escPassthroughSerial.write(txBuf[i]);
        escCrc = byteCrc(txBuf[i], escCrc);
    }

    if (sendCrc) {
        escPassthroughSerial.write(escCrc & 0xff);
        escPassthroughSerial.write((escCrc >> 8) & 0xff);
    }

    escPassthroughSerial.enableTx(false);
    return 0;
}

uint16_t getEsc(uint8_t rxBuf[], uint16_t waitMs) {
    uint16_t waited = 0;
    while (!escPassthroughSerial.available()) {
        delay(1);
        waited++;
        if (waited >= waitMs) {
            return 0;
        }
    }

    uint16_t index = 0;
    while (escPassthroughSerial.available()) {
        rxBuf[index++] = escPassthroughSerial.read();
        delayMicroseconds(100);
    }
    return index;
}

uint8_t handleMsp(uint8_t* buf, uint8_t bufSize) {
    uint8_t type = buf[4];
    uint8_t outSize = 0;

    if (type == MSP_API_VERSION && buf[bufSize - 1] == 0x01) {
        buf[2] = 0x3E; buf[3] = 0x03; buf[4] = MSP_API_VERSION;
        buf[5] = MSP_PROTOCOL_VERSION; buf[6] = API_VERSION_MAJOR; buf[7] = API_VERSION_MINOR;
        outSize = 8;
    } else if (type == MSP_FC_VARIANT && buf[bufSize - 1] == 0x02) {
        buf[2] = 0x3E; buf[3] = 0x04; buf[4] = MSP_FC_VARIANT;
        buf[5] = 'B'; buf[6] = 'T'; buf[7] = 'F'; buf[8] = 'L';
        outSize = 9;
    } else if (type == MSP_FC_VERSION && buf[bufSize - 1] == 0x03) {
        buf[2] = 0x3E; buf[3] = 0x03; buf[4] = MSP_FC_VERSION;
        buf[5] = FC_VERSION_MAJOR; buf[6] = FC_VERSION_MINOR; buf[7] = FC_VERSION_PATCH_LEVEL;
        outSize = 8;
    } else if (type == MSP_BOARD_INFO && buf[bufSize - 1] == 0x04) {
        buf[2] = 0x3E; buf[3] = 0x4F; buf[4] = MSP_BOARD_INFO;
        buf[5] = 'B'; buf[6] = 'P'; buf[7] = 'M'; buf[8] = 'C';
        for (int i = 9; i <= 83; i++) buf[i] = 0x00;
        buf[13] = 0x14;
        const char boardName[] = "BrushlessPower MCTRL";
        for (int i = 0; i < 20; i++) buf[14 + i] = boardName[i];
        outSize = 84;
    } else if (type == MSP_BUILD_INFO && buf[bufSize - 1] == 0x05) {
        buf[2] = 0x3E; buf[3] = 0x1A; buf[4] = MSP_BUILD_INFO;
        for (int i = 5; i <= 30; i++) buf[i] = 0x00;
        outSize = 31;
    } else if (type == MSP_STATUS && buf[bufSize - 1] == 0x65) {
        buf[2] = 0x3E; buf[3] = 0x16; buf[4] = MSP_STATUS;
        for (int i = 5; i <= 26; i++) buf[i] = 0x00;
        outSize = 27;
    } else if (type == MSP_MOTOR_3D_CONFIG && buf[bufSize - 1] == 0x7C) {
        buf[2] = 0x3E; buf[3] = 0x06; buf[4] = MSP_MOTOR_3D_CONFIG;
        for (int i = 5; i <= 10; i++) buf[i] = 0x00;
        outSize = 11;
    } else if (type == MSP_MOTOR_CONFIG && buf[bufSize - 1] == 0x83) {
        buf[2] = 0x3E; buf[3] = 0x0A; buf[4] = MSP_MOTOR_CONFIG;
        buf[5] = 0x2E; buf[6] = 0x04; buf[7] = 0xD0; buf[8] = 0x07; buf[9] = 0xE8; buf[10] = 0x03;
        buf[11] = 0x01; buf[12] = 0x00; buf[13] = 0x00; buf[14] = 0x00;
        outSize = 15;
    } else if (type == MSP_MOTOR && buf[bufSize - 1] == 0x68) {
        buf[2] = 0x3E; buf[3] = 0x10; buf[4] = MSP_MOTOR;
        buf[5] = 0xE8; buf[6] = 0x03;
        for (int i = 7; i <= 20; i++) buf[i] = 0x00;
        outSize = 21;
    } else if (type == MSP_FEATURE_CONFIG && buf[bufSize - 1] == 0x24) {
        buf[2] = 0x3E; buf[3] = 0x04; buf[4] = MSP_FEATURE_CONFIG;
        buf[5] = 0x00; buf[6] = 0x00; buf[7] = 0x00; buf[8] = 0x00;
        outSize = 9;
    } else if (type == MSP_BOXIDS && buf[bufSize - 1] == 0x77) {
        buf[2] = 0x3E; buf[3] = 0x18; buf[4] = MSP_BOXIDS;
        for (int i = 5; i <= 28; i++) buf[i] = 0x00;
        outSize = 29;
    } else if (type == MSP_SET_4WAY_IF && buf[bufSize - 1] == 0xF5) {
        buf[2] = 0x3E; buf[3] = 0x01; buf[4] = MSP_SET_4WAY_IF; buf[5] = 0x01;
        initEscSerial();
        outSize = 6;
    } else if (type == MSP_ADVANCED_CONFIG && buf[bufSize - 1] == 0x5A) {
        buf[2] = 0x3E; buf[3] = 0x14; buf[4] = MSP_ADVANCED_CONFIG;
        for (int i = 5; i <= 24; i++) buf[i] = 0x00;
        buf[8] = 0x06; buf[9] = 0xE0; buf[10] = 0x01;
        outSize = 25;
    } else if (type == MSP_UID && buf[bufSize - 1] == 0xA0) {
        buf[2] = 0x3E; buf[3] = 0x0C; buf[4] = MSP_UID;
        for (int i = 5; i <= 16; i++) buf[i] = 0x00;
        outSize = 17;
    }

    uint8_t crc = buf[3] ^ buf[4];
    for (uint8_t i = 5; i < outSize; i++) crc ^= buf[i];
    buf[outSize] = crc;
    return outSize + 1;
}

uint16_t handle4Way(uint8_t buf[]) {
    uint8_t cmd = buf[1];
    uint8_t addrHigh = buf[2];
    uint8_t addrLow = buf[3];
    uint8_t inParamLen = buf[4];
    uint8_t param = buf[5];
    uint8_t paramBuf[256] = {0};
    uint16_t crc = 0;
    uint16_t outParamLen = 0;
    uint8_t ackOut = ACK_OK;

    for (uint8_t i = 0; i < 5; i++) crc = crcXmodemUpdate(crc, buf[i]);
    uint8_t inBuff = inParamLen;
    uint16_t i = 0;
    do {
        crc = crcXmodemUpdate(crc, buf[i + 5]);
        paramBuf[i] = buf[i + 5];
        i++;
        inBuff--;
    } while (inBuff != 0);

    uint16_t crcIn = ((buf[i + 5] << 8) | buf[i + 6]);
    if (crcIn != crc && cmd < 0x50) {
        buf[0] = CMD_REMOTE_ESCAPE; buf[1] = cmd; buf[2] = addrHigh; buf[3] = addrLow; buf[4] = 0x01;
        buf[5] = 0x00; buf[6] = ACK_I_INVALID_CRC;
        crc = 0;
        for (uint8_t j = 0; j < 7; j++) crc = crcXmodemUpdate(crc, buf[j]);
        buf[7] = (crc >> 8) & 0xff; buf[8] = crc & 0xff;
        return 9;
    }

    crc = 0;
    buf[5] = 0;

    if (cmd == CMD_DEVICE_INIT_FLASH) {
        outParamLen = 0x04;
        if (param == 0x00) {
            uint8_t bootInit[] = {0,0,0,0,0,0,0,0,0x0D,'B','L','H','e','l','i',0xF4,0x7D};
            uint8_t rxBuf[250] = {0};
            sendEsc(bootInit, 17, false);
            delay(50);
            uint16_t rxSize = getEsc(rxBuf, 200);
            if (rxSize > 0 && rxBuf[rxSize - 1] == BR_SUCCESS) {
                buf[5] = rxBuf[5];
                buf[6] = rxBuf[4];
                buf[7] = rxBuf[3];
                buf[8] = IM_ARM_BLB;
                buf[9] = ACK_OK;
            } else {
                buf[5] = 0x06; buf[6] = 0x33; buf[7] = 0x67; buf[8] = IM_ARM_BLB; buf[9] = ACK_D_GENERAL_ERROR;
            }
        } else {
            ackOut = ACK_I_INVALID_CHANNEL;
            buf[9] = ACK_I_INVALID_CHANNEL;
        }
    } else if (cmd == CMD_DEVICE_RESET) {
        outParamLen = 0x01;
        if (param == 0x00) {
            if (enable4Way) {
                uint8_t escData[2] = {RESTART_BOOTLOADER, 0};
                uint8_t rxBuf[5] = {0};
                sendEsc(escData, 2);
                digitalWrite(ESC_PWM_PIN, LOW);
                delay(300);
                digitalWrite(ESC_PWM_PIN, HIGH);
                getEsc(rxBuf, 50);
            } else {
                ackOut = ACK_D_GENERAL_ERROR;
                buf[6] = ACK_D_GENERAL_ERROR;
            }
        } else {
            ackOut = ACK_I_INVALID_CHANNEL;
            buf[6] = ACK_I_INVALID_CHANNEL;
        }
    } else if (cmd == CMD_INTERFACE_TEST_ALIVE) {
        outParamLen = 0x01;
        uint8_t escData[2] = {CMD_KEEP_ALIVE, 0};
        uint8_t rxBuf[250] = {0};
        sendEsc(escData, 2);
        delay(5);
        getEsc(rxBuf, 200);
    } else if (cmd == CMD_DEVICE_READ) {
        uint8_t escData[4] = {CMD_SET_ADDRESS, 0x00, addrHigh, addrLow};
        uint8_t rxBuf[300] = {0};
        sendEsc(escData, 4);
        delay(5);
        uint16_t rxSize = getEsc(rxBuf, 200);
        if (rxSize > 0 && rxBuf[0] == BR_SUCCESS) {
            escData[0] = CMD_READ_FLASH_SIL;
            escData[1] = param;
            sendEsc(escData, 2);
            delay(param == 0 ? 256 : param);
            rxSize = getEsc(rxBuf, 500);
            if (rxSize != 0 && rxBuf[rxSize - 1] == BR_SUCCESS) {
                rxSize -= 3;
                outParamLen = rxSize;
                for (uint16_t j = 5; j < (rxSize + 5); j++) buf[j] = rxBuf[j - 5];
            } else {
                ackOut = ACK_D_GENERAL_ERROR;
                outParamLen = 0x01;
            }
        } else {
            ackOut = ACK_D_GENERAL_ERROR;
            outParamLen = 0x01;
        }
    } else if (cmd == CMD_INTERFACE_EXIT) {
        deinitEscSerial();
        outParamLen = 0x01;
    } else if (cmd == CMD_PROTOCOL_GET_VERSION) {
        outParamLen = 0x01;
        buf[5] = SERIAL_4WAY_PROTOCOL_VER;
    } else if (cmd == CMD_INTERFACE_GET_NAME) {
        outParamLen = 0x09;
        for (uint8_t j = 0; j < 9; j++) buf[5 + j] = SERIAL_4WAY_INTERFACE_NAME_STR[j];
    } else if (cmd == CMD_INTERFACE_GET_VERSION) {
        outParamLen = 0x02;
        buf[5] = SERIAL_4WAY_VERSION_HI;
        buf[6] = SERIAL_4WAY_VERSION_LO;
    } else if (cmd == CMD_INTERFACE_SET_MODE) {
        outParamLen = 0x01;
        if (param != IM_ARM_BLB) {
            ackOut = ACK_I_INVALID_PARAM;
            buf[6] = ACK_I_INVALID_PARAM;
        }
    } else if (cmd == CMD_DEVICE_VERIFY) {
        outParamLen = 0x01;
    } else if (cmd == CMD_DEVICE_PAGE_ERASE) {
        outParamLen = 0x01;
        addrHigh = (param << 2);
        addrLow = 0;
        uint8_t escData[4] = {CMD_SET_ADDRESS, 0, addrHigh, addrLow};
        uint8_t rxBuf[250] = {0};
        sendEsc(escData, 4);
        delay(5);
        uint16_t rxSize = getEsc(rxBuf, 200);
        if (!(rxSize > 0 && rxBuf[0] == BR_SUCCESS)) ackOut = ACK_D_GENERAL_ERROR;
        escData[0] = CMD_ERASE_FLASH;
        escData[1] = 0x01;
        sendEsc(escData, 2);
        delay(50);
        rxSize = getEsc(rxBuf, 100);
        if (!(rxSize > 0 && rxBuf[0] == BR_SUCCESS)) ackOut = ACK_D_GENERAL_ERROR;
    } else if (cmd == CMD_DEVICE_WRITE) {
        outParamLen = 0x01;
        uint8_t escData[4] = {CMD_SET_ADDRESS, 0, addrHigh, addrLow};
        uint8_t rxBuf[250] = {0};
        sendEsc(escData, 4);
        delay(50);
        uint16_t rxSize = getEsc(rxBuf, 100);
        if (!(rxSize > 0 && rxBuf[0] == BR_SUCCESS)) ackOut = ACK_D_GENERAL_ERROR;
        escData[0] = CMD_SET_BUFFER;
        escData[1] = 0x00;
        escData[2] = (inParamLen == 0) ? 0x01 : 0x00;
        escData[3] = inParamLen;
        sendEsc(escData, 4);
        delay(5);
        sendEsc(paramBuf, inParamLen);
        delay(5);
        rxSize = getEsc(rxBuf, 200);
        if (!(rxSize > 0 && rxBuf[0] == BR_SUCCESS)) ackOut = ACK_D_GENERAL_ERROR;
        escData[0] = CMD_PROG_FLASH;
        escData[1] = 0x01;
        sendEsc(escData, 2);
        delay(30);
        rxSize = getEsc(rxBuf, 100);
        if (!(rxSize > 0 && rxBuf[0] == BR_SUCCESS)) ackOut = ACK_D_GENERAL_ERROR;
    } else {
        return 0;
    }

    crc = 0;
    buf[0] = CMD_REMOTE_ESCAPE;
    buf[1] = cmd;
    buf[2] = addrHigh;
    buf[3] = addrLow;
    buf[4] = outParamLen & 0xff;
    buf[outParamLen + 5] = ackOut;
    for (uint16_t j = 0; j < (outParamLen + 6); j++) crc = crcXmodemUpdate(crc, buf[j]);
    buf[outParamLen + 6] = (crc >> 8) & 0xff;
    buf[outParamLen + 7] = crc & 0xff;
    return outParamLen + 8;
}

void processSerialPassthrough() {
    static uint16_t rxCounter = 0;
    static uint16_t txCounter = 0;
    static uint16_t bufferLen = 0;
    static bool serialCommand = false;

    if (Serial.available()) {
        delay(10);
        serialCommand = true;
        while (Serial.available()) {
            serialRx[rxCounter] = Serial.read();
            if ((rxCounter == 4) && (serialRx[0] == CMD_LOCAL_ESCAPE)) {
                bufferLen = (serialRx[4] == 0) ? (256 + 7) : (serialRx[4] + 7);
            }
            if ((rxCounter == 3) && (serialRx[0] == 0x24)) {
                bufferLen = serialRx[3] + 6;
            }
            rxCounter++;
            if (rxCounter == bufferLen) {
                break;
            }
        }
    }

    if (!serialCommand) return;

    if ((serialRx[0] == CMD_LOCAL_ESCAPE) && (rxCounter == bufferLen)) {
        txCounter = handle4Way(serialRx);
        for (uint16_t b = 0; b < txCounter; b++) Serial.write(serialRx[b]);
        serialCommand = false;
        rxCounter = 0;
        txCounter = 0;
    } else if (serialRx[0] == 0x24 && serialRx[1] == 0x4D && serialRx[2] == 0x3C) {
        txCounter = handleMsp(serialRx, rxCounter);
        for (uint16_t b = 0; b < txCounter; b++) Serial.write(serialRx[b]);
        serialCommand = false;
        rxCounter = 0;
        txCounter = 0;
    }
}

}

void requestEscPassthroughModeAndRestart() {
    cancelRamp();
    writeThrottlePercent(0.0f);

    preferences.begin("am32cli", false);
    preferences.putBool(PASSTHROUGH_PREF_KEY, true);
    preferences.end();

    Serial.println("Rebooting into ESC passthrough mode...");
    Serial.println("Reconnect with the configurator after restart.");
    delay(100);
    ESP.restart();
}

void loadMotorPoleCount() {
    preferences.begin("am32cli", true);
    const int storedValue = preferences.getInt(MOTOR_POLES_PREF_KEY, DEFAULT_MOTOR_POLES);
    preferences.end();

    motorPoleCount = isValidMotorPoleCount(storedValue) ? storedValue : DEFAULT_MOTOR_POLES;
}

void printMotorPoleCount() {
    Serial.print("Motor poles: ");
    Serial.println(motorPoleCount);
}

void requestBoardRestart() {
    cancelRamp();
    writeThrottlePercent(0.0f);

    Serial.println("Restarting ESP32 board...");
    delay(100);
    ESP.restart();
}

bool consumeEscPassthroughRequest() {
    preferences.begin("am32cli", false);
    bool shouldEnter = preferences.getBool(PASSTHROUGH_PREF_KEY, false);
    if (shouldEnter) {
        preferences.putBool(PASSTHROUGH_PREF_KEY, false);
    }
    preferences.end();
    return shouldEnter;
}

void runEscPassthroughMode() {
    cancelRamp();
    writeThrottlePercent(0.0f);
    escSerial.end();
    ledcDetachPin(ESC_PWM_PIN);
    pinMode(ESC_PWM_PIN, INPUT_PULLUP);

    Serial.println();
    Serial.println("ESC passthrough mode active.");
    Serial.println("MSP + 4Way passthrough active.");
    Serial.println("Reset the ESP32 to return to CLI mode.");

    while (true) {
        processSerialPassthrough();
    }
}

void readEscParametersDebug() {
    bool shouldResetEsc = false;

    Serial.println();
    Serial.println("ESC params: starting temporary passthrough read.");
    Serial.println("Using AM32 flow: boot init -> set address (eeprom_address - 32) -> read 80 bytes.");
    EscBootInfo bootInfo;

    do {
        if (!beginEscDebugSession(bootInfo, shouldResetEsc)) {
            break;
        }

        const uint16_t readAddress = getEncodedEscReadAddress(bootInfo);
        const uint32_t absoluteReadAddress = bootInfo.absoluteEepromAddress - 32UL;
        Serial.print("Read start address: encoded=0x");
        Serial.print(readAddress, HEX);
        Serial.print(" absolute=0x");
        Serial.println(absoluteReadAddress, HEX);

        uint8_t readPayload[ESC_PARAM_READ_SIZE] = {0};
        uint16_t payloadLength = 0;
        if (!readEscFlashBlock(readAddress, ESC_PARAM_READ_SIZE, readPayload, sizeof(readPayload), payloadLength, true)) {
            break;
        }

        Serial.print("Read payload bytes: ");
        Serial.println(payloadLength);

        printHexBuffer("EEPROM payload[0..47]", readPayload + 32, 48);

        const uint8_t* eeprom = readPayload + 32;
        Serial.println("EEPROM summary:");
        printEscEepromSummary(eeprom, ESC_EEPROM_SUMMARY_SIZE);

        Serial.println("ESC params: read completed.");
    } while (false);

    if (shouldResetEsc) {
        resetEscAfterDebugRead();
    }

    restoreEscCliSession();
    Serial.println("ESC params: passthrough session closed.");
}

void dumpEscEepromDebug() {
    bool shouldResetEsc = false;

    Serial.println();
    Serial.println("ESC dump: starting full EEPROM read.");
    Serial.print("Dump size: ");
    Serial.print(ESC_EEPROM_REGION_SIZE);
    Serial.print(" bytes in ");
    Serial.print(ESC_EEPROM_DUMP_CHUNK_SIZE);
    Serial.println(" byte chunks.");

    EscBootInfo bootInfo;
    uint8_t eepromDump[ESC_EEPROM_REGION_SIZE] = {0};

    do {
        if (!beginEscDebugSession(bootInfo, shouldResetEsc)) {
            break;
        }

        bool readAllChunks = true;
        for (uint16_t offset = 0; offset < ESC_EEPROM_REGION_SIZE; offset += ESC_EEPROM_DUMP_CHUNK_SIZE) {
            const uint32_t absoluteAddress = bootInfo.absoluteEepromAddress + offset;
            const uint16_t encodedAddress = (uint16_t)(absoluteAddress >> bootInfo.addressShift);
            uint16_t payloadLength = 0;

            Serial.print("Reading chunk +0x");
            printHexWord(offset);
            Serial.print(" encoded=0x");
            Serial.print(encodedAddress, HEX);
            Serial.print(" absolute=0x");
            Serial.println(absoluteAddress, HEX);

            if (!readEscFlashBlock(encodedAddress,
                                   ESC_EEPROM_DUMP_CHUNK_SIZE,
                                   eepromDump + offset,
                                   ESC_EEPROM_DUMP_CHUNK_SIZE,
                                   payloadLength,
                                   false)) {
                Serial.print("ESC dump: failed while reading chunk at +0x");
                printHexWord(offset);
                Serial.println();
                readAllChunks = false;
                break;
            }

            if (payloadLength != ESC_EEPROM_DUMP_CHUNK_SIZE) {
                Serial.print("ESC dump: unexpected chunk length ");
                Serial.println(payloadLength);
                readAllChunks = false;
                break;
            }

            if ((offset + ESC_EEPROM_DUMP_CHUNK_SIZE) >= ESC_EEPROM_REGION_SIZE) {
                Serial.println("All EEPROM chunks read.");
            }
        }

        if (!readAllChunks) {
            break;
        }

        int firstUsed = -1;
        int lastUsed = -1;
        for (uint16_t i = 0; i < ESC_EEPROM_REGION_SIZE; i++) {
            if (eepromDump[i] != 0xFF) {
                if (firstUsed < 0) {
                    firstUsed = i;
                }
                lastUsed = i;
            }
        }

        Serial.println();
        Serial.print("EEPROM dump base absolute address: 0x");
        Serial.println(bootInfo.absoluteEepromAddress, HEX);
        if (firstUsed >= 0) {
            Serial.print("Non-FF data window: +0x");
            printHexWord((uint16_t)firstUsed);
            Serial.print(" .. +0x");
            printHexWord((uint16_t)lastUsed);
            Serial.println();
        } else {
            Serial.println("EEPROM region appears fully erased (all 0xFF).");
        }

        Serial.println("EEPROM summary:");
        printEscEepromSummary(eepromDump, ESC_EEPROM_REGION_SIZE);
        Serial.println("EEPROM dump:");
        printStructuredHexDump(eepromDump, ESC_EEPROM_REGION_SIZE);
        Serial.println("ESC dump: completed.");
    } while (false);

    if (shouldResetEsc) {
        resetEscAfterDebugRead();
    }

    restoreEscCliSession();
    Serial.println("ESC dump: passthrough session closed.");
}

void toggleEscDirectionReverseDebug() {
    bool shouldResetEsc = false;

    Serial.println();
    Serial.println("ESC reverse: starting dir_reversed toggle.");
    Serial.println("Reading the legacy EEPROM block, flipping byte 17, writing it back, then verifying.");

    EscBootInfo bootInfo;
    uint8_t eepromData[ESC_LEGACY_EEPROM_WRITE_SIZE] = {0};

    do {
        if (!beginEscDebugSession(bootInfo, shouldResetEsc)) {
            break;
        }

        uint16_t payloadLength = 0;
        if (!readEscFlashBlock(bootInfo.encodedEepromAddress,
                               ESC_LEGACY_EEPROM_WRITE_SIZE,
                               eepromData,
                               sizeof(eepromData),
                               payloadLength,
                               true)) {
            break;
        }

        if (payloadLength != ESC_LEGACY_EEPROM_WRITE_SIZE) {
            Serial.print("ESC reverse: expected ");
            Serial.print(ESC_LEGACY_EEPROM_WRITE_SIZE);
            Serial.print(" EEPROM bytes but got ");
            Serial.println(payloadLength);
            break;
        }

        Serial.println("EEPROM summary before write:");
        printEscEepromSummary(eepromData, payloadLength);

        if (eepromData[1] >= 3) {
            Serial.println("ESC reverse: refusing to write modern EEPROM layouts until their write path is mapped.");
            break;
        }

        const uint8_t oldRaw = eepromData[ESC_LEGACY_DIR_REVERSED_OFFSET];
        const uint8_t newRaw = (oldRaw == 0x01) ? 0x00 : 0x01;

        Serial.print("dir_reversed raw before write: ");
        Serial.println(oldRaw);
        Serial.print("dir_reversed raw after write: ");
        Serial.println(newRaw);

        eepromData[ESC_LEGACY_DIR_REVERSED_OFFSET] = newRaw;

        if (!writeEscFlashBlock(bootInfo.encodedEepromAddress,
                                eepromData,
                                ESC_LEGACY_EEPROM_WRITE_SIZE,
                                true)) {
            break;
        }

        uint8_t verifyData[ESC_LEGACY_EEPROM_WRITE_SIZE] = {0};
        uint16_t verifyLength = 0;
        if (!readEscFlashBlock(bootInfo.encodedEepromAddress,
                               ESC_LEGACY_EEPROM_WRITE_SIZE,
                               verifyData,
                               sizeof(verifyData),
                               verifyLength,
                               true)) {
            Serial.println("ESC reverse: write completed, but read-back verification failed.");
            break;
        }

        if (verifyLength != ESC_LEGACY_EEPROM_WRITE_SIZE) {
            Serial.println("ESC reverse: verification read length was unexpected.");
            break;
        }

        Serial.println("EEPROM summary after write:");
        printEscEepromSummary(verifyData, verifyLength);

        const uint8_t verifyRaw = verifyData[ESC_LEGACY_DIR_REVERSED_OFFSET];
        if (verifyRaw != newRaw) {
            Serial.print("ESC reverse: verification mismatch, expected raw=");
            Serial.print(newRaw);
            Serial.print(" but read back ");
            Serial.println(verifyRaw);
            break;
        }

        Serial.print("ESC reverse: dir_reversed toggled successfully to ");
        Serial.println(verifyRaw == 0x01 ? "true" : "false");
    } while (false);

    if (shouldResetEsc) {
        resetEscAfterDebugRead();
    }

    restoreEscCliSession();
    Serial.println("ESC reverse: passthrough session closed.");
}

void setEscMotorPolesAndSync(int poleCount) {
    if (!isValidMotorPoleCount(poleCount)) {
        Serial.println("ESC poles: value must be between 2 and 100.");
        return;
    }

    bool shouldResetEsc = false;

    Serial.println();
    Serial.print("ESC poles: setting motor_poles to ");
    Serial.println(poleCount);
    Serial.println("Reading the legacy EEPROM block, writing byte 27, then verifying.");

    EscBootInfo bootInfo;
    uint8_t eepromData[ESC_LEGACY_EEPROM_WRITE_SIZE] = {0};
    bool updateSucceeded = false;

    do {
        if (!beginEscDebugSession(bootInfo, shouldResetEsc)) {
            break;
        }

        uint16_t payloadLength = 0;
        if (!readEscFlashBlock(bootInfo.encodedEepromAddress,
                               ESC_LEGACY_EEPROM_WRITE_SIZE,
                               eepromData,
                               sizeof(eepromData),
                               payloadLength,
                               true)) {
            break;
        }

        if (payloadLength != ESC_LEGACY_EEPROM_WRITE_SIZE) {
            Serial.print("ESC poles: expected ");
            Serial.print(ESC_LEGACY_EEPROM_WRITE_SIZE);
            Serial.print(" EEPROM bytes but got ");
            Serial.println(payloadLength);
            break;
        }

        Serial.println("EEPROM summary before write:");
        printEscEepromSummary(eepromData, payloadLength);

        if (eepromData[1] >= 3) {
            Serial.println("ESC poles: refusing to write modern EEPROM layouts until their write path is mapped.");
            break;
        }

        const uint8_t oldRaw = eepromData[ESC_LEGACY_MOTOR_POLES_OFFSET];
        const uint8_t newRaw = (uint8_t)poleCount;

        Serial.print("motor_poles raw before write: ");
        Serial.println(oldRaw);
        Serial.print("motor_poles raw after write: ");
        Serial.println(newRaw);

        eepromData[ESC_LEGACY_MOTOR_POLES_OFFSET] = newRaw;

        if (!writeEscFlashBlock(bootInfo.encodedEepromAddress,
                                eepromData,
                                ESC_LEGACY_EEPROM_WRITE_SIZE,
                                true)) {
            break;
        }

        uint8_t verifyData[ESC_LEGACY_EEPROM_WRITE_SIZE] = {0};
        uint16_t verifyLength = 0;
        if (!readEscFlashBlock(bootInfo.encodedEepromAddress,
                               ESC_LEGACY_EEPROM_WRITE_SIZE,
                               verifyData,
                               sizeof(verifyData),
                               verifyLength,
                               true)) {
            Serial.println("ESC poles: write completed, but read-back verification failed.");
            break;
        }

        if (verifyLength != ESC_LEGACY_EEPROM_WRITE_SIZE) {
            Serial.println("ESC poles: verification read length was unexpected.");
            break;
        }

        Serial.println("EEPROM summary after write:");
        printEscEepromSummary(verifyData, verifyLength);

        const uint8_t verifyRaw = verifyData[ESC_LEGACY_MOTOR_POLES_OFFSET];
        if (verifyRaw != newRaw) {
            Serial.print("ESC poles: verification mismatch, expected raw=");
            Serial.print(newRaw);
            Serial.print(" but read back ");
            Serial.println(verifyRaw);
            break;
        }

        motorPoleCount = poleCount;
        saveMotorPoleCount();
        updateSucceeded = true;

        Serial.print("ESC poles: motor_poles updated successfully to ");
        Serial.println(motorPoleCount);
    } while (false);

    if (shouldResetEsc) {
        resetEscAfterDebugRead();
    }

    restoreEscCliSession();
    Serial.println("ESC poles: passthrough session closed.");

    if (updateSucceeded) {
        printMotorPoleCount();
    }
}

void setEscMotorKv(int motorKv) {
    if (!isValidEscMotorKv(motorKv)) {
        Serial.println("ESC kv: value must be between 20 and 10220.");
        return;
    }

    bool shouldResetEsc = false;

    const uint8_t newRaw = encodeLegacyEscMotorKv(motorKv);
    const int appliedMotorKv = decodeLegacyEscMotorKv(newRaw);

    Serial.println();
    Serial.print("ESC kv: setting motor_kv_estimate to ");
    Serial.println(motorKv);
    Serial.println("Reading the legacy EEPROM block, writing byte 26, then verifying.");
    if (appliedMotorKv != motorKv) {
        Serial.print("ESC kv: requested value will be rounded to ");
        Serial.print(appliedMotorKv);
        Serial.println(" by the legacy AM32 raw*40+20 encoding.");
    }

    EscBootInfo bootInfo;
    uint8_t eepromData[ESC_LEGACY_EEPROM_WRITE_SIZE] = {0};

    do {
        if (!beginEscDebugSession(bootInfo, shouldResetEsc)) {
            break;
        }

        uint16_t payloadLength = 0;
        if (!readEscFlashBlock(bootInfo.encodedEepromAddress,
                               ESC_LEGACY_EEPROM_WRITE_SIZE,
                               eepromData,
                               sizeof(eepromData),
                               payloadLength,
                               true)) {
            break;
        }

        if (payloadLength != ESC_LEGACY_EEPROM_WRITE_SIZE) {
            Serial.print("ESC kv: expected ");
            Serial.print(ESC_LEGACY_EEPROM_WRITE_SIZE);
            Serial.print(" EEPROM bytes but got ");
            Serial.println(payloadLength);
            break;
        }

        Serial.println("EEPROM summary before write:");
        printEscEepromSummary(eepromData, payloadLength);

        if (eepromData[1] >= 3) {
            Serial.println("ESC kv: refusing to write modern EEPROM layouts until their write path is mapped.");
            break;
        }

        const uint8_t oldRaw = eepromData[ESC_LEGACY_MOTOR_KV_OFFSET];

        Serial.print("motor_kv_estimate raw before write: ");
        Serial.println(oldRaw);
        Serial.print("motor_kv_estimate raw after write: ");
        Serial.println(newRaw);

        eepromData[ESC_LEGACY_MOTOR_KV_OFFSET] = newRaw;

        if (!writeEscFlashBlock(bootInfo.encodedEepromAddress,
                                eepromData,
                                ESC_LEGACY_EEPROM_WRITE_SIZE,
                                true)) {
            break;
        }

        uint8_t verifyData[ESC_LEGACY_EEPROM_WRITE_SIZE] = {0};
        uint16_t verifyLength = 0;
        if (!readEscFlashBlock(bootInfo.encodedEepromAddress,
                               ESC_LEGACY_EEPROM_WRITE_SIZE,
                               verifyData,
                               sizeof(verifyData),
                               verifyLength,
                               true)) {
            Serial.println("ESC kv: write completed, but read-back verification failed.");
            break;
        }

        if (verifyLength != ESC_LEGACY_EEPROM_WRITE_SIZE) {
            Serial.println("ESC kv: verification read length was unexpected.");
            break;
        }

        Serial.println("EEPROM summary after write:");
        printEscEepromSummary(verifyData, verifyLength);

        const uint8_t verifyRaw = verifyData[ESC_LEGACY_MOTOR_KV_OFFSET];
        if (verifyRaw != newRaw) {
            Serial.print("ESC kv: verification mismatch, expected raw=");
            Serial.print(newRaw);
            Serial.print(" but read back ");
            Serial.println(verifyRaw);
            break;
        }

        Serial.print("ESC kv: motor_kv_estimate updated successfully to ");
        Serial.println(decodeLegacyEscMotorKv(verifyRaw));
    } while (false);

    if (shouldResetEsc) {
        resetEscAfterDebugRead();
    }

    restoreEscCliSession();
    Serial.println("ESC kv: passthrough session closed.");
}
