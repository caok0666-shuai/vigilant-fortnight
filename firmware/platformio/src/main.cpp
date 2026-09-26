#include <Arduino.h>
#include "syn6288.h"
#include "pir_sensor.h"
#include "gb2312_utils.h"

// ============================================================
// 社区智能书架系统 - C 模块 (语音+感应)
// 负责: SYN6288 语音合成 + HC-SR501 人体感应
// 硬件: ESP32-S3 Dev Module
// ============================================================
//
// 引脚分配:
//   SYN6288 RXD -> GPIO17 (U1TXD)
//   SYN6288 TXD -> GPIO18 (U1RXD)
//   HC-SR501 OUT -> GPIO6  (外部中断)
//
// 协作接口 (与 B 模块):
//   通过 Serial0 接收来自 B 模块 (RFID+屏幕) 和 A 模块 (网络) 的指令
//   协议: 简单文本指令，以换行符分隔

// ============================================================
// 引脚配置
// ============================================================
#define PIR_PIN              6      // HC-SR501 OUT
#define PIR_COOLDOWN_MS      3000   // 冷却时间 (ms)
#define WELCOME_TIMEOUT_MS   60000  // 重新欢迎间隔 (ms)

// ============================================================
// 系统状态机
// ============================================================
enum State {
    S_IDLE,       // 空闲
    S_WELCOME,    // 欢迎
    S_GUIDE,      // 导读
    S_MSG,        // 留言
    S_BUSY        // 忙碌
};

static State g_state = S_IDLE;
static bool g_welcome_played = false;
static unsigned long g_last_welcome_ms = 0;

// ============================================================
// 预定义欢迎语 (GB2312 编码 + 控制标签)
// 使用 [vN][mN][tN] 控制标签
// ============================================================

// 完整欢迎语: "[v10][m0][t3]欢迎来到社区智能书架，请将书籍放在感应区，开始您的阅读之旅"
static const uint8_t FULL_WELCOME[] = {
    0x5B, 0x76, 0x31, 0x30, 0x5D,   // [v10] 音量
    0x5B, 0x6D, 0x30, 0x5D,         // [m0]  无背景音乐
    0x5B, 0x74, 0x33, 0x5D,         // [t3]  语速3
    0xBB, 0xB6, 0xD3, 0xAD,         // 欢迎
    0xC0, 0xB4, 0xB5, 0xBD,         // 来到
    0xC9, 0xE7, 0xC7, 0xF8,         // 社区
    0xD6, 0xC7, 0xC4, 0xDC,         // 智能
    0xCA, 0xE9, 0xBC, 0xDC,         // 书架
    0xA3, 0xAC,                     // ，
    0xC7, 0xEB, 0xBD, 0xAB,         // 请将
    0xCA, 0xE9, 0xBC, 0xAE,         // 书籍
    0xB7, 0xC5, 0xD4, 0xDA,         // 放在
    0xB8, 0xD0, 0xD3, 0xA6,         // 感应
    0xC7, 0xF8,                     // 区
    0xA3, 0xAC,                     // ，
    0xBF, 0xAA, 0xCA, 0xBC,         // 开始
    0xC4, 0xFA, 0xB5, 0xC4,         // 您的
    0xD4, 0xC4, 0xB6, 0xC1,         // 阅读
    0xD6, 0xAE, 0xC2, 0xC3          // 之旅
};
static const size_t FULL_WELCOME_LEN = sizeof(FULL_WELCOME);

// 借书成功: "[v10][m0][t3]借书成功，请拿好您的书籍，祝您阅读愉快"
static const uint8_t MSG_BORROW[] = {
    0x5B, 0x76, 0x31, 0x30, 0x5D, 0x5B, 0x6D, 0x30, 0x5D, 0x5B, 0x74, 0x33, 0x5D,
    0xBD, 0xE8, 0xCA, 0xE9, 0xB3, 0xC9, 0xB9, 0xA6, 0xA3, 0xAC,
    0xC7, 0xEB, 0xC4, 0xC3, 0xBA, 0xC3, 0xC4, 0xFA, 0xB5, 0xC4, 0xCA, 0xE9, 0xBC, 0xAE, 0xA3, 0xAC,
    0xD7, 0xA3, 0xC4, 0xFA, 0xD4, 0xC4, 0xB6, 0xC1, 0xD3, 0xE4, 0xBF, 0xEC
};
static const size_t MSG_BORROW_LEN = sizeof(MSG_BORROW);

// 还书成功: "[v10][m0][t3]还书成功，感谢您的归还，期待您再次借阅"
static const uint8_t MSG_RETURN[] = {
    0x5B, 0x76, 0x31, 0x30, 0x5D, 0x5B, 0x6D, 0x30, 0x5D, 0x5B, 0x74, 0x33, 0x5D,
    0xBB, 0xB9, 0xCA, 0xE9, 0xB3, 0xC9, 0xB9, 0xA6, 0xA3, 0xAC,
    0xB8, 0xD0, 0xD0, 0xBB, 0xC4, 0xFA, 0xB5, 0xC4, 0xB9, 0xE9, 0xBB, 0xB9, 0xA3, 0xAC,
    0xC6, 0xDA, 0xB4, 0xFD, 0xC4, 0xFA, 0xD4, 0xD9, 0xB4, 0xCE, 0xBD, 0xE8, 0xD4, 0xC4
};
static const size_t MSG_RETURN_LEN = sizeof(MSG_RETURN);

