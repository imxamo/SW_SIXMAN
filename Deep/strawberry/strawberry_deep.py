import os
import json
import numpy as np
from PIL import Image
import matplotlib.pyplot as plt
from sklearn.metrics import classification_report, confusion_matrix
import multiprocessing
from multiprocessing import freeze_support

import torch
import torch.nn as nn
import torch.optim as optim
from torch.utils.data import Dataset, DataLoader
from torchvision import models, transforms
from torch.optim.lr_scheduler import ReduceLROnPlateau

# 한글 폰트 설정 - matplotlib에서 한글 표시
import matplotlib.font_manager as fm
# 맑은 고딕 폰트 사용, 시스템에 따라 다른 한글 폰트로 대체 가능
# Windows의 경우
font_path = 'C:/Windows/Fonts/malgun.ttf'  
# Linux/macOS의 경우 다른 경로 사용
# font_path = '/usr/share/fonts/truetype/nanum/NanumGothic.ttf'  

# 시스템에 폰트가 있는지 확인
if os.path.exists(font_path):
    font_prop = fm.FontProperties(fname=font_path)
    plt.rcParams['font.family'] = font_prop.get_name()
    plt.rcParams['axes.unicode_minus'] = False  # 마이너스 기호 깨짐 방지
else:
    print(f"경고: {font_path} 폰트를 찾을 수 없습니다. 기본 폰트를 사용합니다.")
    # 대체 방법 - 사용 가능한 폰트 중 한글 지원 폰트 사용
    plt.rcParams['font.family'] = 'Malgun Gothic'  # 또는 다른 한글 폰트 이름

# 경로 설정 - 직접 수정
base_dir = 'D:\\data_folders'  # 경로를 실제 데이터 위치로 변경해주세요
data_folders = ['train', 'validation', 'test']

# 우리가 사용할 대상 질병 클래스 명시적 지정 (딸기 질병으로 변경)
TARGET_DISEASES = ['0', '7', '8']  # 정상(0), 딸기잿빛곰팡이병(7), 딸기흰가루병(8)

# 한글 이름 매핑 (그래프 표시용)
DISEASE_NAMES = {
    '0': '정상',
    '7': '딸기잿빛곰팡이병',
    '8': '딸기흰가루병'
}

# 작물-질병 매핑 딕셔너리 (하드코딩) - 딸기로 변경
CROP_DISEASE_MAPPING = {
    '4': TARGET_DISEASES,  # 딸기: 정상(0), 딸기잿빛곰팡이병(7), 딸기흰가루병(8)
    # 필요시 다른 작물 추가
}

# 유효한 작물-질병 조합인지 확인하는 함수
def is_valid_crop_disease(crop, disease):
    """작물-질병 조합이 유효한지 확인"""
    crop = str(crop)
    disease = str(disease)
    return crop in CROP_DISEASE_MAPPING and disease in CROP_DISEASE_MAPPING[crop]

# 현재 경로 확인
print("현재 작업 디렉토리:", os.getcwd())
print("기본 디렉토리:", base_dir)
print("기본 디렉토리 존재 여부:", os.path.exists(base_dir))

# 장치 설정 (GPU 또는 CPU)
device = torch.device("cuda:0" if torch.cuda.is_available() else "cpu")
print(f"사용 장치: {device}")

# 이미지 변환 설정 - 학습 및 평가에 사용
train_transform = transforms.Compose([
    transforms.Resize((224, 224)),
    transforms.RandomHorizontalFlip(),
    transforms.RandomRotation(20),
    transforms.ColorJitter(brightness=0.1, contrast=0.1, saturation=0.1, hue=0.1),
    transforms.ToTensor(),
    transforms.Normalize(mean=[0.485, 0.456, 0.406], std=[0.229, 0.224, 0.225])
])

test_transform = transforms.Compose([
    transforms.Resize((224, 224)),
    transforms.ToTensor(),
    transforms.Normalize(mean=[0.485, 0.456, 0.406], std=[0.229, 0.224, 0.225])
])

