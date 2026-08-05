import React, { useState, useEffect, useRef } from 'react';
import { Power, Sun, Palette, Settings, X, Cpu, Upload, CheckCircle, AlertCircle, Music, SlidersHorizontal, Zap, Radio, Activity, Volume2 } from 'lucide-react';

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
  { id: 'music_spectrum', label: '🎵 Spectrum (RGB Bands)', desc: 'R=Bass (20-250Hz), G=Mid, B=Treble direct energy mapping' },
  { id: 'music_pulse', label: '🥁 Beat Pulse', desc: 'Strobe flash triggered dynamically on transient bass beats' },
  { id: 'music_amplitude', label: '🔊 Amplitude Modulation', desc: 'Volume-proportional master brightness intensity' },
  { id: 'music_freq_hue', label: '🌈 Pitch-to-Hue (Zero-Crossing)', desc: 'Continuous pitch frequency zero-crossing color tracking' },
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
    headroom: 150,
    responseAgility: 50,
    beatSens: 45,
    beatDecay: 180,
    pitchLowHz: 120,
    pitchHighHz: 2400,
    pitchSmooth: 8,
    ambientGlow: 0,
    useLogScale: true,
  });

  const [wsConnected, setWsConnected] = useState(false);
  const [activeTab, setActiveTab] = useState('white'); // 'white', 'color', 'music', 'advanced'
  const [deviceInfo, setDeviceInfo] = useState({ ip: '...', ssid: '...', mode: '...' });
  const [otaFile, setOtaFile] = useState(null);
  const [otaProgress, setOtaProgress] = useState(0);
  const [otaStatus, setOtaStatus] = useState('');
  const wsRef = useRef(null);
  const canvasRef = useRef(null);
  const isInteractingRef = useRef(false);
  const seqRef = useRef(0);
  const lastProcessedSeqRef = useRef(0);
  const pendingUpdateRef = useRef(null);
  const sendTimerRef = useRef(null);

  useEffect(() => {
    let wsUrl = `ws://${window.location.host}/ws`;
    if (window.location.host.includes('localhost') || window.location.host.includes('5173')) {
      wsUrl = `ws://rgb-node.local/ws`;
    }
    connectWs(wsUrl);
    fetchStatus();

    return () => {
      if (wsRef.current) wsRef.current.close();
      if (sendTimerRef.current) clearTimeout(sendTimerRef.current);
    };
  }, []);

  const fetchStatus = async () => {
    try {
      const res = await fetch('/api/status');
      if (res.ok) {
        const data = await res.json();
        setDeviceInfo({ ip: data.ip, ssid: data.ssid, mode: data.mode });
        if (!isInteractingRef.current) {
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
            headroom: data.headroom ?? prev.headroom,
            responseAgility: data.responseAgility ?? prev.responseAgility,
            beatSens: data.beatSens ?? prev.beatSens,
            beatDecay: data.beatDecay ?? prev.beatDecay,
            pitchLowHz: data.pitchLowHz ?? prev.pitchLowHz,
            pitchHighHz: data.pitchHighHz ?? prev.pitchHighHz,
            pitchSmooth: data.pitchSmooth ?? prev.pitchSmooth,
            ambientGlow: data.ambientGlow ?? prev.ambientGlow,
            useLogScale: data.useLogScale ?? prev.useLogScale,
          }));
        }
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
          // Sequence-ID Echo Suppression: Drop delayed obsolete echoes in 0ms!
          if (data.seq !== undefined && data.seq !== null) {
            if (data.seq < lastProcessedSeqRef.current) {
              return; // Discard obsolete echo
            }
            lastProcessedSeqRef.current = data.seq;
          }
          if (!isInteractingRef.current) {
            setState((prev) => ({ ...prev, ...data }));
          }
        } catch (err) {}
      };
      wsRef.current = ws;
    } catch (e) {
      setWsConnected(false);
    }
  };

  const sendPayload = (payload) => {
    if (wsRef.current && wsRef.current.readyState === WebSocket.OPEN) {
      wsRef.current.send(JSON.stringify(payload));
    } else {
      fetch('/api/state', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(payload),
      }).catch(() => {});
    }
  };

  const updateState = (updates, isSliding = false) => {
    const nextSeq = ++seqRef.current;
    lastProcessedSeqRef.current = nextSeq;

    setState((prev) => {
      const newState = { ...prev, ...updates, seq: nextSeq };

      if (!isSliding) {
        sendPayload(newState);
      } else {
        pendingUpdateRef.current = newState;
        if (!sendTimerRef.current) {
          sendTimerRef.current = setTimeout(() => {
            sendTimerRef.current = null;
            if (pendingUpdateRef.current) {
              sendPayload(pendingUpdateRef.current);
              pendingUpdateRef.current = null;
            }
          }, 35); // 35ms rate limit window
        }
      }

      return newState;
    });
  };

  const sliderTouchHandlers = {
    onMouseDown: () => { isInteractingRef.current = true; },
    onTouchStart: () => { isInteractingRef.current = true; },
    onMouseUp: () => {
      isInteractingRef.current = false;
      if (pendingUpdateRef.current) {
        sendPayload(pendingUpdateRef.current);
        pendingUpdateRef.current = null;
      }
    },
    onTouchEnd: () => {
      isInteractingRef.current = false;
      if (pendingUpdateRef.current) {
        sendPayload(pendingUpdateRef.current);
        pendingUpdateRef.current = null;
      }
    },
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

  const warmthToKelvin = (w) => Math.round(6500 - (w / 100) * 4500);

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
                  {...sliderTouchHandlers}
                  onChange={(e) => {
                    const w = parseInt(e.target.value);
                    const k = warmthToKelvin(w);
                    updateState({ warmth: w, colorTemp: k, mode: 'white', effect: 'static' }, true);
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
                <span>Master Brightness Ceiling</span>
                <span className="slider-value">{Math.round((state.brightness / 255) * 100)}%</span>
              </div>
              <div className="slider-group">
                <input
                  type="range"
                  min="0"
                  max="255"
                  value={state.brightness}
                  {...sliderTouchHandlers}
                  onChange={(e) => updateState({ brightness: parseInt(e.target.value) }, true)}
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

        {/* TAB 3: DEDICATED MUSIC SYNC PAGE WITH PROFILE-SPECIFIC PANELS */}
        {activeTab === 'music' && (
          <>
            {/* Music Profile Selector */}
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

            {/* Global Audio Pipeline Panel (Always Active for Audio) */}
            <section className="instrument-card">
              <div className="card-title">
                <span>Global Audio Pipeline Controls</span>
                <SlidersHorizontal size={16} color="#94a3b8" />
              </div>

              <div className="grid-two-col" style={{ marginTop: '12px', gap: '20px' }}>
                <div className="slider-group">
                  <div className="slider-header">
                    <span>Microphone Preamp Gain / Sensitivity</span>
                    <span className="slider-value">{state.musicSensitivity}%</span>
                  </div>
                  <input
                    type="range"
                    min="1"
                    max="100"
                    value={state.musicSensitivity}
                    {...sliderTouchHandlers}
                    onChange={(e) => updateState({ musicSensitivity: parseInt(e.target.value) }, true)}
                  />
                  <p style={{ fontSize: '11px', color: 'var(--color-text-muted)', marginTop: '4px' }}>
                    Adjusts INMP441 MEMS microphone gain multiplier to match room sound pressure level.
                  </p>
                </div>

                <div className="slider-group">
                  <div className="slider-header">
                    <span>Universal Noise Floor Cutoff Gate</span>
                    <span className="slider-value">{state.noiseCutoff}%</span>
                  </div>
                  <input
                    type="range"
                    min="0"
                    max="25"
                    value={state.noiseCutoff}
                    {...sliderTouchHandlers}
                    onChange={(e) => updateState({ noiseCutoff: parseInt(e.target.value) }, true)}
                  />
                  <p style={{ fontSize: '11px', color: 'var(--color-text-muted)', marginTop: '4px' }}>
                    Cuts off low ambient room noise floor cleanly to eliminate dim-flickering during quiet pauses.
                  </p>
                </div>
              </div>

              <div className="grid-two-col" style={{ marginTop: '20px', gap: '20px' }}>
                <div className="slider-group">
                  <div className="slider-header">
                    <span>Global PWM Response Agility</span>
                    <span className="slider-value">{state.responseAgility}%</span>
                  </div>
                  <input
                    type="range"
                    min="1"
                    max="100"
                    value={state.responseAgility}
                    {...sliderTouchHandlers}
                    onChange={(e) => updateState({ responseAgility: parseInt(e.target.value) }, true)}
                  />
                  <p style={{ fontSize: '11px', color: 'var(--color-text-muted)', marginTop: '4px' }}>
                    Rate limits PWM change speed (1% = ultra smooth, 100% = instant responsive).
                  </p>
                </div>

                <div className="slider-group">
                  <div className="slider-header">
                    <span>Perceptual Volume Scaling Curve</span>
                    <span className="slider-value">{state.useLogScale ? 'LOGARITHMIC (dB)' : 'LINEAR'}</span>
                  </div>
                  <button
                    className={`effect-btn ${state.useLogScale ? 'active' : ''}`}
                    onClick={() => updateState({ useLogScale: !state.useLogScale })}
                    style={{ padding: '8px 16px', marginTop: '4px' }}
                  >
                    {state.useLogScale ? 'Switch to Linear Scale' : 'Switch to Logarithmic (dB) Scale'}
                  </button>
                  <p style={{ fontSize: '11px', color: 'var(--color-text-muted)', marginTop: '4px' }}>
                    Logarithmic dB scaling matches human ear loudness perception.
                  </p>
                </div>
              </div>
            </section>

            {/* DYNAMIC PROFILE CONTROL PANELS */}

            {/* PANEL 1: SPECTRUM */}
            {state.effect === 'music_spectrum' && (
              <section className="instrument-card">
                <div className="card-title">
                  <span>🎵 Spectrum Profile Controls</span>
                  <Activity size={16} color="#3b82f6" />
                </div>
                <div className="slider-group" style={{ marginTop: '12px' }}>
                  <div className="slider-header">
                    <span>Headroom Margin / Peak Compressor</span>
                    <span className="slider-value">{state.headroom}%</span>
                  </div>
                  <input
                    type="range"
                    min="100"
                    max="250"
                    value={state.headroom}
                    {...sliderTouchHandlers}
                    onChange={(e) => updateState({ headroom: parseInt(e.target.value) }, true)}
                  />
                  <p style={{ fontSize: '11px', color: 'var(--color-text-muted)', marginTop: '4px' }}>
                    Prevents moderate volume hits from clipping/saturating to 100% maximum brightness.
                  </p>
                </div>
              </section>
            )}

            {/* PANEL 2: BEAT PULSE */}
            {state.effect === 'music_pulse' && (
              <section className="instrument-card">
                <div className="card-title">
                  <span>🥁 Beat Pulse Profile Controls</span>
                  <Zap size={16} color="#3b82f6" />
                </div>
                <div className="grid-two-col" style={{ marginTop: '12px', gap: '20px' }}>
                  <div className="slider-group">
                    <div className="slider-header">
                      <span>Beat Sensitivity Threshold</span>
                      <span className="slider-value">{state.beatSens}%</span>
                    </div>
                    <input
                      type="range"
                      min="10"
                      max="90"
                      value={state.beatSens}
                      {...sliderTouchHandlers}
                      onChange={(e) => updateState({ beatSens: parseInt(e.target.value) }, true)}
                    />
                    <p style={{ fontSize: '11px', color: 'var(--color-text-muted)', marginTop: '4px' }}>
                      Peak-over-average multiplier for bass drum beat detection triggers.
                    </p>
                  </div>

                  <div className="slider-group">
                    <div className="slider-header">
                      <span>Pulse Decay Tail Speed</span>
                      <span className="slider-value">{state.beatDecay} ms</span>
                    </div>
                    <input
                      type="range"
                      min="20"
                      max="500"
                      value={state.beatDecay}
                      {...sliderTouchHandlers}
                      onChange={(e) => updateState({ beatDecay: parseInt(e.target.value) }, true)}
                    />
                    <p style={{ fontSize: '11px', color: 'var(--color-text-muted)', marginTop: '4px' }}>
                      Fast drop-off vs slow fading tail after a detected beat hit.
                    </p>
                  </div>
                </div>
              </section>
            )}

            {/* PANEL 3: AMPLITUDE MODULATION */}
            {state.effect === 'music_amplitude' && (
              <section className="instrument-card">
                <div className="card-title">
                  <span>🔊 Amplitude Dynamic Range Controls</span>
                  <Volume2 size={16} color="#3b82f6" />
                </div>
                <div className="grid-two-col" style={{ marginTop: '12px', gap: '20px' }}>
                  <div className="slider-group">
                    <div className="slider-header">
                      <span>Headroom Margin (Saturation Prevention)</span>
                      <span className="slider-value">{state.headroom}%</span>
                    </div>
                    <input
                      type="range"
                      min="100"
                      max="250"
                      value={state.headroom}
                      {...sliderTouchHandlers}
                      onChange={(e) => updateState({ headroom: parseInt(e.target.value) }, true)}
                    />
                    <p style={{ fontSize: '11px', color: 'var(--color-text-muted)', marginTop: '4px' }}>
                      Higher headroom keeps moderate volume in the middle dynamic range (prevents early maxing out).
                    </p>
                  </div>

                  <div className="slider-group">
                    <div className="slider-header">
                      <span>Ambient Background Glow Floor</span>
                      <span className="slider-value">{state.ambientGlow}%</span>
                    </div>
                    <input
                      type="range"
                      min="0"
                      max="30"
                      value={state.ambientGlow}
                      {...sliderTouchHandlers}
                      onChange={(e) => updateState({ ambientGlow: parseInt(e.target.value) }, true)}
                    />
                    <p style={{ fontSize: '11px', color: 'var(--color-text-muted)', marginTop: '4px' }}>
                      Minimum background light level maintained during silence (0% = pure blackout).
                    </p>
                  </div>
                </div>
              </section>
            )}

            {/* PANEL 4: PITCH-TO-HUE */}
            {state.effect === 'music_freq_hue' && (
              <section className="instrument-card">
                <div className="card-title">
                  <span>🌈 Pitch-to-Hue Zero-Crossing Controls</span>
                  <Radio size={16} color="#3b82f6" />
                </div>
                <div className="grid-two-col" style={{ marginTop: '12px', gap: '20px' }}>
                  <div className="slider-group">
                    <div className="slider-header">
                      <span>Low Pitch Bound (🔴 Red)</span>
                      <span className="slider-value">{state.pitchLowHz} Hz</span>
                    </div>
                    <input
                      type="range"
                      min="80"
                      max="500"
                      value={state.pitchLowHz}
                      {...sliderTouchHandlers}
                      onChange={(e) => updateState({ pitchLowHz: parseInt(e.target.value) }, true)}
                    />
                    <p style={{ fontSize: '11px', color: 'var(--color-text-muted)', marginTop: '4px' }}>
                      Frequency floor mapped to Red (humming/bass range).
                    </p>
                  </div>

                  <div className="slider-group">
                    <div className="slider-header">
                      <span>High Pitch Whistle Bound (🔵 Blue/Purple)</span>
                      <span className="slider-value">{state.pitchHighHz} Hz</span>
                    </div>
                    <input
                      type="range"
                      min="1000"
                      max="3500"
                      value={state.pitchHighHz}
                      {...sliderTouchHandlers}
                      onChange={(e) => updateState({ pitchHighHz: parseInt(e.target.value) }, true)}
                    />
                    <p style={{ fontSize: '11px', color: 'var(--color-text-muted)', marginTop: '4px' }}>
                      Upper frequency bound mapped to Blue/Purple (whistling range).
                    </p>
                  </div>
                </div>

                <div className="slider-group" style={{ marginTop: '20px' }}>
                  <div className="slider-header">
                    <span>Pitch Tracking Smoothness / Glide Rate</span>
                    <span className="slider-value">{state.pitchSmooth}</span>
                  </div>
                  <input
                    type="range"
                    min="1"
                    max="30"
                    value={state.pitchSmooth}
                    {...sliderTouchHandlers}
                    onChange={(e) => updateState({ pitchSmooth: parseInt(e.target.value) }, true)}
                  />
                  <p style={{ fontSize: '11px', color: 'var(--color-text-muted)', marginTop: '4px' }}>
                    Controls color glide rate when whistle pitch changes (prevents pitch chatter).
                  </p>
                </div>
              </section>
            )}

            {/* PANEL 5: AMBIENT CHILL */}
            {state.effect === 'music_chill' && (
              <section className="instrument-card">
                <div className="card-title">
                  <span>🌙 Ambient Chill Controls</span>
                  <Sun size={16} color="#3b82f6" />
                </div>
                <div className="slider-group" style={{ marginTop: '12px' }}>
                  <div className="slider-header">
                    <span>Minimum Background Warmth Glow</span>
                    <span className="slider-value">{state.ambientGlow}%</span>
                  </div>
                  <input
                    type="range"
                    min="0"
                    max="30"
                    value={state.ambientGlow}
                    {...sliderTouchHandlers}
                    onChange={(e) => updateState({ ambientGlow: parseInt(e.target.value) }, true)}
                  />
                  <p style={{ fontSize: '11px', color: 'var(--color-text-muted)', marginTop: '4px' }}>
                    Maintains cozy background glow even during total silence.
                  </p>
                </div>
              </section>
            )}
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
