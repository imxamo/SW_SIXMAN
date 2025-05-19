데이터셋은 AI-HUB 에서 다운받아서 사용
https://aihub.or.kr/aihubdata/data/view.do?currMenu=&topMenu=&aihubDataSe=data&dataSetSn=153

![image](https://github.com/user-attachments/assets/042c7156-6b89-43a7-98dd-9810cf6a9b6f)





![image](https://github.com/user-attachments/assets/ee075b31-70d1-4edb-9746-74c5677d35d1)
![image](https://github.com/user-attachments/assets/3c46ea06-4784-414e-91e2-f0075777e08c)



라벨링 데이터의 crops의 클래스 값으로 식물을 구분 + disease값으로 질병을 구분하여 학습한다




12가지의 작물중 질병이 2개가 있는 작물만 진행 예정 (가지 / 고추 / 단호박 / 딸기 / 상추 / 수박 / 참외 / 토마토)
현재 딸기 / 상추 / 토마토 완료 (고추 좀 손봐야함)
남은 목록 : 가지 / 단호박 / 수박 / 참외




각 폴더들 설명
data_processing >> 데이터셋 특성상 test 데이터가 포함되어있지 않음(train 데이터와 validation 데이터만 포함하고있음) 
                   그래서 데이터 분할을 하는 코드 또는 특정 식물과 맞지않은 데이터가 들어있는 경우가 있어 해당 데이터를 찾아내는등
                   데이터 전처리 하는 코드 포함



각종 식물 폴더 
해당 식물의 ai 모델 코드와 학습 결과 또는 진행중인 내역이 포함되어있음




![image](https://github.com/user-attachments/assets/43a744cf-5e7b-4d7b-a537-ea0ba3a46cc0)

모델의 정보를 담은 pth파일이 깃허브에 올라가지 않아서.. 따로 올릴방법 찾아보는중..
