import os
import json
import torch
import torch.nn as nn
import torch.optim as optim
import numpy as np

from PIL import Image
from sklearn.metrics import classification_report, confusion_matrix
from torch.utils.data import Dataset, DataLoader
from torchvision import models, transforms
from torch.optim.lr_scheduler import ReduceLROnPlateau
import matplotlib.pyplot as plt

device = torch.device("cuda:0" if torch.cuda.is_available() else "cpu")

# 상추 관련 클래스만 사용
TARGET_DISEASES = ['0', '9', '10']
disease_to_idx = {d: i for i, d in enumerate(TARGET_DISEASES)}
idx_to_disease = {i: d for d, i in disease_to_idx.items()}

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

class LettuceDataset(Dataset):
    def __init__(self, image_list, label_list, transform):
        self.image_list = image_list
        self.label_list = [disease_to_idx[str(label)] for label in label_list]
        self.transform = transform

    def __len__(self):
        return len(self.image_list)

    def __getitem__(self, idx):
        image = self.image_list[idx]
        if isinstance(image, np.ndarray):
            image = Image.fromarray(image).convert("RGB")
        label = self.label_list[idx]
        if self.transform:
            image = self.transform(image)
        return image, label

def build_model(num_classes):
    model = models.efficientnet_b0(weights='IMAGENET1K_V1')
    num_ftrs = model.classifier[1].in_features
    model.classifier = nn.Sequential(
        nn.Dropout(0.3),
        nn.Linear(num_ftrs, num_classes)
    )
    return model

def train_model(model, optimizer, scheduler, images, labels, epochs=1, batch_size=32):
    dataset = LettuceDataset(images, labels, transform=train_transform)
    dataloader = DataLoader(dataset, batch_size=batch_size, shuffle=True)

    criterion = nn.CrossEntropyLoss()
    print(f"🔄 이번 배치 학습 데이터 수: {len(dataset)}")

    for epoch in range(epochs):
        model.train()
        running_loss = 0.0
        corrects = 0

        for inputs, labels in dataloader:
            inputs, labels = inputs.to(device), labels.to(device)

            optimizer.zero_grad()
            outputs = model(inputs)
            loss = criterion(outputs, labels)
            loss.backward()
            optimizer.step()

            running_loss += loss.item() * inputs.size(0)
            corrects += torch.sum(torch.argmax(outputs, 1) == labels)

        epoch_loss = running_loss / len(dataset)
        epoch_acc = corrects.double() / len(dataset)
        scheduler.step(epoch_loss)

        print(f"🧪 Epoch {epoch+1}: Loss {epoch_loss:.4f}, Acc {epoch_acc:.4f}")

    torch.save(model.state_dict(), "plant_disease_model.pth")
    print("✅ 모델 저장 완료: plant_disease_model.pth")