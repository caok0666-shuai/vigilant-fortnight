import sqlite3
import os
import csv
from datetime import datetime

DB_PATH = os.path.join(os.path.dirname(__file__), 'library.db')

def import_books_from_csv():
    """从 books.csv 导入/更新图书（首次建库或手动导入时使用）"""
    csv_path = os.path.join(os.path.dirname(__file__), 'books.csv')
    if not os.path.exists(csv_path):
        print("[DB] books.csv not found, please create it with Excel")
        return 0

    conn = sqlite3.connect(DB_PATH)
    c = conn.cursor()
    import_count = 0
    with open(csv_path, "r", encoding="utf-8-sig") as f:
        reader = csv.DictReader(f)
        for row in reader:
            uid = row.get("uid", "").strip()
            title = row.get("title", "").strip()
            author = row.get("author", "").strip()
            if not uid or not title:
                continue
            c.execute("SELECT COUNT(*) FROM books WHERE uid=?", (uid,))
            if c.fetchone()[0] > 0:
                c.execute("UPDATE books SET title=?, author=? WHERE uid=?", (title, author, uid))
            else:
                c.execute("INSERT INTO books VALUES (?,?,?,?,?,?)",
                          (uid, title, author, "in", "", ""))
            import_count += 1
    conn.commit()
    conn.close()
    print(f"[DB] from books.csv imported/updated {import_count} books")
    return import_count


def init_db():
    """创建数据库表；仅首次创建数据库时从 books.csv 导入种子数据。

    这样社区工作人员在网页管理中修改或删除图书后，重启服务不会被
    books.csv 重新覆盖。需要重新导入 CSV 时，调用 /api/books/import。
    """
    first_create = not os.path.exists(DB_PATH)

    conn = sqlite3.connect(DB_PATH)
    c = conn.cursor()
    c.execute("""
        CREATE TABLE IF NOT EXISTS books (
            uid TEXT PRIMARY KEY,
            title TEXT NOT NULL,
            author TEXT DEFAULT '',
            status TEXT DEFAULT 'in',
            borrow_time TEXT DEFAULT '',
            return_time TEXT DEFAULT ''
        )
    """)
    c.execute("""
        CREATE TABLE IF NOT EXISTS records (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            uid TEXT NOT NULL,
            action TEXT NOT NULL,
            time TEXT NOT NULL
        )
    """)
    c.execute("""
        CREATE TABLE IF NOT EXISTS thoughts (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            book TEXT NOT NULL,
            text TEXT NOT NULL,
            time TEXT NOT NULL
        )
    """)
    conn.commit()
    conn.close()

    if first_create:
        import_books_from_csv()
    else:
        print("[DB] existing database found, skip books.csv import")
    print("[DB] database init done")

def get_all_books():
    conn = sqlite3.connect(DB_PATH)
    c = conn.cursor()
    c.execute("SELECT uid, title, author, status, borrow_time FROM books ORDER BY title")
    rows = [{"uid": r[0], "title": r[1], "author": r[2], "status": r[3], "borrow_time": r[4]} for r in c.fetchall()]
    conn.close()
    return rows

def get_book_by_uid(uid):
    conn = sqlite3.connect(DB_PATH)
    c = conn.cursor()
    c.execute("SELECT uid, title, author, status FROM books WHERE uid=?", (uid,))
    r = c.fetchone()
    conn.close()
    return {"uid": r[0], "title": r[1], "author": r[2], "status": r[3]} if r else None

def update_book_status(uid, new_status):
    now = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    conn = sqlite3.connect(DB_PATH)
    c = conn.cursor()
    if new_status == "out":
        c.execute("UPDATE books SET status='out', borrow_time=? WHERE uid=?", (now, uid))
    else:
        c.execute("UPDATE books SET status='in', borrow_time='' WHERE uid=?", (uid,))
    conn.commit()
    conn.close()

