#include "motor_control.h"

#include <cstring>
#include <SoftwareSerial.h>

#include "app_config.h"
#include "app_state.h"
#include "esc_telemetry.h"

namespace {

constexpr unsigned long ESC_BOOTLOADER_BAUD = 19200;
const char* PASSTHROUGH_PREF_KEY = "pt_once";

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
    Serial.println("Holding the signal line idle and waiting for the ESC to fall back into its bootloader window...");

    prepareEscDirectSession();

    uint8_t bootInit[] = {0, 0, 0, 0, 0, 0, 0, 0, 0x0D, 'B', 'L', 'H', 'e', 'l', 'i', 0xF4, 0x7D};
    uint8_t bootRx[64] = {0};
    uint8_t setAddressCmd[4] = {0};
    uint8_t setAddressRx[32] = {0};
    uint8_t readCmd[2] = {CMD_READ_FLASH_SIL, ESC_PARAM_READ_SIZE};
    uint8_t readRx[ESC_PARAM_READ_SIZE + 48] = {0};

    do {
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
            } else {
                Serial.println("RX boot init: no bytes");
            }

            if (bootRxSize > 0) {
                break;
            }

            nextAttemptMs = now + 150;
        }

        if (bootRxSize == 0) {
            Serial.println("ESC params: no bootloader response after retry window.");
            Serial.println("Hint: this usually means the ESC never entered the AM32 bootloader.");
            Serial.println("Try powering the ESC first, then run `esc params`, or rerun immediately after an ESC power-cycle.");
            break;
        }

        if (bootRxSize >= 4) {
            printAsciiPreview("Boot header", bootRx, 4);
        }

        if (bootRx[bootRxSize - 1] == BR_SUCCESS) {
            shouldResetEsc = true;
        }

        EscBootInfo bootInfo;
        if (!parseEscBootInfo(bootRx, bootRxSize, bootInfo)) {
            Serial.println("ESC params: bootloader reply did not match a known AM32 device signature.");
            break;
        }

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

        const uint16_t readAddress = getEncodedEscReadAddress(bootInfo);
        const uint32_t absoluteReadAddress = bootInfo.absoluteEepromAddress - 32UL;
        Serial.print("Read start address: encoded=0x");
        Serial.print(readAddress, HEX);
        Serial.print(" absolute=0x");
        Serial.println(absoluteReadAddress, HEX);
        setAddressCmd[0] = CMD_SET_ADDRESS;
        setAddressCmd[1] = 0x00;
        setAddressCmd[2] = (uint8_t)(readAddress >> 8);
        setAddressCmd[3] = (uint8_t)(readAddress & 0xFF);

        printHexBuffer("TX set address", setAddressCmd, sizeof(setAddressCmd));
        sendEsc(setAddressCmd, sizeof(setAddressCmd));
        delay(5);

        uint16_t setAddressRxSize = getEsc(setAddressRx, 200);
        printHexBuffer("RX set address", setAddressRx, setAddressRxSize);
        if (setAddressRxSize == 0 || setAddressRx[0] != BR_SUCCESS) {
            Serial.println("ESC params: set-address command failed.");
            break;
        }

        printHexBuffer("TX read block", readCmd, sizeof(readCmd));
        sendEsc(readCmd, sizeof(readCmd));
        delay(ESC_PARAM_READ_SIZE);

        uint16_t readRxSize = getEsc(readRx, 500);
        printHexBuffer("RX read block (raw)", readRx, readRxSize);
        if (readRxSize == 0) {
            Serial.println("ESC params: read command returned no data.");
            break;
        }

        const uint16_t normalizedReadRxSize = trimReadResponsePrefix(readRx, readRxSize, ESC_PARAM_READ_SIZE);
        if (normalizedReadRxSize != readRxSize) {
            Serial.print("Normalized read response by trimming ");
            Serial.print(readRxSize - normalizedReadRxSize);
            Serial.println(" leading bytes.");
            printHexBuffer("RX read block (normalized)", readRx, normalizedReadRxSize);
        }

        if (normalizedReadRxSize < (ESC_PARAM_READ_SIZE + 3)) {
            Serial.println("ESC params: read response was shorter than expected.");
            break;
        }

        if (readRx[normalizedReadRxSize - 1] != BR_SUCCESS) {
            Serial.println("ESC params: read response did not end with bootloader ACK 0x30.");
            break;
        }

        const uint16_t payloadPlusCrcLength = normalizedReadRxSize - 1;
        const bool crcOk = checkEscResponseCrc(readRx, payloadPlusCrcLength);
        Serial.print("Read CRC check: ");
        Serial.println(crcOk ? "OK" : "FAILED");
        if (!crcOk) {
            break;
        }

        const uint16_t payloadLength = payloadPlusCrcLength - 2;
        Serial.print("Read payload bytes: ");
        Serial.println(payloadLength);
        if (payloadLength < ESC_PARAM_READ_SIZE) {
            Serial.println("ESC params: payload shorter than requested 80 bytes.");
            break;
        }

        printAsciiPreview("Firmware name preview", readRx, 15);
        printHexBuffer("EEPROM payload[0..47]", readRx + 32, 48);

        const uint8_t* eeprom = readRx + 32;
        Serial.println("Basic parameter check:");
        Serial.print("  eeprom_marker=0x");
        printHexByte(eeprom[0]);
        Serial.print("  eeprom_version=");
        Serial.print(eeprom[1]);
        Serial.print("  bootloader_version_byte=");
        Serial.print(eeprom[2]);
        Serial.print("  fw=");
        Serial.print(eeprom[3]);
        Serial.print('.');
        Serial.println(eeprom[4]);

        if (eeprom[1] >= 3) {
            Serial.print("  max_roc raw=");
            Serial.print(eeprom[5]);
            Serial.print(" interpreted=");
            Serial.print(eeprom[5] / 10.0f, 1);
            Serial.println(" (modern AM32 layout)");
        } else {
            Serial.println("  legacy EEPROM layout detected; modern field offsets are not reliable here.");
            printAsciiPreview("  legacy bytes[5..19] ascii", eeprom + 5, 15);
        }

        Serial.println("ESC params: read completed.");
    } while (false);

    if (shouldResetEsc) {
        resetEscAfterDebugRead();
    }

    restoreEscCliSession();
    Serial.println("ESC params: passthrough session closed.");
}
