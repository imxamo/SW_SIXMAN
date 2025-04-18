import json
import cv2
import numpy as np
import torch
from drive import get_drive_service, list_files_in_folder, download_file
from model import train_model, build_model, TARGET_DISEASES
from torch.optim import Adam
from torch.optim.lr_scheduler import ReduceLROnPlateau

device = torch.device("cuda:0" if torch.cuda.is_available() else "cpu")

# 설정
image_folder_id = "1xEv-zOiadj8tHxGYBw4rekUHQekaCllX"
label_folder_id = "1y2kHbK0mwYitTE66rGeWx3AGSiAOIv7k"
BATCH_SIZE = 10  # 필요 시 조정 가능
EPOCHS = 1       # 배치마다 반복할 에폭 수

def preprocess_image(file_stream):
    file_bytes = np.frombuffer(file_stream.read(), np.uint8)
    image = cv2.imdecode(file_bytes, cv2.IMREAD_COLOR)
    return image  # transform은 model.py에서 처리

def preprocess_label(file_stream):
    data = json.load(file_stream)
    return str(data["annotations"]["disease"])  # 문자열로 강제

def is_valid_disease(disease):
    return disease in TARGET_DISEASES

if __name__ == "__main__":
    print("📁 Google Drive 인증 중...")
    service = get_drive_service()

    print("📷 이미지 파일 목록 불러오는 중...")
    image_files = list_files_in_folder(service, image_folder_id)
    label_files = list_files_in_folder(service, label_folder_id)
    label_map = {f['name']: f['id'] for f in label_files}

    # 모델, 옵티마이저, 스케줄러 초기화
    model = build_model(num_classes=len(TARGET_DISEASES)).to(device)
    optimizer = Adam(model.parameters(), lr=0.0001)
    scheduler = ReduceLROnPlateau(optimizer, 'min', factor=0.1, patience=3, verbose=True)

    for i in range(0, len(image_files), BATCH_SIZE):
        images = []
        labels = []

        batch_files = image_files[i:i + BATCH_SIZE]
        for img_file in batch_files:
            img_name = img_file['name']
            label_name = img_name + ".json"
            if label_name not in label_map:
                continue

            img_stream = download_file(service, img_file['id'])
            label_stream = download_file(service, label_map[label_name])
            label = preprocess_label(label_stream)

            if not is_valid_disease(label):
                continue

            image = preprocess_image(img_stream)
            images.append(image)
            labels.append(label)

        if images:
            print(f"🧠 {i}번째부터 {i + len(images)}번째 이미지까지 누적 학습 중...")
            train_model(model, optimizer, scheduler, images, labels, epochs=EPOCHS)

    # 최종 저장
    torch.save(model.state_dict(), "plant_disease_model_final.pth")
    print("✅ 전체 누적 학습 완료. 최종 모델 저장됨: plant_disease_model_final.pth")