def load_dataset(folder_name):
    """데이터셋 로드 및 전처리 함수 - 작물-질병 필터링 적용"""
    images_paths = []
    diseases = []
    
    # 절대 경로 사용
    folder_path = os.path.join(base_dir, folder_name)
    image_dir = os.path.join(folder_path, 'image')
    label_dir = os.path.join(folder_path, 'labeling')
    
    print(f"{folder_name} 경로: {folder_path}")
    print(f"이미지 디렉토리: {image_dir}")
    print(f"라벨링 디렉토리: {label_dir}")
    
    # 디렉토리 존재 확인
    if not os.path.exists(folder_path):
        print(f"경로가 존재하지 않습니다: {folder_path}")
        return [], []
    
    if not os.path.exists(image_dir):
        print(f"이미지 디렉토리가 존재하지 않습니다: {image_dir}")
        return [], []
        
    if not os.path.exists(label_dir):
        print(f"라벨링 디렉토리가 존재하지 않습니다: {label_dir}")
        return [], []
    
    # 라벨링 디렉토리 내용 확인
    label_files = os.listdir(label_dir)
    print(f"라벨링 파일 수: {len(label_files)}")
    if len(label_files) > 0:
        print(f"첫 번째 파일: {label_files[0]}")
    
    # JSON 파일 탐색
    for json_file in label_files:
        if json_file.endswith('.json'):
            json_path = os.path.join(label_dir, json_file)
            
            try:
                with open(json_path, 'r', encoding='utf-8') as f:
                    data = json.load(f)
                
                # 이미지 경로 추출
                img_filename = data['description']['image']
                img_path = os.path.join(image_dir, img_filename)
                
                # 작물과 질병 추출
                crop = data['annotations']['crop']
                disease = str(data['annotations']['disease'])  # 문자열로 변환
                
                # 작물-질병 조합이 유효한지 확인 (딸기 데이터만 사용)
                if is_valid_crop_disease(crop, disease):
                    # 이미지 파일이 존재하는지 확인
                    if os.path.exists(img_path):
                        # 이미지 경로와 레이블 저장
                        images_paths.append(img_path)
                        diseases.append(disease)
                    else:
                        print(f"이미지 파일이 존재하지 않습니다: {img_path}")
            except Exception as e:
                print(f"JSON 파일 처리 오류 {json_path}: {e}")
    
    print(f"로드된 이미지 경로 수: {len(images_paths)}")
    print(f"로드된 레이블 수: {len(diseases)}")
    
    # 레이블 분포 확인 (디버깅용)
    label_counts = {}
    for label in diseases:
        if label in label_counts:
            label_counts[label] += 1
        else:
            label_counts[label] = 1
    
    print(f"\n{folder_name} 레이블 분포:")
    for disease in sorted(label_counts.keys()):
        count = label_counts[disease]
        disease_name = DISEASE_NAMES.get(disease, f"코드 {disease}")
        if len(diseases) > 0:
            print(f"{disease_name}: {count}개 ({count/len(diseases)*100:.2f}%)")
    
    return images_paths, diseases

# 커스텀 데이터셋 클래스 정의
class StrawberryDataset(Dataset):  # 클래스 이름 변경
    def __init__(self, image_paths, labels, disease_to_idx, transform=None):
        self.image_paths = image_paths
        self.labels = [disease_to_idx[label] for label in labels]
        self.transform = transform
        
    def __len__(self):
        return len(self.image_paths)
    
    def __getitem__(self, idx):
        img_path = self.image_paths[idx]
        label = self.labels[idx]
        
        try:
            image = Image.open(img_path).convert('RGB')
            
            if self.transform:
                image = self.transform(image)
                
            return image, label
        except Exception as e:
            print(f"이미지 로드 오류 {img_path}: {e}")
            # 오류 발생 시 첫 번째 이미지 반환 (에러 방지)
            return self.__getitem__(0) if idx != 0 else None

# 모든 폴더에서 데이터 로드 시도
datasets_paths = {}

for folder in data_folders:
    print(f"\n{folder} 데이터셋 로드 중...")
    paths, y_labels = load_dataset(folder)
    
    if len(paths) > 0 and len(y_labels) > 0:
        datasets_paths[folder] = (paths, y_labels)
    else:
        print(f"경고: {folder} 데이터를 로드할 수 없습니다. 건너뜁니다.")

# 모든 데이터셋이 비어있는 경우
if not datasets_paths:
    print("오류: 어떠한 데이터셋도 로드할 수 없습니다. 프로그램을 종료합니다.")
    exit(1)

# 질병 클래스 정의 - 명시적으로 지정된 타겟 질병만 사용
unique_diseases = TARGET_DISEASES  # 이미 지정한 대상 질병만 사용
disease_to_idx = {disease: idx for idx, disease in enumerate(unique_diseases)}
idx_to_disease = {idx: disease for disease, idx in disease_to_idx.items()}

