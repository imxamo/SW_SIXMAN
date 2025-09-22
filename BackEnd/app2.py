import os
import datetime
from flask import Flask, request, Response, jsonify

app = Flask(__name__)

# === 설정 ===
UPLOAD_DIR = os.path.join(os.path.dirname(__file__), "uploads")
os.makedirs(UPLOAD_DIR, exist_ok=True)


@app.get("/get")
def get_poll():
    """
    ESP32-CAM이 주기적으로 GET 요청을 보내는 엔드포인트
    - 무조건 "200"만 반환
    """
    return Response("200", mimetype="text/plain")


@app.post("/upload")
def upload():
    """
    ESP32-CAM이 POST로 이미지를 업로드하는 엔드포인트
    - text/plain: 문자열로 저장 (테스트용)
    - multipart/form-data: file 필드에서 이미지 추출
    - application/octet-stream: 원시 바이트 스트림 저장
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

        return jsonify({"status": "ok", "saved": os.path.basename(saved_path)}), 200

    except Exception as e:
        return jsonify({"status": "fail", "error": str(e)}), 500


if __name__ == "__main__":
    app.run(host="0.0.0.0", port=8000, debug=False)
