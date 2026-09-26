# ============================================================
# 淘晶驰串口屏通信模块
# 通信协议: 文本指令 + 0xFF 0xFF 0xFF 结尾
# 页面:
#   page 0 - booklist 馆藏列表
#   page 1 - detail 详情列表 (书名+作者+状态, 每页6本)
#   page 2 - comment 评论
#   page 3 - p_qrcomment
#   page 4 - p_grsearch
#
# 端口配置集中在 config.py: SCREEN_PORT (默认 COM6)
# ============================================================

import serial

try:
    import config
except ImportError:
    config = None

class ScreenWorker:
    def __init__(self, port=None, baud=None):
        self.port = port or (config.SCREEN_PORT if config else "COM6")
        self.baud = baud or (config.SCREEN_BAUD if config else 115200)
        self.ser = None

    def start(self):
        try:
            self.ser = serial.Serial(self.port, self.baud, timeout=0.5)
            print(f"[屏幕] 已连接 {self.port} @ {self.baud}")
            return True
        except Exception as e:
            print(f"[屏幕] 连接失败 ({self.port}): {e}")
            self.ser = None
            return False

    def _send(self, cmd):
        if self.ser and self.ser.is_open:
            self.ser.write(cmd.encode("utf-8"))
            self.ser.write(b"\xff\xff\xff")
            print(f"[屏幕] -> {cmd}")
        else:
            print(f"[屏幕] 未连接，跳过: {cmd}")

    # ========== 页面切换 ==========
    def show_booklist(self):
        self._send("page 0")

    def show_detail(self):
        self._send("page 1")

    def show_comment(self):
        self._send("page 2")

    # ========== Page 0: 馆藏列表 (t0~t8 书名, b0~b8 按钮) ==========
    def set_booklist(self, books):
        for i, book in enumerate(books[:9]):
            status_label = "在架" if book["status"] == "in" else "借出"
            text = f"{i+1}.{book['title']} [{status_label}]"
            self._send(f't{i}.txt="{text}"')
            uid_clean = book["uid"].replace(" ", "")
            self._send(f'b{i}.val={uid_clean}')
        for i in range(len(books[:9]), 9):
            self._send(f't{i}.txt=""')

    # ========== Page 1: 详情列表 (6行: 书名/作者/状态) ==========
    # 书名: b5, b7, b8, b9, b10, b11
    # 作者: t9, t10, t11, t12, t13, t14
    # 状态: p0, p1, p2, p3, p4, p5
    TITLE_BTNS  = ["b5", "b7", "b8", "b9", "b10", "b11"]
    AUTHOR_TXTS = ["t9", "t10", "t11", "t12", "t13", "t14"]
    STATUS_PICS = ["p0", "p1", "p2", "p3", "p4", "p5"]

    def set_detail_list(self, books, page=0):
        """在详情页显示书籍列表 (每页6本, 支持翻页)"""
        per_page = 6
        start = page * per_page
        chunk = books[start:start + per_page]

        for i in range(per_page):
            if i < len(chunk):
                book = chunk[i]
                status_text = "在架" if book["status"] == "in" else "借出"
                self._send(f'{self.TITLE_BTNS[i]}.txt="{book["title"]}"')
                self._send(f'{self.AUTHOR_TXTS[i]}.txt="{book["author"]}"')
                pic_id = 0 if book["status"] == "in" else 1
                self._send(f'{self.STATUS_PICS[i]}.pic={pic_id}')
            else:
                self._send(f'{self.TITLE_BTNS[i]}.txt=""')
                self._send(f'{self.AUTHOR_TXTS[i]}.txt=""')
                self._send(f'{self.STATUS_PICS[i]}.sta=0')

    # ========== Page 2: 评论/感悟 ==========
    def set_comments(self, thoughts):
        """在评论页显示读者感悟 (最多6条)"""
        for i in range(6):
            ti = f"t{i+9}"
            if i < len(thoughts):
                t = thoughts[i]
                text = f"{t['book']}: {t['text']}"
            else:
                text = ""
            self._send(f'{ti}.txt="{text}"')

    def stop(self):
        if self.ser:
            self.ser.close()