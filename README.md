# 社区智能书架 —— 作品源文件

## 项目简介

社区智能书架是一套面向社区图书馆、班级图书角等小型场景的无人值守智能图书管理系统。
基于 ESP32-S3 主控，集成 MFRC522 RFID 读卡、SYN6288 语音合成、HC-SR501 人体感应三大模块，
通过 USB 串口连接电脑端 Flask 后端（SQLite 数据库），配合串口屏与 Web 网页实现双端实时同步。

"靠近即感、刷卡即知、数据同步"——让每一本书都被看见，让每一次借阅都被记录。

---

## 项目结构

```
益达口香糖/
├── README.md                            # 本文件
├── firmware/                            # 硬件端固件（ESP32-S3）
│   ├── arduino/
│   │   └── arduino_voice_sensor.ino     # Arduino IDE 版本（完整三模块整合）
│   └── platformio/
│       ├── platformio.ini               # PlatformIO 配置
│       ├── README.md                    # PlatformIO 版说明（含接线图）
│       └── src/
│           ├── main.cpp                 # 主程序（状态机 + 串口指令）
│           ├── syn6288.h / syn6288.cpp  # SYN6288 语音合成驱动
│           ├── pir_sensor.h / pir_sensor.cpp  # HC-SR501 人体感应驱动
│           └── gb2312_utils.h / gb2312_utils.cpp  # GB2312 编码转换工具
├── backend/                             # 后端服务器（Python Flask）
│   ├── app.py                           # Flask 主程序（REST API）
│   ├── database.py                      # SQLite 数据库操作
│   ├── serial_worker.py                 # ESP32 USB 串口通信线程
│   ├── screen_worker.py                 # 淘晶驰串口屏通信模块
│   ├── requirements.txt                 # Python 依赖
│   ├── config.py                        # 串口端口配置（换 USB 口只改这里）
│   ├── start.bat                        # Windows 启动脚本
│   ├── books.csv                        # 首次建库的初始图书数据（可选备份格式）
│   └── templates/
│       ├── index.html                   # 读者终端主页（馆藏查询+读者感悟）
│       ├── books_admin.html             # ★ 图书管理页面（网页增删改图书）
│       └── admin.html                   # 感悟管理后台
├── web_frontend/
│   └── 智能书架网页(1).html            # 独立 Web 前端（备用/参考）
├── screen_design/                       # 串口屏设计文件
│   ├── project_4(3).HMI                # 最新版 USART HMI 工程文件
│   └── *.png                           # 各页面截图（共 5 页）
```

---

---

## 团队协作与本地运行（A / B / C）

项目代码托管在 GitHub：

**https://github.com/PatrickNandius/BookManagementSystem**

### 每个人第一次拿到项目

1. 安装 **Git**，并确认电脑上已有 **Python 3.9+**。
2. 克隆仓库：

```bash
git clone https://github.com/PatrickNandius/BookManagementSystem.git
cd BookManagementSystem/backend
```

3. 安装 Python 依赖：

```bat
pip install -r requirements.txt
```

4. 修改自己电脑上的串口号（因电脑而异）：

```python
# backend/config.py
ESP32_PORT  = "COM7"
SCREEN_PORT = "COM6"
```

5. 启动后端：

```bat
python app.py
```

或直接双击 `backend/start.bat`。

> 注意：A/B 只需要运行网页和（如果自己接了硬件）对应串口。没有接 ESP32/串口屏时，后端仍能启动，只是串口相关日志会提示连接失败，网页功能不受影响。
> `library.db` 已在 `.gitignore` 中排除，A/B 首次运行会根据自己的 `books.csv` 自动创建本地数据库。

### 修改后上传到 GitHub

在 VS Code 中打开项目文件夹，使用左侧「源代码管理」：

1. 先点「拉取」同步 A/B 的最新代码。
2. 修改并保存文件。
3. 在「源代码管理」里填写提交信息，例如 `优化图书管理页面`。
4. 点「提交」，再点「推送」。

