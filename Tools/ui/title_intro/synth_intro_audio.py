"""인트로 소리 합성 — 외부 음원 없이 numpy 로 만든다(라이선스 걱정 없음).

입력: SourceArt/UI/Intro/SpaceFireworks_v1/Previs/<v>/events.json (Blender 가 쓴 폭죽 시각·화면 위치)
출력: SourceArt/UI/Intro/SpaceFireworks_v1/Audio/FF_Intro_SpaceFireworks.wav (48kHz 16bit 스테레오, 영상 8초 + 꼬리 2.2초)

구성(모두 `assumption` — 귀로 듣고 고친다):
  우주 앰비언스(낮은 화음·바람) · 별 반짝임
  폭죽마다: 올라가는 휘파람 → 펑(저음+잡음) → 타닥 → 종소리(터질 때마다 한 음씩 올라가 멜로디가 된다)
  한꺼번에 물들 때 차오르는 반짝임 · 마지막 폭죽 일제 · 빛이 퍼질 때 큰 바람 → 밝은 장화음(UE 지구 공개까지 이어짐)

python Tools/ui/title_intro/synth_intro_audio.py [events.json 경로]
"""
from pathlib import Path
import json
import sys
import wave

import numpy as np

ROOT = Path(__file__).resolve().parents[3]
EVENTS = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / 'SourceArt/UI/Intro/SpaceFireworks_v1/Previs/v001/events.json'
OUT = ROOT / 'SourceArt/UI/Intro/SpaceFireworks_v1/Audio/FF_Intro_SpaceFireworks.wav'
SR = 48000
TAIL = 2.2
rng = np.random.default_rng(20261002)


def tvec(seconds):
    return np.arange(int(seconds * SR)) / SR


def env(n, attack, decay, hold=0.0):
    t = np.arange(n) / SR
    a = np.clip(t / max(attack, 1e-4), 0, 1)
    d = np.where(t < attack + hold, 1.0, np.exp(-(t - attack - hold) / max(decay, 1e-4)))
    return a * d


def band(sig, lo, hi):
    """FFT 대역 통과. 경계는 부드럽게."""
    spec = np.fft.rfft(sig)
    f = np.fft.rfftfreq(len(sig), 1 / SR)
    w = np.clip((f - lo * 0.8) / (lo * 0.4 + 1e-6), 0, 1) * np.clip((hi * 1.25 - f) / (hi * 0.5 + 1e-6), 0, 1)
    return np.fft.irfft(spec * w, n=len(sig))


def bell(freq, seconds, bright=1.0):
    t = tvec(seconds)
    out = np.zeros_like(t)
    for ratio, gain, dec in ((1.0, 1.0, 1.4), (2.0, 0.45, 0.9), (2.76, 0.35 * bright, 0.6), (5.4, 0.18 * bright, 0.25), (8.93, 0.08 * bright, 0.12)):
        out += gain * np.sin(2 * np.pi * freq * ratio * t) * np.exp(-t / dec)
    return out * np.clip(t / 0.004, 0, 1)


def pan_gain(x):
    """x: 화면 가로 0~1 → 왼쪽·오른쪽 이득(등전력)."""
    p = np.clip((x - 0.5) * 1.4, -1, 1)
    angle = (p + 1) * np.pi / 4
    return np.cos(angle), np.sin(angle)


class Mix:
    def __init__(self, seconds):
        self.n = int(seconds * SR)
        self.dry = np.zeros((self.n, 2))
        self.wet = np.zeros((self.n, 2))     # 잔향으로 보낼 것

    def add(self, sig, start, gain=1.0, x=0.5, send=0.0):
        i0 = int(start * SR)
        if i0 >= self.n:
            return
        sig = sig[: self.n - i0] * gain
        gl, gr = pan_gain(x)
        self.dry[i0:i0 + len(sig), 0] += sig * gl
        self.dry[i0:i0 + len(sig), 1] += sig * gr
        if send:
            self.wet[i0:i0 + len(sig), 0] += sig * gl * send
            self.wet[i0:i0 + len(sig), 1] += sig * gr * send

    def render(self):
        # 잔향: 지수 감쇠 잡음 임펄스 응답을 FFT 로 컨볼루션(좌우 다른 잡음 → 넓게)
        ir_len = int(2.8 * SR)
        out = self.dry.copy()
        for ch in range(2):
            ir = rng.standard_normal(ir_len) * np.exp(-np.arange(ir_len) / SR / 0.9)
            ir = band(ir, 200, 9000)
            ir /= np.sqrt(np.sum(ir ** 2))
            size = 1 << int(np.ceil(np.log2(self.n + ir_len)))
            conv = np.fft.irfft(np.fft.rfft(self.wet[:, ch], size) * np.fft.rfft(ir, size), size)[: self.n]
            out[:, ch] += conv * 0.55
        return out


