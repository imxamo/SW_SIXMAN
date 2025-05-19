import os
import json
import random
import shutil
from tqdm import tqdm
import numpy as np

base_dir = 'D:\\data_folders'
train_dir = os.path.join(base_dir, 'train') 
test_dir = os.path.join(base_dir, 'test')

# 디렉토리 존재 확인 및 생성
train_image_dir = os.path.join(train_dir, 'image')
train_label_dir = os.path.join(train_dir, 'labeling')
test_image_dir = os.path.join(test_dir, 'image')
test_label_dir = os.path.join(test_dir, 'labeling')


TARGET_DISEASES = ['0', '7', '8']  # 정상(0), 딸기잿빛곰팡이병(7), 딸기흰가루병(8)

# 작물-질병 매핑 딕셔너리
CROP_DISEASE_MAPPING = {
    '4': TARGET_DISEASES,  # 딸기: 정상(0), 딸기잿빛곰팡이병(7), 딸기흰가루병(8)
}

# 유효한 작물-질병 조합인지 확인하는 함수
def is_valid_crop_disease(crop, disease):
    """작물-질병 조합이 유효한지 확인"""
    crop = str(crop)
    disease = str(disease)
    return crop in CROP_DISEASE_MAPPING and disease in CROP_DISEASE_MAPPING[crop]

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
            crop = data['annotations']['crop']
            disease = str(data['annotations']['disease'])
            
            
            if is_valid_crop_disease(crop, disease):
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

def split_and_move_data(test_ratio=0.2, random_seed=42):
    """train 데이터에서 일부를 test로 분할하고 이동"""
    random.seed(random_seed)
    np.random.seed(random_seed)
    
    # 필요한 디렉토리 생성
    create_directory(test_dir)
    create_directory(test_image_dir)
    create_directory(test_label_dir)
    
    # 이미지-라벨링 쌍 가져오기
    pairs = get_image_label_pairs()
    print(f"총 이미지-라벨링 쌍: {len(pairs)}개")
    
    if len(pairs) == 0:
        print("분할할 데이터가 없습니다. 프로그램을 종료합니다.")
        return
    
    # 질병별 데이터 분포 확인
    disease_counts = {}
    for pair in pairs:
        disease = pair['disease']
        if disease in disease_counts:
            disease_counts[disease] += 1
        else:
            disease_counts[disease] = 1
    
    print("\n현재 질병 분포:")
    for disease, count in disease_counts.items():
        print(f"질병 코드 {disease}: {count}개 ({count/len(pairs)*100:.2f}%)")
    
    # 각 질병별로 stratified sampling
    test_pairs = []
    train_remain_pairs = []
    
    for disease in disease_counts.keys():
        disease_pairs = [pair for pair in pairs if pair['disease'] == disease]
        random.shuffle(disease_pairs)  # 무작위로 섞기
        
        # 테스트 데이터 크기 계산
        test_size = int(len(disease_pairs) * test_ratio)
        
        # 분할
        disease_test_pairs = disease_pairs[:test_size]
        disease_train_remain_pairs = disease_pairs[test_size:]
        
        test_pairs.extend(disease_test_pairs)
        train_remain_pairs.extend(disease_train_remain_pairs)
    
    print(f"\n분할 결과:")
    print(f"Train 남은 데이터: {len(train_remain_pairs)}개 ({len(train_remain_pairs)/len(pairs)*100:.2f}%)")
    print(f"Test 데이터: {len(test_pairs)}개 ({len(test_pairs)/len(pairs)*100:.2f}%)")
    
    # 테스트 데이터 이동
    print("\n테스트 데이터 파일 이동 중...")
    
    for pair in tqdm(test_pairs):
        # 이미지 파일 복사
        src_img = pair['image_path']
        dst_img = os.path.join(test_image_dir, pair['image_filename'])
        
        # 라벨링 파일 복사
        src_label = pair['label_path']
        dst_label = os.path.join(test_label_dir, pair['label_filename'])
        
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
    
    test_image_count = len([f for f in os.listdir(test_image_dir) if os.path.isfile(os.path.join(test_image_dir, f))])
    test_label_count = len([f for f in os.listdir(test_label_dir) if f.endswith('.json')])
    
    print("\n데이터 분할 완료!")
    print(f"Train 이미지: {train_image_count}개, 라벨링: {train_label_count}개")
    print(f"Test 이미지: {test_image_count}개, 라벨링: {test_label_count}개")
    
    # 테스트 데이터 질병 분포 확인
    test_disease_counts = {}
    for pair in test_pairs:
        disease = pair['disease']
        if disease in test_disease_counts:
            test_disease_counts[disease] += 1
        else:
            test_disease_counts[disease] = 1
    
    print("\n테스트 데이터 질병 분포:")
    for disease, count in test_disease_counts.items():
        print(f"질병 코드 {disease}: {count}개 ({count/len(test_pairs)*100:.2f}%)")

if __name__ == "__main__":
    print("데이터 분할 시작...")
    print(f"기본 디렉토리: {base_dir}")
    print(f"Train 디렉토리: {train_dir}")
    print(f"Test 디렉토리: {test_dir}")
    
    # 데이터 분할
    split_and_move_data(test_ratio=0.2)