#include "syn6288.h"

// ============================================================
// SYN6288 控制命令 (5字节)
// 格式: 0xFD + 0x00 + 0x02 + 命令字 + 校验和
// 校验和 = 0xFD ^ 0x00 ^ 0x02 ^ 命令字
// ============================================================
const uint8_t SYN_CMD_STOP[]      = {0xFD, 0x00, 0x02, 0x02, 0xFD};
const uint8_t SYN_CMD_PAUSE[]     = {0xFD, 0x00, 0x02, 0x03, 0xFC};
const uint8_t SYN_CMD_RESUME[]    = {0xFD, 0x00, 0x02, 0x04, 0xFB};
const uint8_t SYN_CMD_STATUS[]    = {0xFD, 0x00, 0x02, 0x21, 0xDE};
const uint8_t SYN_CMD_POWERDOWN[] = {0xFD, 0x00, 0x02, 0x88, 0x77};
const size_t  SYN_CMD_LEN = 5;

// SYN6288 的 UART 实例 (使用 HardwareSerial UART1)
static HardwareSerial* syn_serial = nullptr;
static unsigned long syn_last_speak_ms = 0;
static size_t syn_last_text_len = 0;

// ============================================================
// 初始化 SYN6288 串口
// ============================================================
void syn6288_init(unsigned long baud) {
    // ESP32-S3: UART1 使用 GPIO17(TX) 和 GPIO18(RX)
    // 注意: RX=GPIO18 接 SYN6288 的 TXD, TX=GPIO17 接 SYN6288 的 RXD
    syn_serial = &Serial1;
    syn_serial->begin(baud, SERIAL_8N1, 18, 17);  // RX=18, TX=17
    delay(100);
}

// ============================================================
// 发送 raw GB2312 字节进行语音合成
// text:  GB2312 编码的文本
// len:   文本字节长度
// music: 背景音乐 (0-15, 0=无背景音乐)
// ============================================================
void syn6288_speak_raw(const uint8_t* text, size_t len, uint8_t music) {
    if (syn_serial == nullptr || text == nullptr || len == 0) return;

    // ---- 构造 SYN6288 数据帧 ----
    // 帧头: 0xFD
    // 数据区长度高字节: 0x00
    // 数据区长度低字节: len(文本) + 3
    // 命令字: 0x01 (合成播放)
    // 命令参数: 0x01 | (music << 4)
    // 文本数据: GB2312 编码
    // 校验和: 所有字节的异或

    uint8_t frame[256];  // 足够大的帧缓冲区
    size_t frame_len = 0;

    // 帧头
    frame[0] = 0xFD;
    // 数据区长度高字节
    frame[1] = 0x00;
    // 数据区长度低字节 = 文本长度 + 3
    frame[2] = (uint8_t)(len + 3);
    // 命令字: 合成播放
    frame[3] = 0x01;
    // 命令参数: bit[7:4]=背景音乐编号, bit[3:0]=0x01
    frame[4] = 0x01 | (music << 4);
    frame_len = 5;

    // 计算校验和
    uint8_t ecc = 0;
    for (size_t i = 0; i < frame_len; i++) {
        ecc ^= frame[i];
    }
    for (size_t i = 0; i < len; i++) {
        ecc ^= text[i];
    }

    // 追加文本数据
    memcpy(&frame[frame_len], text, len);
    frame_len += len;

    // 追加校验和
    frame[frame_len] = ecc;
    frame_len++;

    // 发送
    syn_serial->write(frame, frame_len);
    syn_serial->flush();

    // 记录发送时间，用于估算忙碌状态
    // 粗略估算: 中文约 4 字/秒, 每字 2 字节 GB2312
    syn_last_text_len = len;
    syn_last_speak_ms = millis();
}

// ============================================================
// 停止合成
// ============================================================
void syn6288_stop() {
    syn6288_send_cmd(SYN_CMD_STOP, SYN_CMD_LEN);
}

// ============================================================
// 暂停合成
// ============================================================
void syn6288_pause() {
    syn6288_send_cmd(SYN_CMD_PAUSE, SYN_CMD_LEN);
}

// ============================================================
// 恢复合成
// ============================================================
void syn6288_resume() {
    syn6288_send_cmd(SYN_CMD_RESUME, SYN_CMD_LEN);
}

// ============================================================
// 发送控制命令
// ============================================================
void syn6288_send_cmd(const uint8_t* cmd, size_t len) {
    if (syn_serial == nullptr || cmd == nullptr) return;
    syn_serial->write(cmd, len);
    syn_serial->flush();
}

// ============================================================
// 检查是否正在播报 (基于文本长度估算)
// 中文约 4 字/秒 => 8 字节/秒
// ============================================================
bool syn6288_is_busy() {
    // 估算播报时间: 每字节 ~125ms (即 8 字节/秒)
    unsigned long estimated_ms = syn_last_text_len * 125;
    if (estimated_ms < 2000) estimated_ms = 2000;  // 最少 2 秒
    return (millis() - syn_last_speak_ms) < estimated_ms;
}
