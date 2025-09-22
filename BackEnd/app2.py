# app.py
import os
import datetime
from flask import Flask, request, jsonify, Response

app = Flask(__name__)

# === 설정 ===
UPLOAD_DIR = os.path.join(os.path.dirname(__file__), "uploads")
os.makedirs(UPLOAD_DIR, exist_ok=True)

# 업로드 요청 트리거(once) 메모리 플래그
# - /trigger를 호출하면 다음 /get 요청에 한 번만 "201"을 내려줌
pending_upload = {"flag": False}


@app.get("/health")
def health():
    return jsonify({"status": "ok"})


@app.post("/trigger")
def trigger_upload_once():
    """
    외부에서 수동으로 '이번에 한 번 업로드 해' 지시를 내리는 엔드포인트.
    이후 첫 번째 /get 요청에서 201을 반환하고 flag를 자동으로 해제합니다.
    간단한 데모용이므로 필요한 경우 토큰 검증을 추가하세요.
    """
    pending_upload["flag"] = True
    return jsonify({"message": "next /get will return 201 once"}), 200


@app.get("/get")
def get_poll():
    """
    ESP32가 폴링하는 엔드포인트.
    - 평소: "200"
    - /trigger 이후 첫 1회: "201" (그 다음엔 다시 "200")
    """
    if pending_upload["flag"]:
        pending_upload["flag"] = False
        return Response("201", mimetype="text/plain")
    return Response("200", mimetype="text/plain")


@app.post("/upload")
def upload():
    """
    ESP32에서 이미지(또는 텍스트)를 업로드하는 엔드포인트.
    - text/plain: 본문을 .txt로 저장 (현재 ESP32 예시 코드와 호환)
    - multipart/form-data: form-field 이름 'file'에 들어온 바이너리 파일 저장
    - application/octet-stream: 원시 바이트 스트림 저장
    """
    ct = request.content_type or ""

    timestamp = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
    saved_path = None

    try:
        if ct.startswith("text/plain"):
            # 현재 ESP32 코드에 맞춘 자리표시자 저장
            body = request.get_data(as_text=True)  # str
            saved_path = os.path.join(UPLOAD_DIR, f"{timestamp}.txt")
            with open(saved_path, "w", encoding="utf-8") as f:
                f.write(body)

        elif "multipart/form-data" in ct:
            # 나중에 실제 사진 전송 시: form-data name="file"
            if "file" not in request.files:
                return jsonify({"error": "form field 'file' not found"}), 400
            file_storage = request.files["file"]
            # 원본 이름이 있으면 확장자만 사용
            ext = os.path.splitext(file_storage.filename or "")[1] or ".bin"
            saved_path = os.path.join(UPLOAD_DIR, f"{timestamp}{ext}")
            file_storage.save(saved_path)

        elif ct.startswith("application/octet-stream"):
            # 바이트 스트림 그대로 저장
            data = request.get_data()
            saved_path = os.path.join(UPLOAD_DIR, f"{timestamp}.bin")
            with open(saved_path, "wb") as f:
                f.write(data)

        else:
            # 기타 콘텐츠 타입: 일단 텍스트로 저장 시도
            body = request.get_data()
            saved_path = os.path.join(UPLOAD_DIR, f"{timestamp}.dat")
            with open(saved_path, "wb") as f:
                f.write(body)

        return jsonify({"status": "ok", "saved": os.path.basename(saved_path)}), 200

    except Exception as e:
        return jsonify({"status": "fail", "error": str(e)}), 500


if __name__ == "__main__":
    # 개발/테스트 실행 (프로덕션이면 gunicorn 권장)
    # 외부 접속 허용 + 임의 포트(예: 8000)
    app.run(host="0.0.0.0", port=8000, debug=False)
