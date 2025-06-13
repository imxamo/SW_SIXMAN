import React, { useEffect, useState, useRef } from "react";
import { Link } from "react-router-dom";

function CameraPage() {
  const [imageUrl, setImageUrl] = useState(null);
  const intervalRef = useRef(null);

  // 백엔드 API 주소
  const backendImageApi = "http://localhost:8080/api/latest-image";

  const fetchImage = async () => {
    try {
      const response = await fetch(backendImageApi, { cache: "no-store" });
      if (!response.ok) throw new Error("이미지 요청 실패");

      const blob = await response.blob();

      if (imageUrl) {
        URL.revokeObjectURL(imageUrl);
      }

      const url = URL.createObjectURL(blob);
      setImageUrl(url);
    } catch (error) {
      console.error("이미지 로드 실패:", error);
    }
  };

  useEffect(() => {
    fetchImage();

    intervalRef.current = setInterval(fetchImage, 30000);

    return () => {
      clearInterval(intervalRef.current);
      if (imageUrl) URL.revokeObjectURL(imageUrl);
    };
  }, []);

  const handleSaveImage = () => {
    if (!imageUrl) return;

    const link = document.createElement("a");
    link.href = imageUrl;
    link.download = `camera_image_${Date.now()}.jpg`;
    document.body.appendChild(link);
    link.click();
    document.body.removeChild(link);
  };

  return (
    <div style={styles.page}>
      <header style={styles.header}>
        <div style={styles.headerInner}>
          <div style={styles.headerLeft}>SIXMAN</div>
          <div style={styles.headerCenter}>
            Open Development Platform 기반의 스마트팜 구축
          </div>
        </div>
      </header>

      <div style={styles.main}>
        <aside style={styles.sidebar}>
          <Link to="/" style={styles.menuBox}>
            🌿 식물 검사
          </Link>
          <Link to="/status" style={styles.menuBox}>
            📊 실시간 상태
          </Link>
          <Link to="/camera" style={styles.menuBox}>
            📷 이미지 확인
          </Link>
        </aside>

        <div style={styles.container}>
          <h1>📷 실시간 카메라 이미지</h1>
          <div style={{ textAlign: "center" }}>
            {imageUrl ? (
              <img src={imageUrl} alt="최신 카메라 이미지" style={styles.cameraImage} />
            ) : (
              <p>이미지 로드 중...</p>
            )}
          </div>
          <button onClick={handleSaveImage} style={styles.saveButton}>
            이미지 저장
          </button>
        </div>
      </div>

      <footer style={styles.footer}>
        <div style={styles.footerContent}>
          <div style={styles.footerLogo}>
            <strong>식스맨</strong>
            <br />
            <span style={{ fontSize: "0.8rem", color: "#ccc" }}>SIXMAN</span>
          </div>
          <div style={styles.footerText}>
            <p>
              <strong>팀장 (프론트 엔드, 하드웨어 담당)</strong> 이춘혁
            </p>
            <p>
              <strong>팀원 (하드웨어, 딥러닝)</strong> 조준영
            </p>
            <p>
              <strong>팀원 (하드웨어, 딥러닝)</strong> 박지용
            </p>
          </div>
          <div style={styles.footerText}>
            <p>
              <strong>팀원 (딥러닝)</strong> 이수아
            </p>
            <p>
              <strong>팀원 (프론트 엔드)</strong> 손지훈
            </p>
            <p>
              <strong>팀원 (백엔드)</strong> 이민혁
            </p>
          </div>
        </div>
      </footer>
    </div>
  );
}

const styles = {
  page: {
    display: "flex",
    flexDirection: "column",
    minHeight: "100vh",
    fontFamily: "'Segoe UI', sans-serif",
    backgroundColor: "#f0f4f8",
    color: "#333",
  },
  header: {
    padding: "20px",
    backgroundColor: "#60a5fa",
    color: "#fff",
  },
  headerInner: {
    display: "flex",
    alignItems: "center",
    padding: "0 30px",
    flexWrap: "wrap",
  },
  headerLeft: {
    fontSize: "1.6rem",
    fontWeight: "bold",
    textAlign: "left",
    marginRight: "auto",
  },
  headerCenter: {
    textAlign: "center",
    fontSize: "1.1rem",
    fontWeight: "500",
    flex: 1,
  },
  main: {
    flex: 1,
    display: "flex",
    flexWrap: "wrap",
    padding: "20px",
    gap: "20px",
  },
  sidebar: {
    width: "200px",
    display: "flex",
    flexDirection: "column",
    gap: "15px",
  },
  menuBox: {
    backgroundColor: "#ffffff",
    padding: "15px 20px",
    borderRadius: "12px",
    boxShadow: "0 4px 8px rgba(0, 0, 0, 0.1)",
    color: "#3b82f6",
    fontWeight: "500",
    textDecoration: "none",
    textAlign: "center",
    transition: "background-color 0.2s ease",
  },
  container: {
    padding: "30px",
    backgroundColor: "#f9fafb",
    borderRadius: "12px",
    flex: 1,
  },
  cameraImage: {
    maxWidth: "100%",
    borderRadius: "12px",
    boxShadow: "0 2px 8px rgba(0,0,0,0.1)",
    marginBottom: "20px",
  },
  saveButton: {
    padding: "12px 24px",
    fontSize: "1rem",
    borderRadius: "8px",
    border: "none",
    backgroundColor: "#3b82f6",
    color: "white",
    cursor: "pointer",
  },
  footer: {
    backgroundColor: "#666666",
    color: "#eee",
    fontSize: "0.9rem",
    padding: "20px 30px",
    borderTop: "1px solid #666",
  },
  footerContent: {
    display: "flex",
    flexDirection: "row",
    gap: "30px",
    flexWrap: "wrap",
    justifyContent: "flex-start",
  },
  footerLogo: {
    minWidth: "150px",
    fontWeight: "bold",
    color: "#fff",
  },
  footerText: {
    flex: 1,
    color: "#ddd",
    lineHeight: "1.6",
  },
};

export default CameraPage;