print(f"\n고유한 질병 클래스: {unique_diseases}")
print(f"질병 클래스 수: {len(unique_diseases)}")
print(f"질병-인덱스 매핑: {disease_to_idx}")

# PyTorch 데이터셋 및 데이터로더 생성
data_loaders = {}
dataset_sizes = {}

for folder in datasets_paths.keys():
    paths, labels = datasets_paths[folder]
    
    # 데이터셋 생성
    if folder == 'train':
        dataset = StrawberryDataset(paths, labels, disease_to_idx, transform=train_transform)  # 클래스 이름 변경
    else:
        dataset = StrawberryDataset(paths, labels, disease_to_idx, transform=test_transform)  # 클래스 이름 변경
    
    # 데이터로더 생성 (Windows에서는 num_workers=0으로 설정하여 멀티프로세싱 문제 해결)
    data_loaders[folder] = DataLoader(dataset, batch_size=32, shuffle=(folder=='train'), num_workers=0)
    dataset_sizes[folder] = len(dataset)
    
    # 레이블 분포 확인
    label_counts = {}
    for label in labels:
        if label in label_counts:
            label_counts[label] += 1
        else:
            label_counts[label] = 1
    
    print(f"\n{folder} 레이블 분포:")
    for disease in unique_diseases:
        count = label_counts.get(disease, 0)
        disease_name = DISEASE_NAMES.get(disease, f"코드 {disease}")
        if len(labels) > 0:
            print(f"{disease_name}: {count}개 ({count/len(labels)*100:.2f}%)")
        else:
            print(f"{disease_name}: {count}개 (0.00%)")
    
    print(f"\n{folder} 데이터셋 크기: {len(paths)}개")

# 모델 구축
def build_model(num_classes):
    # EfficientNet 모델 사용 (EfficientNetB0에 해당)
    model = models.efficientnet_b0(weights='IMAGENET1K_V1')
    
    # 분류기 부분 재정의
    num_ftrs = model.classifier[1].in_features
    model.classifier = nn.Sequential(
        nn.Dropout(0.3),
        nn.Linear(num_ftrs, num_classes)
    )
    
    return model

