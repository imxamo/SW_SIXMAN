import tensorflow as tf
import numpy as np

def train_model(image_arrays, label_arrays):
    print(f"학습 데이터 수: {len(image_arrays)}")

    X = np.array(image_arrays) / 255.0  # 정규화
    y = np.array(label_arrays)

    model = tf.keras.Sequential([
        tf.keras.layers.Flatten(input_shape=X.shape[1:]),
        tf.keras.layers.Dense(128, activation='relu'),
        tf.keras.layers.Dense(len(set(y)), activation='softmax')
    ])

    model.compile(optimizer='adam',
                  loss='sparse_categorical_crossentropy',
                  metrics=['accuracy'])

    model.fit(X, y, epochs=5)
    model.save("lettuce_model.h5")