def add_record(uid, action):
    now = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    conn = sqlite3.connect(DB_PATH)
    c = conn.cursor()
    c.execute("INSERT INTO records (uid, action, time) VALUES (?,?,?)", (uid, action, now))
    conn.commit()
    conn.close()

def get_thoughts():
    conn = sqlite3.connect(DB_PATH)
    c = conn.cursor()
    c.execute("SELECT id, book, text, time FROM thoughts ORDER BY id DESC LIMIT 50")
    rows = [{"id": r[0], "book": r[1], "text": r[2], "time": r[3]} for r in c.fetchall()]
    conn.close()
    return rows

def add_thought(book, text):
    now = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    conn = sqlite3.connect(DB_PATH)
    c = conn.cursor()
    c.execute("INSERT INTO thoughts (book, text, time) VALUES (?,?,?)", (book, text, now))
    conn.commit()
    conn.close()
def delete_thought(tid):
    conn = sqlite3.connect(DB_PATH)
    c = conn.cursor()
    c.execute("DELETE FROM thoughts WHERE id=?", (tid,))
    conn.commit()
    conn.close()

# ============================================================
# 图书管理 (供 Web 管理页面调用, 替代手工编辑 books.csv)
# ============================================================

def add_book(uid, title, author="", status="in"):
    """新增一本书; 若 UID 已存在则返回 False"""
    conn = sqlite3.connect(DB_PATH)
    c = conn.cursor()
    c.execute("SELECT COUNT(*) FROM books WHERE uid=?", (uid,))
    if c.fetchone()[0] > 0:
        conn.close()
        return False
    c.execute("INSERT INTO books VALUES (?,?,?,?,?,?)",
              (uid, title, author, status, "", ""))
    conn.commit()
    conn.close()
    return True

def update_book(uid, title=None, author=None, status=None):
    """按 UID 修改书名/作者/状态"""
    fields, params = [], []
    if title is not None:
        fields.append("title=?"); params.append(title)
    if author is not None:
        fields.append("author=?"); params.append(author)
    if status is not None:
        fields.append("status=?"); params.append(status)
    if not fields:
        return False
    params.append(uid)
    conn = sqlite3.connect(DB_PATH)
    c = conn.cursor()
    c.execute(f"UPDATE books SET {', '.join(fields)} WHERE uid=?", params)
    changed = c.rowcount > 0
    conn.commit()
    conn.close()
    return changed

def delete_book(uid):
    """删除一本书及其借还记录"""
    conn = sqlite3.connect(DB_PATH)
    c = conn.cursor()
    c.execute("DELETE FROM books WHERE uid=?", (uid,))
    book_deleted = c.rowcount > 0      # 必须在这里取, 后面会被覆盖
    c.execute("DELETE FROM records WHERE uid=?", (uid,))
    conn.commit()
    conn.close()
    return book_deleted

def search_books(keyword):
    """按书名/作者/UID 模糊搜索"""
    kw = f"%{keyword}%"
    conn = sqlite3.connect(DB_PATH)
    c = conn.cursor()
    c.execute("""SELECT uid, title, author, status, borrow_time FROM books
                 WHERE title LIKE ? OR author LIKE ? OR uid LIKE ?
                 ORDER BY title""", (kw, kw, kw))
    rows = [{"uid": r[0], "title": r[1], "author": r[2],
             "status": r[3], "borrow_time": r[4]} for r in c.fetchall()]
    conn.close()
    return rows

def export_books_to_csv():
    """把当前数据库导出回 books.csv (备份用)"""
    csv_path = os.path.join(os.path.dirname(__file__), 'books.csv')
    books = get_all_books()
    with open(csv_path, "w", encoding="utf-8-sig", newline="") as f:
        writer = csv.writer(f)
        writer.writerow(["uid", "title", "author"])
        for b in books:
            writer.writerow([b["uid"], b["title"], b["author"]])
    return len(books)
