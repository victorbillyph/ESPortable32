#!/usr/bin/env python3
"""Generate 8-bit 8kHz mono WAV C arrays from note sequences."""
import struct

def notes_to_wav(notes, sr=8000):
    """notes: list of (freq_hz, dur_ms) tuples. freq=0 = silence."""
    total_dur = sum(d for _, d in notes)
    nsamples = total_dur * sr // 1000
    raw = bytearray(nsamples)
    idx = 0
    for freq, dur in notes:
        n = dur * sr // 1000
        if freq > 0:
            half = sr // 2
            for i in range(n):
                phase = (i * freq) % sr
                raw[idx + i] = 0xFF if phase < half else 0x00
        else:
            for i in range(n):
                raw[idx + i] = 0x80
        idx += n

    datasize = len(raw)
    filesize = 36 + datasize

    hdr = bytearray()
    hdr += b'RIFF'
    hdr += struct.pack('<I', filesize)
    hdr += b'WAVE'
    hdr += b'fmt '
    hdr += struct.pack('<I', 16)
    hdr += struct.pack('<H', 1)      # PCM
    hdr += struct.pack('<H', 1)      # mono
    hdr += struct.pack('<I', sr)
    hdr += struct.pack('<I', sr)     # byteRate
    hdr += struct.pack('<H', 1)      # blockAlign
    hdr += struct.pack('<H', 8)      # bitsPerSample
    hdr += b'data'
    hdr += struct.pack('<I', datasize)
    hdr += raw
    return hdr

def array_name(name):
    return f"wav_{name.replace(' ', '_').lower()}"

def gen_c(name, notes, sr=8000):
    wav = notes_to_wav(notes, sr)
    arr = array_name(name)
    lines = [f"// {name} — {len(wav)} bytes @ {sr}Hz 8-bit mono"]
    lines.append(f"static const uint8_t {arr}[{len(wav)}] = {{")
    for i in range(0, len(wav), 16):
        chunk = wav[i:i+16]
        hexs = ', '.join(f'0x{b:02x}' for b in chunk)
        lines.append(f"  {hexs},")
    lines.append("};")
    lines.append(f"#define {arr.upper()}_SIZE {len(wav)}")
    return '\n'.join(lines)

# ─── Mario Theme (~4.5s) ───
# Super Mario Bros ground theme — most recognizable phrase
mario = [
    (659, 100), (659, 100), (0, 100), (659, 100), (0, 100),
    (523, 100), (659, 200), (784, 200), (0, 200),
    (392, 200), (0, 100), (523, 200), (0, 100),
    (392, 100), (0, 100), (330, 100), (0, 100),
    (440, 100), (494, 100), (466, 100), (440, 200),
    (392, 100), (659, 100), (784, 100), (880, 200),
    (698, 100), (784, 100), (659, 200),
    (523, 100), (587, 100), (494, 200), (0, 200),
    (523, 100), (587, 100), (659, 200),
    (523, 100), (587, 100), (659, 200),
    (523, 100), (587, 100), (659, 100), (494, 100), (440, 300),
]

# ─── Rick Roll (~4.5s) ───
# Never Gonna Give You Up — chorus melody
rick = [
    (370, 200), (440, 200), (587, 200), (440, 200),
    (587, 200), (659, 200), (740, 400),
    (784, 200), (659, 200), (587, 200), (494, 200),
    (392, 200), (494, 200), (587, 200), (494, 200),
    (440, 200),
    (370, 200), (440, 200), (587, 200), (440, 200),
    (587, 200), (659, 200), (740, 400),
    (784, 200), (740, 200), (659, 200), (587, 200),
    (494, 200), (440, 200), (587, 200), (494, 200),
    (440, 300),
]

if __name__ == '__main__':
    print(gen_c("Mario Theme", mario))
    print()
    print(gen_c("Rick Roll", rick))
