import torch
import torch.nn as nn
import torch.nn.functional as F
from torch.utils.data import DataLoader, TensorDataset
# tensorflow로 하면 gpu 못써서 pytorch

class DiseaseClassifier(nn.Module):
    def __init__(self, input_shape=(3, 224, 224), num_classes=3): # num_classes : 분류 클래스 3가지지
        super(DiseaseClassifier, self).__init__()
        self.conv1 = nn.Conv2d(in_channels=input_shape[0], out_channels=32, kernel_size=3)
        self.pool = nn.MaxPool2d(kernel_size=2, stride=2)
        self.conv2 = nn.Conv2d(32, 64, 3)
        self.conv3 = nn.Conv2d(64, 128, 3)

        # 계산해서 나온 Flatten 크기를 설정해야 함 (임시로 128 * 26 * 26 넣음 → 실제는 input_shape에 따라 달라짐)
        self.flatten_dim = 128 * 26 * 26
        self.fc1 = nn.Linear(self.flatten_dim, 128)
        self.fc2 = nn.Linear(128, num_classes)

    def forward(self, x):
        x = self.pool(F.relu(self.conv1(x)))   # -> [B, 32, H/2, W/2]
        x = self.pool(F.relu(self.conv2(x)))   # -> [B, 64, H/4, W/4]
        x = self.pool(F.relu(self.conv3(x)))   # -> [B, 128, H/8, W/8]
        
        x = x.view(-1, self.flatten_dim)       # Flatten
        x = F.relu(self.fc1(x))
        x = self.fc2(x)
        return x

def train_model(images, labels, input_shape=(3, 224, 224), num_classes=3, epochs=5, batch_size=32):
    print(f"학습 데이터 수: {len(images)}")

    device = torch.device('cuda' if torch.cuda.is_available() else 'cpu')
    print(f"디바이스: {device}")

    # Numpy → Tensor 변환 및 정규화
    X = torch.tensor(images, dtype=torch.float32).permute(0, 3, 1, 2) / 255.0  # (B, H, W, C) → (B, C, H, W)
    y = torch.tensor(labels, dtype=torch.long)
    
    # 학습 데이터를 묶어서 mini_batch 단위로 묶어줌줌
    dataset = TensorDataset(X, y)
    dataloader = DataLoader(dataset, batch_size=batch_size, shuffle=True)

    model = DiseaseClassifier(input_shape=input_shape, num_classes=num_classes).to(device)
    optimizer = torch.optim.Adam(model.parameters(), lr=0.001)
    loss_fn = nn.CrossEntropyLoss()

    model.train()
    for epoch in range(epochs):
        total_loss = 0
        for xb, yb in dataloader:
            xb, yb = xb.to(device), yb.to(device)
            pred = model(xb)
            loss = loss_fn(pred, yb)

            optimizer.zero_grad()
            loss.backward()
            optimizer.step()

            total_loss += loss.item()
        print(f"Epoch {epoch+1}/{epochs}, Loss: {total_loss:.4f}")

    # 학습된 모델 저장
    torch.save(model.state_dict(), "plant_disease_model.pth")
    print("✅ 모델 저장 완료: plant_disease_model.pth")