命令行等价操作：

```bash
git pull
git add .
git commit -m "优化图书管理页面"
git push
```

首次推送如果提示没有权限，需要在 GitHub 登录或配置 Personal Access Token，不建议把密码写进代码或 README。

### 冲突处理原则

- 每次开始工作前先 `git pull`。
- 不要多人同时改同一个文件；如需改同一文件，先沟通分工。
- 提交前确认没有把 `library.db`、`.venv`、`__pycache__`、串口日志或密钥提交进去。
- `books.csv` 只作为初始数据和备份；社区工作人员日常添加、修改、删除图书请使用网页 `/books`。

## 硬件接线

| 外设 | 引脚 | ESP32-S3 GPIO |
|------|------|---------------|
| MFRC522 SDA | SPI片选 | GPIO9 |
| MFRC522 SCK | SPI时钟 | GPIO10 |
| MFRC522 MOSI | SPI主发从收 | GPIO11 |
| MFRC522 MISO | SPI从发主收 | GPIO12 |
| MFRC522 RST | 复位 | GPIO13 |
| SYN6288 RXD | UART1发送 | GPIO17 |
| SYN6288 TXD | UART1接收 | GPIO18 |
| HC-SR501 OUT | 数字输入 | GPIO6 |

SYN6288 VCC → 5V，HC-SR501 VCC → 5V，MFRC522 VCC → 3.3V

---


## 后端启动

```bash
cd backend
pip install -r requirements.txt
python app.py
```

| 页面 | 地址 | 用途 |
|------|------|------|
| 读者终端 | http://localhost:5000 | 馆藏查询、读者感悟提交（面向读者） |
| 图书管理 | http://localhost:5000/books | **增删改图书**（面向社区工作人员） |
| 感悟管理 | http://localhost:5000/admin | 审核/删除读者感悟 |
| 端口排查 | http://localhost:5000/api/ports | 查看当前可用串口 |

首次运行会自动创建 `library.db` 并导入 `books.csv`；之后以数据库为准，重启不会覆盖网页中的修改。未连接硬件也不影响网页服务。

### 新增：图书管理页面（不再需要手改 CSV）

社区工作人员打开 http://localhost:5000/books 即可：

- **添加图书**：填写 RFID UID、书名、作者，点「添加」
- **修改图书**：点每行「编辑」，改完保存
- **删除图书**：点「删除」（该书借还记录一并清除）
- **切换状态**：一键在「在架 / 借出」间切换
- **搜索**：按书名、作者或 UID 模糊查找
- **导出备份**：把当前数据导出回 `books.csv`

任何改动都会**立即写入数据库并同步到网页与串口屏**，无需重启。

对应的后端接口：

| 方法 | 路径 | 说明 |
|------|------|------|
| GET | `/api/books` | 获取全部图书（支持 `?q=关键词`） |
| POST | `/api/books` | 新增图书 `{uid, title, author}` |
| PUT | `/api/books/<uid>` | 修改图书 `{title?, author?, status?}` |
| DELETE | `/api/books/<uid>` | 删除图书 |
| POST | `/api/books/import` | 重新从 `books.csv` 导入 |
| POST | `/api/books/export` | 导出到 `books.csv` 备份 |

### 串口端口配置（换 USB 口只改一个文件）

所有串口设置集中在 `backend/config.py`：

```python
ESP32_PORT  = "COM7"    # ESP32-S3（盒子右侧、靠近串口屏的 USB 口）
SCREEN_PORT = "COM6"    # 淘晶驰串口屏（USB 转 TTL）
```

也可以不改文件，用环境变量临时覆盖：

```bat
set ESP32_PORT=COM7
set SCREEN_PORT=COM6
python app.py
```

`ESP32_PORT` 写成 `"auto"` 时会自动检测，并**自动跳过串口屏占用的端口**，避免两块设备抢同一个 COM 口。

