import React, { useState, useEffect } from "react";

function App() {
  const [image, setImage] = useState(null);
  const [imagePreview, setImagePreview] = useState(null);
  const [result, setResult] = useState("");
  const [uploadedImages, setUploadedImages] = useState([]);
  const [loading, setLoading] = useState(false);
  const [activeTab, setActiveTab] = useState("upload"); // "upload" or "gallery"

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

  // 파일 선택 (직접 업로드)
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
    setImage({ url: imageUrl, filename }); // URL 기반 이미지로 설정
    setResult("");
    setActiveTab("upload"); // 분석 탭으로 전환
  };

  // 분석 버튼 클릭
  const handleAnalyze = async () => {
    if (!image) {
      alert("이미지를 먼저 업로드하거나 선택해주세요!");
      return;
    }

    setLoading(true);
    setResult("");

    try {
      let formData = new FormData();

      // 직접 업로드한 파일인 경우
      if (image instanceof File) {
        formData.append("file", image);
      } 
      // 갤러리에서 선택한 이미지인 경우
      else if (image.url) {
        // 서버에서 이미지를 다시 가져와서 Blob으로 변환
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

  // 사진 촬영 트리거
  const handleTriggerCamera = async () => {
    try {
      const response = await fetch("/trigger");
      const data = await response.json();
      if (data.status === "ok") {
        alert("카메라 촬영 요청을 보냈습니다!");
        // 3초 후 이미지 목록 새로고침
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
    <div style={styles.container}>
      <h1 style={styles.title}>🌿 상추 질병 AI 분석</h1>

      {/* 탭 메뉴 */}
      <div style={styles.tabContainer}>
        <button
          style={activeTab === "upload" ? styles.tabActive : styles.tab}
          onClick={() => setActiveTab("upload")}
        >
          이미지 분석
        </button>
        <button
          style={activeTab === "gallery" ? styles.tabActive : styles.tab}
          onClick={() => setActiveTab("gallery")}
        >
          촬영 갤러리
        </button>
      </div>

      {/* 이미지 분석 탭 */}
      {activeTab === "upload" && (
        <div style={styles.content}>
          <div style={styles.uploadBox}>
            <label htmlFor="file-upload" style={styles.uploadLabel}>
              📁 이미지 업로드
            </label>
            <input
              id="file-upload"
              type="file"
              accept="image/*"
              onChange={handleFileChange}
              style={styles.fileInput}
            />
          </div>

          {imagePreview && (
            <div style={styles.previewContainer}>
              <img
                src={imagePreview}
                alt="preview"
                style={styles.previewImage}
              />
            </div>
          )}

          <button
            onClick={handleAnalyze}
            style={styles.analyzeButton}
            disabled={loading}
          >
            {loading ? "분석 중..." : "🔍 분석하기"}
          </button>

          {result && (
            <div style={styles.resultBox}>
              <h3 style={styles.resultTitle}>분석 결과</h3>
              <p style={styles.resultText}>{result}</p>
            </div>
          )}
        </div>
      )}

      {/* 갤러리 탭 */}
      {activeTab === "gallery" && (
        <div style={styles.content}>
          <div style={styles.galleryHeader}>
            <button onClick={handleTriggerCamera} style={styles.cameraButton}>
              📷 사진 촬영
            </button>
            <button onClick={fetchUploadedImages} style={styles.refreshButton}>
              🔄 새로고침
            </button>
          </div>

          {uploadedImages.length === 0 ? (
            <p style={styles.emptyText}>업로드된 이미지가 없습니다.</p>
          ) : (
            <div style={styles.gallery}>
              {uploadedImages.map((img, index) => (
                <div key={index} style={styles.galleryItem}>
                  <img
                    src={img.url}
                    alt={img.filename}
                    style={styles.galleryImage}
                    onClick={() => handleSelectFromGallery(img.url, img.filename)}
                  />
                  <p style={styles.galleryCaption}>{img.timestamp}</p>
                </div>
              ))}
            </div>
          )}
        </div>
      )}
    </div>
  );
}

const styles = {
  container: {
    textAlign: "center",
    padding: "30px 20px",
    fontFamily: "'Segoe UI', Arial, sans-serif",
    maxWidth: "1400px",
    margin: "0 auto",
    backgroundColor: "#f5f7fa",
    minHeight: "100vh",
  },
  title: {
    color: "#2c3e50",
    marginBottom: "30px",
    fontSize: "32px",
  },
  tabContainer: {
    display: "flex",
    justifyContent: "center",
    gap: "10px",
    marginBottom: "30px",
  },
  tab: {
    padding: "12px 30px",
    fontSize: "16px",
    cursor: "pointer",
    border: "2px solid #3498db",
    backgroundColor: "#ecf0f1",   // 기본은 연한 회색
    color: "#3498db",             // 글씨 파란색
    borderRadius: "8px",
    transition: "all 0.3s",
  },
  tabActive: {
    padding: "12px 30px",
    fontSize: "16px",
    cursor: "pointer",
    border: "2px solid #3498db",
    backgroundColor: "#3498db",   // 활성화 → 파란색 배경
    color: "white",               // 글씨 흰색
    borderRadius: "8px",
    fontWeight: "bold",
  },
  content: {
    backgroundColor: "white",
    padding: "30px",
    borderRadius: "12px",
    boxShadow: "0 2px 10px rgba(0,0,0,0.1)",
  },
  uploadBox: {
    marginBottom: "20px",
  },
  uploadLabel: {
    display: "inline-block",
    padding: "12px 24px",
    backgroundColor: "#3498db",
    color: "white",
    borderRadius: "8px",
    cursor: "pointer",
    fontSize: "16px",
    transition: "background-color 0.3s",
  },
  fileInput: {
    display: "none",
  },
  previewContainer: {
    margin: "20px 0",
  },
  previewImage: {
    maxWidth: "400px",
    maxHeight: "400px",
    borderRadius: "8px",
    boxShadow: "0 4px 8px rgba(0,0,0,0.1)",
  },
  analyzeButton: {
    padding: "14px 40px",
    marginTop: "20px",
    fontSize: "18px",
    cursor: "pointer",
    backgroundColor: "#27ae60",
    color: "white",
    border: "none",
    borderRadius: "8px",
    fontWeight: "bold",
    transition: "background-color 0.3s",
  },
  resultBox: {
    marginTop: "30px",
    padding: "20px",
    border: "2px solid #27ae60",
    borderRadius: "8px",
    backgroundColor: "#e8f5e9",
    display: "inline-block",
    textAlign: "left",
    minWidth: "300px",
  },
  resultTitle: {
    color: "#27ae60",
    marginBottom: "10px",
  },
  resultText: {
    fontSize: "18px",
    color: "#2c3e50",
    fontWeight: "bold",
  },
  galleryHeader: {
    display: "flex",
    justifyContent: "center",
    gap: "10px",
    marginBottom: "20px",
  },
  cameraButton: {
    padding: "12px 24px",
    fontSize: "16px",
    cursor: "pointer",
    backgroundColor: "#e74c3c",
    color: "white",
    border: "none",
    borderRadius: "8px",
    fontWeight: "bold",
  },
  refreshButton: {
    padding: "12px 24px",
    fontSize: "16px",
    cursor: "pointer",
    backgroundColor: "#95a5a6",
    color: "white",
    border: "none",
    borderRadius: "8px",
    fontWeight: "bold",
  },
  emptyText: {
    color: "#7f8c8d",
    fontSize: "16px",
    padding: "40px",
  },
  gallery: {
    display: "grid",
    gridTemplateColumns: "repeat(auto-fill, minmax(250px, 1fr))",
    gridTemplateColumns: "repeat(auto-fit, minmax(250px, 1fr))",
    gap: "20px",
    marginTop: "20px",
    padding: "10px",
    justifyItems: "center",
  },
  galleryItem: {
    cursor: "pointer",
    transition: "transform 0.2s",
    border: "2px solid #ecf0f1",
    borderRadius: "8px",
    padding: "10px",
    backgroundColor: "#fafafa",
    overflow: "hidden",
    width: "100%",
    maxWidth: "300px",
  },
  galleryImage: {
    width: "100%",
    height: "200px",
    objectFit: "cover",
    borderRadius: "6px",
    display: "block",
  },
  galleryCaption: {
    marginTop: "8px",
    fontSize: "12px",
    color: "#7f8c8d",
    textAlign: "center",
  },
};

export default App;
