import json
import os
import yaml
from glob import glob
import shutil

# 1. JSON을 YOLO 형식의 텍스트 파일로 변환하는 함수
def convert_json_to_yolo(json_file, output_dir):
    with open(json_file, 'r', encoding='utf-8') as f:
        data = json.load(f)
    
    img_width = data['description']['width']
    img_height = data['description']['height']
    img_name = data['description']['image']
    base_name = os.path.splitext(img_name)[0]
    
    # crop이 2면 고추(1), 그 외는 상추(5)로 가정
    crop_type = data['annotations']['crop']
    disease = data['annotations']['disease']
    if crop_type == 5:  # 상추
        if disease == 0:  # 건강한 상추
            class_id = 0
        else:  # 질병에 걸린 상추
            class_id = 1
    elif crop_type == 2:  # 고추
        if disease == 0:  # 건강한 고추
            class_id = 2
        else:  # 질병에 걸린 고추
            class_id = 3
    else:
        print(f"경고: 알 수 없는 작물 유형 - {crop_type}")
        return None, None  # 처리할 수 없는 경우 건너뛰기
    
    # 바운딩 박스 정보
    points = data['annotations']['points']
    
    # YOLO 형식의 레이블 파일 생성
    os.makedirs(output_dir, exist_ok=True)
    txt_path = os.path.join(output_dir, base_name + '.txt')
    
    with open(txt_path, 'w') as f:
        for box in points:
            # 박스 좌표
            xtl = box['xtl']
            ytl = box['ytl']
            xbr = box['xbr']
            ybr = box['ybr']
            
            # YOLO 형식으로 변환 (중심점 x, 중심점 y, 너비, 높이) - 0~1 사이 값으로 정규화
            x_center = ((xtl + xbr) / 2) / img_width
            y_center = ((ytl + ybr) / 2) / img_height
            width = (xbr - xtl) / img_width
            height = (ybr - ytl) / img_height
            
            # 클래스 ID와 좌표 저장
            f.write(f"{class_id} {x_center} {y_center} {width} {height}\n")
    
    return base_name, img_name

# 2. 전체 데이터셋 처리 함수
def process_dataset(json_dir, image_dir, output_dir):
    # 출력 디렉토리 구조 생성
    os.makedirs(os.path.join(output_dir, 'images', 'train'), exist_ok=True)
    os.makedirs(os.path.join(output_dir, 'images', 'test'), exist_ok=True)
    os.makedirs(os.path.join(output_dir, 'labels', 'train'), exist_ok=True)
    os.makedirs(os.path.join(output_dir, 'labels', 'test'), exist_ok=True)
    
    # JSON 파일 목록 가져오기
    json_files = glob(os.path.join(json_dir, '*.json'))
    print(f"총 {len(json_files)}개의 JSON 파일을 찾았습니다.")
    
    # 학습/검증 데이터 분할 (80/20)
    train_count = int(len(json_files) * 0.8)
    
    # 파일 변환 및 복사
    for i, json_file in enumerate(json_files):
        print(f"처리 중: {i+1}/{len(json_files)} - {json_file}")
        
        # JSON을 YOLO 형식으로 변환
        destination = 'train' if i < train_count else 'test'
        try:
            base_name, img_name = convert_json_to_yolo(
                json_file, 
                os.path.join(output_dir, 'labels', destination)
            )
            
            # 이미지 파일 복사
            src_img = os.path.join(image_dir, img_name)
            if os.path.exists(src_img):
                dst_img = os.path.join(output_dir, 'images', destination, img_name)
                shutil.copy(src_img, dst_img)
            else:
                print(f"경고: 이미지를 찾을 수 없습니다 - {src_img}")
        except Exception as e:
            print(f"오류: {json_file} 처리 중 문제 발생 - {str(e)}")
    
    # 3. data.yaml 파일 생성
    yaml_path = os.path.join(output_dir, 'data.yaml')
    yaml_content = {
        'path': os.path.abspath(output_dir),
        'train': 'images/train',
        'val': 'images/val',
        'nc': 4,  # 클래스 수 (4가지)
        'names': [
            'healthy_lettuce',   # 0: 건강한 상추
            'diseased_lettuce',  # 1: 질병에 걸린 상추
            'healthy_pepper',    # 2: 건강한 고추
            'diseased_pepper'    # 3: 질병에 걸린 고추
        ]
    }
    
    with open(yaml_path, 'w') as f:
        yaml.dump(yaml_content, f, sort_keys=False)
    
    print(f"\ndata.yaml 파일이 생성되었습니다: {yaml_path}")
    print("변환 완료! 이제 YOLOv8로 모델을 훈련할 수 있습니다.")

# 메인 실행 코드
if __name__ == "__main__":
    # 경로 설정 (실제 경로로 변경 필요)
    json_directory = "C:/python_project/상추/고추라벨링"    
    image_directory = "C:/python_project/상추/고추이미지"  
    output_directory = "C:/python_project/상추/결과"       
    
    # 함수 실행
    process_dataset(json_directory, image_directory, output_directory)