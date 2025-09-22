import io, os
from PIL import Image
from flask import Flask, request, jsonify

import torch
import torch.nn as nn
from torchvision import models, transforms

# ===== 0) 모델/클래스 설정 =====
# 홈 디렉토리에 파일이 있다면 다음처럼 지정 (이전 스샷 기준)
MODEL_PATH = os.path.expanduser("/home/student_15020/best_lettuce_disease_model.pth")

# 훈련 때 클래스 순서를 그대로 써야 합니다.
CLASS_IDS   = ['0', '9', '10']                 # 라벨 인덱스 -> 질병코드
CLASS_NAMES = {'0':'정상', '9':'상추균핵병', '10':'상추노균병'}

# 입력 전처리: 훈련 코드의 test_transform과 동일
preprocess = transforms.Compose([
    transforms.Resize((224, 224)),
    transforms.ToTensor(),
    transforms.Normalize(mean=[0.485, 0.456, 0.406],
                         std =[0.229, 0.224, 0.225]),
])

# ===== 1) 학습 때와 동일한 네트워크 정의 =====
def build_model(num_classes: int):
    m = models.efficientnet_b0(weights='IMAGENET1K_V1')  # 사전학습 가중치
    in_features = m.classifier[1].in_features
    m.classifier = nn.Sequential(
        nn.Dropout(0.3),
        nn.Linear(in_features, num_classes)
    )
    return m

device = torch.device("cpu")  # 서버에 CUDA 없다고 가정
model = build_model(num_classes=len(CLASS_IDS))
state = torch.load(MODEL_PATH, map_location=device)      # ← state_dict 저장본
model.load_state_dict(state)
model.eval().to(device)

# ===== 2) Flask =====
app = Flask(__name__)
ALLOWED_EXT = {"jpg","jpeg","png","bmp","webp"}

def allowed(filename:str)->bool:
    return "." in filename and filename.rsplit(".",1)[1].lower() in ALLOWED_EXT

@app.post("/api/predict")
def predict():
    if "file" not in request.files:
        return jsonify({"ok": False, "error": "no file"}), 400
    f = request.files["file"]
    if f.filename == "" or not allowed(f.filename):
        return jsonify({"ok": False, "error": "bad filename/ext"}), 400

    try:
        img = Image.open(io.BytesIO(f.read())).convert("RGB")
    except Exception as e:
        return jsonify({"ok": False, "error": f"bad image: {e}"}), 400

    x = preprocess(img).unsqueeze(0).to(device)
    with torch.no_grad():
        logits = model(x)                      # [1, 3]
        prob = torch.softmax(logits, dim=1)[0]
        idx = int(prob.argmax().item())
        conf = float(prob[idx].item())

    disease_code = CLASS_IDS[idx]
    disease_name = CLASS_NAMES[disease_code]

    return jsonify({
        "ok": True,
        "class_idx": idx,
        "disease_code": disease_code,
        "disease_name": disease_name,
        "confidence": round(conf, 4)
    })

@app.get("/api/health")
def health():
    return {"ok": True}

if __name__ == "__main__":
    # 학교가 지정한 포트로 바꾸세요
    app.run(host="0.0.0.0", port=15020)
