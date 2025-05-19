import os
import json  
import pandas as pd
from tqdm import tqdm


base_dir = 'D:\\data_folders'
data_folders = ['train', 'validation', 'test'] 

def find_orphaned_images(folder_name):
    """
    라벨링 파일이 없는 이미지 파일을 찾는 함수
    folder_name: 분석할 폴더 이름 (train, validation, test 등)
    """
    print(f"\n===== {folder_name} 폴더에서 라벨링 없는 이미지 찾는 중 =====")
    
    # 경로 설정
    folder_path = os.path.join(base_dir, folder_name)
    image_dir = os.path.join(folder_path, 'image')
    label_dir = os.path.join(folder_path, 'labeling')
    
    # 디렉토리 존재 확인
    if not os.path.exists(folder_path):
        print(f"경로가 존재하지 않습니다: {folder_path}")
        return None
        
    if not os.path.exists(image_dir):
        print(f"이미지 디렉토리가 존재하지 않습니다: {image_dir}")
        return None
        
    if not os.path.exists(label_dir):
        print(f"라벨링 디렉토리가 존재하지 않습니다: {label_dir}")
        return None
    
    # 이미지 파일과 라벨링 파일 목록 가져오기
    image_files = [f for f in os.listdir(image_dir) if f.lower().endswith(('.png', '.jpg', '.jpeg', '.bmp', '.tif', '.tiff'))]
    label_files = [f for f in os.listdir(label_dir) if f.endswith('.json')]
    
    print(f"이미지 파일 수: {len(image_files)}")
    print(f"라벨링 파일 수: {len(label_files)}")
    
    # 라벨링 파일에서 이미지 파일명 추출
    labeled_images = set()
    
    print("라벨링 파일에서 이미지 파일명 추출 중...")
    for json_file in tqdm(label_files):
        # 라벨 파일명에서 이미지 파일명 추출
        json_path = os.path.join(label_dir, json_file)
        try:
            with open(json_path, 'r', encoding='utf-8') as f:
                data = json.load(f)
            
            # 이미지 파일명 추출
            img_filename = data['description']['image']
            labeled_images.add(img_filename)
        except Exception as e:
            print(f"JSON 파일 처리 오류 {json_path}: {e}")
    
    # 라벨링이 없는 이미지 파일 찾기
    orphaned_images = []
    
    print("라벨링 없는 이미지 찾는 중...")
    for img_file in tqdm(image_files):
        if img_file not in labeled_images:
            orphaned_images.append({
                'folder': folder_name,
                'image_file': img_file,
                'image_path': os.path.join(image_dir, img_file)
            })
    
    # 결과 요약
    print(f"\n라벨링 없는 이미지 파일 수: {len(orphaned_images)} / {len(image_files)} ({len(orphaned_images)/len(image_files)*100:.2f}% 비율)")
    
    return orphaned_images

def find_images_without_labels(folder_name):
    """
    이미지 파일을 참조하지 않는 라벨링 파일을 찾는 함수
    folder_name: 분석할 폴더 이름 (train, validation, test 등)
    """
    print(f"\n===== {folder_name} 폴더에서 이미지가 없는 라벨링 찾는 중 =====")
    
    # 경로 설정
    folder_path = os.path.join(base_dir, folder_name)
    image_dir = os.path.join(folder_path, 'image')
    label_dir = os.path.join(folder_path, 'labeling')
    
    # 디렉토리 존재 확인
    if not os.path.exists(folder_path):
        print(f"경로가 존재하지 않습니다: {folder_path}")
        return None
        
    if not os.path.exists(image_dir):
        print(f"이미지 디렉토리가 존재하지 않습니다: {image_dir}")
        return None
        
    if not os.path.exists(label_dir):
        print(f"라벨링 디렉토리가 존재하지 않습니다: {label_dir}")
        return None
    
    # 이미지 파일 목록
    image_files = set([f for f in os.listdir(image_dir) if f.lower().endswith(('.png', '.jpg', '.jpeg', '.bmp', '.tif', '.tiff'))])
    label_files = [f for f in os.listdir(label_dir) if f.endswith('.json')]
    
    print(f"이미지 파일 수: {len(image_files)}")
    print(f"라벨링 파일 수: {len(label_files)}")
    
    # 이미지가 없는 라벨링 파일 찾기
    orphaned_labels = []
    
    print("이미지가 없는 라벨링 찾는 중...")
    for json_file in tqdm(label_files):
        json_path = os.path.join(label_dir, json_file)
        try:
            with open(json_path, 'r', encoding='utf-8') as f:
                data = json.load(f)
            
            # 이미지 파일명 추출
            img_filename = data['description']['image']
            
            # 이미지 파일이 존재하는지 확인
            if img_filename not in image_files:
                orphaned_labels.append({
                    'folder': folder_name,
                    'label_file': json_file,
                    'referenced_image': img_filename,
                    'label_path': os.path.join(label_dir, json_file)
                })
        except Exception as e:
            print(f"JSON 파일 처리 오류 {json_path}: {e}")
    
    # 결과 요약
    print(f"\n이미지 없는 라벨링 파일 수: {len(orphaned_labels)} / {len(label_files)} ({len(orphaned_labels)/len(label_files)*100:.2f}% 비율)")
    
    return orphaned_labels

