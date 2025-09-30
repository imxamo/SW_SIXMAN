import React, { useState, useEffect } from "react";

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

  // 센서 데이터 fetch
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
      const response = await fetch("/trigger/cam");
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

  // 센서 새로고침
  const handleTriggerSensor = async () => {
    try {
      const response = await fetch("/trigger/esp32");
      const data = await response.json();
      if (data.status === "ok") {
        alert("센서 새 데이터 요청을 보냈습니다!");
        setTimeout(() => {
          fetch("/api/sensor")
            .then((res) => res.json())
            .then((data) => {
              if (data.ok) setSensorData(data.data);
            });
        }, 3000);
      }
    } catch (error) {
      console.error("센서 트리거 실패:", error);
      alert("센서 연결에 실패했습니다.");
    }
  }; // ← 이 중괄호가 빠져있었습니다!

  return (
    <div className="container" style={{ fontFamily: 'Arial, sans-serif', maxWidth: '1400px', margin: '0 auto', padding: '20px' }}>
      <div className="layout" style={{ display: 'flex', gap: '20px' }}>
        {/* 왼쪽: AI 분석 */}
        <div className="mainContent" style={{ flex: 1, backgroundColor: 'white', borderRadius: '12px', padding: '24px', boxShadow: '0 2px 8px rgba(0,0,0,0.1)' }}>
          <h1 style={{ fontSize: '28px', marginBottom: '24px', color: '#2d3748' }}>🌿 상추 질병 AI 분석</h1>

          {/* 탭 */}
          <div style={{ display: 'flex', gap: '8px', marginBottom: '20px', borderBottom: '2px solid #e2e8f0' }}>
            <button
              style={{
                padding: '12px 24px',
                border: 'none',
                backgroundColor: activeTab === "upload" ? '#48bb78' : 'transparent',
                color: activeTab === "upload" ? 'white' : '#718096',
                borderRadius: '8px 8px 0 0',
                cursor: 'pointer',
                fontWeight: '600',
                transition: 'all 0.2s'
              }}
              onClick={() => setActiveTab("upload")}
            >
              이미지 분석
            </button>
            <button
              style={{
                padding: '12px 24px',
                border: 'none',
                backgroundColor: activeTab === "gallery" ? '#48bb78' : 'transparent',
                color: activeTab === "gallery" ? 'white' : '#718096',
                borderRadius: '8px 8px 0 0',
                cursor: 'pointer',
                fontWeight: '600',
                transition: 'all 0.2s'
              }}
              onClick={() => setActiveTab("gallery")}
            >
              촬영 갤러리
            </button>
          </div>

          {/* 업로드 탭 */}
          {activeTab === "upload" && (
            <div>
              <div style={{ marginBottom: '20px' }}>
                <label htmlFor="file-upload" style={{
                  display: 'inline-block',
                  padding: '12px 24px',
                  backgroundColor: '#4299e1',
                  color: 'white',
                  borderRadius: '8px',
                  cursor: 'pointer',
                  fontWeight: '600'
                }}>
                  📁 이미지 업로드
                </label>
                <input
                  id="file-upload"
                  type="file"
                  accept="image/*"
                  onChange={handleFileChange}
                  style={{ display: 'none' }}
                />
              </div>

              {imagePreview && (
                <div style={{ marginBottom: '20px', textAlign: 'center' }}>
                  <img src={imagePreview} alt="preview" style={{ maxWidth: '100%', maxHeight: '400px', borderRadius: '8px', boxShadow: '0 2px 8px rgba(0,0,0,0.1)' }} />
                </div>
              )}

              <button
                onClick={handleAnalyze}
                disabled={loading}
                style={{
                  width: '100%',
                  padding: '16px',
                  backgroundColor: loading ? '#cbd5e0' : '#48bb78',
                  color: 'white',
                  border: 'none',
                  borderRadius: '8px',
                  fontSize: '18px',
                  fontWeight: '600',
                  cursor: loading ? 'not-allowed' : 'pointer',
                  marginBottom: '20px'
                }}
              >
                {loading ? "분석 중..." : "🔍 분석하기"}
              </button>

              {result && (
                <div style={{ padding: '20px', backgroundColor: '#f0fff4', borderRadius: '8px', border: '2px solid #48bb78' }}>
                  <h3 style={{ marginBottom: '12px', color: '#2f855a', fontSize: '20px' }}>분석 결과</h3>
                  <p style={{ fontSize: '18px', color: '#2d3748', fontWeight: '600' }}>{result}</p>
                </div>
              )}
            </div>
          )}

          {/* 갤러리 탭 */}
          {activeTab === "gallery" && (
            <div>
              <div style={{ display: 'flex', gap: '12px', marginBottom: '20px' }}>
                <button onClick={handleTriggerCamera} style={{
                  padding: '12px 24px',
                  backgroundColor: '#805ad5',
                  color: 'white',
                  border: 'none',
                  borderRadius: '8px',
                  cursor: 'pointer',
                  fontWeight: '600'
                }}>
                  📷 사진 촬영
                </button>
                <button onClick={fetchUploadedImages} style={{
                  padding: '12px 24px',
                  backgroundColor: '#4299e1',
                  color: 'white',
                  border: 'none',
                  borderRadius: '8px',
                  cursor: 'pointer',
                  fontWeight: '600'
                }}>
                  🔄 새로고침
                </button>
              </div>

              {uploadedImages.length === 0 ? (
                <p style={{ textAlign: 'center', color: '#718096', padding: '40px' }}>업로드된 이미지가 없습니다.</p>
              ) : (
                <div style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fill, minmax(200px, 1fr))', gap: '16px' }}>
                  {uploadedImages.map((img, index) => (
                    <div key={index} style={{ cursor: 'pointer' }}>
                      <img
                        src={img.url}
                        alt={img.filename}
                        style={{ width: '100%', height: '200px', objectFit: 'cover', borderRadius: '8px', boxShadow: '0 2px 4px rgba(0,0,0,0.1)' }}
                        onClick={() => handleSelectFromGallery(img.url, img.filename)}
                      />
                      <p style={{ marginTop: '8px', fontSize: '12px', color: '#718096', textAlign: 'center' }}>{img.timestamp}</p>
                    </div>
                  ))}
                </div>
              )}
            </div>
          )}
        </div>

        {/* 오른쪽: 센서값 */}
        <div style={{ width: '320px', backgroundColor: '#edf2f7', borderRadius: '12px', padding: '24px', boxShadow: '0 2px 8px rgba(0,0,0,0.1)' }}>
          <h3 style={{ fontSize: '20px', marginBottom: '20px', color: '#2d3748' }}>🌡️ 실시간 센서값</h3>
          {sensorData ? (
            <ul style={{ listStyle: 'none', padding: 0 }}>
              <li style={{ padding: '12px', backgroundColor: 'white', borderRadius: '8px', marginBottom: '8px' }}>온도: {sensorData.temperature} °C</li>
              <li style={{ padding: '12px', backgroundColor: 'white', borderRadius: '8px', marginBottom: '8px' }}>습도: {sensorData.humidity} %</li>
              <li style={{ padding: '12px', backgroundColor: 'white', borderRadius: '8px', marginBottom: '8px' }}>토양 수분: {sensorData.soil_moisture}</li>
              <li style={{ padding: '12px', backgroundColor: 'white', borderRadius: '8px', marginBottom: '8px' }}>수위: {sensorData.water_level} %</li>
              <li style={{ padding: '12px', backgroundColor: 'white', borderRadius: '8px', fontSize: '12px', color: '#718096' }}>⏱ {sensorData.timestamp}</li>
            </ul>
          ) : (
            <p style={{ textAlign: 'center', color: '#718096' }}>데이터 수신 대기중...</p>
          )}

          <button onClick={handleTriggerSensor} style={{
            width: '100%',
            marginTop: '16px',
            padding: '12px',
            backgroundColor: '#48bb78',
            color: 'white',
            border: 'none',
            borderRadius: '8px',
            cursor: 'pointer',
            fontWeight: '600'
          }}>
            🔄 새로고침
          </button>
        </div>
      </div>
    </div>
  );
}

export default App;
}

export default App;
