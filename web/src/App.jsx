import React, { useState, useEffect, useRef } from 'react';
import { Power, Sliders, Settings, X, Wifi, Cpu, Activity, Upload, CheckCircle, AlertCircle } from 'lucide-react';

const PRESET_SWATCHES = [
  { name: 'Warm White', r: 255, g: 241, b: 224 },
  { name: 'Pure White', r: 255, g: 255, b: 255 },
  { name: 'Red', r: 255, g: 0, b: 0 },
  { name: 'Green', r: 0, g: 255, b: 0 },
  { name: 'Blue', r: 0, g: 0, b: 255 },
  { name: 'Cyan', r: 0, g: 240, b: 255 },
  { name: 'Purple', r: 138, g: 43, b: 226 },
  { name: 'Amber', r: 255, g: 149, b: 0 },
];

const EFFECTS = [
  { id: 'static', label: 'Static' },
  { id: 'hue_cycle', label: 'Rainbow Cycle' },
  { id: 'breathe', label: 'Breathe' },
  { id: 'candle', label: 'Candle Flicker' },
  { id: 'strobe', label: 'Strobe' },
  { id: 'music_spectrum', label: '🎵 Music: Spectrum' },
  { id: 'music_pulse', label: '🎵 Music: Beat Pulse' },
  { id: 'music_amplitude', label: '🎵 Music: Amplitude' },
  { id: 'music_freq_hue', label: '🎵 Music: Pitch-to-Hue' },
  { id: 'music_chill', label: '🎵 Music: Ambient Chill' },
];

