import os
import io
import datetime
import sqlite3
from flask import Flask, request, Response, jsonify, send_from_directory, render_template
from PIL import Image

import torch
import torch.nn as nn
from torchvision import models, transforms

# ===== 경로 설정 =====
BACKEND_DIR = os.path.dirname(__file__)
REACT_DIST = os.path.abspath(os.path.join(BACKEND_DIR, "../FrontEnd/dist"))
FRONTEND_DIR = os.path.join(BACKEND_DIR, "..", "FrontEnd")
UPLOAD_DIR = os.path.join(BACKEND_DIR, "uploads")
DB_PATH = os.path.join(BACKEND_DIR, "cam_server.db")
SENSOR_DB_PATH = os.path.join(BACKEND_DIR, "sensor_server.db")  # ✅ 센서 전용 DB 추가

os.makedirs(UPLOAD_DIR, exist_ok=True)

app = Flask(__name__, 
            static_folder=REACT_DIST, 
            static_url_path="/",
            template_folder=FRONTEND_DIR)

# ===== AI 모델 설정 =====
MODEL_PATH = os.path.expanduser("/home/student_15020/best_lettuce_disease_model.pth")
CLASS_IDS = ['0', '9', '10']
CLASS_NAMES = {'0': '정상', '9': '상추균핵병', '10': '상추노균병'}

preprocess = transforms.Compose([
    transforms.Resize((224, 224)),
    transforms.ToTensor(),
    transforms.Normalize([0.485, 0.456, 0.406], [0.229, 0.224, 0.225]),
])

def build_model(num_classes):
    m = models.efficientnet_b0(weights='IMAGENET1K_V1')
    in_f = m.classifier[1].in_features
    m.classifier = nn.Sequential(nn.Dropout(0.3), nn.Linear(in_f, num_classes))
    return m

device = torch.device("cpu")
model = build_model(len(CLASS_IDS))
state = torch.load(MODEL_PATH, map_location=device)
model.load_state_dict(state)
model.eval().to(device)

# ===== 전역 flag =====
flags = {
    "cam_pending": False,
    "esp32_pending": False
}

# ===== 센서 데이터 저장 (메모리) =====
latest_sensor_data = {
    "temperature": None,
    "humidity": None,
    "soil_moisture": None,
    "water_level": None,
    "timestamp": None
}

# ===== DB 초기화 =====
def init_db():
    conn = sqlite3.connect(DB_PATH)
    cur = conn.cursor()
    cur.execute("""
        CREATE TABLE IF NOT EXISTS device_logs (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            device_id TEXT,
            timestamp TEXT,
            status TEXT
        )
    """)
    cur.execute("""
        CREATE TABLE IF NOT EXISTS uploads (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            file_path TEXT,
            timestamp TEXT
        )
    """)
    conn.commit()
    conn.close()

