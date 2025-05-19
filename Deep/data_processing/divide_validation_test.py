import os
import json
import random
import shutil
from tqdm import tqdm
import numpy as np

base_dir = 'D:\\data_folders'
train_dir = os.path.join(base_dir, 'train')  # 훈련 데이터 폴더
validation_dir = os.path.join(base_dir, 'validation')  # 검증 데이터 폴더
test_dir = os.path.join(base_dir, 'test')  # 테스트 데이터 폴더 (새로 생성할 폴더)

# 디렉토리 존재 확인 및 생성
validation_image_dir = os.path.join(validation_dir, 'image')
validation_label_dir = os.path.join(validation_dir, 'labeling')
test_image_dir = os.path.join(test_dir, 'image')
test_label_dir = os.path.join(test_dir, 'labeling')


TARGET_DISEASES = ['0','18', '19']  # 토마토잎곰팡이병(18), 토마토황화잎말이바이러스병(19)


CROP_DISEASE_MAPPING = {
    '11': TARGET_DISEASES,  # 토마토: 토마토잎곰팡이병(18), 토마토황화잎말이바이러스병(19)
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

def get_image_label_pairs(image_dir, label_dir):
    """지정된 디렉토리에서 이미지와 라벨링 파일 쌍을 찾아서 반환"""
    pairs = []
    
    if not os.path.exists(label_dir):
        print(f"라벨링 디렉토리가 존재하지 않습니다: {label_dir}")
        return pairs
    
    # 라벨링 파일 순회
    for json_file in os.listdir(label_dir):
        if not json_file.endswith('.json'):
            continue
            
        json_path = os.path.join(label_dir, json_file)
        
        try:
            with open(json_path, 'r', encoding='utf-8') as f:
                data = json.load(f)
            
            # 이미지 파일명 추출
            img_filename = data['description']['image']
            img_path = os.path.join(image_dir, img_filename)
            
            # 작물과 질병 추출
            crop = data['annotations']['crop']
            disease = str(data['annotations']['disease'])
            
            # 유효한 작물-질병 조합인지 확인 (토마토 데이터만 사용)
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

def split_validation_to_test(test_size=2500, random_seed=42):
    """검증 데이터에서 일부를 테스트로 분할하고 이동"""
    random.seed(random_seed)
    np.random.seed(random_seed)
    
    # 필요한 디렉토리 생성
    create_directory(test_dir)
    create_directory(test_image_dir)
    create_directory(test_label_dir)
    
    # 검증 데이터의 이미지-라벨링 쌍 가져오기
    validation_pairs = get_image_label_pairs(validation_image_dir, validation_label_dir)
    print(f"총 검증 데이터 이미지-라벨링 쌍: {len(validation_pairs)}개")
    
    if len(validation_pairs) == 0:
        print("분할할 검증 데이터가 없습니다. 프로그램을 종료합니다.")
        return
    
    # 현재 훈련 데이터 개수 확인 (참고용)
    train_image_count = len([f for f in os.listdir(os.path.join(train_dir, 'image')) if os.path.isfile(os.path.join(train_dir, 'image', f))])
    print(f"현재 훈련 데이터 개수: {train_image_count}개")
    print(f"현재 검증 데이터 개수: {len(validation_pairs)}개")
    
    # 질병별 데이터 분포 확인
    disease_counts = {}
    for pair in validation_pairs:
        disease = pair['disease']
        if disease in disease_counts:
            disease_counts[disease] += 1
        else:
            disease_counts[disease] = 1
    
    print("\n현재 검증 데이터 질병 분포:")
    for disease, count in disease_counts.items():
        disease_name = "토마토잎곰팡이병" if disease == "18" else "토마토황화잎말이바이러스병"
        print(f"질병 코드 {disease} ({disease_name}): {count}개 ({count/len(validation_pairs)*100:.2f}%)")
    
    # 총 데이터 수 계산
    total_data_count = train_image_count + len(validation_pairs)
    print(f"\n총 데이터 수: {total_data_count}개")
    
    # 요청한 테스트 데이터 개수 검증
    if test_size > len(validation_pairs):
        print(f"요청한 테스트 데이터 크기({test_size})가 현재 검증 데이터 수({len(validation_pairs)})보다 큽니다.")
        print(f"검증 데이터의 50%만 테스트로 이동합니다.")
        test_size = len(validation_pairs) // 2
    
    print(f"\n검증 데이터에서 테스트로 이동할 데이터 수: {test_size}개")
    
    # 각 질병별로 stratified sampling
    test_pairs = []
    validation_remain_pairs = []
    
    for disease in disease_counts.keys():
        disease_pairs = [pair for pair in validation_pairs if pair['disease'] == disease]
        random.shuffle(disease_pairs)  # 무작위로 섞기
        
        # 테스트 데이터 크기 계산 (비율에 맞게)
        disease_ratio = len(disease_pairs) / len(validation_pairs)
        disease_test_size = int(test_size * disease_ratio)
        
        # 분할
        disease_test_pairs = disease_pairs[:disease_test_size]
        disease_validation_remain_pairs = disease_pairs[disease_test_size:]
        
        test_pairs.extend(disease_test_pairs)
        validation_remain_pairs.extend(disease_validation_remain_pairs)
    
    # 실제 분할된 개수 조정 (정확한 개수 맞추기)
    if len(test_pairs) < test_size:
        diff = test_size - len(test_pairs)
        additional_pairs = validation_remain_pairs[:diff]
        test_pairs.extend(additional_pairs)
        validation_remain_pairs = validation_remain_pairs[diff:]
    elif len(test_pairs) > test_size:
        excess = len(test_pairs) - test_size
        moved_back = test_pairs[-excess:]
        test_pairs = test_pairs[:-excess]
        validation_remain_pairs.extend(moved_back)
    
    print(f"\n분할 결과:")
    print(f"훈련 데이터: {train_image_count}개 ({train_image_count/total_data_count*100:.2f}%)")
    print(f"검증 데이터 (남은): {len(validation_remain_pairs)}개 ({len(validation_remain_pairs)/total_data_count*100:.2f}%)")
    print(f"테스트 데이터 (이동): {len(test_pairs)}개 ({len(test_pairs)/total_data_count*100:.2f}%)")
    
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
            
            # 복사 후 원본 파일 삭제 (검증 폴더에서 제거)
            os.remove(src_img)
            os.remove(src_label)
        except Exception as e:
            print(f"파일 이동 중 오류 발생: {e}")
    
    # 이동 후 각 폴더의 파일 수 확인
    train_image_count = len([f for f in os.listdir(os.path.join(train_dir, 'image')) if os.path.isfile(os.path.join(train_dir, 'image', f))])
    train_label_count = len([f for f in os.listdir(os.path.join(train_dir, 'labeling')) if f.endswith('.json')])
    
    validation_image_count = len([f for f in os.listdir(validation_image_dir) if os.path.isfile(os.path.join(validation_image_dir, f))])
    validation_label_count = len([f for f in os.listdir(validation_label_dir) if f.endswith('.json')])
    
    test_image_count = len([f for f in os.listdir(test_image_dir) if os.path.isfile(os.path.join(test_image_dir, f))])
    test_label_count = len([f for f in os.listdir(test_label_dir) if f.endswith('.json')])
    
    print("\n데이터 분할 완료!")
    print(f"훈련 이미지: {train_image_count}개, 라벨링: {train_label_count}개")
    print(f"검증 이미지: {validation_image_count}개, 라벨링: {validation_label_count}개")
    print(f"테스트 이미지: {test_image_count}개, 라벨링: {test_label_count}개")
    
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
        disease_name = "토마토잎곰팡이병" if disease == "18" else "토마토황화잎말이바이러스병"
        print(f"질병 코드 {disease} ({disease_name}): {count}개 ({count/len(test_pairs)*100:.2f}%)")
    
    # 남은 검증 데이터 질병 분포 확인
    validation_disease_counts = {}
    for pair in validation_remain_pairs:
        disease = pair['disease']
        if disease in validation_disease_counts:
            validation_disease_counts[disease] += 1
        else:
            validation_disease_counts[disease] = 1
    
    print("\n남은 검증 데이터 질병 분포:")
    for disease, count in validation_disease_counts.items():
        disease_name = "토마토잎곰팡이병" if disease == "18" else "토마토황화잎말이바이러스병"
        print(f"질병 코드 {disease} ({disease_name}): {count}개 ({count/len(validation_remain_pairs)*100:.2f}%)")

if __name__ == "__main__":
    print("검증 데이터에서 테스트 데이터로 분할 시작...")
    print(f"기본 디렉토리: {base_dir}")
    print(f"훈련 디렉토리: {train_dir}")
    print(f"검증 디렉토리: {validation_dir}")
    print(f"테스트 디렉토리: {test_dir}")
    
    split_validation_to_test(test_size=2500)