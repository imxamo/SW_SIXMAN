from flask import Flask, request, jsonify
from flask_cors import CORS
import os
from datetime import datetime

# 🔵 반드시 route보다 먼저 있어야 함
app = Flask(__name__)
CORS(app)

UPLOAD_FOLDER = 'uploads'
os.makedirs(UPLOAD_FOLDER, exist_ok=True)

@app.route('/upload', methods=['POST', 'GET'])
def upload():
    if request.method == 'GET':
        user_agent = request.headers.get('User-Agent', '').lower()
        if user_agent.startswith('esp32'):
            return "200 OK - ESP32 identified", 200
        return "Not ESP32 device", 403

    user_agent = request.headers.get('User-Agent', '').lower()
    content_type = request.headers.get('Content-Type', '').lower()
    print("[📥] POST 요청 수신됨")
    print(f"User-Agent: {user_agent}")
    print(f"Content-Type: {content_type}")

    if content_type == "image/jpeg":
        image_data = request.get_data()
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        filename = f"esp32_{timestamp}.jpg"
        filepath = os.path.join(UPLOAD_FOLDER, filename)

        with open(filepath, 'wb') as f:
            f.write(image_data)

        print(f"[✔] ESP32 image saved to {filepath}")
        return "Image received and saved", 200

    image = request.files.get('image')
    plant = request.form.get('plant', 'unknown')

    if image:
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        filename = f"{plant}_{timestamp}.jpg"
        filepath = os.path.join(UPLOAD_FOLDER, filename)
        image.save(filepath)
        print(f"[✔] Image saved: {filepath}")
        return jsonify({
            "result": {
                "disease": "예시 병명",
                "description": f"{plant} 분석 결과입니다."
            }
        }), 200

    return jsonify({
        "result": None,
        "error": "No image received"
    }), 400

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=3000)