export default function App() {
  const [state, setState] = useState({
    power: true,
    r: 0,
    g: 240,
    b: 255,
    brightness: 255,
    effect: 'static',
    speed: 50,
    musicSensitivity: 50,
  });

  const [wsConnected, setWsConnected] = useState(false);
  const [activeTab, setActiveTab] = useState('main'); // 'main' or 'advanced'
  const [deviceInfo, setDeviceInfo] = useState({ ip: '...', ssid: '...', mode: '...' });
  const [otaFile, setOtaFile] = useState(null);
  const [otaProgress, setOtaProgress] = useState(0);
  const [otaStatus, setOtaStatus] = useState(''); // 'uploading', 'success', 'error'
  const wsRef = useRef(null);
  const canvasRef = useRef(null);

  // Auto Connect WebSocket
  useEffect(() => {
    let wsUrl = `ws://${window.location.host}/ws`;
    if (window.location.host.includes('localhost') || window.location.host.includes('5173')) {
      wsUrl = `ws://rgb-node.local/ws`;
    }
    connectWs(wsUrl);
    fetchStatus();

    return () => {
      if (wsRef.current) wsRef.current.close();
    };
  }, []);

  const fetchStatus = async () => {
    try {
      const res = await fetch('/api/status');
      if (res.ok) {
        const data = await res.json();
        setDeviceInfo({ ip: data.ip, ssid: data.ssid, mode: data.mode });
        setState((prev) => ({
          ...prev,
          power: data.power ?? prev.power,
          r: data.r ?? prev.r,
          g: data.g ?? prev.g,
          b: data.b ?? prev.b,
          brightness: data.brightness ?? prev.brightness,
          effect: data.effect ?? prev.effect,
          speed: data.speed ?? prev.speed,
          musicSensitivity: data.musicSensitivity ?? prev.musicSensitivity,
        }));
      }
    } catch (e) {
      console.log('API status poll offline, waiting for WebSocket');
    }
  };

  const connectWs = (url) => {
    try {
      if (wsRef.current) wsRef.current.close();
      const ws = new WebSocket(url);
      ws.onopen = () => setWsConnected(true);
      ws.onclose = () => {
        setWsConnected(false);
        setTimeout(() => connectWs(url), 3000);
      };
      ws.onmessage = (event) => {
        try {
          const data = JSON.parse(event.data);
          setState((prev) => ({ ...prev, ...data }));
        } catch (err) {}
      };
      wsRef.current = ws;
    } catch (e) {
      setWsConnected(false);
    }
  };

  const updateState = (updates) => {
    const newState = { ...state, ...updates };
    setState(newState);
    if (wsRef.current && wsRef.current.readyState === WebSocket.OPEN) {
      wsRef.current.send(JSON.stringify(newState));
    }
  };

  const handleOtaUpload = () => {
    if (!otaFile) return;
    setOtaStatus('uploading');
    setOtaProgress(0);

    const formData = new FormData();
    formData.append('update', otaFile);

    const xhr = new XMLHttpRequest();
    xhr.open('POST', '/update', true);

    xhr.upload.onprogress = (e) => {
      if (e.lengthComputable) {
        const percent = Math.round((e.loaded / e.total) * 100);
        setOtaProgress(percent);
      }
    };

    xhr.onload = () => {
      if (xhr.status === 200) {
        setOtaStatus('success');
        setTimeout(() => window.location.reload(), 5000);
      } else {
        setOtaStatus('error');
      }
    };

    xhr.onerror = () => setOtaStatus('error');
    xhr.send(formData);
  };

  // Draw Color Wheel Canvas
  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;
    const ctx = canvas.getContext('2d');
    const width = canvas.width;
    const height = canvas.height;
    const radius = width / 2;

    ctx.clearRect(0, 0, width, height);

    for (let angle = 0; angle < 360; angle += 1) {
      const startAngle = (angle - 0.5) * (Math.PI / 180);
      const endAngle = (angle + 0.5) * (Math.PI / 180);

      ctx.beginPath();
      ctx.moveTo(radius, radius);
      ctx.arc(radius, radius, radius - 2, startAngle, endAngle);
      ctx.closePath();

      const gradient = ctx.createRadialGradient(radius, radius, 0, radius, radius, radius);
      gradient.addColorStop(0, '#ffffff');
      gradient.addColorStop(1, `hsl(${angle}, 100%, 50%)`);
      ctx.fillStyle = gradient;
      ctx.fill();
    }
  }, []);

  const handleWheelClick = (e) => {
    const canvas = canvasRef.current;
    if (!canvas) return;
    const rect = canvas.getBoundingClientRect();
    const x = (e.clientX || e.touches?.[0]?.clientX) - rect.left - canvas.width / 2;
    const y = (e.clientY || e.touches?.[0]?.clientY) - rect.top - canvas.height / 2;

    const distance = Math.sqrt(x * x + y * y);
    const maxRadius = canvas.width / 2;
    if (distance > maxRadius) return;

    let angle = Math.atan2(y, x) * (180 / Math.PI);
    if (angle < 0) angle += 360;

    const sat = Math.min(1, distance / maxRadius);
    const { r, g, b } = hslToRgb(angle, sat, 0.5);

    updateState({ r, g, b });
  };

  const hslToRgb = (h, s, l) => {
    const c = (1 - Math.abs(2 * l - 1)) * s;
    const x = c * (1 - Math.abs(((h / 60) % 2) - 1));
    const m = l - c / 2;
    let r1 = 0, g1 = 0, b1 = 0;

    if (0 <= h && h < 60) { r1 = c; g1 = x; b1 = 0; }
    else if (60 <= h && h < 120) { r1 = x; g1 = c; b1 = 0; }
    else if (120 <= h && h < 180) { r1 = 0; g1 = c; b1 = x; }
    else if (180 <= h && h < 240) { r1 = 0; g1 = x; b1 = c; }
    else if (240 <= h && h < 300) { r1 = x; g1 = 0; b1 = c; }
    else if (300 <= h && h < 360) { r1 = c; g1 = 0; b1 = x; }

    return {
      r: Math.round((r1 + m) * 255),
      g: Math.round((g1 + m) * 255),
      b: Math.round((b1 + m) * 255),
    };
  };

  const hexColor = `#${((1 << 24) + (state.r << 16) + (state.g << 8) + state.b).toString(16).slice(1).toUpperCase()}`;

  return (
    <div className="app-container">
      <header className="app-header">
        <div className="logo-container">
          <Cpu size={20} color="#94a3b8" />
          <span className="logo-title">RGB-NODE</span>
          <span className="logo-tag">ESP32-C3</span>
        </div>

        <div className="nav-tabs">
          <button
            className={`nav-tab-btn ${activeTab === 'main' ? 'active' : ''}`}
            onClick={() => setActiveTab('main')}
          >
            Controller
          </button>
          <button
            className={`nav-tab-btn ${activeTab === 'advanced' ? 'active' : ''}`}
            onClick={() => setActiveTab('advanced')}
          >
            Advanced
          </button>
        </div>

        <div className="status-badge">
          <div className={`status-dot ${wsConnected ? 'online' : ''}`}></div>
          <span>{wsConnected ? 'ONLINE' : 'DISCONNECTED'}</span>
        </div>
      </header>

      <main className="app-main">
        {/* Power Toggle Card */}
        <section className="instrument-card">
          <div className="card-title">
            <span>Power State</span>
            <span style={{ color: state.power ? 'var(--color-success)' : 'var(--color-text-muted)' }}>
              {state.power ? 'ACTIVE' : 'OFF'}
            </span>
          </div>
          <button
            className={`power-btn ${state.power ? 'active' : ''}`}
            onClick={() => updateState({ power: !state.power })}
          >
            <Power size={22} />
            <span>{state.power ? 'POWER ON' : 'POWER OFF'}</span>
          </button>
        </section>

        {/* Color & Brightness Grid */}
        <div className="grid-two-col">
          {/* Color Wheel Card */}
          <section className="instrument-card">
            <div className="card-title">
              <span>Color Selection</span>
              <span style={{ fontFamily: 'var(--font-mono)' }}>{hexColor}</span>
            </div>
            <div className="color-wheel-wrapper">
              <canvas
                ref={canvasRef}
                width={200}
                height={200}
                className="color-canvas"
                onClick={handleWheelClick}
                onTouchMove={handleWheelClick}
              />
              <div className="readout-box">
                <div className="readout-item">R: {state.r}</div>
                <div className="readout-item">G: {state.g}</div>
                <div className="readout-item">B: {state.b}</div>
              </div>
            </div>
          </section>

          {/* Preset Swatches & Brightness */}
          <section className="instrument-card">
            <div className="card-title">
              <span>Brightness & Swatches</span>
              <span className="slider-value">{Math.round((state.brightness / 255) * 100)}%</span>
            </div>

            <div className="slider-group">
              <input
                type="range"
                min="0"
                max="255"
                value={state.brightness}
                onChange={(e) => updateState({ brightness: parseInt(e.target.value) })}
              />
            </div>

            <div className="card-title" style={{ marginTop: '24px' }}>
              <span>Preset Swatches</span>
            </div>

            <div className="swatch-grid">
              {PRESET_SWATCHES.map((swatch, i) => (
                <button
                  key={i}
                  className="swatch-btn"
                  style={{ backgroundColor: `rgb(${swatch.r}, ${swatch.g}, ${swatch.b})` }}
                  onClick={() => updateState({ r: swatch.r, g: swatch.g, b: swatch.b })}
                  title={swatch.name}
                />
              ))}
            </div>
          </section>
        </div>

        {/* Dynamic Effects Card */}
        <section className="instrument-card">
          <div className="card-title">
            <span>Dynamic Lighting Effects</span>
            <span style={{ fontFamily: 'var(--font-mono)' }}>{state.effect.toUpperCase()}</span>
          </div>

          <div className="effect-grid">
            {EFFECTS.map((eff) => (
              <button
                key={eff.id}
                className={`effect-btn ${state.effect === eff.id ? 'active' : ''}`}
                onClick={() => updateState({ effect: eff.id })}
              >
                {eff.label}
              </button>
            ))}
          </div>

          {state.effect !== 'static' && (
            <div className="slider-group" style={{ marginTop: '16px' }}>
              <div className="slider-header">
                <span>Effect Speed</span>
                <span className="slider-value">{state.speed}%</span>
              </div>
              <input
                type="range"
                min="1"
                max="100"
                value={state.speed}
                onChange={(e) => updateState({ speed: parseInt(e.target.value) })}
              />
            </div>
          )}

          {state.effect.startsWith('music_') && (
            <div className="slider-group" style={{ marginTop: '16px' }}>
              <div className="slider-header">
                <span>Microphone Gain / Sensitivity</span>
                <span className="slider-value">{state.musicSensitivity}%</span>
              </div>
              <input
                type="range"
                min="1"
                max="100"
                value={state.musicSensitivity}
                onChange={(e) => updateState({ musicSensitivity: parseInt(e.target.value) })}
              />
            </div>
          )}
        </section>
      </main>

      {/* Advanced Modal / Tab */}
      {activeTab === 'advanced' && (
        <div className="modal-overlay">
          <div className="modal-card">
            <div className="modal-header">
              <div className="logo-container">
                <Settings size={18} color="#94a3b8" />
                <span className="logo-title">Advanced Settings</span>
              </div>
              <button className="close-btn" onClick={() => setActiveTab('main')}>
                <X size={20} />
              </button>
            </div>

            <div className="instrument-card">
              <div className="card-title">Device & Network Info</div>
              <div className="slider-header" style={{ margin: '6px 0' }}>
                <span>mDNS Host:</span>
                <span className="slider-value">http://rgb-node.local/</span>
              </div>
              <div className="slider-header" style={{ margin: '6px 0' }}>
                <span>Wi-Fi IP Address:</span>
                <span className="slider-value">{deviceInfo.ip}</span>
              </div>
              <div className="slider-header" style={{ margin: '6px 0' }}>
                <span>Network SSID:</span>
                <span className="slider-value">{deviceInfo.ssid}</span>
              </div>
              <div className="slider-header" style={{ margin: '6px 0' }}>
                <span>Wi-Fi Mode:</span>
                <span className="slider-value">{deviceInfo.mode}</span>
              </div>
            </div>

            {/* OTA Wireless Update Card */}
            <div className="instrument-card">
              <div className="card-title">
                <span>Wireless OTA Update</span>
                <Upload size={16} color="#94a3b8" />
              </div>
              <p style={{ fontSize: '12px', color: 'var(--color-text-secondary)', marginBottom: '12px' }}>
                Upload compiled <code style={{ color: 'var(--color-text-primary)' }}>firmware.bin</code> or <code style={{ color: 'var(--color-text-primary)' }}>littlefs.bin</code> binary file.
              </p>

              <div style={{ display: 'flex', flexDirection: 'column', gap: '10px' }}>
                <input
                  type="file"
                  accept=".bin"
                  onChange={(e) => setOtaFile(e.target.files[0])}
                  style={{ fontSize: '12px', color: 'var(--color-text-secondary)' }}
                />
                <button
                  className="effect-btn active"
                  onClick={handleOtaUpload}
                  disabled={!otaFile || otaStatus === 'uploading'}
                  style={{ cursor: otaFile ? 'pointer' : 'not-allowed', padding: '10px' }}
                >
                  {otaStatus === 'uploading' ? `Uploading (${otaProgress}%)...` : 'Flash OTA Update'}
                </button>

                {otaStatus === 'uploading' && (
                  <div className="slider-group">
                    <input type="range" min="0" max="100" value={otaProgress} readOnly />
                  </div>
                )}

                {otaStatus === 'success' && (
                  <div style={{ display: 'flex', alignItems: 'center', gap: '6px', color: 'var(--color-success)', fontSize: '13px' }}>
                    <CheckCircle size={16} />
                    <span>Update complete! Rebooting device in 5s...</span>
                  </div>
                )}

                {otaStatus === 'error' && (
                  <div style={{ display: 'flex', alignItems: 'center', gap: '6px', color: 'var(--color-danger)', fontSize: '13px' }}>
                    <AlertCircle size={16} />
                    <span>OTA update failed. Check connection and file binary.</span>
                  </div>
                )}
              </div>
            </div>

            <div className="instrument-card">
              <div className="card-title">Gamma Calibration</div>
              <div className="slider-group">
                <div className="slider-header">
                  <span>Gamma Correction</span>
                  <span className="slider-value">2.8 (Fixed)</span>
                </div>
                <p style={{ fontSize: '12px', color: 'var(--color-text-secondary)' }}>
                  12-bit perceptual gamma curve active across all 3 PWM channels.
                </p>
              </div>
            </div>
          </div>
        </div>
      )}
    </div>
  );
}
