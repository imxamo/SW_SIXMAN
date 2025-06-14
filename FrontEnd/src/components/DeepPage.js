import { useState } from "react";
import { Link } from 'react-router-dom';

function DeepPage() {
  const [imagePreview, setImagePreview] = useState(null);
  const [selectedPlant, setSelectedPlant] = useState("");
  const [result, setResult] = useState(null);
  const [isLoading, setIsLoading] = useState(false);
  
  const handleFileChange = (e) => {
    const file = e.target.files[0];
    if (file) {
      setImagePreview(URL.createObjectURL(file));
    }
  };

  const handlePlantChange = (e) => {
    setSelectedPlant(e.target.value);
  };

  const handleSendToServer = () => {
    if (!selectedPlant || !imagePreview) {
      alert("식물과 사진을 모두 선택해주세요.");
      return;
    }

    const fileInput = document.querySelector("input[type=file]");
    const file = fileInput.files[0];

    const formData = new FormData();
    formData.append("image", file);
    formData.append("plant", selectedPlant);

    setIsLoading(true);
    setResult(null);

    fetch("http://localhost:5000/upload", {
      method: "POST",
      body: formData,
    })
      .then((res) => res.json())
      .then((data) => {
        if (data.result) {
          setResult(data.result);
        } else {
          setResult({
            disease: "결과 없음",
            description: "서버에서 분석 결과를 반환하지 않았습니다.",
          });
        }
      })
      .catch((err) => {
        setResult({
          disease: "에러",
          description: "서버 통신 중 오류가 발생했습니다.",
        });
      })
      .finally(() => {
        setIsLoading(false);
      });
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

        <section style={styles.content}>
          <h1 style={styles.title}>
            <b style={{ color: "#60a5fa" }}>🌿 AI</b> 식물 병 분석
          </h1>

          <div style={styles.uploadArea}>
            <label style={styles.uploadButton}>
              파일 선택
              <input
                type="file"
                onChange={handleFileChange}
                style={styles.hiddenInput}
              />
            </label>

            <select
              value={selectedPlant}
              onChange={handlePlantChange}
              style={styles.selectBox}
            >
              <option value="" disabled>
                식물을 선택해주세요
              </option>
              <option value="lettuce">상추</option>
              <option value="spinach">시금치</option>
              <option value="cabbage">양배추</option>
            </select>

            {imagePreview && (
              <div style={styles.previewBox}>
                <img
                  src={imagePreview}
                  alt="미리보기"
                  style={styles.previewImage}
                />
              </div>
            )}

            {imagePreview && (
              <button style={styles.sendButton} onClick={handleSendToServer}>
                서버로 전송
              </button>
            )}

            {isLoading && (
              <div style={styles.resultBox}>
                <h3>🔄 로딩중...</h3>
              </div>
            )}

            {!isLoading && result && (
              <div style={styles.resultBox}>
                <h3>🩺 진단 결과: {result.disease}</h3>
                <p>{result.description}</p>
              </div>
            )}
          </div>
        </section>
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
    padding: "0 30px", // 좌우 패딩
    flexWrap: "wrap",
  },
  headerLeft: {
    fontSize: "1.6rem",
    fontWeight: "bold",
    textAlign: "left",
    marginRight: "auto", // 왼쪽 끝 정렬
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
    flexDirection: "column", // 세로 정렬
    gap: "15px", // 메뉴 간 간격
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

  link: {
    color: "#3b82f6",
    textDecoration: "none",
    fontWeight: "500",
  },
  content: {
    flex: 1,
    backgroundColor: "#ffffff",
    borderRadius: "12px",
    padding: "30px",
    boxShadow: "0 4px 12px rgba(0, 0, 0, 0.1)",
  },
  title: {
    fontSize: "1.8rem",
    marginBottom: "25px",
  },
  uploadArea: {
    display: "flex",
    flexDirection: "column",
    gap: "15px",
  },
  uploadButton: {
    display: "inline-block",
    padding: "12px 30px",
    backgroundColor: "#60a5fa",
    color: "#fff",
    fontSize: "1rem",
    fontWeight: "bold",
    borderRadius: "8px",
    cursor: "pointer",
    textAlign: "center",
  },
  hiddenInput: {
    display: "none",
  },
  selectBox: {
    width: "100%",
    padding: "12px",
    fontSize: "1rem",
    borderRadius: "8px",
    border: "1px solid #ccc",
    backgroundColor: "#f9fafb",
  },
  previewBox: {
    marginTop: "10px",
    padding: "15px",
    backgroundColor: "#f9fafb",
    borderRadius: "10px",
    border: "1px dashed #ccc",
    display: "flex",
    justifyContent: "center",
  },
  previewImage: {
    maxWidth: "100%",
    maxHeight: "300px",
    borderRadius: "10px",
  },
  sendButton: {
    padding: "12px 30px",
    backgroundColor: "#60a5fa",
    color: "#fff",
    fontSize: "1rem",
    fontWeight: "bold",
    border: "none",
    borderRadius: "8px",
    cursor: "pointer",
  },
  resultBox: {
    marginTop: "20px",
    padding: "20px",
    backgroundColor: "#fef9c3",
    border: "1px solid #facc15",
    borderRadius: "10px",
    color: "#333",
    fontSize: "1rem",
    boxShadow: "0 2px 4px rgba(0,0,0,0.1)",
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

export default DeepPage;
