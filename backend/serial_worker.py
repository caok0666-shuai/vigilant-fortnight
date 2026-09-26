# ============================================================
# 串口通信模块 (监听 ESP32-S3)
# ============================================================
# ESP32 通过 USB 串口发送: BORROW:A0 13 1E E5 / RETURN:A0 13 1E E5
#
# 端口配置集中在 config.py:
#   ESP32_PORT = "COM7"    ← 盒子右侧, 靠近串口屏的 USB 口
#   设为 "auto" 则自动检测(会自动跳过串口屏占用的端口)

import serial
import serial.tools.list_ports
import threading
import time

try:
    import config
except ImportError:
    config = None

def find_esp32_port(exclude=()):
    """自动查找 ESP32 所在的 COM 口; exclude 中的端口会被跳过"""
    ports = list(serial.tools.list_ports.comports())
    exclude = {p.upper() for p in exclude if p}

    # 第一轮: 按描述匹配常见 USB 串口芯片
    for p in ports:
        if p.device.upper() in exclude:
            continue
        desc = (p.description or "")
        if any(k in desc for k in
               ("USB", "Serial", "CP210", "CH340", "CH343", "Silicon Labs")):
            return p.device

    # 第二轮: 任意可用端口(排除串口屏)
    for p in ports:
        if p.device.upper() not in exclude:
            return p.device
    return None

class SerialWorker:
    """串口监听线程，接收 ESP32 的 BORROW/RETURN 指令"""

    def __init__(self, on_card, port=None, baud=None):
        self.on_card = on_card      # 回调: on_card(uid)
        self.port = port            # None 表示按 config/自动检测
        self.baud = baud
        self.ser = None
        self.running = False
        self.thread = None

    def _resolve_port(self):
        """确定要打开的端口"""
        if self.port:
            return self.port
        if config:
            if config.ESP32_PORT and config.ESP32_PORT.lower() != "auto":
                return config.ESP32_PORT
        # 自动检测时跳过串口屏端口, 防止两块设备抢同一个口
        exclude = []
        if config and config.SCREEN_PORT:
            exclude.append(config.SCREEN_PORT)
        return find_esp32_port(exclude=exclude)

    def start(self, port=None, baud=None):
        if port:
            self.port = port
        if baud:
            self.baud = baud
        port = self._resolve_port()
        baud = self.baud or (config.ESP32_BAUD if config else 115200)

        if port is None:
            print("[串口] 未找到 ESP32 设备，串口功能禁用")
            print("[串口] 可用端口:", [p.device for p in serial.tools.list_ports.comports()])
            return False

        try:
            self.ser = serial.Serial(port, baud, timeout=1)
            print(f"[串口] 已连接 {port} @ {baud}")
            self.running = True
            self.thread = threading.Thread(target=self._listen, daemon=True)
            self.thread.start()
            return True
        except Exception as e:
            print(f"[串口] 连接失败 ({port}): {e}")
            self.ser = None
            return False

    def _listen(self):
        """后台线程: 持续读取 ESP32 发来的数据"""
        buf = b""
        while self.running:
            try:
                if self.ser and self.ser.is_open and self.ser.in_waiting > 0:
                    buf += self.ser.read(self.ser.in_waiting)
                    while b"\n" in buf:
                        line, buf = buf.split(b"\n", 1)
                        self._process(line.decode("utf-8", errors="replace").strip())
                else:
                    time.sleep(0.05)
            except Exception as e:
                print(f"[串口] 读取错误: {e}")
                time.sleep(1)

    def _process(self, line):
        if not line:
            return
        print(f"[ESP32] {line}")

        if line.startswith("CARD:"):
            uid = line[5:].strip()
            print(f"  → 刷卡: UID={uid}")
            self.on_card(uid)

        elif line.startswith("BORROW:"):
            uid = line[7:].strip()
            print(f"  → 借书请求: UID={uid}")
            self.on_card(uid)

        elif line.startswith("RETURN:"):
            uid = line[7:].strip()
            print(f"  → 还书请求: UID={uid}")
            self.on_card(uid)

    def send(self, text):
        """向 ESP32 发送指令"""
        if self.ser and self.ser.is_open:
            self.ser.write((text + "\n").encode())
            self.ser.flush()
            print(f"[串口] → ESP32: {text}")

    def stop(self):
        self.running = False
        if self.ser:
            self.ser.close()