import React, { useState } from "react";
import { Routes, Route } from "react-router-dom";
import DeepPage from "./components/DeepPage";
import StatusPage from "./components/StatusPage";
import CameraPage from "./components/CameraPage";

function App() {
  return (
    <Routes>
      <Route path="/" element={<DeepPage />} />
      <Route path="/status" element={<StatusPage />} />
      <Route path="/camera" element={<CameraPage />} />
    </Routes>
  );
}

export default App;
