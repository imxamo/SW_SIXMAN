import os, io
from flask import Flask, send_from_directory, request, jsonify
from PIL import Image

import torch, torch.nn as nn
from torchvision import models, transforms

# ===== React 정적 폴더 경로 =====
REACT_DIST = os.path.abspath(os.path.join(os.path.dirname(__file__), "../FrontEnd/dist"))
app = Flask(__name__, static_folder=REACT_DIST, static_url_path="/")

# ===== (예시) 모델/전처리 =====
MODEL_PATH = os.path.expanduser("/home/student_15020/best_lettuce_disease_model.pth")
CLASS_IDS   = ['0','9','10']
CLASS_NAMES = {'0':'정상','9':'상추균핵병','10':'상추노균병'}

preprocess = transforms.Compose([
    transforms.Resize((224,224)),
    transforms.ToTensor(),
    transforms.Normalize([0.485,0.456,0.406],[0.229,0.224,0.225]),
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

# ===== 프론트 페이지(SPA) 서빙 =====
@app.route("/", defaults={"path": ""})
@app.route("/<path:path>")
def serve_react(path):
    file_path = os.path.join(REACT_DIST, path)
    if os.path.exists(file_path) and os.path.isfile(file_path):
        return send_from_directory(REACT_DIST, path)
    return send_from_directory(REACT_DIST, "index.html")

# ===== API =====
@app.get("/api/health")
def health():
    return {"ok": True}

ALLOWED = {"jpg","jpeg","png","bmp","webp"}
@app.post("/api/predict")
def predict():
    if "file" not in request.files:
        return jsonify({"ok":False,"error":"no file"}), 400
    f = request.files["file"]
    ext_ok = "." in f.filename and f.filename.rsplit(".",1)[1].lower() in ALLOWED
    if not f.filename or not ext_ok:
        return jsonify({"ok":False,"error":"bad filename/ext"}), 400

    try:
        img = Image.open(io.BytesIO(f.read())).convert("RGB")
    except Exception as e:
        return jsonify({"ok":False,"error":f"bad image: {e}"}), 400

    x = preprocess(img).unsqueeze(0).to(device)
    with torch.no_grad():
        logits = model(x)
        prob = torch.softmax(logits, dim=1)[0]
        idx = int(prob.argmax().item())
        conf = float(prob[idx].item())

    code = CLASS_IDS[idx]
    return jsonify({"ok":True,"class_idx":idx,"disease_code":code,
                    "disease_name":CLASS_NAMES[code],"confidence":round(conf,4)})


if __name__ == "__main__":
    app.run(host="0.0.0.0", port=15020)   # 학교 지정 포트 유지