### 读者感悟上屏（B 的需求）

后端在以下时机会把感悟推送到串口屏的评论页（`page 2`）：

- 后端启动时
- 读者在网页提交新感悟后
- 管理员删除感悟后

---

## 开源项目使用说明

本作品在开发过程中参考或使用了以下开源项目，特此说明：

### 1. ESP32 Arduino Core
- **链接**: https://github.com/espressif/arduino-esp32
- **用途**: 作为 ESP32-S3 的底层开发框架，提供 Arduino 硬件抽象层（GPIO、UART、SPI 等接口）
- **参考/使用内容**: 直接依赖运行，代码中通过 #include <Arduino.h> 引用

### 2. MFRC522 Arduino Library (miguelbalboa)
- **链接**: https://github.com/miguelbalboa/MFRC522
- **用途**: MFRC522 RFID 读写模块的 Arduino 驱动库
- **参考/使用内容**: 直接依赖运行，代码中通过 #include <MFRC522.h> 引用，实现 ISO 14443A 卡片识别和 UID 读取

### 3. Flask (Pallets Project)
- **链接**: https://github.com/pallets/flask
- **用途**: Python Web 后端框架，提供 REST API 与 Web 页面服务
- **参考/使用内容**: 直接依赖运行，构建 /api/books、/api/thoughts、/api/records 等接口

### 4. pyserial
- **链接**: https://github.com/pyserial/pyserial
- **用途**: Python 串口通信库，实现与 ESP32 及串口屏的数据收发
- **参考/使用内容**: 直接依赖运行，用于 serial_worker.py 和 screen_worker.py

### 5. Tailwind CSS
- **链接**: https://tailwindcss.com
- **用途**: Web 前端 CSS 框架
- **参考/使用内容**: 通过 CDN (https://cdn.tailwindcss.com) 引用，用于 index.html 和 dmin.html 的页面样式

### 6. FontAwesome
- **链接**: https://fontawesome.com
- **用途**: Web 图标库
- **参考/使用内容**: 通过 CDN (https://cdnjs.cloudflare.com/ajax/libs/font-awesome/6.4.0/css/all.min.css) 引用，用于 Web 前端图标

### 7. Library-system（参考架构设计）
- **链接**: https://github.com/XiangSirui/Library-system
- **用途**: 参考其图书管理系统的业务流程设计思路
- **参考/使用内容**: 参考了其借还书业务逻辑流程，但本作品的全部代码（ESP32 固件、后端、前端、串口屏）均为独立实现，未直接复制其代码

### 8. Flask-CORS（跨域访问支持）

- **链接**: https://github.com/corydolphin/flask-cors
- **用途**: 允许 A 的远程网页跨域调用本后端 API
- **参考/使用内容**: 以 Python 依赖方式引入，对 API 开启 CORS 响应头（可选，见 requirements.txt）

### 9. Waitress（生产环境 WSGI 服务器）

- **链接**: https://github.com/Pylons/waitress
- **用途**: 替代 Flask 自带的开发服务器，用于对外提供稳定服务
- **参考/使用内容**: 以 Python 依赖方式引入，在 start.bat / 部署脚本中使用（可选）

### 10. SQLite

- **链接**: https://www.sqlite.org
- **用途**: 轻量级嵌入式数据库
- **参考/使用内容**: Python 标准库 sqlite3 直接使用，存储图书、借还记录与读者感悟

---

---

## 硬件模块清单

| 模块 | 型号 | 数量 |
|------|------|------|
| 主控 | ESP32-S3 Dev Module | 1 |
| RFID读写器 | MFRC-522 RC522 | 1 |
| 语音合成 | SYN6288（含喇叭） | 1 |
| 人体感应 | HC-SR501 | 1 |
| 串口屏 | 淘晶驰 TJC4832T135_011C_A01 3.5寸 | 1 |
| NFC标签 | Mifare 1K S50 不干胶标签 | 若干 |