// 系统就绪: "[v8][m0][t3]系统已就绪"
static const uint8_t MSG_READY[] = {
    0x5B, 0x76, 0x38, 0x5D, 0x5B, 0x6D, 0x30, 0x5D, 0x5B, 0x74, 0x33, 0x5D,
    0xCF, 0xB5, 0xCD, 0xB3, 0xD2, 0xD1, 0xBE, 0xCD, 0xD0, 0xF7
};
static const size_t MSG_READY_LEN = sizeof(MSG_READY);

// ============================================================
// 对外接口 (供 B 模块 extern 调用)
// ============================================================
void voice_borrow_ok();
void voice_return_ok();
void voice_speak_gb(const uint8_t* data, size_t len);
int  voice_get_state();
bool voice_is_busy();

// ============================================================
// 初始化
// ============================================================
void setup() {
    Serial.begin(115200);
    delay(500);

    Serial.println(F("=== 社区智能书架 - C模块 (语音+感应) ==="));
    Serial.println(F("SYN6288: UART1 GPIO17/18 @ 9600bps"));
    Serial.println(F("HC-SR501: GPIO6 INT"));

    // 初始化 SYN6288
    syn6288_init(9600);
    delay(500);

    // 初始化 HC-SR501 (CHANGE: 上升沿和下降沿都触发)
    pir_init(PIR_PIN, CHANGE, nullptr);

    // 上电自检播报
    delay(1000);
    syn6288_speak_raw(MSG_READY, MSG_READY_LEN, 0);
    delay(3000);

    Serial.println(F("初始化完成! 等待读者靠近..."));
    Serial.println(F("指令格式: BORROW / RETURN / READ:xxx / MSG:xxx / STOP"));
}

// ============================================================
// 主循环
// ============================================================
void loop() {
    static bool last_pir = false;
    bool pir_now = pir_is_present();
    unsigned long now = millis();

    // ---- 1. 人体感应处理 ----
    // 上升沿: 无人 -> 有人
    if (pir_now && !last_pir && !pir_in_cooldown(PIR_COOLDOWN_MS)) {
        Serial.println(F("[PIR] 检测到读者靠近!"));

        if (!g_welcome_played || (now - g_last_welcome_ms > WELCOME_TIMEOUT_MS)) {
            if (!syn6288_is_busy()) {
                g_state = S_WELCOME;
                syn6288_speak_raw(FULL_WELCOME, FULL_WELCOME_LEN, 0);
                g_welcome_played = true;
                g_last_welcome_ms = now;
                g_state = S_BUSY;
            }
        }
    }

    // 下降沿: 有人 -> 无人
    if (!pir_now && last_pir) {
        Serial.println(F("[PIR] 读者离开"));
        g_welcome_played = false;
    }

    last_pir = pir_now;

    // ---- 2. 串口指令处理 (来自 B/A 模块) ----
    if (Serial.available() > 0) {
        String cmd = Serial.readStringUntil('\n');
        cmd.trim();

        if (cmd.length() > 0) {
            Serial.print(F("[CMD] "));
            Serial.println(cmd);

            if (cmd == "STOP") {
                syn6288_stop();
                g_state = S_IDLE;
            }
            else if (cmd == "BORROW") {
                g_state = S_BUSY;
                syn6288_speak_raw(MSG_BORROW, MSG_BORROW_LEN, 0);
            }
            else if (cmd == "RETURN") {
                g_state = S_BUSY;
                syn6288_speak_raw(MSG_RETURN, MSG_RETURN_LEN, 0);
            }
            else if (cmd.startsWith("READ:")) {
                // 导读: 在前面加控制标签
                // 格式: READ:[v10][m0][t3]为您导读xxx
                g_state = S_GUIDE;
                String info = cmd.substring(5);
                // 直接发送 (假设info已包含GB2312编码)
                syn6288_speak_raw((const uint8_t*)info.c_str(), info.length(), 0);
                g_state = S_BUSY;
            }
            else if (cmd.startsWith("MSG:")) {
                // 社区留言
                g_state = S_MSG;
                String msg = cmd.substring(4);
                syn6288_speak_raw((const uint8_t*)msg.c_str(), msg.length(), 1);
                g_state = S_BUSY;
            }
            else {
                Serial.println(F("[CMD] 未知指令"));
            }
        }
    }

    // ---- 3. 播报完成自动恢复 ----
    if (g_state == S_BUSY && !syn6288_is_busy()) {
        g_state = S_IDLE;
        Serial.println(F("[State] -> IDLE"));
    }

    delay(100);
}

// ============================================================
// 对外接口
// ============================================================
void voice_borrow_ok() {
    if (!syn6288_is_busy()) {
        g_state = S_BUSY;
        syn6288_speak_raw(MSG_BORROW, MSG_BORROW_LEN, 0);
    }
}

void voice_return_ok() {
    if (!syn6288_is_busy()) {
        g_state = S_BUSY;
        syn6288_speak_raw(MSG_RETURN, MSG_RETURN_LEN, 0);
    }
}

void voice_speak_gb(const uint8_t* data, size_t len) {
    syn6288_stop();
    delay(50);
    g_state = S_BUSY;
    syn6288_speak_raw(data, len, 0);
}

int voice_get_state() {
    return (int)g_state;
}

bool voice_is_busy() {
    return syn6288_is_busy();
}
