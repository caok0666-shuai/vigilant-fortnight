# 社区智能书架 - C模块 (语音播报 + 人体感应)

## 负责人
C (你)

## 硬件
- ESP32-S3 Dev Module (开发板)
- SYN6288 语音合成模块 (带喇叭)
- HC-SR501 人体红外感应模块

## 接线

| 外设       | 引脚       | 开发板引脚 | 说明         |
|-----------|-----------|-----------|-------------|
| SYN6288 VCC | 5V       | 5V 排针    | 电源         |
| SYN6288 GND | GND      | GND 排针   | 地           |
| SYN6288 RXD | TX       | GPIO17    | U1TXD (ESP32发) |
| SYN6288 TXD | RX       | GPIO18    | U1RXD (ESP32收) |
| HC-SR501 VCC| 5V       | 5V 排针    | 电源         |
| HC-SR501 GND| GND      | GND 排针   | 地           |
| HC-SR501 OUT| OUT      | GPIO6     | 中断输入     |

## 项目结构

esp32s3_voice_sensor/
├── platformio.ini          # PlatformIO 配置
├── README.md               # 本文件
└── src/
    ├── main.cpp            # 主程序 (状态机 + 串口指令)
    ├── syn6288.h           # SYN6288 驱动头文件
    ├── syn6288.cpp         # SYN6288 驱动实现
    ├── pir_sensor.h        # HC-SR501 驱动头文件
    ├── pir_sensor.cpp      # HC-SR501 驱动实现
    ├── gb2312_utils.h      # GB2312 编码工具头文件
    └── gb2312_utils.cpp    # GB2312 编码工具实现

## 编译与烧录

### PlatformIO
1. 安装 VS Code + PlatformIO 插件
2. 打开 esp32s3_voice_sensor 文件夹
3. 选择开发板: esp32-s3-devkitc-1
4. 编译: Ctrl+Alt+B
5. 烧录: 连接 Micro-USB，点击 Upload

### Arduino IDE
1. 安装 ESP32 开发板包 (https://espressif-docs.readthedocs.io)
2. 选择开发板: ESP32S3 Dev Module
3. USB CDC On Boot: Disabled
4. 烧录用 Micro-USB 接口

## 关键避坑
1. 供电: 必须 12V DC 或 Type-C PD (>6.65V)，Micro-USB 仅烧录
2. USB CDC On Boot 必须 Disabled
3. GPIO 26-32 严禁使用
4. SYN6288 使用 GB2312 编码，不是 UTF-8

## 串口指令协议 (与 B/A 模块通信)

| 指令       | 说明              |
|-----------|------------------|
| BORROW    | 播报借书成功       |
| RETURN    | 播报还书成功       |
| READ:xxx  | 播报导读内容       |
| MSG:xxx   | 播报社区留言       |
| STOP      | 停止当前播报       |

## 协作接口 (供 B 模块调用)

extern void voice_borrow_ok();           // 播报借书成功
extern void voice_return_ok();           // 播报还书成功
extern void voice_speak_gb(data, len);   // 播报自定义GB2312内容
extern int  voice_get_state();           // 获取当前状态
extern bool voice_is_busy();             // 是否正在播报