def save_orphaned_files_to_csv(orphaned_files, filename):
    """발견된 고아 파일들을 CSV로 저장"""
    if orphaned_files and len(orphaned_files) > 0:
        df = pd.DataFrame(orphaned_files)
        df.to_csv(filename, index=False, encoding='utf-8-sig')
        print(f"파일 목록이 {filename}에 저장되었습니다.")
    else:
        print(f"저장할 파일이 없습니다.")

def delete_orphaned_images(orphaned_images, prompt=True):
    """라벨링이 없는 이미지 파일을 삭제하는 함수"""
    if not orphaned_images or len(orphaned_images) == 0:
        print("삭제할 이미지가 없습니다.")
        return
    
    if prompt:
        confirm = input(f"총 {len(orphaned_images)}개의 라벨링 없는 이미지를 삭제하시겠습니까? (y/n): ")
        if confirm.lower() != 'y':
            print("삭제 취소됨")
            return
    
    deleted_count = 0
    failed_count = 0
    
    print("라벨링 없는 이미지 삭제 중...")
    for item in tqdm(orphaned_images):
        image_path = item['image_path']
        try:
            os.remove(image_path)
            deleted_count += 1
        except Exception as e:
            print(f"파일 삭제 오류 {image_path}: {e}")
            failed_count += 1
    
    print(f"삭제 완료: {deleted_count}개 삭제됨, {failed_count}개 실패")

def main():
    print("데이터셋 분석 시작...")
    print(f"기본 디렉토리: {base_dir}")
    
    all_orphaned_images = []
    all_orphaned_labels = []
    
    for folder in data_folders:
        # 라벨링 없는 이미지 찾기
        orphaned_images = find_orphaned_images(folder)
        if orphaned_images:
            all_orphaned_images.extend(orphaned_images)
            
        # 이미지 없는 라벨링 찾기
        orphaned_labels = find_images_without_labels(folder)
        if orphaned_labels:
            all_orphaned_labels.extend(orphaned_labels)
    
    # 결과 저장
    save_orphaned_files_to_csv(all_orphaned_images, "orphaned_images.csv")
    save_orphaned_files_to_csv(all_orphaned_labels, "orphaned_labels.csv")
    
    # 전체 요약
    print("\n===== 전체 데이터셋 요약 =====")
    print(f"라벨링 없는 이미지 총 개수: {len(all_orphaned_images)}개")
    print(f"이미지 없는 라벨링 총 개수: {len(all_orphaned_labels)}개")
    
    for folder in data_folders:
        folder_images = [item for item in all_orphaned_images if item['folder'] == folder]
        folder_labels = [item for item in all_orphaned_labels if item['folder'] == folder]
        print(f"{folder}: 라벨링 없는 이미지 {len(folder_images)}개, 이미지 없는 라벨링 {len(folder_labels)}개")
    
    # 라벨링 없는 이미지 삭제 옵션
    if len(all_orphaned_images) > 0:
        delete_option = input("\n라벨링 없는 이미지를 삭제하시겠습니까? (y/n): ")
        if delete_option.lower() == 'y':
            delete_orphaned_images(all_orphaned_images, prompt=False)

if __name__ == "__main__":
    main()