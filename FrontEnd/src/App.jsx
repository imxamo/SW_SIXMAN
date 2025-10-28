import React, { useState, useEffect } from "react";
import "./App.css";

function App() {
  const [image, setImage] = useState(null);
  const [imagePreview, setImagePreview] = useState(null);
  const [result, setResult] = useState("");
  const [uploadedImages, setUploadedImages] = useState([]);
  const [loading, setLoading] = useState(false);
  const [activeTab, setActiveTab] = useState("upload");
  const [sensorData, setSensorData] = useState(null);

  useEffect(() => {
    fetchUploadedImages();
  }, []);

  const fetchUploadedImages = async () => {
    try {
      const res = await fetch("/api/uploads");
      const data = await res.json();
      if (data.ok) setUploadedImages(data.uploads);
    } catch (e) {
      console.error("이미지 목록 로딩 실패:", e);
    }
  };

  useEffect(() => {
    const fetchSensor = async () => {
      try {
        const res = await fetch("/api/sensor");
        const data = await res.json();
        if (data.ok) setSensorData(data.data);
      } catch (e) {
        console.error("센서 데이터 로딩 실패:", e);
      }
    };
    fetchSensor();
    const interval = setInterval(fetchSensor, 5000);
    return () => clearInterval(interval);
  }, []);

  const handleFileChange = (e) => {
    const file = e.target.files[0];
    if (file) {
      setImage(file);
      setImagePreview(URL.createObjectURL(file));
      setResult("");
    }
  };

  const handleSelectFromGallery = (url, filename) => {
    setImage({ url, filename });
    setImagePreview(url);
    setResult("");
    setActiveTab("upload");
  };

  const handleAnalyze = async () => {
    if (!image) return alert("이미지를 선택해주세요!");
    setLoading(true);
    setResult("");
    try {
      const formData = new FormData();
      if (image instanceof File) formData.append("file", image);
      else if (image.url) {
        const res = await fetch(image.url);
        const blob = await res.blob();
        formData.append("file", blob, image.filename);
      }

      const res = await fetch("/api/predict", { method: "POST", body: formData });
      const data = await res.json();
      if (!res.ok || !data.ok) throw new Error(data.error || "서버 오류");
      setResult(`${data.disease_name} (${data.disease_code}) · ${(data.confidence * 100).toFixed(1)}%`);
    } catch (e) {
      console.error(e);
      setResult("분석 중 오류 발생!");
    } finally {
      setLoading(false);
    }
  };

  const handleTriggerCamera = async () => {
    try {
      const res = await fetch("/trigger/cam");
      const data = await res.json();
      if (data.status === "ok") {
        alert("카메라 촬영 요청 완료!");
        setTimeout(fetchUploadedImages, 3000);
      }
    } catch (e) {
      console.error("촬영 실패:", e);
      alert("카메라 연결 실패");
    }
  };

  const handleTriggerSensor = async () => {
    try {
      const res = await fetch("/trigger/esp32");
      const data = await res.json();
      if (data.status === "ok") {
        alert("센서 데이터 요청 완료!");
        setTimeout(() => {
          fetch("/api/sensor").then(r => r.json()).then(d => { if(d.ok) setSensorData(d.data); });
        }, 3000);
      }
    } catch (e) {
      console.error("센서 트리거 실패:", e);
      alert("센서 연결 실패");
    }
  };

  return (
    <div className="container">
      <div className="layout">
        {/* 왼쪽: 센서 */}
        <div className="sensorBox">
          <h3>🌡️ 실시간 센서값</h3>
          {sensorData ? (
            <ul style={{ listStyle: 'none', padding: 0 }}>
              <li>온도: {sensorData?.temperature} °C</li>
              <li>습도: {sensorData?.humidity} %</li>
              <li>토양 수분: {sensorData?.soil_moisture}</li>
              <li>수위: {sensorData?.water_level} %</li>
              <li>쿨링팬 상태: {sensorData?.cooling_fan}</li>
              <li>LED 상태: {sensorData?.led}</li>
              <li>⏱ {sensorData?.timestamp}</li>
            </ul>
          ) : (
            <p>데이터 수신 대기중...</p>
          )}
          <button onClick={handleTriggerSensor} style={{ marginTop: '16px', padding: '12px', width: '100%', backgroundColor: '#48bb78', color: 'white', borderRadius: '8px' }}>🔄 새로고침</button>
        </div>

        {/* 오른쪽: AI 분석 */}
        <div className="mainContent">
          <h1>🌿 상추 질병 AI 분석</h1>
          {/* 탭 */}
          <div style={{ display: 'flex', gap: '8px', marginBottom: '20px' }}>
            <button style={{ flex: 1, backgroundColor: activeTab === 'upload' ? '#48bb78' : '#e2e8f0', color: activeTab === 'upload' ? 'white' : '#718096', padding: '12px' }} onClick={() => setActiveTab('upload')}>이미지 분석</button>
            <button style={{ flex: 1, backgroundColor: activeTab === 'gallery' ? '#48bb78' : '#e2e8f0', color: activeTab === 'gallery' ? 'white' : '#718096', padding: '12px' }} onClick={() => setActiveTab('gallery')}>촬영 갤러리</button>
          </div>

          {/* 업로드 탭 */}
          {activeTab === 'upload' && (
            <div>
              <label htmlFor="file-upload" style={{ display: 'block', padding: '12px', backgroundColor: '#4299e1', color: 'white', borderRadius: '8px', cursor: 'pointer', marginBottom: '12px' }}>📁 이미지 업로드</label>
              <input id="file-upload" type="file" accept="image/*" onChange={handleFileChange} style={{ display: 'none' }} />
              {imagePreview && <img src={imagePreview} alt="preview" style={{ maxWidth: '100%', maxHeight: '400px', margin: '12px 0' }} />}
              <button onClick={handleAnalyze} disabled={loading} style={{ width: '100%', padding: '12px', backgroundColor: '#48bb78', color: 'white', borderRadius: '8px', marginTop: '12px' }}>{loading ? '분석 중...' : '🔍 분석하기'}</button>
              {result && <p style={{ marginTop: '12px', padding: '12px', backgroundColor: '#f0fff4', borderRadius: '8px' }}>{result}</p>}
            </div>
          )}

          {/* 갤러리 탭 */}
          {activeTab === 'gallery' && (
            <div>
              <button onClick={handleTriggerCamera} style={{ padding: '12px', backgroundColor: '#805ad5', color: 'white', borderRadius: '8px', marginRight: '8px' }}>📷 사진 촬영</button>
              <button onClick={fetchUploadedImages} style={{ padding: '12px', backgroundColor: '#4299e1', color: 'white', borderRadius: '8px' }}>🔄 새로고침</button>
              <div style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fill, minmax(200px, 1fr))', gap: '12px', marginTop: '12px' }}>
                {uploadedImages.map((img, idx) => (
                  <div key={idx} onClick={() => handleSelectFromGallery(img.url, img.filename)} style={{ cursor: 'pointer' }}>
                    <img src={img.url} alt={img.filename} style={{ width: '100%', height: '150px', objectFit: 'cover', borderRadius: '8px' }} />
                    <p style={{ textAlign: 'center', fontSize: '12px' }}>{img.timestamp}</p>
                  </div>
                ))}
              </div>
            </div>
          )}

        </div>
      </div>
    </div>
  );
}

export default App;