# 멀티프로세싱을 위한 보호 코드 추가
if __name__ == '__main__':
    # Windows에서 멀티프로세싱 지원
    freeze_support()
    
    # 이하 코드는 데이터셋이 정상적으로 로드된 후에만 실행
    if 'train' in data_loaders:
        # 모델 생성
        num_classes = len(unique_diseases)
        model = build_model(num_classes)
        model = model.to(device)

    # 모델 요약 정보 출력 (PyTorch에서는 직접적인 summary 함수 없음)
    print(model)
    
    # 손실 함수, 최적화 알고리즘, 스케줄러 설정
    criterion = nn.CrossEntropyLoss()
    optimizer = optim.Adam(model.parameters(), lr=0.0001)
    scheduler = ReduceLROnPlateau(optimizer, mode='min', factor=0.1, patience=5, verbose=True)

    # 모델 훈련 함수
    def train_model(model, criterion, optimizer, scheduler, num_epochs=50):
        best_acc = 0.0
        training_history = {
            'train_loss': [], 'train_acc': [],
            'val_loss': [], 'val_acc': []
        }
        
        for epoch in range(num_epochs):
            print(f'Epoch {epoch+1}/{num_epochs}')
            print('-' * 10)
            
            # 각 에폭마다 훈련 및 검증
            for phase in ['train', 'validation']:
                if phase == 'train':
                    model.train()
                else:
                    model.eval()
                
                running_loss = 0.0
                running_corrects = 0
                
                # 데이터 반복
                for inputs, labels in data_loaders[phase]:
                    inputs = inputs.to(device)
                    labels = labels.to(device)
                    
                    # 매개변수 기울기 초기화
                    optimizer.zero_grad()
                    
                    # 순전파
                    # 훈련 시에만 연산 기록 추적
                    with torch.set_grad_enabled(phase == 'train'):
                        outputs = model(inputs)
                        _, preds = torch.max(outputs, 1)
                        loss = criterion(outputs, labels)
                        
                        # 훈련 단계인 경우 역전파 + 최적화
                        if phase == 'train':
                            loss.backward()
                            optimizer.step()
                    
                    # 통계
                    running_loss += loss.item() * inputs.size(0)
                    running_corrects += torch.sum(preds == labels.data)
                
                # 에폭 손실 및 정확도 계산
                epoch_loss = running_loss / dataset_sizes[phase]
                epoch_acc = running_corrects.double() / dataset_sizes[phase]
                
                # 히스토리 저장
                if phase == 'train':
                    training_history['train_loss'].append(epoch_loss)
                    training_history['train_acc'].append(epoch_acc.item())
                else:
                    training_history['val_loss'].append(epoch_loss)
                    training_history['val_acc'].append(epoch_acc.item())
                    # 검증 손실에 따라 학습률 조정
                    scheduler.step(epoch_loss)
                
                print(f'{phase} Loss: {epoch_loss:.4f} Acc: {epoch_acc:.4f}')
                
                # 모델 저장 (검증 정확도가 향상된 경우)
                if phase == 'validation' and epoch_acc > best_acc:
                    best_acc = epoch_acc
                    torch.save(model.state_dict(), 'best_strawberry_disease_model.pth')  # 파일명 변경
            
            print()
        
        print(f'최고 검증 정확도: {best_acc:.4f}')
        return model, training_history

    # 모델 훈련 실행
    print("\n모델 훈련 시작...")
    model, history = train_model(model, criterion, optimizer, scheduler, num_epochs=50)

    # 학습 과정 시각화
    def plot_history(history):
        plt.figure(figsize=(12, 4))
        
        plt.subplot(1, 2, 1)
        plt.plot(history['train_acc'])
        plt.plot(history['val_acc'])
        plt.title('모델 정확도')
        plt.ylabel('정확도')
        plt.xlabel('에폭')
        plt.legend(['학습', '검증'], loc='lower right')
        
        plt.subplot(1, 2, 2)
        plt.plot(history['train_loss'])
        plt.plot(history['val_loss'])
        plt.title('모델 손실')
        plt.ylabel('손실')
        plt.xlabel('에폭')
        plt.legend(['학습', '검증'], loc='upper right')
        
        plt.tight_layout()
        plt.savefig('training_history.png')
        plt.show()

    plot_history(history)

    # 테스트 데이터가 있는 경우 평가
    if 'test' in data_loaders:
        print("\n테스트 세트에서 모델 평가 중...")
        model.eval()
        
        y_true = []
        y_pred = []
        
        with torch.no_grad():
            for inputs, labels in data_loaders['test']:
                inputs = inputs.to(device)
                labels = labels.to(device)
                
                outputs = model(inputs)
                _, preds = torch.max(outputs, 1)
                
                y_true.extend(labels.cpu().numpy())
                y_pred.extend(preds.cpu().numpy())
        
        # 정확도 계산
        test_acc = np.mean(np.array(y_true) == np.array(y_pred))
        print(f"테스트 정확도: {test_acc:.4f}")
        
        # 분류 보고서
        print("\n분류 보고서:")
        # 한글 질병명으로 변환
        disease_names = [DISEASE_NAMES[idx_to_disease[i]] for i in range(len(unique_diseases))]
        print(classification_report(y_true, y_pred, target_names=disease_names))
        
        # 혼동 행렬 시각화
        cm = confusion_matrix(y_true, y_pred)
        plt.figure(figsize=(10, 8))
        plt.imshow(cm, interpolation='nearest', cmap=plt.cm.Blues)
        plt.title('혼동 행렬')
        plt.colorbar()
        tick_marks = np.arange(len(disease_names))
        plt.xticks(tick_marks, disease_names, rotation=45)
        plt.yticks(tick_marks, disease_names)
        plt.tight_layout()
        plt.ylabel('실제 레이블')
        plt.xlabel('예측 레이블')
        plt.savefig('confusion_matrix.png')
        plt.show()

    # 최종 모델 저장
    torch.save(model.state_dict(), 'strawberry_disease_classification_model.pth')  # 파일명 변경
    print("\n모델이 저장되었습니다: strawberry_disease_classification_model.pth")
    
    # 모델 구조 저장 (필요시 로드할 때 사용)
    torch.save({
        'model_state_dict': model.state_dict(),
        'optimizer_state_dict': optimizer.state_dict(),
        'disease_to_idx': disease_to_idx,
        'class_names': unique_diseases,
        'disease_names': DISEASE_NAMES
    }, 'strawberry_disease_model_full.pth')  # 파일명 변경
    
else:
    print("오류: 학습 데이터를 찾을 수 없습니다. 프로그램을 종료합니다.")
    exit(1)