def whistle(seconds=0.38):
    t = tvec(seconds)
    f = 700 + 1300 * (t / seconds) ** 1.4 + 25 * np.sin(2 * np.pi * 23 * t)
    phase = 2 * np.pi * np.cumsum(f) / SR
    hiss = band(rng.standard_normal(len(t)), 2500, 9000) * 0.35
    return (np.sin(phase) * 0.6 + hiss) * np.sin(np.pi * t / seconds) ** 0.7 * 0.35


def boom(big=False):
    seconds = 1.6 if big else 1.1
    t = tvec(seconds)
    f = (75 if big else 90) * np.exp(-t / 0.25) + 42
    thump = np.sin(2 * np.pi * np.cumsum(f) / SR) * np.exp(-t / (0.32 if big else 0.22))
    burst = band(rng.standard_normal(len(t)), 120, 4500) * np.exp(-t / 0.09)
    air = band(rng.standard_normal(len(t)), 600, 7000) * np.exp(-t / 0.5) * 0.25
    return (thump * 1.0 + burst * 0.7 + air) * np.clip(t / 0.002, 0, 1)


def crackle(seconds=1.4, density=90):
    n = int(seconds * SR)
    out = np.zeros(n)
    count = int(density * seconds)
    for _ in range(count):
        at = rng.uniform(0.08, seconds) ** 1.3 / seconds ** 0.3
        i = int(at * SR)
        if i >= n - 400:
            continue
        click = rng.standard_normal(300) * np.exp(-np.arange(300) / 40.0)
        out[i:i + 300] += click * rng.uniform(0.2, 1.0) * np.exp(-at / 0.7)
    return band(out, 1500, 12000) * 0.5


def ambience(seconds):
    t = tvec(seconds)
    drone = np.zeros_like(t)
    for f, g in ((55.0, 0.5), (82.4, 0.32), (110.0, 0.25), (164.8, 0.12), (220.0, 0.08)):
        drone += g * np.sin(2 * np.pi * f * t + rng.uniform(0, 6)) * (0.8 + 0.2 * np.sin(2 * np.pi * 0.11 * t + f))
    wind = band(rng.standard_normal(len(t)), 150, 1200) * 0.35 * (0.6 + 0.4 * np.sin(2 * np.pi * 0.07 * t))
    return drone * 0.5 + wind


def twinkles(seconds, count=26):
    out = np.zeros(int(seconds * SR))
    for _ in range(count):
        at = rng.uniform(0.3, seconds - 0.5)
        tone = bell(rng.uniform(2400, 5200), 0.5, bright=0.4) * rng.uniform(0.05, 0.12)
        i = int(at * SR)
        out[i:i + len(tone)] += tone[: len(out) - i]
    return out


