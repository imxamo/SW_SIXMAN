import os
import datetime
import sqlite3
from flask import Flask, request, Response, jsonify

app = Flask(__name__)

# === 설정 ===
UPLOAD_DIR = os.path.join(os.path.dirname(__file__), "uploads")
os.makedirs(UPLOAD_DIR, exist_ok=True)

DB_PATH = os.path.join(os.path.dirname(__file__), "cam_server.db")


# === DB 초기화 ===
def init_db():
    conn = sqlite3.connect(DB_PATH)
    cur = conn.cursor()
    # 장치 로그 테이블
    cur.execute("""
        CREATE TABLE IF NOT EXISTS device_logs (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            device_id TEXT,
            timestamp TEXT,
            status TEXT
        )
    """)
    # 업로드 로그 테이블
    cur.execute("""
        CREATE TABLE IF NOT EXISTS uploads (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            file_path TEXT,
            timestamp TEXT
        )
    """)
    conn.commit()
    conn.close()


def insert_device_log(device_id, status):
    ts = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    conn = sqlite3.connect(DB_PATH)
    cur = conn.cursor()
    cur.execute("INSERT INTO device_logs (device_id, timestamp, status) VALUES (?, ?, ?)",
                (device_id, ts, status))
    conn.commit()
    conn.close()


def insert_upload_log(file_path):
    ts = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    conn = sqlite3.connect(DB_PATH)
    cur = conn.cursor()
    cur.execute("INSERT INTO uploads (file_path, timestamp) VALUES (?, ?)",
                (file_path, ts))
    conn.commit()
    conn.close()


@app.get("/get")
def get_poll():
    """
    ESP32-CAM GET 신호
    - 무조건 "200" 반환
    - DB에 {device_id, timestamp, "Offline"} 저장
    """
    device_id = request.args.get("id", "ESP32CAM-UNKNOWN")
    insert_device_log(device_id, "Offline")
    return Response("200", mimetype="text/plain")


@app.post("/upload")
def upload():
    """
    ESP32-CAM POST 신호 (이미지 업로드)
    - 파일 저장 후 DB 기록
    """
    ct = request.content_type or ""
    timestamp = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
    saved_path = None

    try:
        if ct.startswith("text/plain"):
            body = request.get_data(as_text=True)
            saved_path = os.path.join(UPLOAD_DIR, f"{timestamp}.txt")
            with open(saved_path, "w", encoding="utf-8") as f:
                f.write(body)

        elif "multipart/form-data" in ct:
            if "file" not in request.files:
                return jsonify({"error": "form field 'file' not found"}), 400
            file_storage = request.files["file"]
            ext = os.path.splitext(file_storage.filename or "")[1] or ".bin"
            saved_path = os.path.join(UPLOAD_DIR, f"{timestamp}{ext}")
            file_storage.save(saved_path)

        elif ct.startswith("application/octet-stream"):
            data = request.get_data()
            saved_path = os.path.join(UPLOAD_DIR, f"{timestamp}.bin")
            with open(saved_path, "wb") as f:
                f.write(data)

        else:
            data = request.get_data()
            saved_path = os.path.join(UPLOAD_DIR, f"{timestamp}.dat")
            with open(saved_path, "wb") as f:
                f.write(data)

        # DB 저장
        insert_upload_log(saved_path)

        return jsonify({"status": "ok", "saved": os.path.basename(saved_path)}), 200

    except Exception as e:
        return jsonify({"status": "fail", "error": str(e)}), 500


if __name__ == "__main__":
    init_db()
    app.run(host="0.0.0.0", port=8000, debug=False)
