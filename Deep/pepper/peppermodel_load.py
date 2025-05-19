import os
import torch
import torch.nn as nn
import torchvision.models as models
from PIL import Image
import torchvision.transforms as transforms
import matplotlib.pyplot as plt
import numpy as np

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

# 장치 설정
device = torch.device("cuda:0" if torch.cuda.is_available() else "cpu")
print(f"사용 장치: {device}")

# 나머지 코드는 동일...
# 모델 구축 함수
def build_model(num_classes):
    # EfficientNet 모델 사용
    model = models.efficientnet_b0(weights=None)  # 가중치는 로드할 것이므로 초기값 필요 없음
    
    # 분류기 부분 재정의
    num_ftrs = model.classifier[1].in_features
    model.classifier = nn.Sequential(
        nn.Dropout(0.3),
        nn.Linear(num_ftrs, num_classes)
    )
    
    return model

# 질병 이름을 표시하기 위한 매핑
disease_names = {
    '0': '정상',
    '3': '고추마일드모틀바이러스병',
    '4': '고추점무늬병'
}

# 전체 모델 정보 파일 로드 (경로 변경)
model_path = 'pepper_disease_model_full.pth'

if os.path.exists(model_path):
    # 저장된 체크포인트 로드
    checkpoint = torch.load(model_path, map_location=device)
    
    # 체크포인트에서 정보 추출
    model_state_dict = checkpoint['model_state_dict']
    optimizer_state_dict = checkpoint['optimizer_state_dict']
    disease_to_idx = checkpoint['disease_to_idx']
    class_names = checkpoint['class_names']
    
    # 클래스 인덱스와 질병 코드 간의 매핑 생성
    idx_to_disease = {idx: disease for idx, disease in enumerate(class_names)}
    
    print(f"모델 체크포인트가 성공적으로 로드되었습니다: {model_path}")
    print(f"질병 클래스: {class_names}")
    print(f"질병-인덱스 매핑: {disease_to_idx}")
else:
    print(f"모델 파일을 찾을 수 없습니다: {model_path}")
    exit(1)

# 모델 구조 생성 및 가중치 로드
num_classes = len(class_names)
model = build_model(num_classes)
model.load_state_dict(model_state_dict)
model.to(device)
model.eval()  # 평가 모드로 설정

# 이미지 전처리를 위한 변환
test_transform = transforms.Compose([
    transforms.Resize((224, 224)),
    transforms.ToTensor(),
    transforms.Normalize(mean=[0.485, 0.456, 0.406], std=[0.229, 0.224, 0.225])
])

# 단일 이미지를 예측하는 함수 수정
def predict_disease(image_path):
    # 이미지 로드 및 전처리
    try:
        image = Image.open(image_path).convert('RGB')
        image_tensor = test_transform(image).unsqueeze(0).to(device)  # 배치 차원 추가
        
        # 예측
        with torch.no_grad():
            outputs = model(image_tensor)
            probabilities = torch.nn.functional.softmax(outputs, dim=1)[0]
            probabilities = probabilities.cpu().numpy()
            _, predicted = torch.max(outputs, 1)
            disease_idx = predicted.item()
            disease_code = idx_to_disease[disease_idx]
            
        # 결과 출력
        print(f"\n이미지: {image_path}")
        print(f"예측된 클래스: {disease_code} ({disease_names.get(disease_code, '알 수 없음')})")
        print("클래스별 확률:")
        
        # 가장 높은 확률을 가진 클래스 찾기
        max_prob_idx = np.argmax(probabilities)
        max_prob_disease = idx_to_disease[max_prob_idx]
        
        for i, prob in enumerate(probabilities):
            disease_code_i = idx_to_disease[i]
            name = disease_names.get(disease_code_i, '알 수 없음')
            print(f"  - {disease_code_i} ({name}): {prob*100:.2f}%")
        
        # 이미지 표시 및 결과 시각화
        plt.figure(figsize=(10, 5))
        
        # 이미지 표시 - 가장 높은 확률의 질병 표시
        plt.subplot(1, 2, 1)
        plt.imshow(image)
        plt.title(f"predict: {max_prob_disease} ({disease_names.get(max_prob_disease, '알 수 없음')})")
        plt.axis('off')
        
        # 확률 막대 그래프
        plt.subplot(1, 2, 2)
        classes = [f"{code} ({disease_names.get(code, '알 수 없음')})" for code in class_names]
        plt.barh(classes, probabilities)
        plt.xlim(0, 1)
        plt.xlabel('probability')
        plt.title('predict probability')
        plt.tight_layout()
        plt.show()
        
        return max_prob_disease, probabilities
    
    except Exception as e:
        print(f"이미지 예측 중 오류 발생: {e}")
        return None, None

# 이미지 폴더로부터 여러 이미지를 예측하는 함수
def predict_folder(folder_path, limit=5):
    print(f"\n폴더 내 이미지 예측: {folder_path}")
    
    if not os.path.exists(folder_path):
        print(f"폴더를 찾을 수 없습니다: {folder_path}")
        return
    
    # 이미지 파일 찾기
    image_extensions = ['.jpg', '.jpeg', '.png']
    image_files = []
    
    for file in os.listdir(folder_path):
        ext = os.path.splitext(file)[1].lower()
        if ext in image_extensions:
            image_files.append(os.path.join(folder_path, file))
    
    if not image_files:
        print("폴더에 이미지 파일이 없습니다.")
        return
    
    # 이미지 수 제한
    if limit > 0 and len(image_files) > limit:
        image_files = image_files[:limit]
    
    # 각 이미지 예측
    results = []
    for image_path in image_files:
        disease_code, probs = predict_disease(image_path)
        if disease_code:
            results.append((image_path, disease_code, probs))
    
    # 요약 통계
    print(f"\n총 {len(results)}개 이미지 분석 완료")
    class_counts = {}
    for _, disease, _ in results:
        class_counts[disease] = class_counts.get(disease, 0) + 1
    
    print("질병 클래스 분포:")
    for disease in class_names:
        count = class_counts.get(disease, 0)
        print(f"  - {disease} ({disease_names.get(disease, '알 수 없음')}): {count}개 ({count/len(results)*100 if results else 0:.2f}%)")

# 사용 예시:
# 1. 단일 이미지 예측
# predict_disease('경로/이미지파일.jpg')

# 2. 폴더 내 여러 이미지 예측 (최대 5개)
# predict_folder('이미지경로폴더', limit=10)

# 학습 정보 확인 (옵티마이저 상태 등)
def print_optimizer_info():
    if 'optimizer_state_dict' in checkpoint:
        print("\n옵티마이저 정보:")
        print(f"학습률(Learning Rate): {checkpoint['optimizer_state_dict']['param_groups'][0]['lr']}")
    else:
        print("옵티마이저 정보가 없습니다.")

print_optimizer_info()