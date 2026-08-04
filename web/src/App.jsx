import React, { useState, useEffect, useRef } from 'react';
import { Power, Sun, Palette, Settings, X, Wifi, Cpu, Activity, Upload, CheckCircle, AlertCircle, Music, Volume2, SlidersHorizontal, Zap, Sliders } from 'lucide-react';

const RGB_SWATCHES = [
  { name: 'Red', r: 255, g: 0, b: 0 },
  { name: 'Orange', r: 255, g: 100, b: 0 },
  { name: 'Amber', r: 255, g: 170, b: 0 },
  { name: 'Green', r: 0, g: 255, b: 0 },
  { name: 'Cyan', r: 0, g: 240, b: 255 },
  { name: 'Blue', r: 0, g: 0, b: 255 },
  { name: 'Purple', r: 138, g: 43, b: 226 },
  { name: 'Magenta', r: 255, g: 0, b: 128 },
];

const WHITE_PRESETS = [
  { name: 'Candle & Sunset', kelvin: 2000, warmth: 100, label: 'Candle' },
  { name: 'Warm White', kelvin: 2700, warmth: 84, label: '2700K Warm' },
  { name: 'Soft Living', kelvin: 3000, warmth: 78, label: '3000K Soft' },
  { name: 'Neutral Workspace', kelvin: 4000, warmth: 56, label: '4000K Neutral' },
  { name: 'Daylight Studio', kelvin: 5000, warmth: 33, label: '5000K Daylight' },
  { name: 'Cool Crisp', kelvin: 6500, warmth: 0, label: '6500K Cool' },
];

const BASIC_EFFECTS = [
  { id: 'static', label: 'Static' },
  { id: 'hue_cycle', label: 'Rainbow Cycle' },
  { id: 'breathe', label: 'Breathe' },
  { id: 'candle', label: 'Candle Flicker' },
  { id: 'strobe', label: 'Strobe' },
];

const MUSIC_PROFILES = [
  { id: 'music_spectrum', label: '🎵 Spectrum (R=Bass, G=Mid, B=Treb)', desc: 'Direct 3-band frequency energy channel mapping' },
  { id: 'music_pulse', label: '🥁 Beat Pulse', desc: 'Strobe flash triggered dynamically on transient bass beats' },
  { id: 'music_amplitude', label: '🔊 Amplitude Intensity', desc: 'Volume-proportional master brightness modulation' },
  { id: 'music_freq_hue', label: '🌈 Pitch-to-Hue', desc: 'Zero-crossing continuous pitch frequency color tracking' },
  { id: 'music_chill', label: '🌙 Ambient Chill', desc: 'Low-pass filtered soothing room audio ambience' },
];

