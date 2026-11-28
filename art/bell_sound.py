"""The temple bell's sound (zbel.wav), made from scratch: our own sound, not
the game's (owner, 2026-11-27: the bell on the temple top rings).

A struck bronze bell is a sum of decaying partials at the bell's classic
ratios to its hum note (hum 1, prime 2, tierce 2.4, quint 3, nominal 4, and
the upper partials 5.33, 6.67, 8), the upper ones dying away much faster
than the hum, each split into a pair a fraction of a hertz apart so it
beats slowly as real bells do, with a short clank of the clapper at the
start. The hum is 98 Hz (G2): a great bell.

    python3 bell_sound.py OUT.wav [ring|soft|echo|chime0 ... chime4]

writes a 22050 Hz, 16-bit mono WAV, 5.5 s long: the bell rung (zbel.wav);
"soft", rung gently by itself at dawn and dusk (zbes.wav: the clapper's
clank and the bright upper partials much quieter, the whole at under half
the level); "echo", its sound coming back off the fog (zbee.wav: the upper
partials muffled, a slow swell instead of a strike, faint); "chime0" to
"chime4", the temple's five hanging chimes (zch0-zch4.wav: small bells,
brighter and shorter, 2.5 s, their strike notes D5, F5, A5, B5 and D6, the
five notes its hidden tunes are played on).
"""
import sys
import wave
import numpy as np

RATE = 22050
LENGTH = 5.5
HUM = 98.0
#        ratio, level, decay (s to 1/e), beat (Hz)
PARTIALS = [
    (1.00, 0.55, 3.2, 0.35),
    (2.00, 0.50, 2.2, 0.50),
    (2.40, 0.42, 1.8, 0.70),
    (3.00, 0.26, 1.4, 0.55),
    (4.00, 0.60, 1.5, 0.90),
    (5.33, 0.24, 0.8, 1.10),
    (6.67, 0.18, 0.55, 1.30),
    (8.00, 0.12, 0.40, 1.60),
]


CHIME_NOTES = [587.33, 698.46, 880.0, 987.77, 1174.66]  # D5 F5 A5 B5 D6


def make(kind='ring'):
    if kind.startswith('chime'):
        return make_chime(CHIME_NOTES[int(kind[5:])])
    upper = {'ring': 1.0, 'soft': 0.45, 'echo': 0.2}[kind]
    clank_level = {'ring': 0.35, 'soft': 0.08, 'echo': 0.0}[kind]
    peak = {'ring': 0.7, 'soft': 0.3, 'echo': 0.16}[kind]
    attack_s = {'ring': 0.004, 'soft': 0.01, 'echo': 0.12}[kind]
    t = np.arange(int(RATE * LENGTH)) / RATE
    rng = np.random.default_rng(1998)
    out = np.zeros_like(t)
    for ratio, level, decay, beat in PARTIALS:
        f = HUM * ratio
        ph = rng.uniform(0, 2 * np.pi, 2)
        env = np.exp(-t / decay)
        lv = level * (upper if ratio > 3.0 else 1.0)
        out += lv * env * 0.5 * (np.sin(2 * np.pi * f * t + ph[0]) + np.sin(2 * np.pi * (f + beat) * t + ph[1]))
    # the clapper's clank: a few ms of bright noise, softened
    n = int(0.012 * RATE)
    clank = rng.standard_normal(n) * np.exp(-np.arange(n) / (0.003 * RATE))
    clank = np.convolve(clank, np.ones(3) / 3, mode='same')
    out[:n] += clank_level * clank
    # a 4 ms attack (no click) and a fade over the last half second
    a = int(attack_s * RATE)
    out[:a] *= np.linspace(0, 1, a)
    f = int(0.5 * RATE)
    out[-f:] *= np.linspace(1, 0, f) ** 2
    out *= peak / np.max(np.abs(out))   # the ring about -3 dB
    return (out * 32767).astype('<i2')


def make_chime(strike):
    """A small bell: its strike note is the nominal (4 x the hum), and it
    dies away fast; the smaller, the brighter."""
    length = 2.5
    t = np.arange(int(RATE * length)) / RATE
    rng = np.random.default_rng(int(strike))
    hum = strike / 4.0
    out = np.zeros_like(t)
    for ratio, level, decay, beat in PARTIALS:
        f = hum * ratio
        if f > RATE / 2.2:
            continue
        ph = rng.uniform(0, 2 * np.pi, 2)
        env = np.exp(-t / (decay * 0.45))
        out += level * env * 0.5 * (np.sin(2 * np.pi * f * t + ph[0]) + np.sin(2 * np.pi * (f + beat * 2) * t + ph[1]))
    n = int(0.006 * RATE)
    out[:n] += 0.2 * rng.standard_normal(n) * np.exp(-np.arange(n) / (0.0015 * RATE))
    a = int(0.002 * RATE)
    out[:a] *= np.linspace(0, 1, a)
    f = int(0.3 * RATE)
    out[-f:] *= np.linspace(1, 0, f) ** 2
    out *= 0.5 / np.max(np.abs(out))
    return (out * 32767).astype('<i2')


def main():
    data = make(sys.argv[2] if len(sys.argv) > 2 else 'ring')
    with wave.open(sys.argv[1], 'wb') as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes(data.tobytes())
    print('wrote', sys.argv[1], len(data) / RATE, 's')


if __name__ == '__main__':
    main()
