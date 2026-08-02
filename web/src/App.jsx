import React, { useState, useEffect, useRef } from 'react';
import { 
  Play, 
  Pause, 
  RotateCcw, 
  Plus, 
  Trash2, 
  Download, 
  Copy, 
  Sparkles, 
  Layers, 
  Sliders, 
  Cpu, 
  Check, 
  HelpCircle, 
  MapPin,
  SlidersHorizontal,
  Link,
  Link2Off,
  Settings,
  X
} from 'lucide-react';

const PRESET_LIGHTS = [
  { name: 'Red', hex: '#ff0000', rgb: { r: 255, g: 0, b: 0 } },
  { name: 'Orange', hex: '#ffaa00', rgb: { r: 255, g: 170, b: 0 } },
  { name: 'Yellow', hex: '#ffff00', rgb: { r: 255, g: 255, b: 0 } },
  { name: 'Green', hex: '#00ff00', rgb: { r: 0, g: 255, b: 0 } },
  { name: 'Cyan', hex: '#00f0ff', rgb: { r: 0, g: 240, b: 255 } },
  { name: 'Blue', hex: '#0000ff', rgb: { r: 0, g: 0, b: 255 } },
  { name: 'Purple', hex: '#8a2be2', rgb: { r: 138, g: 43, b: 226 } },
  { name: 'Pink', hex: '#ff007f', rgb: { r: 255, g: 0, b: 127 } },
];

const PRESETS_TIMELINE = {
  cyberpunk: [
    { time: 0.0, color: { r: 0, g: 240, b: 255 } }, // Cyan
    { time: 0.3, color: { r: 255, g: 0, b: 127 } }, // Pink
    { time: 0.65, color: { r: 138, g: 43, b: 226 } }, // Purple
    { time: 1.0, color: { r: 0, g: 240, b: 255 } }  // Cyan (Loop)
  ],
  sunset: [
    { time: 0.0, color: { r: 255, g: 80, b: 0 } },  // Orange
    { time: 0.35, color: { r: 255, g: 0, b: 100 } }, // Pinkish Red
    { time: 0.7, color: { r: 100, g: 0, b: 180 } }, // Violet
    { time: 1.0, color: { r: 255, g: 80, b: 0 } }
  ],
  aurora: [
    { time: 0.0, color: { r: 0, g: 255, b: 100 } }, // Green
    { time: 0.4, color: { r: 0, g: 200, b: 255 } }, // Cyan/Blue
    { time: 0.8, color: { r: 150, g: 0, b: 255 } }, // Purple
    { time: 1.0, color: { r: 0, g: 255, b: 100 } }
  ],
  fireplace: [
    { time: 0.0, color: { r: 255, g: 60, b: 0 } },
    { time: 0.25, color: { r: 200, g: 30, b: 0 } },
    { time: 0.5, color: { r: 255, g: 100, b: 0 } },
    { time: 0.75, color: { r: 150, g: 20, b: 0 } },
    { time: 1.0, color: { r: 255, g: 60, b: 0 } }
  ]
};

