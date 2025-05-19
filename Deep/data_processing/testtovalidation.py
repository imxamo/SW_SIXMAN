import os
import json
import random
import shutil
from tqdm import tqdm
import numpy as np


base_dir = 'D:\\data_folders'
train_dir = os.path.join(base_dir, 'train')
validation_dir = os.path.join(base_dir, 'validation')

# 디렉토리 존재 확인 및 생성
train_image_dir = os.path.join(train_dir, 'image')
train_label_dir = os.path.join(train_dir, 'labeling')
validation_image_dir = os.path.join(validation_dir, 'image')
validation_label_dir = os.path.join(validation_dir, 'labeling')

TARGET_DISEASES = ['0', '7', '8']  # 정상(0), 딸기잿빛곰팡이병(7), 딸기흰가루병(8)
TARGET_CROP = '4'  # 딸기

# validation 데이터셋으로 이동할 비율 (train 데이터의 몇 %를 이동할지)
MOVE_RATIO = 0.10  # 10%를 이동

def create_directory(directory):
    """디렉토리가 없으면 생성"""
    if not os.path.exists(directory):
        os.makedirs(directory)
        print(f"디렉토리 생성: {directory}")
    else:
        print(f"디렉토리 이미 존재: {directory}")

def get_image_label_pairs():
    """train 디렉토리에서 이미지와 라벨링 파일 쌍을 찾아서 반환"""
    pairs = []
    
    if not os.path.exists(train_label_dir):
        print(f"라벨링 디렉토리가 존재하지 않습니다: {train_label_dir}")
        return pairs
    
    # 라벨링 파일 순회
    for json_file in os.listdir(train_label_dir):
        if not json_file.endswith('.json'):
            continue
            
        json_path = os.path.join(train_label_dir, json_file)
        
        try:
            with open(json_path, 'r', encoding='utf-8') as f:
                data = json.load(f)
            
            # 이미지 파일명 추출
            img_filename = data['description']['image']
            img_path = os.path.join(train_image_dir, img_filename)
            
            # 작물과 질병 추출
            crop = str(data['annotations']['crop'])
            disease = str(data['annotations']['disease'])
            
            # 딸기이면서 지정된 질병 코드인 경우만 포함
            if crop == TARGET_CROP and disease in TARGET_DISEASES:
                # 이미지 파일이 존재하는지 확인
                if os.path.exists(img_path):
                    # 이미지 파일과 라벨링 파일의 쌍을 저장
                    pairs.append({
                        'image_path': img_path,
                        'label_path': json_path,
                        'image_filename': img_filename,
                        'label_filename': json_file,
                        'crop': crop,
                        'disease': disease
                    })
                else:
                    print(f"이미지 파일이 존재하지 않습니다: {img_path}")
        except Exception as e:
            print(f"JSON 파일 처리 오류 {json_path}: {e}")
    
    return pairs

