import os
import io
import json
import cv2
import numpy as np
import tensorflow as tf
from google_auth_oauthlib.flow import InstalledAppFlow
from googleapiclient.discovery import build
from googleapiclient.http import MediaIoBaseDownload

# --- Google Drive 인증 ---
def authenticate_drive():
    scopes = ['https://www.googleapis.com/auth/drive.readonly']
    flow = InstalledAppFlow.from_client_secrets_file("credentials.json", scopes, redirect_uri="urn:ietf:wg:oauth:2.0:oob")
    auth_url, _ = flow.authorization_url(prompt='consent')
    print("🔗 브라우저에서 다음 주소를 열고 인증 후 코드를 붙여넣으세요:")
    print(auth_url)
    code = input("📥 인증 코드: ")
    flow.fetch_token(code=code)
    return build('drive', 'v3', credentials=flow.credentials)

# --- Google Drive 폴더 내 파일 목록 가져오기 ---
def list_files_in_folder(service, folder_id):
    query = f"'{folder_id}' in parents and trashed=false"
    results = service.files().list(q=query, pageSize=1000, fields="files(id, name)").execute()
    return results.get('files', [])

# --- 파일 다운로드 ---
def download_file(service, file_id):
    request = service.files().get_media(fileId=file_id)
    file_stream = io.BytesIO()
    downloader = MediaIoBaseDownload(file_stream, request)
    done = False
    while not done:
        status, done = downloader.next_chunk()
    file_stream.seek(0)
    return file_stream

# --- JSON에서 라벨 추출 ---
def parse_json_label(json_stream):
    data = json.load(json_stream)
    return data["annotations"]["disease"]

# --- 데이터셋 로드 ---
def load_dataset_from_drive(service, image_folder_id, label_folder_id):
    image_files = list_files_in_folder(service, image_folder_id)
    label_files = list_files_in_folder(service, label_folder_id)
    label_dict = {f["name"]: f["id"] for f in label_files}

    images, labels = [], []
    label_to_index = {}
    next_index = 0

    for image_file in image_files:
        name = image_file["name"]
        if not name.lower().endswith((".jpg", ".jpeg")):
            continue

        json_name = name + ".json"
        if json_name not in label_dict:
            continue

        # 이미지 로딩
        img_stream = download_file(service, image_file["id"])
        img_array = np.asarray(bytearray(img_stream.read()), dtype=np.uint8)
        image = cv2.imdecode(img_array, cv2.IMREAD_GRAYSCALE)
        if image is None:
            continue
        image = cv2.resize(image, (64, 64))
        images.append(image)

        # 라벨 로딩
        json_stream = download_file(service, label_dict[json_name])
        label_str = parse_json_label(json_stream)

        if label_str not in label_to_index:
            label_to_index[label_str] = next_index
            next_index += 1
        labels.append(label_to_index[label_str])

    return images, labels, label_to_index

# --- 모델 정의 ---
def build_model(input_shape, num_classes):
    model = tf.keras.Sequential([
        tf.keras.layers.Input(shape=input_shape),
        tf.keras.layers.Conv2D(32, (3, 3), activation='relu'),
        tf.keras.layers.MaxPooling2D((2, 2)),
        tf.keras.layers.Conv2D(64, (3, 3), activation='relu'),
        tf.keras.layers.MaxPooling2D((2, 2)),
        tf.keras.layers.Flatten(),
        tf.keras.layers.Dense(64, activation='relu'),
        tf.keras.layers.Dense(num_classes, activation='softmax')
    ])
    model.compile(optimizer='adam',
                  loss='sparse_categorical_crossentropy',
                  metrics=['accuracy'])
    return model

# --- 메인 실행 ---
def main():
    service = authenticate_drive()

    # 🔽 실제 구글 드라이브 폴더 ID를 여기에 입력해
    train_image_folder_id = "1xEv-zOiadj8tHxGYBw4rekUHQekaCllX"
    train_label_folder_id = "1y2kHbK0mwYitTE66rGeWx3AGSiAOIv7k"

    train_images, train_labels, label_map = load_dataset_from_drive(service, train_image_folder_id, train_label_folder_id)

    if not train_images:
        print("❌ 학습용 이미지가 없습니다.")
        return

    X_train = np.array(train_images).reshape(-1, 64, 64, 1) / 255.0
    y_train = np.array(train_labels)

    print(f"✅ 총 학습 이미지 수: {len(X_train)}")
    print(f"✅ 클래스 수: {len(label_map)}")
    print("✅ 클래스 맵:", label_map)

    model = build_model((64, 64, 1), num_classes=len(label_map))
    model.fit(X_train, y_train, epochs=5)

if __name__ == "__main__":
    main()
