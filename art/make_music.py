#!/usr/bin/env python3
"""Процедурная музыка для игры: синтез на numpy, бесшовные петли, OGG через ffmpeg.

Использование: python3 art/make_music.py engine/assets/music
Треки: village.ogg — спокойная ночная деревня; battle.ogg — бой с барабанами.
Инструменты: щипковая струна (Карплус — Стронг, звучит как арфа/лютня), мягкий струнный пэд, бас,
FM-колокольчики, барабаны (тайко). Реверберация — свёртка с синтетическим откликом зала через FFT.
"""
import os
import subprocess
import sys
import tempfile
import wave

import numpy as np

DST = sys.argv[1] if len(sys.argv) > 1 else "engine/assets/music"
SR = 32000


def hz(midi):
    return 440.0 * 2 ** ((midi - 69) / 12)


def env(n, a, r, sustain=1.0):
    """Огибающая: атака a, затухание до конца (r — экспонента)."""
    t = np.arange(n) / SR
    e = np.minimum(1, t / max(a, 1e-4)) * np.exp(-t / r) * sustain
    return e


def pluck(f, dur, bright=0.5, decay=0.996, rng=None):
    """Карплус — Стронг, векторизованный по блокам длиной в период."""
    n = int(dur * SR)
    N = max(2, int(SR / f))
    rng = rng or np.random.default_rng()
    buf = rng.uniform(-1, 1, N)
    # смягчить начальный шум — тембр теплее
    for _ in range(2 + int((1 - bright) * 8)):
        buf = 0.5 * (buf + np.roll(buf, 1))
    out = np.empty(n + N)
    out[:N] = buf
    i = N
    prev_last = buf[-1]
    while i < n + N:
        blk = out[i - N:i]
        shifted = np.concatenate(([prev_last], blk[:-1]))
        new = decay * 0.5 * (blk + shifted)
        m = min(N, n + N - i)
        out[i:i + m] = new[:m]
        prev_last = blk[-1]
        i += N
    y = out[:n]
    y *= np.minimum(1, np.arange(n) / (0.002 * SR))
    return y


def pad(freqs, dur, vol=0.12):
    n = int(dur * SR)
    t = np.arange(n) / SR
    y = np.zeros(n)
    for f in freqs:
        for det in (-0.12, 0.0, 0.11):
            ff = f * 2 ** (det / 12)
            for h, a in ((1, 1.0), (2, 0.35), (3, 0.18), (4, 0.08)):
                y += a * np.sin(2 * np.pi * ff * h * t + h * det * 7)
    e = np.minimum(1, t / (dur * 0.35)) * np.minimum(1, (dur - t) / (dur * 0.35))
    vib = 1 + 0.15 * np.sin(2 * np.pi * 0.2 * t)
    return y * e * vib * vol / (len(freqs) * 3)


def bass(f, dur, vol=0.35):
    n = int(dur * SR)
    t = np.arange(n) / SR
    y = np.sin(2 * np.pi * f * t) + 0.25 * np.sin(4 * np.pi * f * t)
    return y * env(n, 0.03, dur * 0.6) * vol


def bell(f, dur, vol=0.12):
    n = int(dur * SR)
    t = np.arange(n) / SR
    idx = 3.0 * np.exp(-t * 2.5)
    y = np.sin(2 * np.pi * f * t + idx * np.sin(2 * np.pi * f * 3.5 * t))
    return y * env(n, 0.002, 1.1) * vol


def drum(dur=0.6, f0=110, f1=45, vol=0.9, noise=0.25, rng=None):
    n = int(dur * SR)
    t = np.arange(n) / SR
    f = f1 + (f0 - f1) * np.exp(-t * 25)
    ph = 2 * np.pi * np.cumsum(f) / SR
    rng = rng or np.random.default_rng()
    y = np.sin(ph) * np.exp(-t * 6) + noise * rng.uniform(-1, 1, n) * np.exp(-t * 40)
    return y * vol


def lowpass(x, fc):
    """Мягкий срез верхов через FFT: 4-й порядок, без фазовых искажений."""
    X = np.fft.rfft(x)
    f = np.fft.rfftfreq(len(x), 1 / SR)
    return np.fft.irfft(X / (1 + (f / fc) ** 4), len(x))


def reverb(x, seconds=2.4, wet=0.32, seed=3):
    rng = np.random.default_rng(seed)
    n = int(seconds * SR)
    t = np.arange(n) / SR
    ir = rng.uniform(-1, 1, n) * np.exp(-t * 3.2)
    ir[: int(0.012 * SR)] = 0
    # чуть тусклее хвост: сглаживание
    for _ in range(12):
        ir = 0.5 * (ir + np.roll(ir, 1))
    ir /= np.sqrt(np.sum(ir ** 2))
    L = len(x) + n
    size = 1 << (L - 1).bit_length()
    y = np.fft.irfft(np.fft.rfft(x, size) * np.fft.rfft(ir, size), size)[:L]
    out = np.concatenate((x, np.zeros(n))) * (1 - wet) + y * wet * 0.6
    return out


class Track:
    def __init__(self, seconds, fc=3200):
        self.fc = fc
        self.n = int(seconds * SR)
        self.buf = np.zeros(self.n + 6 * SR)

    def add(self, at, sig):
        i = int(at * SR)
        self.buf[i:i + len(sig)] += sig[: len(self.buf) - i]

    def render(self, wet=0.32):
        y = lowpass(reverb(self.buf, wet=wet), self.fc)
        loop = y[: self.n].copy()
        tail = y[self.n:]
        loop[: len(tail)] += tail[: self.n]   # хвост петли заворачиваем в начало — шов не слышен
        loop /= np.max(np.abs(loop)) + 1e-9
        return loop * 0.85


