import os
import json
from collections import Counter
import pandas as pd
from tqdm import tqdm

base_dir = 'D:\\data_folders'
data_folders = ['train', 'validation', 'test'] 


TARGET_CROP = '11'  # 딸기
TARGET_DISEASES = ['18', '19', '0']  # 정상(0), 딸기잿빛곰팡이병(7), 딸기흰가루병(8)

def analyze_dataset(folder_name):
    """
    데이터셋 분석 함수 - 타겟이 아닌 데이터 식별
    folder_name: 분석할 폴더 이름 (train, validation, test 등)
    """
    print(f"\n===== {folder_name} 데이터셋 분석 중 =====")
    
    # 경로 설정
    folder_path = os.path.join(base_dir, folder_name)
    label_dir = os.path.join(folder_path, 'labeling')
    
    # 디렉토리 존재 확인
    if not os.path.exists(folder_path):
        print(f"경로가 존재하지 않습니다: {folder_path}")
        return None
        
    if not os.path.exists(label_dir):
        print(f"라벨링 디렉토리가 존재하지 않습니다: {label_dir}")
        return None
    
    # 결과를 저장할 리스트
    non_target_files = []
    all_crop_disease_pairs = []
    
    # 라벨링 파일 수 확인
    label_files = [f for f in os.listdir(label_dir) if f.endswith('.json')]
    print(f"라벨링 파일 수: {len(label_files)}")
    
    # JSON 파일 탐색
    print("라벨링 파일 분석 중...")
    for json_file in tqdm(label_files):
        json_path = os.path.join(label_dir, json_file)
        
        try:
            with open(json_path, 'r', encoding='utf-8') as f:
                data = json.load(f)
            
            # 이미지 파일명 추출
            img_filename = data['description']['image']
            
            # 작물과 질병 추출
            crop = str(data['annotations']['crop'])
            disease = str(data['annotations']['disease'])
            
            # 작물-질병 쌍 저장
            all_crop_disease_pairs.append((crop, disease))
            
            # 타겟이 아닌 경우 (딸기가 아니거나, 딸기인데 지정 질병이 아닌 경우)
            if crop != TARGET_CROP or (crop == TARGET_CROP and disease not in TARGET_DISEASES):
                non_target_files.append({
                    'label_file': json_file,
                    'image_file': img_filename,
                    'crop': crop,
                    'disease': disease
                })
                
        except Exception as e:
            print(f"JSON 파일 처리 오류 {json_path}: {e}")
    
    # 결과 요약
    print(f"\n타겟이 아닌 파일 수: {len(non_target_files)} / {len(label_files)} ({len(non_target_files)/len(label_files)*100:.2f}%)")
    
    # 작물-질병 분포 분석
    crop_disease_counter = Counter(all_crop_disease_pairs)
    print("\n모든 작물-질병 조합 분포:")
    
    # 정렬된 결과를 DataFrame으로 변환하여 표시
    crop_disease_df = pd.DataFrame([
        {'작물 코드': crop, '질병 코드': disease, '개수': count, '비율(%)': count/len(all_crop_disease_pairs)*100}
        for (crop, disease), count in sorted(crop_disease_counter.items())
    ])
    
    if not crop_disease_df.empty:
        print(crop_disease_df.to_string(index=False))
    else:
        print("데이터가 없습니다.")
    
    return non_target_files, crop_disease_df

# 타겟이 아닌 파일 목록을 CSV로 저장하는 함수
def save_non_target_files(non_target_files, folder_name):
    """타겟이 아닌 파일 목록을 CSV로 저장"""
    if non_target_files and len(non_target_files) > 0:
        df = pd.DataFrame(non_target_files)
        output_file = f"{folder_name}_non_target_files.csv"
        df.to_csv(output_file, index=False, encoding='utf-8-sig')
        print(f"타겟이 아닌 파일 목록이 {output_file}에 저장되었습니다.")
    else:
        print(f"{folder_name}에 타겟이 아닌 파일이 없습니다.")

# 메인 실행 코드
def main():
    print("데이터 분석 시작...")
    print(f"기본 디렉토리: {base_dir}")
    print(f"타겟 작물: {TARGET_CROP} (딸기)")
    print(f"타겟 질병: {TARGET_DISEASES} (정상, 딸기잿빛곰팡이병, 딸기흰가루병)")
    
    # 각 폴더 분석
    all_results = {}
    all_crop_disease_dfs = []
    
    for folder in data_folders:
        results, crop_disease_df = analyze_dataset(folder)
        
        if results is not None:
            all_results[folder] = results
            all_crop_disease_dfs.append((folder, crop_disease_df))
            
            # 타겟이 아닌 파일 목록 저장
            save_non_target_files(results, folder)
    
    # 전체 데이터셋 요약
    print("\n===== 전체 데이터셋 요약 =====")
    total_files = sum(len(results) for results in all_results.values())
    print(f"타겟이 아닌 총 파일 수: {total_files}")
    
    for folder, count in [(folder, len(results)) for folder, results in all_results.items()]:
        print(f"{folder}: {count}개")
    
    # 타겟 데이터 통계 계산
    print("\n===== 타겟 데이터 통계 =====")
    for folder, crop_disease_df in all_crop_disease_dfs:
        if not crop_disease_df.empty:
            # 딸기 데이터만 필터링
            strawberry_data = crop_disease_df[crop_disease_df['작물 코드'] == TARGET_CROP]
            
            # 타겟 질병 데이터만 필터링
            target_data = strawberry_data[strawberry_data['질병 코드'].isin(TARGET_DISEASES)]
            
            print(f"\n{folder} 타겟 데이터:")
            if not target_data.empty:
                print(target_data.to_string(index=False))
                total = target_data['개수'].sum()
                print(f"타겟 데이터 총 개수: {total}")
            else:
                print(f"{folder}에 타겟 데이터가 없습니다.")

if __name__ == "__main__":
    main()