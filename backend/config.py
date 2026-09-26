# ============================================================
# 社区智能书架 - 全局配置文件
# ============================================================
# 所有串口设置集中在这里。换电脑 / 换 USB 口时只改这个文件，
# 不需要动 app.py、serial_worker.py、screen_worker.py。

import os

# ---------------- ESP32-S3 串口 ----------------
# 物理位置: 盒子右侧, 靠近串口屏的那个 USB 口
# 留空或写 "auto" 则自动检测(会自动跳过串口屏占用的口)
ESP32_PORT = os.environ.get("ESP32_PORT", "COM7")
ESP32_BAUD = int(os.environ.get("ESP32_BAUD", "115200"))

# ---------------- 淘晶驰串口屏 ----------------
# 物理位置: 盒子右侧 USB 转 TTL
SCREEN_PORT = os.environ.get("SCREEN_PORT", "COM6")
SCREEN_BAUD = int(os.environ.get("SCREEN_BAUD", "115200"))