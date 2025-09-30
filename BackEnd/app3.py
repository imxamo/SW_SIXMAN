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
FRONTEND_DIR = os.path.join(BACKEND_DIR, "..", "FrontEnd")  # gallery.html 위치
UPLOAD_DIR = os.path.join(BACKEND_DIR, "uploads")
DB_PATH = os.path.join(BACKEND_DIR, "cam_server.db")

# uploads 폴더가 없으면 생성
os.makedirs(UPLOAD_DIR, exist_ok=True)

# Flask 앱 생성 (React SPA + 템플릿 폴더 모두 지원)
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

# ===== ESP32-CAM 전역 flag =====
trigger_flag = {"pending": False}

# ===== DB 초기화 =====
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

# ===== React SPA 서빙 (메인 페이지) =====
@app.route("/", defaults={"path": ""})
@app.route("/<path:path>")
def serve_react(path):
    # API 엔드포인트는 건너뛰기
    if path.startswith("api/") or path in ["get", "upload", "trigger", "gallery", "uploads"]:
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

# ===== ESP32-CAM: GET 폴링 =====
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

# ===== ESP32-CAM: 이미지 업로드 =====
@app.post("/upload")
def upload():
    """ESP32-CAM POST 신호 (이미지 업로드)"""
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
            saved_path = os.path.join(UPLOAD_DIR, f"{timestamp}.jpg")
            with open(saved_path, "wb") as f:
                f.write(data)

        else:
            data = request.get_data()
            saved_path = os.path.join(UPLOAD_DIR, f"{timestamp}.dat")
            with open(saved_path, "wb") as f:
                f.write(data)

        insert_upload_log(saved_path)

        return jsonify({"status": "ok", "saved": os.path.basename(saved_path)}), 200

    except Exception as e:
        return jsonify({"status": "fail", "error": str(e)}), 500

# ===== 프론트: 촬영 트리거 =====
@app.get("/trigger")
def trigger():
    """프론트에서 '사진 찍기' 버튼 누르면 호출"""
    trigger_flag["pending"] = True
    return jsonify({"status": "ok", "message": "다음 GET 시 CAM에 201 반환 예정"})

# ===== 프론트: 갤러리 페이지 =====
@app.get("/gallery")
def gallery():
    """업로드된 사진들을 웹 페이지로 보여줌"""
    conn = sqlite3.connect(DB_PATH)
    cur = conn.cursor()
    cur.execute("SELECT file_path, timestamp FROM uploads ORDER BY id DESC")
    rows = cur.fetchall()
    conn.close()
    return render_template("gallery.html", rows=rows)

# ===== 업로드 파일 서빙 =====
@app.get("/uploads/<path:filename>")
def uploaded_file(filename):
    """업로드된 파일을 서빙"""
    return send_from_directory(UPLOAD_DIR, filename)

# ===== API: 최근 업로드 이미지 목록 =====
@app.get("/api/uploads")
def api_uploads():
    """업로드된 이미지 목록을 JSON으로 반환 (React 앱에서 사용 가능)"""
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
    init_db()
    app.run(host="0.0.0.0", port=15020, debug=False)