def move_data_to_validation(random_seed=42):
    """train 데이터의 일부를 validation으로 이동"""
    random.seed(random_seed)
    np.random.seed(random_seed)
    
    # 필요한 디렉토리 확인
    if not os.path.exists(validation_dir):
        create_directory(validation_dir)
    if not os.path.exists(validation_image_dir):
        create_directory(validation_image_dir)
    if not os.path.exists(validation_label_dir):
        create_directory(validation_label_dir)
    
    # 이미지-라벨링 쌍 가져오기
    pairs = get_image_label_pairs()
    print(f"총 이미지-라벨링 쌍: {len(pairs)}개")
    
    if len(pairs) == 0:
        print("이동할 데이터가 없습니다. 프로그램을 종료합니다.")
        return
    
    # 질병별 데이터 분포 확인
    disease_counts = {}
    for pair in pairs:
        disease = pair['disease']
        if disease in disease_counts:
            disease_counts[disease] += 1
        else:
            disease_counts[disease] = 1
    
    print("\n현재 train 질병 분포:")
    for disease, count in disease_counts.items():
        print(f"질병 코드 {disease}: {count}개 ({count/len(pairs)*100:.2f}%)")
    
    # 각 질병별로 stratified sampling
    validation_pairs = []
    train_remain_pairs = []
    
    for disease in disease_counts.keys():
        disease_pairs = [pair for pair in pairs if pair['disease'] == disease]
        random.shuffle(disease_pairs)  # 무작위로 섞기
        
        # 이동할 데이터 크기 계산
        move_size = int(len(disease_pairs) * MOVE_RATIO)
        
        # 분할
        disease_validation_pairs = disease_pairs[:move_size]
        disease_train_remain_pairs = disease_pairs[move_size:]
        
        validation_pairs.extend(disease_validation_pairs)
        train_remain_pairs.extend(disease_train_remain_pairs)
    
    print(f"\n분할 결과:")
    print(f"Train 남은 데이터: {len(train_remain_pairs)}개 ({len(train_remain_pairs)/len(pairs)*100:.2f}%)")
    print(f"Validation으로 이동할 데이터: {len(validation_pairs)}개 ({len(validation_pairs)/len(pairs)*100:.2f}%)")
    
    # 현재 validation 데이터 상태 확인
    validation_files = []
    if os.path.exists(validation_label_dir):
        validation_files = [f for f in os.listdir(validation_label_dir) if f.endswith('.json')]
    
    print(f"\n현재 validation 데이터 수: {len(validation_files)}개")
    print(f"이동 후 예상 validation 데이터 수: {len(validation_files) + len(validation_pairs)}개")
    
    # 사용자 확인 요청
    user_input = input("\n데이터를 이동하시겠습니까? (y/N): ").strip().lower()
    if user_input != 'y':
        print("사용자 취소. 프로그램을 종료합니다.")
        return
    
    # validation 데이터로 이동
    print("\nvalidation 데이터로 파일 이동 중...")
    
    for pair in tqdm(validation_pairs):
        # 이미지 파일 복사
        src_img = pair['image_path']
        dst_img = os.path.join(validation_image_dir, pair['image_filename'])
        
        # 라벨링 파일 복사
        src_label = pair['label_path']
        dst_label = os.path.join(validation_label_dir, pair['label_filename'])
        
        try:
            # 파일 복사
            shutil.copy2(src_img, dst_img)
            shutil.copy2(src_label, dst_label)
            
            # 복사 후 원본 파일 삭제 (train 폴더에서 제거)
            os.remove(src_img)
            os.remove(src_label)
        except Exception as e:
            print(f"파일 이동 중 오류 발생: {e}")
    
    # 이동 후 각 폴더의 파일 수 확인
    train_image_count = len([f for f in os.listdir(train_image_dir) if os.path.isfile(os.path.join(train_image_dir, f))])
    train_label_count = len([f for f in os.listdir(train_label_dir) if f.endswith('.json')])
    
    validation_image_count = len([f for f in os.listdir(validation_image_dir) if os.path.isfile(os.path.join(validation_image_dir, f))])
    validation_label_count = len([f for f in os.listdir(validation_label_dir) if f.endswith('.json')])
    
    print("\n데이터 이동 완료!")
    print(f"Train 이미지: {train_image_count}개, 라벨링: {train_label_count}개")
    print(f"Validation 이미지: {validation_image_count}개, 라벨링: {validation_label_count}개")
    
    # 이동된 데이터 질병 분포 확인
    moved_disease_counts = {}
    for pair in validation_pairs:
        disease = pair['disease']
        if disease in moved_disease_counts:
            moved_disease_counts[disease] += 1
        else:
            moved_disease_counts[disease] = 1
    
    print("\n이동된 데이터 질병 분포:")
    for disease, count in moved_disease_counts.items():
        print(f"질병 코드 {disease}: {count}개 ({count/len(validation_pairs)*100:.2f}%)")

if __name__ == "__main__":
    print("데이터 이동 시작...")
    print(f"기본 디렉토리: {base_dir}")
    print(f"Train 디렉토리: {train_dir}")
    print(f"Validation 디렉토리: {validation_dir}")
    print(f"이동 비율: {MOVE_RATIO * 100}%")
    
    # 데이터 이동 (train에서 validation으로)
    move_data_to_validation()