# лады: ре минор (дорийский оттенок)
D = 62
CHORDS_V = [(0, [0, 3, 7]), (-4, [0, 4, 7]), (3, [0, 4, 7]), (-2, [0, 4, 7]),   # Dm  Bb  F  C
            (0, [0, 3, 7]), (5, [0, 3, 7]), (-4, [0, 4, 7]), (-5, [0, 4, 7])]  # Dm  Gm  Bb  A
SCALE = [0, 2, 3, 5, 7, 9, 10]


def village():
    rng = np.random.default_rng(42)
    bpm = 72
    beat = 60 / bpm
    bars = 16
    tr = Track(bars * 4 * beat)
    for bar in range(bars):
        root, tri = CHORDS_V[bar % 8]
        t0 = bar * 4 * beat
        notes = [D + root + i for i in tri]
        tr.add(t0, pad([hz(n - 12) for n in notes], 4 * beat + 1.0))
        tr.add(t0, bass(hz(D + root - 24), 2 * beat + 0.5))
        tr.add(t0 + 2 * beat, bass(hz(D + root - 24 + (7 if bar % 2 else 0)), 2 * beat + 0.5, 0.28))
        # арпеджио арфы: восьмые, рисунок вверх-вниз
        arp = notes + [notes[0] + 12, notes[1] + 12, notes[0] + 12] + notes[2:0:-1]
        for k in range(8):
            if bar % 4 == 3 and k in (6, 7):
                continue
            n = arp[k % len(arp)]
            tr.add(t0 + k * beat / 2 + rng.uniform(0, 0.012), pluck(hz(n), 2.2, 0.35, 0.9965, rng) * (0.22 if k % 2 else 0.3))
        # мелодия на лютне: случайное блуждание по ладу, на сильных долях — звуки аккорда
        if bar >= 4:
            deg = 4
            for k in range(4 if bar % 2 == 0 else 3):
                if k == 0:
                    n = notes[rng.integers(0, 3)] + 12
                else:
                    deg = int(np.clip(deg + rng.choice([-2, -1, 1, 2]), 0, 9))
                    n = D + SCALE[deg % 7] + 12 * (deg // 7) + 12
                dur = beat * (1.5 if k == 0 else 0.75)
                at = t0 + [0, 1.5, 2.5, 3.0][k] * beat
                tr.add(at, pluck(hz(n), 2.5, 0.6, 0.9975, rng) * 0.32)
        if bar % 4 == 2:
            tr.add(t0 + 3 * beat, bell(hz(notes[2] + 24), 3.0, 0.08))
    return tr.render(0.34)


CHORDS_B = [(0, [0, 3, 7]), (0, [0, 3, 7]), (-4, [0, 4, 7]), (-2, [0, 4, 7]),
            (0, [0, 3, 7]), (3, [0, 4, 7]), (-5, [0, 4, 7]), (-5, [0, 4, 7])]


def battle():
    rng = np.random.default_rng(7)
    bpm = 112
    beat = 60 / bpm
    bars = 16
    tr = Track(bars * 4 * beat, 3800)
    for bar in range(bars):
        root, tri = CHORDS_B[bar % 8]
        t0 = bar * 4 * beat
        notes = [D + root + i - 2 for i in tri]  # до минор — чуть ниже и темнее
        tr.add(t0, pad([hz(n - 12) for n in notes] + [hz(notes[0] - 24)], 4 * beat + 0.6, 0.1))
        # барабаны: тайко на 1 и 3, лёгкие удары на слабые
        for k, (f0, f1, v) in enumerate([(120, 48, 1.0), (260, 140, 0.25), (110, 46, 0.8), (260, 140, 0.25)]):
            tr.add(t0 + k * beat, drum(0.7, f0, f1, v, 0.12, rng))
        if bar % 2 == 1:
            tr.add(t0 + 3.5 * beat, drum(0.5, 130, 55, 0.6, 0.12, rng))
        # остинато: шестнадцатые на струнах
        for k in range(16):
            n = [notes[0], notes[0] + 12, notes[2], notes[0] + 12][k % 4]
            tr.add(t0 + k * beat / 4, pluck(hz(n), 0.6, 0.7, 0.992, rng) * (0.2 if k % 4 else 0.28))
        # тема: короткие героические мотивы на сильных тактах
        if bar % 2 == 0 and bar >= 2:
            motif = [notes[0] + 12, notes[1] + 12, notes[2] + 12, notes[0] + 24]
            for k, n in enumerate(motif):
                tr.add(t0 + k * beat * 0.75, pluck(hz(n), 1.8, 0.8, 0.997, rng) * 0.34)
        tr.add(t0, bass(hz(notes[0] - 24), 4 * beat, 0.4))
    return tr.render(0.22)


def save_ogg(x, path):
    with tempfile.NamedTemporaryFile(suffix=".wav", delete=False) as f:
        tmp = f.name
    with wave.open(tmp, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(SR)
        w.writeframes((np.clip(x, -1, 1) * 32000).astype(np.int16).tobytes())
    subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-i", tmp, "-c:a", "libvorbis", "-q:a", "3", path], check=True)
    os.unlink(tmp)
    print("music:", path, os.path.getsize(path) // 1024, "KB")


os.makedirs(DST, exist_ok=True)
save_ogg(village(), os.path.join(DST, "village.ogg"))
save_ogg(battle(), os.path.join(DST, "battle.ogg"))