def swell(seconds, lo=300, hi=6000):
    """차오르는 바람: 잡음 대역을 구간마다 올리며 이어 붙인다."""
    n = int(seconds * SR)
    out = np.zeros(n)
    steps = 12
    seg = n // steps
    for s in range(steps):
        a = s / (steps - 1)
        chunk = band(rng.standard_normal(seg * 2), lo * (1 + 6 * a), min(hi, lo * (3 + 20 * a)))
        win = np.hanning(seg * 2)
        i0 = max(0, s * seg - seg // 2)
        i1 = min(n, i0 + seg * 2)
        out[i0:i1] += (chunk * win)[: i1 - i0] * (0.2 + 0.8 * a)
    return out * np.linspace(0.2, 1.0, n) ** 1.5


def chord(freqs, seconds, attack=0.6, decay=2.5):
    t = tvec(seconds)
    out = np.zeros_like(t)
    for f in freqs:
        for detune in (-0.12, 0.0, 0.12):
            out += np.sin(2 * np.pi * (f + detune) * t + rng.uniform(0, 6)) * 0.33
        out += 0.25 * np.sin(2 * np.pi * f * 2 * t) * np.exp(-t / 1.5)
    shape = np.clip(t / attack, 0, 1) ** 2 * np.where(t < attack, 1.0, np.exp(-(t - attack) / decay))
    return out * shape / len(freqs)


def main():
    events = json.loads(EVENTS.read_text(encoding='utf-8'))
    end = events['end'] + TAIL
    mix = Mix(end)
    fade_in = events['fade_in']

    amb = ambience(events['sky'][1] + 0.6)
    amb *= np.clip(np.arange(len(amb)) / SR / (fade_in + 0.6), 0, 1)
    amb *= np.clip((events['sky'][1] + 0.6 - np.arange(len(amb)) / SR) / 1.2, 0, 1)
    mix.add(amb, 0.0, gain=0.22, send=0.2)
    mix.add(twinkles(5.5), 0.2, gain=0.6, x=0.6, send=0.6)

    # 도시 폭죽: A 장조 오음계로 한 음씩 올라간다 → 멜로디
    scale = [440.0 * 2 ** (s / 12) for s in (0, 2, 4, 7, 9, 12, 14, 16, 19, 21, 24, 26, 28)]
    city = [b for b in events['bursts'] if not b['finale']]
    for i, b in enumerate(city):
        vis = 1.0 if b['visible'] else 0.45
        mix.add(whistle(), b['t'] - 0.36, gain=0.5 * vis, x=b['x'], send=0.25)
        mix.add(boom(), b['t'], gain=0.9 * vis, x=b['x'], send=0.35)
        mix.add(crackle(), b['t'] + 0.12, gain=0.55 * vis, x=b['x'], send=0.3)
        note = scale[min(i, len(scale) - 1)]
        mix.add(bell(note, 2.2) * 0.5, b['t'] + 0.04, gain=0.42 * vis, x=b['x'], send=0.7)
        mix.add(bell(note * 1.5, 1.6) * 0.18, b['t'] + 0.04, gain=0.42 * vis, x=b['x'], send=0.7)

    # 한꺼번에 물들기: 차오르는 반짝임과 바람
    a0, a1 = events['all']
    mix.add(swell(a1 - a0 + 0.3, 500, 9000), a0, gain=0.22, send=0.5)
    for j in range(10):
        f = scale[j % len(scale)] * 2
        mix.add(bell(f, 1.0, bright=0.6), a0 + j * (a1 - a0) / 10, gain=0.16, x=0.3 + 0.4 * (j % 2), send=0.8)

    # 마지막 폭죽 일제
    for b in [b for b in events['bursts'] if b['finale']]:
        mix.add(whistle(0.3), b['t'] - 0.3, gain=0.35, x=b['x'], send=0.25)
        mix.add(boom(big=True), b['t'], gain=0.75, x=b['x'], send=0.45)
        mix.add(crackle(1.8, 120), b['t'] + 0.15, gain=0.5, x=b['x'], send=0.35)

    # 빛이 퍼짐: 큰 바람 → 밝은 장화음(A 장조), 흰 빛 정점에 맞춰 크게 → UE 지구 공개까지 이어진다
    s0, s1 = events['sky']
    f0, f1 = events['flash']
    mix.add(swell(f1 - s0, 250, 12000), s0, gain=0.42, send=0.6)
    mix.add(chord([220.0, 277.18, 329.63, 440.0, 554.37, 659.25], end - (f0 - 0.6), attack=f1 - f0 + 0.6, decay=1.6), f0 - 0.6,
            gain=0.55, send=0.5)
    for j, f in enumerate((880.0, 1108.73, 1318.51, 1760.0)):
        mix.add(bell(f, 2.6), f1 - 0.25 + j * 0.06, gain=0.28, x=0.35 + 0.1 * j, send=0.8)

    out = mix.render()
    # 꼬리 정리와 마스터: 부드러운 리미터 → -1 dBFS
    t = np.arange(mix.n) / SR
    out *= np.clip((end - t) / 0.6, 0, 1)[:, None]
    out = np.tanh(out * 1.4)
    out *= 10 ** (-1 / 20) / max(np.max(np.abs(out)), 1e-6)
    pcm = (out * 32767).astype('<i2')
    OUT.parent.mkdir(parents=True, exist_ok=True)
    with wave.open(str(OUT), 'wb') as w:
        w.setnchannels(2)
        w.setsampwidth(2)
        w.setframerate(SR)
        w.writeframes(pcm.tobytes())
    print('저장:', OUT, f'{end:.2f}초', '도시 폭죽', len(city), '마지막', len(events['bursts']) - len(city))


if __name__ == '__main__':
    main()
