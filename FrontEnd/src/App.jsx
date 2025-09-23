// App.jsx
import React, { useState } from "react";

function App() {
  const [image, setImage] = useState(null);
  const [result, setResult] = useState("");

  // 파일 선택
  const handleFileChange = (e) => {
    setImage(e.target.files[0]);
    setResult(""); // 새로운 파일 올리면 결과 초기화
  };

  // 분석 버튼 클릭
  const handleAnalyze = async () => {
    if (!image) {
      alert("이미지를 먼저 업로드해주세요!");
      return;
    }

    // 예시: FormData로 이미지 전송 (실제 API 주소로 바꿔야 함)
    const formData = new FormData();
    formData.append("file", image);

    try {
      const response = await fetch("/api/predict", {
        method: "POST",
        body: formData,
      });
      const data = await response.json();

      if (!response.ok || !data.ok) {
      throw new Error(data.error || "서버 오류");
    }

      setResult(data.result); // API 결과 표시
    } catch (error) {
      console.error(error);
      setResult("분석 중 오류 발생!");
    }
  };

  return (
    <div style={styles.container}>
      <h1>AI 이미지 분석</h1>
      <input type="file" accept="image/*" onChange={handleFileChange} />
      {image && (
        <div style={{ margin: "10px 0" }}>
          <img
            src={URL.createObjectURL(image)}
            alt="preview"
            style={{ maxWidth: "300px", maxHeight: "300px" }}
          />
        </div>
      )}
      <button onClick={handleAnalyze} style={styles.button}>
        분석하기
      </button>
      {result && (
        <div style={styles.resultBox}>
          <h3>분석 결과</h3>
          <p>{result}</p>
        </div>
      )}
    </div>
  );
}

const styles = {
  container: {
    textAlign: "center",
    padding: "50px",
    fontFamily: "Arial, sans-serif",
  },
  button: {
    padding: "10px 20px",
    marginTop: "10px",
    fontSize: "16px",
    cursor: "pointer",
  },
  resultBox: {
    marginTop: "20px",
    padding: "15px",
    border: "1px solid #ccc",
    display: "inline-block",
    textAlign: "left",
    maxWidth: "400px",
  },
};

export default App;
