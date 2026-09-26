# ============================================================
# 社区智能书架 - Flask 主程序
# 运行: python app.py
# 访问: http://localhost:5000         读者终端网页
#       http://localhost:5000/books   图书管理页面 (社区工作人员用)
#       http://localhost:5000/admin   感悟管理后台
# ============================================================

from flask import Flask, jsonify, request, render_template
import database as db
from serial_worker import SerialWorker
from screen_worker import ScreenWorker

try:
    import config
except ImportError:
    config = None

app = Flask(__name__)

# 允许 A 的独立网页跨域调用本后端 API（未安装时不阻止本地运行）
try:
    from flask_cors import CORS
    CORS(app)
    print("[Web] Flask-CORS 已启用，允许跨域访问 API")
except ImportError:
    print("[Web] 未安装 Flask-CORS，网页需与后端同源访问")

# 确保数据库和表存在（首次运行自动创建, 无需手工操作）
db.init_db()

# ------------------------------------------------------------
# 串口工作线程 (端口在 config.py 中配置)
# ------------------------------------------------------------
serial_worker = SerialWorker(
    on_card = lambda uid: handle_card(uid)
)

screen = ScreenWorker()

def refresh_screen():
    """把最新的书籍状态和感悟推送到串口屏"""
    try:
        books = db.get_all_books()
        screen.set_booklist(books)
        screen.set_detail_list(books)
    except Exception as e:
        print(f"[屏幕] 刷新书籍列表失败: {e}")
    try:
        # B 的需求: 读者感悟也要显示到串口屏
        screen.set_comments(db.get_thoughts())
    except Exception as e:
        print(f"[屏幕] 刷新感悟失败: {e}")

def handle_card(uid):
    """处理刷卡事件 - 根据数据库状态判断借/还"""
    book = db.get_book_by_uid(uid)

    if book is None:
        print(f"  [WARN] 未知卡 UID={uid}")
        serial_worker.send("UNKNOWN")
        return

    # 根据数据库状态判断：在架→借出，已借出→归还
    if book["status"] == "in":
        # 书在架 → 借书
        db.update_book_status(uid, "out")
        db.add_record(uid, "borrow")
        print(f"  [OK] 借书成功: 《{book['title']}》")
        serial_worker.send("BORROW_OK")
    else:
        # 书已借出 → 还书
        db.update_book_status(uid, "in")
        db.add_record(uid, "return")
        print(f"  [OK] 还书成功: 《{book['title']}》")
        serial_worker.send("RETURN_OK")

    refresh_screen()

# ------------------------------------------------------------
# Web 页面路由
# ------------------------------------------------------------

@app.route("/")
def index():
    """读者终端主页"""
    return render_template("index.html")

@app.route("/books")
def books_admin():
    """图书管理页面 (社区工作人员用, 替代手工编辑 books.csv)"""
    return render_template("books_admin.html")

@app.route("/admin")
def admin():
    """感悟管理后台"""
    return render_template("admin.html")

# ------------------------------------------------------------
# 图书 API
# ------------------------------------------------------------

@app.route("/api/books")
def api_books():
    """获取所有图书状态; 支持 ?q=关键词 模糊搜索"""
    keyword = request.args.get("q", "").strip()
    if keyword:
        return jsonify(db.search_books(keyword))
    return jsonify(db.get_all_books())

@app.route("/api/books", methods=["POST"])
def api_add_book():
    """新增图书  body: {uid, title, author?}"""
    data = request.get_json(silent=True) or {}
    uid = (data.get("uid") or "").strip().upper()
    title = (data.get("title") or "").strip()
    author = (data.get("author") or "").strip()

    if not uid or not title:
        return jsonify({"error": "UID 和书名不能为空"}), 400
    if not db.add_book(uid, title, author):
        return jsonify({"error": f"UID {uid} 已存在"}), 409

    print(f"[图书] 新增: {uid} 《{title}》")
    refresh_screen()
    return jsonify({"status": "ok", "uid": uid}), 201

@app.route("/api/books/<path:uid>", methods=["PUT"])
def api_update_book(uid):
    """修改图书  body: {title?, author?, status?}"""
    data = request.get_json(silent=True) or {}
    status = data.get("status")
    if status is not None and status not in ("in", "out"):
        return jsonify({"error": "status 只能是 in 或 out"}), 400

    ok = db.update_book(
        uid,
        title=(data.get("title").strip() if data.get("title") is not None else None),
        author=(data.get("author").strip() if data.get("author") is not None else None),
        status=status,
    )
    if not ok:
        return jsonify({"error": "未找到该图书"}), 404

    print(f"[图书] 修改: {uid}")
    refresh_screen()
    return jsonify({"status": "ok"})

