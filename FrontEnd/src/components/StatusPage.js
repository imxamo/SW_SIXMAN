import { useEffect, useState } from "react";
import { Link } from 'react-router-dom';

function StatusPage() {
  const [sensorData, setSensorData] = useState({
    temp: null,           // 온도
    humidity: null,       // 공기 습도
    soilHumidity: null,   // 흙 습도
    waterTank: null       // 물통 상태
  });

  const fetchData = async () => {
    try {
      const res = await fetch("http://localhost:8080/api/sensors");
      const data = await res.json();
      setSensorData(data);
    } catch (error) {
      console.error("센서 데이터 불러오기 실패:", error);
    }
  };

  useEffect(() => {
    fetchData();
    const interval = setInterval(fetchData, 2000);
    return () => clearInterval(interval);
  }, []);

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
          <Link to="/" style={styles.menuBox}>🌿 식물 검사</Link>
          <Link to="/status" style={styles.menuBox}>📊 실시간 상태</Link>
          <Link to="/camera" style={styles.menuBox}>📷 이미지 확인</Link>
        </aside>

        <div style={styles.container}>
          <h1>📊 스마트팜 상태 보기</h1>

          <div style={styles.sensorBox}>
            <p> 온도: <strong>{sensorData.temp ?? "--"} °C</strong></p>
            <p> 공기 습도: <strong>{sensorData.humidity ?? "--"} %</strong></p>
            <p> 흙 습도: <strong>{sensorData.soilHumidity ?? "--"} %</strong></p>
            <p> 물통 상태: <strong>{sensorData.waterTank ?? "--"} %</strong></p>
          </div>
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
            <p><strong>팀장 (프론트 엔드, 하드웨어 담당)</strong> 이춘혁</p>
            <p><strong>팀원 (하드웨어, 딥러닝)</strong> 조준영</p>
            <p><strong>팀원 (하드웨어, 딥러닝)</strong> 박지용</p>
          </div>
          <div style={styles.footerText}>
            <p><strong>팀원 (딥러닝)</strong> 이수아</p>
            <p><strong>팀원 (프론트 엔드)</strong> 손지훈</p>
            <p><strong>팀원 (백엔드)</strong> 이민혁</p>
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
  sensorBox: {
    backgroundColor: "#ffffff",
    padding: "20px",
    borderRadius: "12px",
    boxShadow: "0 2px 8px rgba(0,0,0,0.1)",
    marginBottom: "30px",
    fontSize: "1.2rem",
    lineHeight: "2",
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

export default StatusPage;
