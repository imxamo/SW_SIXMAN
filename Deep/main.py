import json
import cv2
import numpy as np
from drive import get_drive_service, list_files_in_folder, download_file
from model import train_model

# 설정
image_folder_id = "구글드라이브_이미지_폴더ID"
label_folder_id = "구글드라이브_라벨_폴더ID"

def preprocess_image(file_stream):
    file_bytes = np.frombuffer(file_stream.read(), np.uint8)
    image = cv2.imdecode(file_bytes, cv2.IMREAD_COLOR)
    return image

def preprocess_label(file_stream):
    data = json.load(file_stream)
    return data['label']  # 또는 적절한 키 사용

if __name__ == "__main__":
    print("📁 Google Drive 인증 중...")
    service = get_drive_service()

    print("📷 이미지 파일 목록 불러오는 중...")
    image_files = list_files_in_folder(service, image_folder_id)
    label_files = list_files_in_folder(service, label_folder_id)

    label_map = {f['name']: f['id'] for f in label_files}
    images = []
    labels = []

    for img_file in image_files:
        img_name = img_file['name']
        label_name = img_name + ".json"
        if label_name not in label_map:
            continue

        img_stream = download_file(service, img_file['id'])
        label_stream = download_file(service, label_map[label_name])

        image = preprocess_image(img_stream)
        label = preprocess_label(label_stream)

        images.append(image)
        labels.append(label)

    print("🧠 딥러닝 학습 시작...")
    train_model(images, labels)
