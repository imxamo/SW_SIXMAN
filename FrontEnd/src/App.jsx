import React, { useState, useEffect } from "react";
import "./App.css";   // ✅ CSS 파일 가져오기

function App() {
  const [image, setImage] = useState(null);
  const [imagePreview, setImagePreview] = useState(null);
  const [result, setResult] = useState("");
  const [uploadedImages, setUploadedImages] = useState([]);
  const [loading, setLoading] = useState(false);
  const [activeTab, setActiveTab] = useState("upload");
  const [sensorData, setSensorData] = useState(null);

  // 업로드된 이미지 목록 가져오기
  useEffect(() => {
    fetchUploadedImages();
  }, []);

  const fetchUploadedImages = async () => {
    try {
      const response = await fetch("/api/uploads");
      const data = await response.json();
      if (data.ok) {
        setUploadedImages(data.uploads);
      }
    } catch (error) {
      console.error("이미지 목록 로딩 실패:", error);
    }
  };

  // 센서 데이터 주기적 fetch
  useEffect(() => {
    const fetchSensor = async () => {
      try {
        const response = await fetch("/api/sensor");
        const data = await response.json();
        if (data.ok) {
          setSensorData(data.data);
        }
      } catch (err) {
        console.error("센서 데이터 불러오기 실패:", err);
      }
    };

    fetchSensor();
    const interval = setInterval(fetchSensor, 5000);
    return () => clearInterval(interval);
  }, []);

  // 파일 선택
  const handleFileChange = (e) => {
    const file = e.target.files[0];
    if (file) {
      setImage(file);
      setImagePreview(URL.createObjectURL(file));
      setResult("");
    }
  };

  // 갤러리에서 이미지 선택
  const handleSelectFromGallery = (imageUrl, filename) => {
    setImagePreview(imageUrl);
    setImage({ url: imageUrl, filename });
    setResult("");
    setActiveTab("upload");
  };

  // 분석 버튼
  const handleAnalyze = async () => {
    if (!image) {
      alert("이미지를 먼저 업로드하거나 선택해주세요!");
      return;
    }

    setLoading(true);
    setResult("");

    try {
      let formData = new FormData();

      if (image instanceof File) {
        formData.append("file", image);
      } else if (image.url) {
        const response = await fetch(image.url);
        const blob = await response.blob();
        formData.append("file", blob, image.filename);
      }

      const response = await fetch("/api/predict", {
        method: "POST",
        body: formData,
      });

      const data = await response.json();

      if (!response.ok || !data.ok) {
        throw new Error(data.error || "서버 오류");
      }

      const pretty = `${data.disease_name} (${data.disease_code}) · ${(
        data.confidence * 100
      ).toFixed(1)}%`;
      setResult(pretty);
    } catch (error) {
      console.error(error);
      setResult("분석 중 오류 발생!");
    } finally {
      setLoading(false);
    }
  };

  // 촬영 트리거
  const handleTriggerCamera = async () => {
    try {
      const response = await fetch("/trigger");
      const data = await response.json();
      if (data.status === "ok") {
        alert("카메라 촬영 요청을 보냈습니다!");
        setTimeout(() => {
          fetchUploadedImages();
        }, 3000);
      }
    } catch (error) {
      console.error("촬영 트리거 실패:", error);
      alert("카메라 연결에 실패했습니다.");
    }
  };

  return (
    <div className="container">
      <div className="layout">
        {/* 왼쪽: AI 분석 */}
        <div className="mainContent">
          <h1 className="title">🌿 상추 질병 AI 분석</h1>

          {/* 탭 */}
          <div className="tabContainer">
            <button
              className={activeTab === "upload" ? "tabActive" : "tab"}
              onClick={() => setActiveTab("upload")}
            >
              이미지 분석
            </button>
            <button
              className={activeTab === "gallery" ? "tabActive" : "tab"}
              onClick={() => setActiveTab("gallery")}
            >
              촬영 갤러리
            </button>
          </div>

          {/* 이미지 분석 탭 */}
          {activeTab === "upload" && (
            <div className="content">
              <div className="uploadBox">
                <label htmlFor="file-upload" className="uploadLabel">
                  📁 이미지 업로드
                </label>
                <input
                  id="file-upload"
                  type="file"
                  accept="image/*"
                  onChange={handleFileChange}
                  className="fileInput"
                />
              </div>

              {imagePreview && (
                <div className="previewContainer">
                  <img src={imagePreview} alt="preview" className="previewImage" />
                </div>
              )}

              <button
                onClick={handleAnalyze}
                className="analyzeButton"
                disabled={loading}
              >
                {loading ? "분석 중..." : "🔍 분석하기"}
              </button>

              {result && (
                <div className="resultBox">
                  <h3 className="resultTitle">분석 결과</h3>
                  <p className="resultText">{result}</p>
                </div>
              )}
            </div>
          )}

          {/* 갤러리 탭 */}
          {activeTab === "gallery" && (
            <div className="content">
              <div className="galleryHeader">
                <button onClick={handleTriggerCamera} className="cameraButton">
                  📷 사진 촬영
                </button>
                <button onClick={fetchUploadedImages} className="refreshButton">
                  🔄 새로고침
                </button>
              </div>

              {uploadedImages.length === 0 ? (
                <p className="emptyText">업로드된 이미지가 없습니다.</p>
              ) : (
                <div className="gallery">
                  {uploadedImages.map((img, index) => (
                    <div key={index} className="galleryItem">
                      <img
                        src={img.url}
                        alt={img.filename}
                        className="galleryImage"
                        onClick={() => handleSelectFromGallery(img.url, img.filename)}
                      />
                      <p className="galleryCaption">{img.timestamp}</p>
                    </div>
                  ))}
                </div>
              )}
            </div>
          )}
        </div>

        {/* 오른쪽: 센서값 */}
        <div className="sensorBox">
          <h3>🌡️ 실시간 센서값</h3>
          {sensorData ? (
            <ul className="sensorList">
              <li>온도: {sensorData.temperature} °C</li>
              <li>습도: {sensorData.humidity} %</li>
              <li>토양 수분: {sensorData.soil_moisture}</li>
              <li>수위: {sensorData.water_level} %</li>
              <li>⏱ {sensorData.timestamp}</li>
            </ul>
          ) : (
            <p>데이터 수신 대기중...</p>
          )}

          {/* 새로고침 버튼 */}
          <button onClick={() => window.location.reload()} className="sensorRefresh">
            🔄 새로고침
          </button>
          
        </div>
      </div>
    </div>
  );
}

export default App;
