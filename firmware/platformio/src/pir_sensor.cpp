#include "pir_sensor.h"

// ============================================================
// HC-SR501 人体红外感应模块
// ============================================================

static uint8_t  pir_pin = 0;
static pir_callback_t pir_cb = nullptr;
static volatile bool pir_state = false;
static volatile unsigned long pir_trigger_time = 0;
static bool pir_enabled = true;

// 中断服务函数
static void IRAM_ATTR pir_isr_handler() {
    pir_state = (digitalRead(pir_pin) == HIGH);
    pir_trigger_time = millis();
    // 不在这里调用回调，避免ISR中执行耗时操作
}

// ============================================================
// 初始化
// ============================================================
void pir_init(uint8_t pin, uint8_t mode, pir_callback_t callback) {
    pir_pin = pin;
    pir_cb = callback;

    pinMode(pir_pin, INPUT);
    attachInterrupt(digitalPinToInterrupt(pir_pin), pir_isr_handler, mode);
}

// ============================================================
// 读取当前状态
// ============================================================
bool pir_is_present() {
    return (digitalRead(pir_pin) == HIGH);
}

// ============================================================
// 设置回调函数
// ============================================================
void pir_set_callback(pir_callback_t callback) {
    pir_cb = callback;
}

// ============================================================
// 获取最近一次触发的时间戳
// ============================================================
unsigned long pir_last_trigger_time() {
    return pir_trigger_time;
}

// ============================================================
// 使能传感器
// ============================================================
void pir_enable() {
    if (!pir_enabled) {
        pir_enabled = true;
        attachInterrupt(digitalPinToInterrupt(pir_pin), pir_isr_handler, CHANGE);
    }
}

// ============================================================
// 禁用传感器
// ============================================================
void pir_disable() {
    if (pir_enabled) {
        pir_enabled = false;
        detachInterrupt(digitalPinToInterrupt(pir_pin));
    }
}

// ============================================================
// 检查是否在冷却期 (防抖)
// ============================================================
bool pir_in_cooldown(unsigned long cooldown_ms) {
    return (millis() - pir_trigger_time) < cooldown_ms;
}
