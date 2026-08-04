# ADR-002: Dynamic Range Headroom Compression, Universal Noise Floor Gating & PWM Dual-Slew Rate Limiting for INMP441 Audio DSP

- **Status**: Accepted
- **Date**: 2026-08-03
- **Deciders**: User & AI Pair
- **Technical Context**: INMP441 I2S MEMS Microphone Audio DSP, 12-Bit Gamma 2.8 PWM Engine (`rgb-node`)

---

## 1. Context and Problem Statement

During audio-reactive lighting performance testing, two primary physical/visual usability issues were identified:
1. **Flickering & Chatter**: Un-damped micro-fluctuations in room ambient noise near $0.0\text{f}$ volume caused rapid, unsightly 60Hz PWM duty cycle chatter due to the non-linear $x^{2.8}$ perceptual gamma curve.
2. **Early Saturation / Clipping**: Whispering or moderate whistling at medium room volume caused calculated audio energy to saturate to $100\%$ maximum brightness instantly, crushing dynamic range.
3. **UI Slider Misalignment**: Generic sliders were displayed across all 5 music profiles in the React Web UI, even though certain parameters (such as beat threshold sensitivity) only mathematically apply to specific audio profiles.

---

## 2. Decision Outcome

We decided to implement a **Universal Dynamic Range & Dual-Slew Rate Limiting Architecture** in C++ coupled with **Profile-Aware Contextual UI Panels** in React.

### Key Architectural Decisions:

1. **Universal Noise Floor Cutoff Gate**:
   - Applied a configurable floor threshold ($0\%\dots25\%$) universally across all audio energy bands (`bass`, `mid`, `treble`, `totalAmp`).
   - Below the noise threshold, signal output drops to clean $0.0\text{f}$ (absolute blackout) to eliminate dim-flicker during quiet moments.

2. **Dynamic Headroom Margin AGC ($100\%\dots250\%$)**:
   - Expanded Automatic Gain Control (AGC) peak tracking headroom so normal speaking and whistling operate in the middle $40\%\dots80\%$ of output range, reserving $100\%$ saturation strictly for loud transients.

3. **Logarithmic (dB) vs Linear Loudness Scale**:
   - Added perceptual logarithmic volume mapping:
     $$\text{logAmp} = \log_{10}(1.0 + 9.0 \times \text{normTotal})$$
     matching human ear sound pressure perception.

4. **Dual-Slew Rate Limiter (Response Agility)**:
   - Implemented an IIR low-pass rate limiter ($\alpha = 0.04 \dots 0.40$) on normalized RGB outputs (`g_currentR`, `g_currentG`, `g_currentB`) before 12-bit Gamma 2.8 duty cycle mapping, enforcing smooth PWM change rates.

5. **Master Brightness as Absolute Ceiling**:
   - Capped all audio dynamic energy output under Master Brightness as the absolute upper bound limit:
     $$\text{Final PWM} = \text{Gamma12}\left(\text{ProfileEnergy} \times \frac{\text{MasterBrightness}}{255}\right)$$

6. **Contextual Profile UI Panels**:
   - Separated the React Web UI Music Sync page into clean, profile-specific control cards. Generic sliders were replaced with profile-appropriate knobs (e.g. Beat Sensitivity only on Beat Pulse, Pitch Glide on Pitch-to-Hue).

---

## 3. Consequences

### Positive:
- **Zero Flickering**: Quiet ambient room noise results in steady, flicker-free blackout.
- **Rich Dynamic Range**: Whistling glides smoothly across middle output levels without hitting max saturation.
- **Clean UI**: Web UI only displays controls that actively affect the selected music profile.

### Negative / Trade-offs:
- **State Complexity**: Expanded `LightState` schema with 8 new parameters (`headroom`, `responseAgility`, `beatDecay`, `pitchLowHz`, `pitchHighHz`, `pitchSmooth`, `ambientGlow`, `useLogScale`).
