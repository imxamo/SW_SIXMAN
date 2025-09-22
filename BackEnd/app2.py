import os
import datetime
import sqlite3
from flask import Flask, request, Response, jsonify, render_template, send_from_directory

app = Flask(__name__)

# === 설정 ===
BASE_DIR = os.path.dirname(__file__)
UPLOAD_DIR = os.path.join(BASE_DIR, "uploads")
os.makedirs(UPLOAD_DIR, exist_ok=True)

DB_PATH = os.path.join(BASE_DIR, "cam_server.db")


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


# === 새로 추가되는 부분 ===
@app.get("/gallery")
def gallery():
    """업로드된 사진들을 웹 페이지로 보여줌"""
    conn = sqlite3.connect(DB_PATH)
    cur = conn.cursor()
    cur.execute("SELECT file_path, timestamp FROM uploads ORDER BY id DESC")
    rows = cur.fetchall()
    conn.close()

    # HTML 직접 생성 (템플릿 없이도 가능)
    html = """
    <!DOCTYPE html>
    <html lang="ko">
    <head>
        <meta charset="UTF-8">
        <title>ESP32-CAM 업로드 갤러리</title>
        <style>
            body { font-family: Arial, sans-serif; margin: 20px; background: #f9f9f9; }
            h1 { color: #333; }
            .item { background: white; border: 1px solid #ddd;
                    padding: 10px; margin-bottom: 20px;
                    box-shadow: 2px 2px 5px rgba(0,0,0,0.1);
                    max-width: 400px; }
            .item img { max-width: 100%; border-radius: 5px; }
            .timestamp { font-size: 0.9em; color: #666; margin-bottom: 8px; }
            a { color: #0066cc; text-decoration: none; }
        </style>
    </head>
    <body>
        <h1>📷 ESP32-CAM 업로드 갤러리</h1>
    """

    for file_path, ts in rows:
        fname = os.path.basename(file_path)
        html += "<div class='item'>"
        html += f"<div class='timestamp'>업로드 시각: {ts}</div>"
        if fname.lower().endswith((".png", ".jpg", ".jpeg", ".gif")):
            html += f"<img src='/uploads/{fname}' alt='uploaded image'>"
        else:
            html += f"<a href='/uploads/{fname}'>파일 다운로드 ({fname})</a>"
        html += "</div>"

    html += "</body></html>"
    return html


@app.get("/uploads/<path:filename>")
def uploaded_file(filename):
    """업로드된 파일을 서빙"""
    return send_from_directory(UPLOAD_DIR, filename)


if __name__ == "__main__":
    init_db()
    app.run(host="0.0.0.0", port=8000, debug=False)
