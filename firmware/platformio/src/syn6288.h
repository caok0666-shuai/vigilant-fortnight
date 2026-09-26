#ifndef SYN6288_H
#define SYN6288_H

#include <Arduino.h>

// ============================================================
// SYN6288 中文语音合成模块驱动
// 绿深旗舰店 SYN6288 语音合成模块
// UART 串口通信，默认波特率 9600
// 发送 GB2312 编码的中文字符串即可朗读
// ============================================================
//
// 接线定义 (ESP32-S3):
//   VCC  -> 5V
//   GND  -> GND
//   RXD  -> GPIO17 (ESP32-S3 TX)
//   TXD  -> GPIO18 (ESP32-S3 RX)
//
// 语音控制标签 (放在文本中):
//   [v0~v16]  音量:   0=静音, 16=最大
//   [m0~m16]  背景音乐音量: 0=静音, 16=最大
//   [t0~t5]   语速:   0=最慢, 5=最快
//   [g0~g15]  背景音乐选择: 0=无背景音乐

// --- 控制命令 (5字节) ---
extern const uint8_t SYN_CMD_STOP[];
extern const uint8_t SYN_CMD_PAUSE[];
extern const uint8_t SYN_CMD_RESUME[];
extern const uint8_t SYN_CMD_STATUS[];
extern const uint8_t SYN_CMD_POWERDOWN[];
extern const size_t  SYN_CMD_LEN;

// --- 初始化 ---
void syn6288_init(unsigned long baud = 9600);

// --- 发送 raw GB2312 字节进行语音合成 ---
void syn6288_speak_raw(const uint8_t* text, size_t len, uint8_t music = 0);

// --- 便捷控制命令 ---
void syn6288_stop();
void syn6288_pause();
void syn6288_resume();
void syn6288_send_cmd(const uint8_t* cmd, size_t len);

// --- 忙碌状态 (根据文本长度估算) ---
bool syn6288_is_busy();

#endif