function App() {
  const [strips, setStrips] = useState([
    {
      id: 'strip-1',
      name: 'LED Strip',
      location: 'Main Room',
      pinR: 9,
      pinG: 10,
      pinB: 11,
      length: 30,
      type: 'analog',
      keyframes: [
        { time: 0.0, color: { r: 0, g: 240, b: 255 } },   // Cyan
        { time: 0.5, color: { r: 255, g: 0, b: 127 } },   // Pink
        { time: 1.0, color: { r: 0, g: 240, b: 255 } }    // Cyan
      ]
    }
  ]);

  const [selectedStripId, setSelectedStripId] = useState('strip-1');
  const [selectedKeyframeIndex, setSelectedKeyframeIndex] = useState(0);
  const [isPlaying, setIsPlaying] = useState(true);
  const [currentTime, setCurrentTime] = useState(0.0); // 0.0 to 1.0
  const [loopDuration, setLoopDuration] = useState(30); // in seconds
  const [speedMultiplier, setSpeedMultiplier] = useState(1.0);
  const [copied, setCopied] = useState(false);

  // Web Serial API State
  const [port, setPort] = useState(null);
  const [writer, setWriter] = useState(null);
  const [isSerialConnected, setIsSerialConnected] = useState(false);

  // WebSocket / Wi-Fi State
  const [wsConnected, setWsConnected] = useState(false);
  const wsRef = useRef(null);

  // Auto-connect WebSocket if hosted on ESP32 or configured IP
  useEffect(() => {
    const host = window.location.host;
    if (host && !host.includes('localhost') && !host.includes('127.0.0.1') && !host.includes('5173')) {
      connectWebSocket(`ws://${host}/ws`);
    }
  }, []);

  const connectWebSocket = (url) => {
    try {
      if (wsRef.current) wsRef.current.close();
      const socket = new WebSocket(url);
      socket.onopen = () => {
        setWsConnected(true);
        console.log("WebSocket connected to ESP32:", url);
      };
      socket.onclose = () => setWsConnected(false);
      socket.onerror = (err) => console.error("WebSocket error:", err);
      wsRef.current = socket;
    } catch (err) {
      console.error("Failed to connect WebSocket:", err);
    }
  };

  // New Strip Form State
  const [newStripName, setNewStripName] = useState('');
  const [newStripLocation, setNewStripLocation] = useState('Office');
  const [newStripPinR, setNewStripPinR] = useState(5);
  const [newStripPinG, setNewStripPinG] = useState(6);
  const [newStripPinB, setNewStripPinB] = useState(7);

  // Clean UI Mode & Calibration State
  const [controlMode, setControlMode] = useState('solid'); // 'solid' or 'timeline'
  const [activePreset, setActivePreset] = useState(null);
  const [solidColors, setSolidColors] = useState({
    'strip-1': { r: 0, g: 240, b: 255 }
  });
  const [gammaEnabled, setGammaEnabled] = useState(true);
  const [calibration, setCalibration] = useState({ r: 1.0, g: 0.85, b: 0.90 });
  const [isSettingsOpen, setIsSettingsOpen] = useState(false);

  const requestRef = useRef();
  const previousTimeRef = useRef();
  const lastSerialTxRef = useRef(0);
  const encoderRef = useRef(new TextEncoder());

  // Animation Loop
  useEffect(() => {
    const animate = (time) => {
      if (previousTimeRef.current !== undefined && isPlaying) {
        const deltaTime = time - previousTimeRef.current;
        const deltaFraction = (deltaTime * speedMultiplier) / (loopDuration * 1000);
        setCurrentTime((prev) => {
          let next = prev + deltaFraction;
          if (next >= 1.0) next = next % 1.0;
          return next;
        });
      }
      previousTimeRef.current = time;
      requestRef.current = requestAnimationFrame(animate);
    };

    requestRef.current = requestAnimationFrame(animate);
    return () => {
      if (requestRef.current) cancelAnimationFrame(requestRef.current);
    };
  }, [isPlaying, loopDuration, speedMultiplier]);

  // Direct Streaming over WebSockets and Web Serial (Throttled to 25 FPS/40ms)
  useEffect(() => {
    if (!isSerialConnected && !wsConnected) return;

    const now = Date.now();
    if (now - lastSerialTxRef.current > 40) {
      lastSerialTxRef.current = now;

      // Stream colors for all strips in the layout
      strips.forEach((strip, index) => {
        const color = getActiveColor(strip);
        sendColorToTarget(index, color);
      });
    }
  }, [currentTime, strips, isSerialConnected, wsConnected, writer, controlMode, solidColors, gammaEnabled, calibration]);


  const activeStrip = strips.find((s) => s.id === selectedStripId) || strips[0];

  // Helper: linear interpolation of R, G, B channels
  const lerp = (start, end, t) => {
    return Math.round(start + (end - start) * t);
  };

  const getInterpolatedColor = (keyframes, time) => {
    if (!keyframes || keyframes.length === 0) return { r: 0, g: 0, b: 0 };
    if (keyframes.length === 1) return keyframes[0].color;

    const sorted = [...keyframes].sort((a, b) => a.time - b.time);
    const first = sorted[0];
    const last = sorted[sorted.length - 1];

    if (time <= first.time) {
      const dt = (1.0 - last.time) + first.time;
      const t = dt > 0 ? (time + (1.0 - last.time)) / dt : 0;
      return {
        r: lerp(last.color.r, first.color.r, t),
        g: lerp(last.color.g, first.color.g, t),
        b: lerp(last.color.b, first.color.b, t)
      };
    }

    if (time >= last.time) {
      const dt = (1.0 - last.time) + first.time;
      const t = dt > 0 ? (time - last.time) / dt : 0;
      return {
        r: lerp(last.color.r, first.color.r, t),
        g: lerp(last.color.g, first.color.g, t),
        b: lerp(last.color.b, first.color.b, t)
      };
    }

    for (let i = 0; i < sorted.length - 1; i++) {
      const start = sorted[i];
      const end = sorted[i + 1];
      if (time >= start.time && time <= end.time) {
        const t = (time - start.time) / (end.time - start.time);
        return {
          r: lerp(start.color.r, end.color.r, t),
          g: lerp(start.color.g, end.color.g, t),
          b: lerp(start.color.b, end.color.b, t)
        };
      }
    }

    return { r: 0, g: 0, b: 0 };
  };

  const getActiveColor = (strip) => {
    if (controlMode === 'solid') {
      return solidColors[strip.id] || { r: 128, g: 128, b: 128 };
    }
    return getInterpolatedColor(strip.keyframes, currentTime);
  };

  const rgbToHex = ({ r, g, b }) => {
    const toHex = (c) => {
      const hex = Math.max(0, Math.min(255, c)).toString(16);
      return hex.length === 1 ? '0' : '' + hex;
    };
    return `#${toHex(r)}${toHex(g)}${toHex(b)}`;
  };

  const hexToRgb = (hex) => {
    const result = /^#?([a-f\d]{2})([a-f\d]{2})([a-f\d]{2})$/i.exec(hex);
    return result ? {
      r: parseInt(result[1], 16),
      g: parseInt(result[2], 16),
      b: parseInt(result[3], 16)
    } : { r: 0, g: 0, b: 0 };
  };

  // Web Serial Handlers
  const handleConnectSerial = async () => {
    if (!('serial' in navigator)) {
      alert("Web Serial API is not supported in this browser. Please use Google Chrome, Microsoft Edge, or Opera.");
      return;
    }

    try {
      const selectedPort = await navigator.serial.requestPort();
      await selectedPort.open({ baudRate: 115200 });
      const serialWriter = selectedPort.writable.getWriter();
      
      setPort(selectedPort);
      setWriter(serialWriter);
      setIsSerialConnected(true);
    } catch (err) {
      console.error("Web Serial connection failed:", err);
      alert("Could not open serial connection. Make sure the Arduino is connected and no other program (like the Arduino IDE Serial Monitor) has the port open.");
    }
  };

  const handleDisconnectSerial = async () => {
    try {
      if (writer) {
        writer.releaseLock();
      }
      if (port) {
        await port.close();
      }
    } catch (err) {
      console.error("Error closing port:", err);
    } finally {
      setPort(null);
      setWriter(null);
      setIsSerialConnected(false);
    }
  };

  const applyCalibrationAndGamma = ({ r, g, b }) => {
    // 1. Apply calibration weights
    let cr = r * (calibration?.r ?? 1.0);
    let cg = g * (calibration?.g ?? 1.0);
    let cb = b * (calibration?.b ?? 1.0);

    // 2. Apply gamma correction (gamma = 2.2) if enabled
    if (gammaEnabled) {
      cr = Math.pow(cr / 255, 2.2) * 255;
      cg = Math.pow(cg / 255, 2.2) * 255;
      cb = Math.pow(cb / 255, 2.2) * 255;
    }

    return {
      r: Math.max(0, Math.min(255, Math.round(cr))),
      g: Math.max(0, Math.min(255, Math.round(cg))),
      b: Math.max(0, Math.min(255, Math.round(cb)))
    };
  };

  const sendColorToTarget = async (stripIdx, color) => {
    const corrected = applyCalibrationAndGamma(color);
    const packet = `${stripIdx},${corrected.r},${corrected.g},${corrected.b}\n`;

    // Send over WebSocket if connected
    if (wsRef.current && wsRef.current.readyState === WebSocket.OPEN) {
      wsRef.current.send(JSON.stringify({ r: corrected.r, g: corrected.g, b: corrected.b }));
    }

    // Send over Web Serial if connected
    if (writer) {
      try {
        await writer.write(encoderRef.current.encode(packet));
      } catch (err) {
        console.error("Failed to transmit serial command:", err);
      }
    }
  };


  // Strip Form handlers
  const handleAddStrip = (e) => {
    e.preventDefault();
    if (!newStripName.trim()) return;

    const newStrip = {
      id: `strip-${Date.now()}`,
      name: newStripName,
      location: newStripLocation,
      pinR: newStripPinR,
      pinG: newStripPinG,
      pinB: newStripPinB,
      length: 30,
      type: 'analog',
      keyframes: [
        { time: 0.0, color: { r: 0, g: 240, b: 255 } },
        { time: 1.0, color: { r: 0, g: 240, b: 255 } }
      ]
    };

    setStrips([...strips, newStrip]);
    setSelectedStripId(newStrip.id);
    setSelectedKeyframeIndex(0);
    setNewStripName('');
  };

  const handleRemoveStrip = (id, e) => {
    e.stopPropagation();
    if (strips.length === 1) {
      alert("You need to keep at least one LED strip!");
      return;
    }
    const filtered = strips.filter((s) => s.id !== id);
    setStrips(filtered);
    setSelectedStripId(filtered[0].id);
    setSelectedKeyframeIndex(0);
  };

  // Keyframe handlers
  const handleAddKeyframe = (stripId, clickTime) => {
    const roundedTime = Math.round(clickTime * 100) / 100;
    const targetStrip = strips.find((s) => s.id === stripId);
    if (!targetStrip) return;

    if (targetStrip.keyframes.some((kf) => Math.abs(kf.time - roundedTime) < 0.03)) {
      return;
    }

    const colorAtTime = getInterpolatedColor(targetStrip.keyframes, roundedTime);
    const newKeyframe = {
      time: roundedTime,
      color: colorAtTime
    };

    const updatedStrips = strips.map((s) => {
      if (s.id === stripId) {
        const newKfs = [...s.keyframes, newKeyframe].sort((a, b) => a.time - b.time);
        return { ...s, keyframes: newKfs };
      }
      return s;
    });

    setStrips(updatedStrips);
    
    const newStrip = updatedStrips.find((s) => s.id === stripId);
    const newIndex = newStrip.keyframes.findIndex((kf) => kf.time === roundedTime);
    setSelectedKeyframeIndex(newIndex);
  };

  const handleColorSelect = (colorRgb) => {
    setControlMode('solid');
    setIsPlaying(false);
    setActivePreset(null);
    setSolidColors((prev) => ({
      ...prev,
      [selectedStripId]: colorRgb
    }));
  };

  const handleUpdateKeyframeColor = (colorRgb) => {
    const updatedStrips = strips.map((s) => {
      if (s.id === selectedStripId) {
        const updatedKfs = s.keyframes.map((kf, idx) => {
          if (idx === selectedKeyframeIndex) {
            return { ...kf, color: colorRgb };
          }
          return kf;
        });
        return { ...s, keyframes: updatedKfs };
      }
      return s;
    });
    setStrips(updatedStrips);
  };

  const handleUpdateKeyframeTime = (newTimeFraction) => {
    setIsPlaying(false);
    const roundedTime = Math.round(newTimeFraction * 100) / 100;
    const updatedStrips = strips.map((s) => {
      if (s.id === selectedStripId) {
        const updatedKfs = s.keyframes.map((kf, idx) => {
          if (idx === selectedKeyframeIndex) {
            return { ...kf, time: roundedTime };
          }
          return kf;
        });
        const sortedKfs = [...updatedKfs].sort((a, b) => a.time - b.time);
        return { ...s, keyframes: sortedKfs };
      }
      return s;
    });

    setStrips(updatedStrips);

    const nextActiveStrip = updatedStrips.find((s) => s.id === selectedStripId);
    const nextIndex = nextActiveStrip.keyframes.findIndex((kf) => kf.time === roundedTime);
    if (nextIndex !== -1) {
      setSelectedKeyframeIndex(nextIndex);
    }
  };

  const handleDeleteKeyframe = () => {
    if (activeStrip.keyframes.length <= 2) {
      alert("An LED strip timeline needs at least 2 keyframes to animate!");
      return;
    }
    
    const updatedStrips = strips.map((s) => {
      if (s.id === selectedStripId) {
        const filteredKfs = s.keyframes.filter((_, idx) => idx !== selectedKeyframeIndex);
        return { ...s, keyframes: filteredKfs };
      }
      return s;
    });

    setStrips(updatedStrips);
    setSelectedKeyframeIndex(0);
  };

  // Preset Applicator
  const applyPresetTimeline = (presetKey) => {
    const timelineData = PRESETS_TIMELINE[presetKey];
    if (!timelineData) return;

    setControlMode('timeline');
    setIsPlaying(true);
    setActivePreset(presetKey);

    const updatedStrips = strips.map((s) => {
      if (s.id === selectedStripId) {
        return { ...s, keyframes: JSON.parse(JSON.stringify(timelineData)) };
      }
      return s;
    });
    setStrips(updatedStrips);
    setSelectedKeyframeIndex(0);
  };

  // Generate Arduino Code for Standalone Timeline playback
  const generateArduinoCode = () => {
    const durationMs = loopDuration * 1000;
    
    let pinDefs = `// ==========================================\n`;
    pinDefs += `// PWM Pin Assignments & Calibration Constants\n`;
    pinDefs += `// ==========================================\n`;
    pinDefs += `#define CALIBRATION_R ${calibration.r.toFixed(2)}\n`;
    pinDefs += `#define CALIBRATION_G ${calibration.g.toFixed(2)}\n`;
    pinDefs += `#define CALIBRATION_B ${calibration.b.toFixed(2)}\n`;
    pinDefs += `#define GAMMA_CORRECTION_ENABLED ${gammaEnabled ? 1 : 0}\n\n`;
    
    strips.forEach((strip) => {
      const cleanName = strip.name.toUpperCase().replace(/\s+/g, '_');
      pinDefs += `#define STRIP_${cleanName}_PIN_R ${strip.pinR}\n`;
      pinDefs += `#define STRIP_${cleanName}_PIN_G ${strip.pinG}\n`;
      pinDefs += `#define STRIP_${cleanName}_PIN_B ${strip.pinB}\n`;
    });

    let keyframeData = `// ==========================================\n`;
    keyframeData += `// Timeline Configurations stored in PROGMEM\n`;
    keyframeData += `// ==========================================\n`;

    strips.forEach((strip) => {
      const cleanName = strip.name.toUpperCase().replace(/\s+/g, '_');
      const sorted = [...strip.keyframes].sort((a, b) => a.time - b.time);
      keyframeData += `const int STRIP_${cleanName}_KF_COUNT = ${sorted.length};\n`;
      keyframeData += `const Keyframe STRIP_${cleanName}_TIMELINE[] PROGMEM = {\n`;
      sorted.forEach((kf, idx) => {
        const ms = Math.round(kf.time * durationMs);
        keyframeData += `  { ${ms}, ${kf.color.r}, ${kf.color.g}, ${kf.color.b} }${idx < sorted.length - 1 ? ',' : ''} // ${Math.round(kf.time * 100)}%\n`;
      });
      keyframeData += `};\n\n`;
    });

    let loopCalls = `  // Get current timeline position in milliseconds\n`;
    loopCalls += `  uint32_t currentMs = millis() % ${durationMs}UL;\n\n`;
    strips.forEach((strip) => {
      const cleanName = strip.name.toUpperCase().replace(/\s+/g, '_');
      loopCalls += `  updateStrip(STRIP_${cleanName}_TIMELINE, STRIP_${cleanName}_KF_COUNT, currentMs, STRIP_${cleanName}_PIN_R, STRIP_${cleanName}_PIN_G, STRIP_${cleanName}_PIN_B);\n`;
    });

    return `/*
  Lumina - Generated Multi-Zone LED Controller Sketch
  Board Target: Arduino Uno / Nano (ATmega328P)
  Loop Cycle: ${loopDuration} seconds
*/

#include <avr/pgmspace.h>

struct Keyframe {
  uint32_t ms; // Timestamp in cycle (milliseconds)
  uint8_t r;
  uint8_t g;
  uint8_t b;
};

${pinDefs}
#if GAMMA_CORRECTION_ENABLED
// 2.2 Gamma correction lookup table stored in PROGMEM (Flash Memory)
const uint8_t GAMMA_TABLE[] PROGMEM = {
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   1,   1,   1,   1,   1,   1,   1,   1,   1,   2,   2,   2,   2,
    2,   2,   3,   3,   3,   3,   3,   4,   4,   4,   4,   5,   5,   5,   5,   6,
    6,   6,   7,   7,   7,   8,   8,   8,   9,   9,   9,  10,  10,  11,  11,  11,
   12,  12,  13,  13,  14,  14,  15,  15,  16,  16,  17,  17,  18,  18,  19,  19,
   20,  20,  21,  22,  22,  23,  24,  24,  25,  25,  26,  27,  27,  28,  29,  30,
   30,  31,  32,  33,  33,  34,  35,  36,  36,  37,  38,  39,  40,  41,  41,  42,
   43,  44,  45,  46,  47,  48,  49,  50,  50,  51,  52,  53,  54,  55,  56,  57,
   58,  59,  60,  61,  62,  63,  64,  65,  66,  67,  68,  69,  70,  71,  72,  73,
   74,  75,  77,  78,  79,  80,  81,  82,  84,  85,  86,  87,  89,  90,  91,  92,
   94,  95,  96,  98,  99, 100, 102, 103, 104, 106, 107, 109, 110, 111, 113, 114,
  116, 117, 119, 120, 122, 123, 125, 126, 128, 130, 131, 133, 134, 136, 138, 139,
  141, 143, 144, 146, 148, 149, 151, 153, 155, 156, 158, 160, 162, 164, 165, 167,
  169, 171, 173, 175, 177, 179, 180, 182, 184, 186, 188, 190, 192, 194, 196, 198,
  200, 202, 204, 206, 208, 210, 212, 214, 216, 218, 220, 222, 224, 226, 229, 231,
  233, 235, 237, 239, 242, 244, 246, 248, 250, 253, 255, 255, 255, 255, 255, 255
};
#endif

${keyframeData}
// Linear interpolation between color bytes
uint8_t lerpChannel(uint8_t val1, uint8_t val2, float t) {
  return val1 + (val2 - val1) * t;
}

// Safely reads keyframe struct fields from Flash memory (PROGMEM)
Keyframe readKeyframe(const Keyframe* array, int index) {
  Keyframe kf;
  kf.ms = pgm_read_dword(&(array[index].ms));
  kf.r = pgm_read_byte(&(array[index].r));
  kf.g = pgm_read_byte(&(array[index].g));
  kf.b = pgm_read_byte(&(array[index].b));
  return kf;
}

// Apply calibration scaling & gamma lookup, then output PWM values
void writeColor(int pinR, int pinG, int pinB, uint8_t r, uint8_t g, uint8_t b) {
  uint16_t cr = (uint16_t)r * CALIBRATION_R;
  uint16_t cg = (uint16_t)g * CALIBRATION_G;
  uint16_t cb = (uint16_t)b * CALIBRATION_B;

  // Cap values at 255 after scaling
  uint8_t finalR = cr > 255 ? 255 : cr;
  uint8_t finalG = cg > 255 ? 255 : cg;
  uint8_t finalB = cb > 255 ? 255 : cb;

#if GAMMA_CORRECTION_ENABLED
  finalR = pgm_read_byte(&(GAMMA_TABLE[finalR]));
  finalG = pgm_read_byte(&(GAMMA_TABLE[finalG]));
  finalB = pgm_read_byte(&(GAMMA_TABLE[finalB]));
#endif

  analogWrite(pinR, finalR);
  analogWrite(pinG, finalG);
  analogWrite(pinB, finalB);
}

// Calculate and set color for a specific strip based on current timeline time
void updateStrip(const Keyframe* timeline, int kfCount, uint32_t currentMs, int pinR, int pinG, int pinB) {
  if (kfCount == 0) return;
  if (kfCount == 1) {
    Keyframe kf = readKeyframe(timeline, 0);
    writeColor(pinR, pinG, pinB, kf.r, kf.g, kf.b);
    return;
  }

  Keyframe firstKf = readKeyframe(timeline, 0);
  Keyframe lastKf = readKeyframe(timeline, kfCount - 1);

  // Wrap around case (time is between last frame and first frame)
  if (currentMs < firstKf.ms || currentMs >= lastKf.ms) {
    float t;
    uint32_t totalInterval = (${durationMs} - lastKf.ms) + firstKf.ms;
    
    if (currentMs >= lastKf.ms) {
      t = (float)(currentMs - lastKf.ms) / totalInterval;
    } else {
      t = (float)(currentMs + (${durationMs} - lastKf.ms)) / totalInterval;
    }
    
    writeColor(pinR, pinG, pinB, 
               lerpChannel(lastKf.r, firstKf.r, t),
               lerpChannel(lastKf.g, firstKf.g, t),
               lerpChannel(lastKf.b, firstKf.b, t));
    return;
  }

  // Normal interpolation between adjacent keyframes
  for (int i = 0; i < kfCount - 1; i++) {
    Keyframe k1 = readKeyframe(timeline, i);
    Keyframe k2 = readKeyframe(timeline, i + 1);
    
    if (currentMs >= k1.ms && currentMs < k2.ms) {
      float t = (float)(currentMs - k1.ms) / (k2.ms - k1.ms);
      writeColor(pinR, pinG, pinB, 
                 lerpChannel(k1.r, k2.r, t),
                 lerpChannel(k1.g, k2.g, t),
                 lerpChannel(k1.b, k2.b, t));
      return;
    }
  }
}

void setup() {
${strips.map((s) => {
  const cleanName = s.name.toUpperCase().replace(/\s+/g, '_');
  return `  pinMode(STRIP_${cleanName}_PIN_R, OUTPUT);\n  pinMode(STRIP_${cleanName}_PIN_G, OUTPUT);\n  pinMode(STRIP_${cleanName}_PIN_B, OUTPUT);`;
}).join('\n')}
}

void loop() {
${loopCalls}
  delay(10); // Stability buffer
}
`;
  };

  const copyToClipboard = () => {
    navigator.clipboard.writeText(generateArduinoCode());
    setCopied(true);
    setTimeout(() => setCopied(false), 2000);
  };

  const downloadInoFile = () => {
    const element = document.createElement("a");
    const file = new Blob([generateArduinoCode()], {type: 'text/plain'});
    element.href = URL.createObjectURL(file);
    element.download = "luminaRGB.ino";
    document.body.appendChild(element);
    element.click();
    document.body.removeChild(element);
  };

  const activeKeyframe = activeStrip.keyframes[selectedKeyframeIndex] || activeStrip.keyframes[0];

  return (
    <div className="app-container">
      {/* HEADER */}
      <header className="app-header">
        <div className="logo-container">
          <Sparkles className="logo-icon" size={26} />
          <span className="logo-text">Lumina</span>
          <span className="header-tagline">RGB Light Controller</span>
        </div>

        <div style={{ display: 'flex', gap: '16px', alignItems: 'center' }}>
          {/* Web Serial Connector Button */}
          {isSerialConnected ? (
            <button 
              className="btn-secondary" 
              onClick={handleDisconnectSerial} 
              style={{ 
                borderColor: 'var(--color-success)', 
                background: 'rgba(57, 255, 20, 0.05)',
                color: 'var(--color-success)',
                display: 'flex',
                alignItems: 'center',
                gap: '8px'
              }}
            >
              <span style={{ 
                width: '8px', 
                height: '8px', 
                borderRadius: '50%', 
                backgroundColor: 'var(--color-success)',
                boxShadow: '0 0 8px var(--color-success)',
                display: 'inline-block'
              }} />
              Connected
              <Link2Off size={14} />
            </button>
          ) : (
            <button 
              className="btn-primary" 
              onClick={handleConnectSerial}
              style={{ 
                background: 'linear-gradient(135deg, var(--color-pink) 0%, var(--color-accent) 100%)',
                boxShadow: '0 4px 15px rgba(255, 0, 127, 0.25)',
                padding: '8px 16px',
                fontSize: '13px'
              }}
            >
              <Link size={14} />
              Connect Arduino
            </button>
          )}

          {/* Settings Button */}
          <button 
            className="btn-secondary"
            onClick={() => setIsSettingsOpen(true)}
            title="Settings & Exporter"
            style={{ padding: '8px 10px' }}
          >
            <Settings size={16} />
          </button>
        </div>
      </header>

      {/* MAIN CONTENT AREA */}
      <main className="app-main">
        {/* TOP: VISUALIZER */}
        <section className="simulation-window">
          <div className="sim-header">
            <span className="sim-title">
              <Cpu size={16} /> Real-Time Environment Simulator
            </span>
            <span className="sim-badge" style={{ 
              borderColor: isSerialConnected ? 'var(--color-success)' : 'var(--color-warning)',
              color: isSerialConnected ? 'var(--color-success)' : 'var(--color-warning)',
              background: isSerialConnected ? 'rgba(57,255,20,0.05)' : 'rgba(255,184,0,0.05)'
            }}>
              {isSerialConnected ? 'Live Connection Active' : 'Offline Preview'}
            </span>
          </div>

          <div className="sim-viewport">
            {strips.map((strip, index) => {
              const activeColor = getActiveColor(strip);
              const activeHex = rgbToHex(activeColor);

              return (
                <div key={strip.id} style={{ position: 'relative' }}>
                  <div className="strip-label">
                    {strip.name} ({strip.location})
                  </div>
                  <div className="led-strip-simulation">
                    <div 
                      className="strip-glow-overlay"
                      style={{ 
                        background: `linear-gradient(90deg, ${activeHex}, rgba(0,0,0,0.5), ${activeHex})`,
                        boxShadow: `0 0 35px 12px ${activeHex}`
                      }}
                    />
                    <div className="strip-physical-leds">
                      {Array.from({ length: 15 }).map((_, i) => (
                        <div 
                          key={i} 
                          className="led-node" 
                          style={{ 
                            backgroundColor: activeHex, 
                            boxShadow: `0 0 10px 2px ${activeHex}` 
                          }}
                        />
                      ))}
                    </div>
                  </div>
                </div>
              );
            })}
          </div>
        </section>

        {/* BOTTOM WORKSPACE GRID: SYMMETRIC CONTROLLERS */}
        <div className="workspace-grid">
          
          {/* LEFT COLUMN: PRESET EFFECTS CONTROLLER */}
          <div className="timeline-pane" style={{ display: 'flex', flexDirection: 'column', gap: '16px' }}>
            <div className="timeline-header">
              <h3 style={{ fontSize: '15px', fontWeight: '700', color: 'var(--color-cyan)', display: 'flex', alignItems: 'center', gap: '8px' }}>
                <Sparkles size={18} /> Preset Effects
              </h3>
              <span className="text-muted" style={{ fontSize: '11px' }}>
                Dynamic color loops
              </span>
            </div>

            <div className="glass-card" style={{ flexGrow: 1, display: 'flex', flexDirection: 'column', gap: '20px' }}>
              <div className="presets-grid" style={{ gridTemplateColumns: '1fr 1fr', gap: '12px' }}>
                <button 
                  className={`btn-secondary ${activePreset === 'cyberpunk' ? 'active' : ''}`}
                  onClick={() => applyPresetTimeline('cyberpunk')}
                  style={{ height: '54px', fontSize: '13px', fontWeight: '600' }}
                >
                  Cyber Neon
                </button>
                <button 
                  className={`btn-secondary ${activePreset === 'sunset' ? 'active' : ''}`}
                  onClick={() => applyPresetTimeline('sunset')}
                  style={{ height: '54px', fontSize: '13px', fontWeight: '600' }}
                >
                  Sunset Loop
                </button>
                <button 
                  className={`btn-secondary ${activePreset === 'aurora' ? 'active' : ''}`}
                  onClick={() => applyPresetTimeline('aurora')}
                  style={{ height: '54px', fontSize: '13px', fontWeight: '600' }}
                >
                  Aurora Wave
                </button>
                <button 
                  className={`btn-secondary ${activePreset === 'fireplace' ? 'active' : ''}`}
                  onClick={() => applyPresetTimeline('fireplace')}
                  style={{ height: '54px', fontSize: '13px', fontWeight: '600' }}
                >
                  Fireplace
                </button>
              </div>

              {/* Playback & Speed Controls */}
              <div style={{ marginTop: 'auto', borderTop: '1px solid rgba(255,255,255,0.06)', paddingTop: '16px', display: 'flex', flexDirection: 'column', gap: '12px' }}>
                <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                  <span className="text-muted" style={{ fontSize: '12px' }}>Animation Playback</span>
                  <span className="time-readout" style={{ fontSize: '11px', fontFamily: 'var(--font-mono)' }}>
                    {controlMode === 'timeline' ? `${Math.round(currentTime * loopDuration * 10) / 10}s / ${loopDuration}s` : 'Paused'}
                  </span>
                </div>

                <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', gap: '12px' }}>
                  <div className="playback-buttons" style={{ margin: 0 }}>
                    <button 
                      className={`play-btn ${isPlaying ? 'active' : ''}`}
                      disabled={controlMode !== 'timeline'}
                      onClick={() => setIsPlaying(!isPlaying)}
                      title={isPlaying ? "Pause Preset" : "Play Preset"}
                    >
                      {isPlaying ? <Pause size={16} /> : <Play size={16} />}
                    </button>
                    <button 
                      className="play-btn"
                      disabled={controlMode !== 'timeline'}
                      onClick={() => setCurrentTime(0.0)}
                      title="Reset Preset Loop"
                    >
                      <RotateCcw size={16} />
                    </button>
                  </div>

                  <div style={{ display: 'flex', gap: '4px' }}>
                    {[1, 2, 5].map((multiplier) => (
                      <button 
                        key={multiplier}
                        className={`btn-secondary ${speedMultiplier === multiplier && controlMode === 'timeline' ? 'active' : ''}`}
                        disabled={controlMode !== 'timeline'}
                        style={{ 
                          padding: '4px 8px', 
                          fontSize: '11px',
                          borderColor: speedMultiplier === multiplier && controlMode === 'timeline' ? 'var(--color-cyan)' : 'rgba(255,255,255,0.1)'
                        }}
                        onClick={() => setSpeedMultiplier(multiplier)}
                      >
                        {multiplier}x
                      </button>
                    ))}
                  </div>

                  <select 
                    className="input-field" 
                    disabled={controlMode !== 'timeline'}
                    value={loopDuration} 
                    onChange={(e) => setLoopDuration(Number(e.target.value))}
                    style={{ padding: '4px 8px', fontSize: '11px', width: '90px' }}
                  >
                    <option value={10}>10s Loop</option>
                    <option value={30}>30s Loop</option>
                    <option value={60}>60s Loop</option>
                  </select>
                </div>
              </div>
            </div>
          </div>

          {/* RIGHT COLUMN: MANUAL COLOR CONTROLLER */}
          <div className="timeline-pane" style={{ display: 'flex', flexDirection: 'column', gap: '16px' }}>
            <div className="timeline-header">
              <h3 style={{ fontSize: '15px', fontWeight: '700', color: 'var(--color-success)', display: 'flex', alignItems: 'center', gap: '8px' }}>
                <Sliders size={18} /> Solid Color
              </h3>
              <span className="text-muted" style={{ fontSize: '11px' }}>
                Static strip coloring
              </span>
            </div>

            <div className="glass-card" style={{ flexGrow: 1, display: 'flex', flexDirection: 'column', gap: '16px' }}>
              <div className="editor-panel-header" style={{ color: 'var(--color-success)', borderColor: 'rgba(163,190,140,0.15)' }}>
                Active Color Preview
              </div>

              <div style={{ display: 'flex', gap: '20px', alignItems: 'center' }}>
                {/* Dynamic Color Preview Circle */}
                <div 
                  style={{ 
                    width: '74px', 
                    height: '74px', 
                    borderRadius: '50%', 
                    background: rgbToHex(controlMode === 'solid' ? (solidColors[selectedStripId] || {r:0,g:0,b:0}) : getActiveColor(activeStrip)),
                    boxShadow: `0 0 24px ${rgbToHex(controlMode === 'solid' ? (solidColors[selectedStripId] || {r:0,g:0,b:0}) : getActiveColor(activeStrip))}`,
                    border: '2px solid rgba(255,255,255,0.2)',
                    flexShrink: 0,
                    transition: 'all 0.15s ease'
                  }} 
                />

                <div style={{ flexGrow: 1 }}>
                  <label style={{ fontSize: '11px', color: 'var(--color-text-secondary)', marginBottom: '6px', display: 'block' }}>
                    Select Custom Color
                  </label>
                  <input 
                    type="color" 
                    value={rgbToHex(controlMode === 'solid' ? (solidColors[selectedStripId] || {r:0,g:0,b:0}) : getActiveColor(activeStrip))}
                    onChange={(e) => handleColorSelect(hexToRgb(e.target.value))}
                    style={{ 
                      width: '100%', 
                      height: '42px', 
                      border: 'none', 
                      borderRadius: '8px', 
                      cursor: 'pointer',
                      background: 'transparent'
                    }}
                  />
                </div>
              </div>

              <div style={{ marginTop: 'auto' }}>
                <label style={{ fontSize: '11px', color: 'var(--color-text-secondary)', display: 'block', marginBottom: '8px' }}>
                  Quick Swatches
                </label>
                <div className="preset-colors" style={{ gridTemplateColumns: 'repeat(4, 1fr)', gap: '8px' }}>
                  {PRESET_LIGHTS.map((preset) => (
                    <div 
                      key={preset.name}
                      className="preset-color-dot"
                      style={{ backgroundColor: preset.hex, width: '100%', height: '32px', borderRadius: '6px' }}
                      onClick={() => handleColorSelect(preset.rgb)}
                      title={preset.name}
                    />
                  ))}
                </div>
              </div>
            </div>
          </div>

        </div>
      </main>

      {/* SETTINGS MODAL */}
      {isSettingsOpen && (
        <div className="modal-backdrop" onClick={() => setIsSettingsOpen(false)}>
          <div className="modal-content-card" onClick={(e) => e.stopPropagation()}>
            <div className="modal-header-section">
              <span className="modal-title">Hardware & Calibration Settings</span>
              <button className="modal-close-btn" onClick={() => setIsSettingsOpen(false)}>
                <X size={18} />
              </button>
            </div>

            <div className="settings-badge-info">
              <Sparkles size={24} style={{ color: 'var(--color-warning)', flexShrink: 0 }} />
              <div>
                <strong>Notice:</strong> Adjusting channel scales tunes the live browser visualizer and the serial packets immediately. Updating pin counts or flashing standalone programs requires code compilation.
              </div>
            </div>

            {/* SECTION 1: GAMMA & CALIBRATION */}
            <div className="modal-section-title">Color Calibration & Gamma</div>
            
            <div className="form-group" style={{ flexDirection: 'row', alignItems: 'center', gap: '10px', marginBottom: '16px' }}>
              <input 
                type="checkbox" 
                id="gammaToggle" 
                checked={gammaEnabled} 
                onChange={(e) => setGammaEnabled(e.target.checked)} 
                style={{ width: '16px', height: '16px', accentColor: 'var(--color-cyan)', cursor: 'pointer' }}
              />
              <label htmlFor="gammaToggle" style={{ fontSize: '13px', fontWeight: '600', color: 'var(--color-text-primary)', cursor: 'pointer' }}>
                Enable Gamma Correction (2.2)
              </label>
            </div>

            <div className="calibration-group">
              <div style={{ fontSize: '12px', color: 'var(--color-text-secondary)', fontWeight: '600', marginBottom: '4px' }}>
                RGB Brightness Calibration Offsets
              </div>
              <div className="calibration-slider">
                <label>Red Scale</label>
                <input 
                  type="range" 
                  min="0.5" 
                  max="1.0" 
                  step="0.05" 
                  value={calibration.r} 
                  onChange={(e) => setCalibration({ ...calibration, r: parseFloat(e.target.value) })} 
                />
                <span>{Math.round(calibration.r * 100)}%</span>
              </div>
              <div className="calibration-slider">
                <label>Green Scale</label>
                <input 
                  type="range" 
                  min="0.5" 
                  max="1.0" 
                  step="0.05" 
                  value={calibration.g} 
                  onChange={(e) => setCalibration({ ...calibration, g: parseFloat(e.target.value) })} 
                />
                <span>{Math.round(calibration.g * 100)}%</span>
              </div>
              <div className="calibration-slider">
                <label>Blue Scale</label>
                <input 
                  type="range" 
                  min="0.5" 
                  max="1.0" 
                  step="0.05" 
                  value={calibration.b} 
                  onChange={(e) => setCalibration({ ...calibration, b: parseFloat(e.target.value) })} 
                />
                <span>{Math.round(calibration.b * 100)}%</span>
              </div>
            </div>

            {/* SECTION 2: CONFIGURE LED STRIPS */}
            <div className="modal-section-title">Hardware Zone Config</div>
            <div style={{ marginBottom: '16px' }}>
              {strips.map((strip, index) => (
                <div key={strip.id} className="modal-strip-row">
                  <div style={{ display: 'flex', flexDirection: 'column', gap: '2px' }}>
                    <span style={{ fontSize: '14px', fontWeight: '700' }}>{strip.name}</span>
                    <span style={{ fontSize: '11px', color: 'var(--color-text-secondary)' }}>
                      Pins: Red={strip.pinR}, Green={strip.pinG}, Blue={strip.pinB}
                    </span>
                  </div>
                  <button 
                    className="action-btn"
                    onClick={(e) => handleRemoveStrip(strip.id, e)}
                    style={{ padding: '6px' }}
                    title="Delete Zone"
                  >
                    <Trash2 size={16} />
                  </button>
                </div>
              ))}
            </div>

            {/* SECTION 3: ARDUINO EXPORTER (ADVANCED) */}
            <div className="modal-section-title">Arduino Standalone Exporter</div>
            <p className="text-muted" style={{ fontSize: '11px', margin: '4px 0 12px 0' }}>
              To program your Arduino to run independent lighting loops without a PC connection, copy this code:
            </p>
            <div style={{ display: 'flex', gap: '8px', marginBottom: '12px' }}>
              <button 
                className="btn-secondary" 
                onClick={copyToClipboard}
                style={{ padding: '6px 12px', fontSize: '12px' }}
              >
                {copied ? <Check size={14} style={{ color: 'var(--color-success)' }} /> : <Copy size={14} />}
                {copied ? 'Copied!' : 'Copy Code'}
              </button>
              <button 
                className="btn-primary" 
                onClick={downloadInoFile}
                style={{ padding: '6px 12px', fontSize: '12px' }}
              >
                <Download size={14} /> Download Sketch (.ino)
              </button>
            </div>
            <div className="code-container" style={{ maxHeight: '180px' }}>
              <pre><code>{generateArduinoCode()}</code></pre>
            </div>

            <button 
              className="btn-secondary" 
              onClick={() => setIsSettingsOpen(false)}
              style={{ width: '100%', marginTop: '16px', padding: '12px' }}
            >
              Done / Close
            </button>
          </div>
        </div>
      )}
    </div>
  );
}

export default App;
