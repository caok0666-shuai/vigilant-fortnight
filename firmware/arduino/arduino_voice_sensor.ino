// ============================================================
//  社区智能书架 - 完整硬件端
//  ESP32-S3 Dev Module
//  SYN6288 语音 + HC-SR501 人体感应 + MFRC522 RFID
//  用 Arduino IDE 打开本文件直接烧录
// ============================================================
//
// 开发板设置 (Arduino IDE -> 工具):
//   开发板:           ESP32S3 Dev Module
//   USB CDC On Boot:  Disabled ← 必须!
//   PSRAM:            8MB
//   Flash Size:       16MB (需手动配置)
//   Upload Speed:     115200
//
// 接线:
//   MFRC522: SDA->GPIO9 SCK->GPIO10 MOSI->GPIO11 MISO->GPIO12 RST->GPIO13 VCC->3.3V GND->GND
//   SYN6288: RXD->GPIO17 TXD->GPIO18 VCC->5V GND->GND
//   HC-SR501: OUT->GPIO6 VCC->5V GND->GND

#include <Arduino.h>
#include <SPI.h>
#include <MFRC522.h>
#include <string.h>

// ============================================================
// 1. MFRC522 RFID 配置
// ============================================================
#define RST_PIN    13
#define SS_PIN     9

MFRC522 mfrc522(SS_PIN, RST_PIN);
MFRC522::MIFARE_Key defaultKey = {{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}};

byte currentUID[10] = {0};
byte currentUIDSize = 0;

unsigned long lastCardTime = 0;
byte lastCardUID[10] = {0};
byte lastCardUIDSize = 0;
#define CARD_COOLDOWN_MS  5000
#define CARD_ABSENT_MS    2000

enum CardState { CARD_NONE, CARD_PRESENT, CARD_LEFT };
CardState cardState = CARD_NONE;
unsigned long cardPresentTime = 0;

// ============================================================
// 2. SYN6288 语音模块 (UART1)
// ============================================================
const uint8_t SYN_CMD_STOP[]   = {0xFD, 0x00, 0x02, 0x02, 0xFD};
const uint8_t SYN_CMD_PAUSE[]  = {0xFD, 0x00, 0x02, 0x03, 0xFC};
const uint8_t SYN_CMD_RESUME[] = {0xFD, 0x00, 0x02, 0x04, 0xFB};

static HardwareSerial* synSerial = nullptr;
static unsigned long synLastMs = 0;
static size_t synTextLen = 0;

void syn6288_init(unsigned long baud = 9600) {
    synSerial = &Serial1;
    synSerial->begin(baud, SERIAL_8N1, 18, 17);
    delay(100);
}

void syn6288_speak_raw(const uint8_t* text, size_t len, uint8_t music = 0) {
    if (!synSerial || !text || !len) return;
    uint8_t frame[256];
    frame[0] = 0xFD;
    frame[1] = 0x00;
    frame[2] = len + 3;
    frame[3] = 0x01;
    frame[4] = 0x01 | (music << 4);
    uint8_t ecc = 0;
    for (int i = 0; i < 5; i++) ecc ^= frame[i];
    for (size_t i = 0; i < len; i++) ecc ^= text[i];
    memcpy(&frame[5], text, len);
    frame[5 + len] = ecc;
    synSerial->write(frame, 5 + len + 1);
    synSerial->flush();
    synTextLen = len;
    synLastMs = millis();
}

void syn6288_stop() {
    if (synSerial) { synSerial->write(SYN_CMD_STOP, 5); synSerial->flush(); }
    synTextLen = 0;
    synLastMs = 0;
}

bool syn6288_is_busy() {
    unsigned long est = synTextLen * 150;
    if (est < 2000) est = 2000;
    if (est > 15000) est = 15000;
    return (millis() - synLastMs) < est;
}

// ============================================================
// 3. HC-SR501 人体感应 (GPIO6 中断)
// ============================================================
#define PIR_PIN      6
#define PIR_COOLDOWN 15000
#define WELCOME_MS   300000

static volatile bool pirTriggered = false;
static unsigned long pirLastProcessed = 0;

void IRAM_ATTR pir_isr() { pirTriggered = true; }

void pir_init() {
    pinMode(PIR_PIN, INPUT);
    attachInterrupt(digitalPinToInterrupt(PIR_PIN), pir_isr, RISING);
}

bool pir_is_present() { return digitalRead(PIR_PIN) == HIGH; }

bool pir_in_cooldown() {
    if (pirLastProcessed == 0) return false;
    return (millis() - pirLastProcessed) < PIR_COOLDOWN;
}