def init_sensor_db():
    conn = sqlite3.connect(SENSOR_DB_PATH)
    cur = conn.cursor()
    cur.execute("""
        CREATE TABLE IF NOT EXISTS sensor_data (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            temperature REAL,
            humidity REAL,
            soil_moisture INTEGER,
            water_level REAL,
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

def insert_sensor_data(temp, hum, soil, water):
    """센서 데이터를 sensor_server.db에 저장"""
    ts = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    conn = sqlite3.connect(SENSOR_DB_PATH)
    cur = conn.cursor()
    cur.execute("""
        INSERT INTO sensor_data (temperature, humidity, soil_moisture, water_level, timestamp)
        VALUES (?, ?, ?, ?, ?)
    """, (temp, hum, soil, water, ts))
    conn.commit()
    conn.close()

# ===== React SPA 서빙 =====
@app.route("/", defaults={"path": ""})
@app.route("/<path:path>")
def serve_react(path):
    if path.startswith("api/") or path in ["get", "upload", "trigger", "gallery", "uploads", "level"]:
        return jsonify({"error": "Not Found"}), 404
    
    file_path = os.path.join(REACT_DIST, path)
    if os.path.exists(file_path) and os.path.isfile(file_path):
        return send_from_directory(REACT_DIST, path)
    return send_from_directory(REACT_DIST, "index.html")

# ===== API: Health Check =====
@app.get("/api/health")
def health():
    return {"ok": True}

# ===== API: AI 예측 =====
ALLOWED = {"jpg", "jpeg", "png", "bmp", "webp"}

@app.post("/api/predict")
def predict():
    if "file" not in request.files:
        return jsonify({"ok": False, "error": "no file"}), 400
    f = request.files["file"]
    ext_ok = "." in f.filename and f.filename.rsplit(".", 1)[1].lower() in ALLOWED
    if not f.filename or not ext_ok:
        return jsonify({"ok": False, "error": "bad filename/ext"}), 400

    try:
        img = Image.open(io.BytesIO(f.read())).convert("RGB")
    except Exception as e:
        return jsonify({"ok": False, "error": f"bad image: {e}"}), 400

    x = preprocess(img).unsqueeze(0).to(device)
    with torch.no_grad():
        logits = model(x)
        prob = torch.softmax(logits, dim=1)[0]
        idx = int(prob.argmax().item())
        conf = float(prob[idx].item())

    code = CLASS_IDS[idx]
    return jsonify({
        "ok": True,
        "class_idx": idx,
        "disease_code": code,
        "disease_name": CLASS_NAMES[code],
        "confidence": round(conf, 4)
    })

# ===== ESP32: GET 폴링 =====
@app.get("/get")
def get_poll():
    device_id = request.args.get("id", "UNKNOWN")
    insert_device_log(device_id, "Online")

    if device_id.startswith("ESP32CAM"):
        if flags["cam_pending"]:
            flags["cam_pending"] = False
            return Response("201", mimetype="text/plain")
        return Response("200", mimetype="text/plain")

    elif device_id.startswith("ESP32"):
        if flags["esp32_pending"]:
            flags["esp32_pending"] = False
            return Response("201", mimetype="text/plain")
        return Response("200", mimetype="text/plain")

    return Response("400", mimetype="text/plain")  # 알 수 없는 장치

# ===== ESP32 / ESP32-CAM: 업로드 처리 =====
@app.post("/upload")
def upload():
    device_id = request.args.get("id", "UNKNOWN")   # ?id=ESP32CAM-123 또는 ?id=ESP32-123
    ct = request.content_type or ""
    timestamp = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
    saved_path = None

    try:
        # ==============================
        # 1. ESP32-CAM (사진 업로드)
        # ==============================
        if device_id.startswith("ESP32CAM"):
            # raw binary (application/octet-stream) 또는 multipart/form-data 가능
            if "multipart/form-data" in ct:
                if "file" not in request.files:
                    return jsonify({"status": "fail", "error": "form field 'file' not found"}), 400
                file_storage = request.files["file"]
                ext = os.path.splitext(file_storage.filename or "")[1] or ".jpg"
                saved_path = os.path.join(UPLOAD_DIR, f"CAM_{timestamp}{ext}")
                file_storage.save(saved_path)
            else:
                # 기본적으로 raw binary 데이터를 jpg로 저장
                data = request.get_data()
                saved_path = os.path.join(UPLOAD_DIR, f"CAM_{timestamp}.jpg")
                with open(saved_path, "wb") as f:
                    f.write(data)

            insert_upload_log(saved_path)
            print(f"[CAM 업로드] {saved_path}")

        # ==============================
        # 2. ESP32 (센서 데이터 업로드)
        # ==============================
        elif device_id.startswith("ESP32"):
            body = request.get_data(as_text=True)
            saved_path = os.path.join(UPLOAD_DIR, f"ESP32_{timestamp}_sensor.txt")
            with open(saved_path, "w", encoding="utf-8") as f:
                f.write(body)

            # (옵션) 센서 데이터 파싱
            try:
                lines = body.strip().split("\n")
                temp = float(lines[0].split(":")[1].strip().replace("C", "").strip())
                hum = float(lines[1].split(":")[1].strip().replace("%", "").strip())
                soil = int(lines[2].split(":")[1].strip())
                water = float(lines[3].split(":")[1].strip().replace("%", "").strip())

                # 최신 센서값 메모리에 저장
                latest_sensor_data["temperature"] = temp
                latest_sensor_data["humidity"] = hum
                latest_sensor_data["soil_moisture"] = soil
                latest_sensor_data["water_level"] = water
                latest_sensor_data["timestamp"] = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")

                # ✅ 센서 전용 DB에 저장
                insert_sensor_data(temp, hum, soil, water)

                print(f"[ESP32 센서 업로드] 온도:{temp}°C, 습도:{hum}%, 토양:{soil}, 수위:{water}%")
            except Exception as e:
                print(f"[ESP32 센서 파싱 오류] {e}")

        # ==============================
        # 3. 알 수 없는 장치
        # ==============================
        else:
            return jsonify({"status": "fail", "error": "unknown device"}), 400

        return jsonify({"status": "ok", "saved": os.path.basename(saved_path) if saved_path else "sensor_data"}), 200

    except Exception as e:
        return jsonify({"status": "fail", "error": str(e)}), 500


# ===== API: 최신 센서 데이터 조회 =====
@app.get("/api/sensor")
def api_sensor():
    """프론트엔드에서 실시간 센서 데이터 조회"""
    return jsonify({
        "ok": True,
        "data": latest_sensor_data
    })

# ===== ESP32: 식물 생장 단계 조회 =====
@app.get("/level")
def plant_level():
    """ESP32에서 식물 생장 단계를 요청하면 반환 (임시로 300 반환)"""
    return Response("300", mimetype="text/plain")

@app.get("/trigger/cam")
def trigger_cam():
    flags["cam_pending"] = True
    return jsonify({"status": "ok", "message": "다음 GET 시 CAM에 201 반환 예정"})

@app.get("/trigger/esp32")
def trigger_esp32():
    flags["esp32_pending"] = True
    return jsonify({"status": "ok", "message": "다음 GET 시 ESP32에 201 반환 예정"})

# ===== 프론트: 갤러리 페이지 =====
@app.get("/gallery")
def gallery():
    conn = sqlite3.connect(DB_PATH)
    cur = conn.cursor()
    cur.execute("SELECT file_path, timestamp FROM uploads ORDER BY id DESC")
    rows = cur.fetchall()
    conn.close()
    return render_template("gallery.html", rows=rows)

# ===== 업로드 파일 서빙 =====
@app.get("/uploads/<path:filename>")
def uploaded_file(filename):
    return send_from_directory(UPLOAD_DIR, filename)

# ===== API: 최근 업로드 이미지 목록 =====
@app.get("/api/uploads")
def api_uploads():
    conn = sqlite3.connect(DB_PATH)
    cur = conn.cursor()
    cur.execute("SELECT file_path, timestamp FROM uploads ORDER BY id DESC LIMIT 50")
    rows = cur.fetchall()
    conn.close()
    
    uploads = []
    for file_path, timestamp in rows:
        filename = os.path.basename(file_path)
        uploads.append({
            "filename": filename,
            "url": f"/uploads/{filename}",
            "timestamp": timestamp
        })
    
    return jsonify({"ok": True, "uploads": uploads})

if __name__ == "__main__":
    init_db()          # 기존 cam_server.db 초기화
    init_sensor_db()   # ✅ sensor_server.db 초기화
    app.run(host="0.0.0.0", port=15020, debug=False)
