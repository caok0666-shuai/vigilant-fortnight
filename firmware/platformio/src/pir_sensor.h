#ifndef PIR_SENSOR_H
#define PIR_SENSOR_H

#include <Arduino.h>

// ============================================================
// HC-SR501 人体红外感应模块驱动
// 宽压输入，3.3V 逻辑电平输出
// 灵敏度与延时物理可调
// ============================================================
//
// 接线定义 (ESP32-S3):
//   VCC  -> 5V
//   GND  -> GND
//   OUT  -> GPIO6 (配置为外部中断输入)
//
// 模块参数:
//   - 触发方式: 可通过跳线帽选择 L(不可重复触发) / H(可重复触发)
//   - 延时调节: 顺时针增大 (0.3s ~ 300s)
//   - 灵敏度调节: 顺时针增大 (3m ~ 7m)
//   - 默认输出: 高电平=有人, 低电平=无人

// --- 回调函数类型 ---
typedef void (*pir_callback_t)(void);

// --- 初始化 ---
// pin:       GPIO引脚号
// mode:      RISING(有人出现), FALLING(人离开), CHANGE(两种都触发)
// callback:  回调函数 (可为nullptr)
void pir_init(uint8_t pin, uint8_t mode = RISING, pir_callback_t callback = nullptr);

// --- 读取当前状态 ---
// 返回 true 表示检测到人体
bool pir_is_present();

// --- 设置回调函数 ---
void pir_set_callback(pir_callback_t callback);

// --- 获取最近一次触发的时间戳 ---
unsigned long pir_last_trigger_time();

// --- 使能/禁用传感器 ---
void pir_enable();
void pir_disable();

// --- 检查是否在冷却期 (防抖) ---
bool pir_in_cooldown(unsigned long cooldown_ms = 2000);

#endif