@app.route("/api/books/<path:uid>", methods=["DELETE"])
def api_delete_book(uid):
    """删除图书"""
    if not db.delete_book(uid):
        return jsonify({"error": "未找到该图书"}), 404
    print(f"[图书] 删除: {uid}")
    refresh_screen()
    return jsonify({"status": "ok"})

@app.route("/api/books/import", methods=["POST"])
def api_import_books():
    """从 config 指定的 books.csv 重新导入 (兼容旧流程)"""
    try:
        count = db.import_books_from_csv()
        refresh_screen()
        return jsonify({"status": "ok", "count": count})
    except Exception as e:
        return jsonify({"error": str(e)}), 500

@app.route("/api/books/export", methods=["POST"])
def api_export_books():
    """把数据库导出回 books.csv (备份用)"""
    try:
        count = db.export_books_to_csv()
        return jsonify({"status": "ok", "count": count})
    except Exception as e:
        return jsonify({"error": str(e)}), 500

# ------------------------------------------------------------
# 感悟 / 记录 API
# ------------------------------------------------------------

@app.route("/api/thoughts", methods=["GET", "POST"])
def api_thoughts():
    """获取/提交社区留言"""
    if request.method == "POST":
        data = request.get_json()
        book = data.get("book", "")
        text = data.get("text", "")
        if not book or not text:
            return jsonify({"error": "请填写书名和感悟"}), 400
        db.add_thought(book, text)
        msg = f"社区读者说：{text}"
        serial_worker.send(f"MSG:{msg}")
        # B 的需求: 新感悟立刻上屏
        try:
            screen.set_comments(db.get_thoughts())
        except Exception as e:
            print(f"[屏幕] 感悟上屏失败: {e}")
        return jsonify({"status": "ok"})
    else:
        return jsonify(db.get_thoughts())

@app.route("/api/thoughts/<int:tid>", methods=["DELETE"])
def api_delete_thought(tid):
    """删除一条感悟"""
    db.delete_thought(tid)
    try:
        screen.set_comments(db.get_thoughts())
    except Exception:
        pass
    return jsonify({"status": "ok"})

@app.route("/api/records")
def api_records():
    """获取借还记录"""
    import sqlite3
    conn = sqlite3.connect(db.DB_PATH)
    c = conn.cursor()
    c.execute("""
        SELECT r.uid, r.action, r.time, COALESCE(b.title, '未知')
        FROM records r LEFT JOIN books b ON r.uid = b.uid
        ORDER BY r.id DESC LIMIT 50
    """)
    rows = [{"uid": r[0], "action": r[1], "time": r[2], "title": r[3]} for r in c.fetchall()]
    conn.close()
    return jsonify(rows)

@app.route("/api/ports")
def api_ports():
    """列出当前可用的串口, 方便排查端口问题"""
    import serial.tools.list_ports
    ports = [{"device": p.device, "description": p.description}
             for p in serial.tools.list_ports.comports()]
    return jsonify({
        "esp32_configured": config.ESP32_PORT if config else None,
        "screen_configured": config.SCREEN_PORT if config else None,
        "available": ports,
    })

@app.route("/api/manual", methods=["POST"])
def api_manual():
    """手动触发操作（调试用，无需真实串口）"""
    data = request.get_json()
    uid = data.get("uid", "")
    if uid:
        handle_card(uid)
        return jsonify({"status": "ok"})
    return jsonify({"error": "参数错误"}), 400

# ------------------------------------------------------------
# 启动
# ------------------------------------------------------------

if __name__ == "__main__":
    # 数据库已在模块加载时初始化

    # 尝试连接 ESP32 串口（失败不影响 Web 服务启动）
    print("\n[启动] 正在查找 ESP32 串口...")
    if not serial_worker.start():
        print("[启动] ESP32 未连接，可通过 /api/manual 手动测试")
        print("[启动] 或通过网页上的测试按钮模拟刷卡")

    print("[启动] 正在连接串口屏...")
    if screen.start():
        screen.show_booklist()
        refresh_screen()

    print("\n" + "=" * 50)
    print("  社区智能书架后端已启动!")
    print("  读者网页: http://localhost:5000")
    print("  图书管理: http://localhost:5000/books")
    print("  感悟管理: http://localhost:5000/admin")
    print("  API:      http://localhost:5000/api/books")
    print("=" * 50 + "\n")

    app.run(host="0.0.0.0", port=5000, debug=True)