export default function App() {
  const [state, setState] = useState({
    power: true,
    r: 0,
    g: 240,
    b: 255,
    brightness: 255,
    mode: 'white',
    colorTemp: 2700,
    warmth: 84,
    effect: 'static',
    speed: 50,
    musicSensitivity: 50,
    noiseCutoff: 8,
    beatSens: 45,
  });

  const [wsConnected, setWsConnected] = useState(false);
  const [activeTab, setActiveTab] = useState('white'); // 'white', 'color', 'music', 'advanced'
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
          mode: data.mode ?? prev.mode,
          colorTemp: data.colorTemp ?? prev.colorTemp,
          warmth: data.warmth ?? prev.warmth,
          effect: data.effect ?? prev.effect,
          speed: data.speed ?? prev.speed,
          musicSensitivity: data.musicSensitivity ?? prev.musicSensitivity,
          noiseCutoff: data.noiseCutoff ?? prev.noiseCutoff,
          beatSens: data.beatSens ?? prev.beatSens,
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

  // Convert Warmth (0..100) to Kelvin (6500K..2000K)
  const warmthToKelvin = (w) => Math.round(6500 - (w / 100) * 4500);
  const kelvinToWarmth = (k) => Math.round(((6500 - k) / 4500) * 100);

  // Draw Color Wheel Canvas when Color tab active
  useEffect(() => {
    if (activeTab !== 'color') return;
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
  }, [activeTab]);

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

    updateState({ r, g, b, mode: 'color', effect: 'static' });
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
            className={`nav-tab-btn ${activeTab === 'white' ? 'active' : ''}`}
            onClick={() => setActiveTab('white')}
          >
            <Sun size={15} style={{ marginRight: '6px' }} />
            Natural Lighting
          </button>
          <button
            className={`nav-tab-btn ${activeTab === 'color' ? 'active' : ''}`}
            onClick={() => setActiveTab('color')}
          >
            <Palette size={15} style={{ marginRight: '6px' }} />
            Decorative RGB
          </button>
          <button
            className={`nav-tab-btn ${activeTab === 'music' ? 'active' : ''}`}
            onClick={() => setActiveTab('music')}
          >
            <Music size={15} style={{ marginRight: '6px' }} />
            Music Sync
          </button>
          <button
            className={`nav-tab-btn ${activeTab === 'advanced' ? 'active' : ''}`}
            onClick={() => setActiveTab('advanced')}
          >
            <Settings size={15} style={{ marginRight: '6px' }} />
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

        {/* TAB 1: NATURAL LIGHTING (CCT KELVIN) */}
        {activeTab === 'white' && (
          <>
            <section className="instrument-card">
              <div className="card-title">
                <span>Color Temperature (Kelvin)</span>
                <span className="slider-value">{state.colorTemp}K</span>
              </div>

              <div className="slider-group" style={{ marginTop: '16px' }}>
                <div className="slider-header">
                  <span>Warmth Spectrum (6500K Cool $\rightarrow$ 2000K Warm)</span>
                  <span className="slider-value">{state.warmth}%</span>
                </div>
                <input
                  type="range"
                  min="0"
                  max="100"
                  value={state.warmth}
                  onChange={(e) => {
                    const w = parseInt(e.target.value);
                    const k = warmthToKelvin(w);
                    updateState({ warmth: w, colorTemp: k, mode: 'white', effect: 'static' });
                  }}
                />
              </div>

              <div className="card-title" style={{ marginTop: '24px' }}>
                <span>Quick White Presets</span>
              </div>
              <div className="swatch-grid" style={{ gridTemplateColumns: 'repeat(auto-fit, minmax(130px, 1fr))' }}>
                {WHITE_PRESETS.map((p, i) => (
                  <button
                    key={i}
                    className={`effect-btn ${state.mode === 'white' && state.colorTemp === p.kelvin ? 'active' : ''}`}
                    onClick={() => updateState({ colorTemp: p.kelvin, warmth: p.warmth, mode: 'white', effect: 'static' })}
                  >
                    {p.label}
                  </button>
                ))}
              </div>
            </section>

            <section className="instrument-card">
              <div className="card-title">
                <span>Master Brightness</span>
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
            </section>
          </>
        )}

        {/* TAB 2: DECORATIVE RGB */}
        {activeTab === 'color' && (
          <>
            <div className="grid-two-col">
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
                  {RGB_SWATCHES.map((swatch, i) => (
                    <button
                      key={i}
                      className="swatch-btn"
                      style={{ backgroundColor: `rgb(${swatch.r}, ${swatch.g}, ${swatch.b})` }}
                      onClick={() => updateState({ r: swatch.r, g: swatch.g, b: swatch.b, mode: 'color', effect: 'static' })}
                      title={swatch.name}
                    />
                  ))}
                </div>
              </section>
            </div>

            <section className="instrument-card">
              <div className="card-title">
                <span>Standard Dynamic Patterns</span>
                <span style={{ fontFamily: 'var(--font-mono)' }}>{state.effect.toUpperCase()}</span>
              </div>

              <div className="effect-grid">
                {BASIC_EFFECTS.map((eff) => (
                  <button
                    key={eff.id}
                    className={`effect-btn ${state.effect === eff.id ? 'active' : ''}`}
                    onClick={() => updateState({ effect: eff.id })}
                  >
                    {eff.label}
                  </button>
                ))}
              </div>

              {state.effect !== 'static' && !state.effect.startsWith('music_') && (
                <div className="slider-group" style={{ marginTop: '16px' }}>
                  <div className="slider-header">
                    <span>Pattern Cycle Speed</span>
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
            </section>
          </>
        )}

        {/* TAB 3: DEDICATED MUSIC SYNC PAGE */}
        {activeTab === 'music' && (
          <>
            <section className="instrument-card">
              <div className="card-title">
                <span>INMP441 Music Sync Profiles</span>
                <Music size={18} color="#3b82f6" />
              </div>

              <div className="effect-grid" style={{ gridTemplateColumns: 'repeat(auto-fit, minmax(220px, 1fr))' }}>
                {MUSIC_PROFILES.map((prof) => (
                  <button
                    key={prof.id}
                    className={`effect-btn ${state.effect === prof.id ? 'active' : ''}`}
                    onClick={() => updateState({ effect: prof.id })}
                    style={{ textAlign: 'left', padding: '14px', display: 'flex', flexDirection: 'column', gap: '4px' }}
                  >
                    <span style={{ fontWeight: 600, fontSize: '13px' }}>{prof.label}</span>
                    <span style={{ fontSize: '11px', color: 'var(--color-text-secondary)' }}>{prof.desc}</span>
                  </button>
                ))}
              </div>
            </section>

            <div className="grid-two-col">
              {/* Audio DSP Processing Controls */}
              <section className="instrument-card">
                <div className="card-title">
                  <span>Audio DSP Gain & Noise Gate</span>
                  <SlidersHorizontal size={16} color="#94a3b8" />
                </div>

                <div className="slider-group" style={{ marginTop: '12px' }}>
                  <div className="slider-header">
                    <span>Microphone Preamp Gain / Sensitivity</span>
                    <span className="slider-value">{state.musicSensitivity}%</span>
                  </div>
                  <input
                    type="range"
                    min="1"
                    max="100"
                    value={state.musicSensitivity}
                    onChange={(e) => updateState({ musicSensitivity: parseInt(e.target.value) })}
                  />
                  <p style={{ fontSize: '11px', color: 'var(--color-text-muted)', marginTop: '4px' }}>
                    Adjusts INMP441 digital microphone AGC multiplier to match room sound pressure level.
                  </p>
                </div>

                <div className="slider-group" style={{ marginTop: '20px' }}>
                  <div className="slider-header">
                    <span>Low-End Cutoff Noise Gate</span>
                    <span className="slider-value">{state.noiseCutoff ?? 8}%</span>
                  </div>
                  <input
                    type="range"
                    min="0"
                    max="25"
                    value={state.noiseCutoff ?? 8}
                    onChange={(e) => updateState({ noiseCutoff: parseInt(e.target.value) })}
                  />
                  <p style={{ fontSize: '11px', color: 'var(--color-text-muted)', marginTop: '4px' }}>
                    Suppresses low ambient room noise floor to eliminate dim-flicker during quiet pauses.
                  </p>
                </div>
              </section>

              {/* Advanced Beat & Strobe Controls */}
              <section className="instrument-card">
                <div className="card-title">
                  <span>Beat Detection & Transient Trigger</span>
                  <Zap size={16} color="#94a3b8" />
                </div>

                <div className="slider-group" style={{ marginTop: '12px' }}>
                  <div className="slider-header">
                    <span>Beat Threshold Sensitivity</span>
                    <span className="slider-value">{state.beatSens ?? 45}%</span>
                  </div>
                  <input
                    type="range"
                    min="10"
                    max="90"
                    value={state.beatSens ?? 45}
                    onChange={(e) => updateState({ beatSens: parseInt(e.target.value) })}
                  />
                  <p style={{ fontSize: '11px', color: 'var(--color-text-muted)', marginTop: '4px' }}>
                    Controls transient peak detection multiplier for Beat Pulse strobe triggers.
                  </p>
                </div>

                <div className="slider-group" style={{ marginTop: '20px' }}>
                  <div className="slider-header">
                    <span>Smooth Transition Speed</span>
                    <span className="slider-value">{state.speed}%</span>
                  </div>
                  <input
                    type="range"
                    min="1"
                    max="100"
                    value={state.speed}
                    onChange={(e) => updateState({ speed: parseInt(e.target.value) })}
                  />
                  <p style={{ fontSize: '11px', color: 'var(--color-text-muted)', marginTop: '4px' }}>
                    Adjusts exponential decay rate and color wheel cycle rate for music profiles.
                  </p>
                </div>
              </section>
            </div>
          </>
        )}

        {/* TAB 4: ADVANCED SETTINGS */}
        {activeTab === 'advanced' && (
          <>
            <section className="instrument-card">
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
            </section>

            {/* OTA Wireless Update Card */}
            <section className="instrument-card">
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
            </section>

            <section className="instrument-card">
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
            </section>
          </>
        )}
      </main>
    </div>
  );
}
