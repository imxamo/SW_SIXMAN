import os
import io
import json
import cv2
import numpy as np
import tensorflow as tf
from google.oauth2.credentials import Credentials
from google_auth_oauthlib.flow import InstalledAppFlow
from googleapiclient.discovery import build
from googleapiclient.http import MediaIoBaseDownload

# --- STEP 1: Google Drive 인증 ---
from google_auth_oauthlib.flow import InstalledAppFlow
from googleapiclient.discovery import build

def authenticate_drive():
    scopes = ['https://www.googleapis.com/auth/drive.readonly']
    flow = InstalledAppFlow.from_client_secrets_file("credentials.json", scopes, redirect_uri="urn:ietf:wg:oauth:2.0:oob")

    # 인증 URL 생성
    auth_url, _ = flow.authorization_url(prompt='consent')
    print("🔗 다음 주소를 브라우저에 복사해서 여세요:")
    print(auth_url)

    # 인증 코드 입력
    code = input("📥 복사한 인증 코드를 입력하세요: ")
    flow.fetch_token(code=code)

    # 서비스 생성
    return build('drive', 'v3', credentials=flow.credentials)


# --- STEP 2: 파일 ID 찾기 ---
def get_file_id_by_name(service, filename):
    query = f"name='{filename}'"
    results = service.files().list(q=query, fields="files(id, name)").execute()
    items = results.get('files', [])
    if items:
        return items[0]['id']
    return None

# --- STEP 3: 파일 다운로드 (streaming) ---
def download_file(service, file_id):
    request = service.files().get_media(fileId=file_id)
    file_stream = io.BytesIO()
    downloader = MediaIoBaseDownload(file_stream, request)
    done = False
    while not done:
        status, done = downloader.next_chunk()
    file_stream.seek(0)
    return file_stream

# --- STEP 4: JSON에서 라벨 추출 ---
def get_label_from_json(json_stream):
    data = json.load(json_stream)
    plant = data['plant']
    disease = data['disease']
    return f"{plant}_{disease}"

# --- STEP 5: 간단한 CNN 모델 정의 ---
def build_model(input_shape, num_classes):
    model = tf.keras.Sequential([
        tf.keras.layers.Conv2D(32, (3,3), activation='relu', input_shape=input_shape),
        tf.keras.layers.MaxPooling2D((2,2)),
        tf.keras.layers.Conv2D(64, (3,3), activation='relu'),
        tf.keras.layers.MaxPooling2D((2,2)),
        tf.keras.layers.Flatten(),
        tf.keras.layers.Dense(64, activation='relu'),
        tf.keras.layers.Dense(num_classes, activation='softmax')
    ])
    model.compile(optimizer='adam',
                  loss='sparse_categorical_crossentropy',
                  metrics=['accuracy'])
    return model

# --- STEP 6: 메인 실행 ---
def main():
    service = authenticate_drive()

    # 사용할 파일 이름 목록 (예시)
    image_filenames = ['image_001.jpg', 'image_002.jpg']
    json_filenames = ['image_001.json', 'image_002.json']

    images = []
    labels = []
    label_dict = {}
    label_index = 0

    for img_name, json_name in zip(image_filenames, json_filenames):
        # 이미지 다운로드
        img_id = get_file_id_by_name(service, img_name)
        if img_id is None: continue
        img_stream = download_file(service, img_id)
        img_array = np.asarray(bytearray(img_stream.read()), dtype=np.uint8)
        image = cv2.imdecode(img_array, cv2.IMREAD_GRAYSCALE)  # 흑백 처리
        image = cv2.resize(image, (64, 64))  # 크기 통일
        images.append(image)

        # 라벨 추출
        json_id = get_file_id_by_name(service, json_name)
        if json_id is None: continue
        json_stream = download_file(service, json_id)
        label_str = get_label_from_json(json_stream)

        if label_str not in label_dict:
            label_dict[label_str] = label_index
            label_index += 1
        labels.append(label_dict[label_str])

    # 넘파이 배열로 변환
    images = np.array(images).reshape(-1, 64, 64, 1) / 255.0
    labels = np.array(labels)

    print(f"총 클래스: {label_dict}")
    print(f"총 이미지 수: {len(images)}")

    model = build_model((64, 64, 1), len(label_dict))
    model.fit(images, labels, epochs=5)

if __name__ == "__main__":
    main()
