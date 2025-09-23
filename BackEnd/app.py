import os
import datetime
import sqlite3
from flask import Flask, request, Response, jsonify, send_from_directory, render_template

# === 경로 설정 ===
BACKEND_DIR = os.path.dirname(__file__)
FRONTEND_DIR = os.path.join(BACKEND_DIR, "..", "FrontEnd")  # gallery.html 위치
UPLOAD_DIR = os.path.join(BACKEND_DIR, "uploads")
DB_PATH = os.path.join(BACKEND_DIR, "cam_server.db")

# Flask 앱 생성
app = Flask(__name__, template_folder=FRONTEND_DIR)

# === 전역 flag (사진 촬영 요청용) ===
trigger_flag = {"pending": False}

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
    """ESP32-CAM GET 신호"""
    device_id = request.args.get("id", "ESP32CAM-UNKNOWN")
    insert_device_log(device_id, "Online")

    if trigger_flag["pending"]:
        trigger_flag["pending"] = False
        return Response("201", mimetype="text/plain")
    else:
        return Response("200", mimetype="text/plain")


@app.post("/upload")
def upload():
    """ESP32-CAM POST 신호 (이미지 업로드)"""
    ct = request.content_type or ""
    timestamp = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
    saved_path = None

    try:
        if ct.startswith("text/plain"):
            # 텍스트 업로드 (테스트 용도)
            body = request.get_data(as_text=True)
            saved_path = os.path.join(UPLOAD_DIR, f"{timestamp}.txt")
            with open(saved_path, "w", encoding="utf-8") as f:
                f.write(body)

        elif "multipart/form-data" in ct:
            # 브라우저 업로드용
            if "file" not in request.files:
                return jsonify({"error": "form field 'file' not found"}), 400
            file_storage = request.files["file"]
            ext = os.path.splitext(file_storage.filename or "")[1] or ".bin"
            saved_path = os.path.join(UPLOAD_DIR, f"{timestamp}{ext}")
            file_storage.save(saved_path)

        elif ct.startswith("image/jpeg"):
            # ESP32-CAM JPEG 스트림
            data = request.get_data()
            saved_path = os.path.join(UPLOAD_DIR, f"{timestamp}.jpg")
            with open(saved_path, "wb") as f:
                f.write(data)

        elif ct.startswith("application/octet-stream"):
            # 일반 바이너리 업로드
            data = request.get_data()
            saved_path = os.path.join(UPLOAD_DIR, f"{timestamp}.bin")
            with open(saved_path, "wb") as f:
                f.write(data)

        else:
            # 기타 모든 경우
            data = request.get_data()
            saved_path = os.path.join(UPLOAD_DIR, f"{timestamp}.dat")
            with open(saved_path, "wb") as f:
                f.write(data)

        insert_upload_log(saved_path)
        return jsonify({"status": "ok", "saved": os.path.basename(saved_path)}), 200

    except Exception as e:
        return jsonify({"status": "fail", "error": str(e)}), 500


@app.get("/trigger")
def trigger():
    """프론트에서 '사진 찍기' 버튼 누르면 호출"""
    trigger_flag["pending"] = True
    return jsonify({"status": "ok", "message": "다음 GET 시 CAM에 201 반환 예정"})


@app.get("/gallery")
def gallery():
    """업로드된 사진들을 웹 페이지로 보여줌"""
    conn = sqlite3.connect(DB_PATH)
    cur = conn.cursor()
    cur.execute("SELECT file_path, timestamp FROM uploads ORDER BY id DESC")
    rows = cur.fetchall()
    conn.close()
    return render_template("gallery.html", rows=rows)


@app.get("/uploads/<path:filename>")
def uploaded_file(filename):
    """업로드된 파일을 서빙"""
    return send_from_directory(UPLOAD_DIR, filename)


if __name__ == "__main__":
    init_db()
    app.run(host="0.0.0.0", port=15020, debug=False)