// ============================================================
// 4. GB2312 预定义语音
// ============================================================
const uint8_t WELCOME[] = {
    0x5B,0x76,0x31,0x30,0x5D,0x5B,0x6D,0x30,0x5D,0x5B,0x74,0x33,0x5D,
    0xBB,0xB6,0xD3,0xAD,0xC0,0xB4,0xB5,0xBD,
    0xC9,0xE7,0xC7,0xF8,0xD6,0xC7,0xC4,0xDC,0xCA,0xE9,0xBC,0xDC,
    0xA3,0xAC,0xC7,0xEB,0xBD,0xAB,0xCA,0xE9,0xBC,0xAE,
    0xB7,0xC5,0xD4,0xDA,0xB8,0xD0,0xD3,0xA6,0xC7,0xF8,
    0xA3,0xAC,0xBF,0xAA,0xCA,0xBC,0xC4,0xFA,0xB5,0xC4,
    0xD4,0xC4,0xB6,0xC1,0xD6,0xAE,0xC2,0xC3
};

const uint8_t MSG_BORROW[] = {
    0x5B,0x76,0x31,0x30,0x5D,0x5B,0x6D,0x30,0x5D,0x5B,0x74,0x33,0x5D,
    0xBD,0xE8,0xCA,0xE9,0xB3,0xC9,0xB9,0xA6,0xA3,0xAC,
    0xC7,0xEB,0xC4,0xC3,0xBA,0xC3,0xC4,0xFA,0xB5,0xC4,0xCA,0xE9,0xBC,0xAE,
    0xA3,0xAC,0xD7,0xA3,0xC4,0xFA,0xD4,0xC4,0xB6,0xC1,0xD3,0xE4,0xBF,0xEC
};

const uint8_t MSG_RETURN[] = {
    0x5B,0x76,0x31,0x30,0x5D,0x5B,0x6D,0x30,0x5D,0x5B,0x74,0x33,0x5D,
    0xBB,0xB9,0xCA,0xE9,0xB3,0xC9,0xB9,0xA6,0xA3,0xAC,
    0xB8,0xD0,0xD0,0xBB,0xC4,0xFA,0xB5,0xC4,0xB9,0xE9,0xBB,0xB9,
    0xA3,0xAC,0xC6,0xDA,0xB4,0xFD,0xC4,0xFA,0xD4,0xD9,0xB4,0xCE,0xBD,0xE8,0xD4,0xC4
};

const uint8_t MSG_READY[] = {
    0x5B,0x76,0x38,0x5D,0x5B,0x6D,0x30,0x5D,0x5B,0x74,0x33,0x5D,
    0xCF,0xB5,0xCD,0xB3,0xD2,0xD1,0xBE,0xCD,0xD0,0xF7
};

const uint8_t MSG_PLACE_CARD[] = {
    0x5B,0x76,0x31,0x30,0x5D,0x5B,0x6D,0x30,0x5D,0x5B,0x74,0x33,0x5D,
    0xC7,0xEB,0xBD,0xAB,0xCA,0xE9,0xBC,0xAE,
    0xCC,0xF9,0xBD,0xFC,0xB8,0xD0,0xD3,0xA6,0xC7,0xF8
};

const uint8_t MSG_CARD_FOUND[] = {
    0x5B,0x76,0x31,0x30,0x5D,0x5B,0x6D,0x30,0x5D,0x5B,0x74,0x33,0x5D,
    0xD2,0xD1,0xCA,0xB6,0xB1,0xF0,0xB5,0xBD,
    0xCA,0xE9,0xBC,0xAE,0xB1,0xEA,0xC7,0xA9
};

// ============================================================
// 5. 系统状态
// ============================================================
enum { S_IDLE, S_BUSY };
int  state = S_IDLE;
bool welcomePlayed = false;
unsigned long lastWelcomeMs = 0;

// 卡片语音播报状态机 (非阻塞)
enum CardSpState { CS_NONE, CS_WAIT_FOUND, CS_WAIT_BACKEND, CS_WAIT_ACTION, CS_WAIT_DONE };
CardSpState cardSpState = CS_NONE;
unsigned long cardSpStateMs = 0;
#define CARDSP_TIMEOUT_MS 10000
bool cardSpBorrow = false;

// ============================================================
// 6. 辅助函数
// ============================================================

bool uidMatch(byte* a, byte aSize, byte* b, byte bSize) {
    if (aSize != bSize) return false;
    for (byte i = 0; i < aSize; i++) {
        if (a[i] != b[i]) return false;
    }
    return true;
}

void uidCopy(byte* dst, byte* src, byte size) {
    for (byte i = 0; i < size; i++) dst[i] = src[i];
}

void uidPrint(byte* uid, byte size) {
    for (byte i = 0; i < size; i++) {
        if (uid[i] < 0x10) Serial.print(F("0"));
        Serial.print(uid[i], HEX);
        Serial.print(F(" "));
    }
}

