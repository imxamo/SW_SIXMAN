import torch

# 기존 모델과 필요한 정보들
best_model_path = 'best_pepper_disease_model.pth'
model_state_dict = torch.load(best_model_path)

# 질병 클래스 정보 (원본 코드에서 가져온 정보)
TARGET_DISEASES = ['0', '3', '4']  # 정상(0), 고추마일드모틀바이러스병(3), 고추점무늬병(4)
DISEASE_NAMES = {
    '0': '정상',
    '3': '고추마일드모틀바이러스병',
    '4': '고추점무늬병'
}
disease_to_idx = {disease: idx for idx, disease in enumerate(TARGET_DISEASES)}

# 전체 모델 정보 생성 및 저장
full_model_info = {
    'model_state_dict': model_state_dict,
    'disease_to_idx': disease_to_idx,
    'class_names': TARGET_DISEASES,
    'disease_names': DISEASE_NAMES
}

# 저장
torch.save(full_model_info, 'pepper_disease_model_full.pth')
print("Full model saved successfully!")