// 播放语音 + 串口日志
void speakLog(const uint8_t* data, size_t len, const __FlashStringHelper* label) {
    Serial.print(F("[语音] "));
    Serial.println(label);
    if (!syn6288_is_busy()) {
        syn6288_speak_raw(data, len, 0);
        state = S_BUSY;
    } else {
        Serial.println(F("[语音] 跳过(忙碌)"));
    }
}

// ============================================================
// 7. setup
// ============================================================
void setup() {
    Serial.begin(115200);
    delay(500);

    Serial.println(F(""));
    Serial.println(F("=================================="));
    Serial.println(F(" 社区智能书架 - 完整硬件端"));
    Serial.println(F(" ESP32-S3 + RC522 + SYN6288 + PIR"));
    Serial.println(F("=================================="));

    SPI.begin(10, 12, 11, 9);
    Serial.println(F("[RC522] SPI 初始化完成"));

    mfrc522.PCD_Init();
    mfrc522.PCD_DumpVersionToSerial();
    Serial.println(F("[RC522] 模块初始化完成"));

    Serial.println(F("[SYN6288] 初始化 UART1(GPIO17/18)..."));
    syn6288_init(9600);
    delay(500);

    Serial.println(F("[PIR] 初始化 GPIO6..."));
    pir_init();

    delay(1000);
    Serial.println(F("[语音] 系统已就绪"));
    syn6288_speak_raw(MSG_READY, sizeof(MSG_READY), 0);
    delay(3000);

    Serial.println(F(""));
    Serial.println(F("=== 就绪! ==="));
    Serial.println(F("串口指令: BORROW | RETURN | STOP"));
    Serial.println(F("靠近书架自动欢迎, 刷卡自动识别"));
    Serial.println(F(""));
}

// ============================================================
// 8. loop
// ============================================================
void loop() {
    unsigned long now = millis();

    // ---- 8.1 RFID 刷卡检测 ----
    bool cardDetected = false;

    if (mfrc522.PICC_IsNewCardPresent()) {
        if (mfrc522.PICC_ReadCardSerial()) {
            cardDetected = true;

            currentUIDSize = mfrc522.uid.size;
            for (byte i = 0; i < currentUIDSize; i++) {
                currentUID[i] = mfrc522.uid.uidByte[i];
            }

            Serial.print(F("[RC522] 检测到卡 UID: "));
            uidPrint(currentUID, currentUIDSize);
            Serial.println();

            bool sameCard = (cardState == CARD_PRESENT) &&
                            uidMatch(currentUID, currentUIDSize, lastCardUID, lastCardUIDSize);

            if (!sameCard) {
                if (cardState == CARD_PRESENT) {
                    Serial.println(F("[RC522] 上一张卡已离开"));
                }

                // 冷却期检查
                if (uidMatch(currentUID, currentUIDSize, lastCardUID, lastCardUIDSize) &&
                    (now - lastCardTime) < CARD_COOLDOWN_MS) {
                    // 只打印一次冷却提示
                    static unsigned long lastCoolMsg = 0;
                    if (now - lastCoolMsg > 3000) {
                        Serial.println(F("[RC522] 冷却中, 请稍候..."));
                        lastCoolMsg = now;
                    }
                } else {
                    cardState = CARD_PRESENT;
                    cardPresentTime = now;
                    uidCopy(lastCardUID, currentUID, currentUIDSize);
                    lastCardUIDSize = currentUIDSize;
                    lastCardTime = now;

                    // === 刷卡上报后端 (后端判断借/还) ===
                    if (cardSpState != CS_NONE) {
                        Serial.print(F("[语音] 播放中，跳过刷卡: "));
                        uidPrint(currentUID, currentUIDSize);
                        Serial.println();
                        cardPresentTime = now;
                    } else {
                        syn6288_stop();
                        delay(50);
                        Serial.print(F("[系统] 刷卡上报: UID="));
                        uidPrint(currentUID, currentUIDSize);
                        Serial.println();
                        Serial.print(F("CARD:"));
                        uidPrint(currentUID, currentUIDSize);
                        Serial.println();
                        speakLog(MSG_CARD_FOUND, sizeof(MSG_CARD_FOUND), F("已识别到书籍标签"));
                        cardSpState = CS_WAIT_FOUND;
                        cardSpStateMs = now;
                    }

                    mfrc522.PICC_HaltA();
                    mfrc522.PCD_StopCrypto1();
                }
            } else {
                mfrc522.PICC_HaltA();
                mfrc522.PCD_StopCrypto1();
            }
        }
    }

    // 检测卡离开
    if (!cardDetected && cardState == CARD_PRESENT) {
        if (now - cardPresentTime > CARD_ABSENT_MS) {
            Serial.println(F("[RC522] 卡已移开"));
            cardState = CARD_NONE;
        }
    }

    // ---- 8.1b 卡片状态超时保护 ----
    if (cardSpState == CS_WAIT_BACKEND && (now - cardSpStateMs > CARDSP_TIMEOUT_MS)) {
        Serial.println(F("[系统] 后端响应超时，自动恢复"));
        cardSpState = CS_NONE;
    }

    // ---- 8.2 人体感应 ----
    if (pirTriggered && !pir_in_cooldown()) {
        pirTriggered = false;
        Serial.println(F("[PIR] 有人靠近!"));

        if (!welcomePlayed || (now - lastWelcomeMs > WELCOME_MS)) {
            // 【关键】卡片语音播放中（识别→借/还→完成），不打断
            if (cardSpState != CS_NONE) {
                Serial.println(F("[PIR] 卡片处理中，跳过欢迎语"));
            } else if (!syn6288_is_busy()) {
                speakLog(WELCOME, sizeof(WELCOME), F("欢迎来到社区智能书架"));
                welcomePlayed = true;
                lastWelcomeMs = now;
                state = S_BUSY;
                pirLastProcessed = now;
            }
        }
    }

    // 人离开检测
    static bool lastPir = false;
    static unsigned long lowStart = 0;
    bool nowPir = pir_is_present();
    if (!nowPir && lastPir) {
        lowStart = now;
    }
    if (!nowPir && lowStart > 0 && (now - lowStart > 10000)) {
        if (welcomePlayed) {
            Serial.println(F("[PIR] 读者离开"));
            welcomePlayed = false;
        }
        lowStart = 0;
    }
    if (nowPir) lowStart = 0;
    lastPir = nowPir;

    // ---- 8.3 串口指令 (来自 A 网络模块) ----
    if (Serial.available() > 0) {
        String cmd = Serial.readStringUntil('\n');
        cmd.trim();
        if (cmd.length() > 0) {
            Serial.print(F("[CMD] ")); Serial.println(cmd);

            if (cmd == "STOP") {
                syn6288_stop();
                state = S_IDLE;
                cardSpState = CS_NONE;
            }
            else if (cmd.startsWith("BORROW_OK")) {
                Serial.println(F("[语音] 借书成功"));
                cardSpBorrow = true;
                if (cardSpState == CS_WAIT_FOUND || cardSpState == CS_WAIT_BACKEND) {
                    cardSpState = CS_WAIT_ACTION;
                    cardSpStateMs = now;
                }
            }
            else if (cmd.startsWith("RETURN_OK")) {
                Serial.println(F("[语音] 还书成功"));
                cardSpBorrow = false;
                if (cardSpState == CS_WAIT_FOUND || cardSpState == CS_WAIT_BACKEND) {
                    cardSpState = CS_WAIT_ACTION;
                    cardSpStateMs = now;
                }
            }
            else if (cmd.startsWith("UNKNOWN")) {
                speakLog(MSG_PLACE_CARD, sizeof(MSG_PLACE_CARD), F("未知书籍标签"));
                cardSpState = CS_NONE;
            }
else if (cmd.startsWith("MSG:")) {
                String t = cmd.substring(4);
                syn6288_speak_raw((const uint8_t*)t.c_str(), t.length(), 1);
                Serial.print(F("[语音] MSG: "));
                Serial.println(t);
                state = S_BUSY;
            }
        }
    }

    // ---- 8.4 卡片语音播报状态机 (非阻塞) ----
    // 阶段1: "已识别到书籍标签"播完 → 自动播"借书/还书成功"
    if (cardSpState == CS_WAIT_FOUND && !syn6288_is_busy()) {
        cardSpState = CS_WAIT_BACKEND;
        cardSpStateMs = now;
    }

    // 阶段2: "借书/还书成功"播完 → 恢复空闲
    if (cardSpState == CS_WAIT_ACTION && !syn6288_is_busy()) {
        if (cardSpBorrow) {
            speakLog(MSG_BORROW, sizeof(MSG_BORROW), F("借书成功，请拿好您的书籍"));
        } else {
            speakLog(MSG_RETURN, sizeof(MSG_RETURN), F("还书成功，感谢您的归还"));
        }
        cardSpState = CS_WAIT_DONE;
        cardSpStateMs = now;
    }

    // CS_WAIT_DONE: wait for borrow/return speech to finish
    if (cardSpState == CS_WAIT_DONE && !syn6288_is_busy()) {
        cardSpState = CS_NONE;
    }

    // ---- 8.5 状态恢复 ----
    if (state == S_BUSY && !syn6288_is_busy()) {
        state = S_IDLE;
    }

    delay(50);
}
