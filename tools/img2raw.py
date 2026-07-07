#!/usr/bin/env python3
"""
Convert images or video to 128x64 raw monochrome frames for ESP32 OLED VideoApp.

Usage:
    python img2raw.py input.png -o output.raw
    python img2raw.py frame1.png frame2.png frame3.png -o anim.raw
    python img2raw.py frames/*.png -o animation.raw
    python img2raw.py video.mp4 -o anim.raw
    python img2raw.py video.mkv -o anim.raw --fps 10

The .raw file can be uploaded via http://<ESP_IP>/upload
Each frame is 1024 bytes (128x64 monochrome, 1bpp packed in column-major order).
Max file size: 40KB total (shared with MP3 audio). Video: ~40 frames. MP3: ~5s at 64kbps.
"""

import sys
import os
import argparse
import subprocess
import tempfile
from PIL import Image

WIDTH, HEIGHT = 128, 64
FRAME_SIZE = WIDTH * HEIGHT // 8
MAX_BYTES = 40960

VIDEO_EXT = {".mp4", ".mkv", ".avi", ".mov", ".webm", ".flv", ".gif"}

def image_to_raw(img):
    img = img.convert("1")
    img = img.resize((WIDTH, HEIGHT), Image.NEAREST)
    pixels = list(img.getdata())
    buf = bytearray(FRAME_SIZE)
    for x in range(WIDTH):
        for y in range(HEIGHT):
            page = y // 8
            bit = y % 8
            idx = x + page * WIDTH
            if pixels[y * WIDTH + x] == 0:
                buf[idx] |= (1 << bit)
    return bytes(buf)

def extract_frames_ffmpeg(path, fps):
    """Extract frames from video as PNGs via ffmpeg pipe."""
    import shutil
    if not shutil.which("ffmpeg"):
        print("ERROR: ffmpeg not found. Install ffmpeg or use PNG inputs.", file=sys.stderr)
        sys.exit(1)
    # Calculate frame interval to get ~20 evenly spaced frames
    cmd = [
        "ffprobe", "-v", "error",
        "-show_entries", "format=duration",
        "-of", "default=noprint_wrappers=1:nokey=1",
        path
    ]
    try:
        dur = float(subprocess.check_output(cmd).strip())
    except Exception:
        dur = 0

    target = min(int(dur * fps), MAX_BYTES // FRAME_SIZE)
    if target < 1:
        target = min(20, MAX_BYTES // FRAME_SIZE)

    # Extract frames using ffmpeg to temp PNGs
    tmpdir = tempfile.mkdtemp(prefix="img2raw_")
    pattern = os.path.join(tmpdir, "frame_%04d.png")
    sel = "1" if target <= 1 else f"not(mod(n,{max(1,int(dur*fps/target))}))"
    ffmpeg_cmd = [
        "ffmpeg", "-i", path,
        "-vf", f"fps={fps},scale={WIDTH}:{HEIGHT}:flags=neighbor",
        "-frames:v", str(max(1, min(target, 99))),
        "-y", pattern
    ]
    subprocess.run(ffmpeg_cmd, capture_output=True)

    files = sorted(os.listdir(tmpdir))
    frames = []
    for fn in files:
        fp = os.path.join(tmpdir, fn)
        img = Image.open(fp)
        frames.append(image_to_raw(img))
        os.remove(fp)
    os.rmdir(tmpdir)
    return frames

def main():
    ap = argparse.ArgumentParser(description="Convert images/video to 128x64 raw frames")
    ap.add_argument("input", nargs="+", help="Input image or video files")
    ap.add_argument("-o", "--output", default="output.raw", help="Output .raw file")
    ap.add_argument("--fps", type=float, default=10, help="FPS for video extraction (default: 10)")
    args = ap.parse_args()

    frames = []
    for path in args.input:
        ext = os.path.splitext(path)[1].lower()
        if ext in VIDEO_EXT:
            print(f"[video] {path}: extracting frames at {args.fps}fps...")
            vframes = extract_frames_ffmpeg(path, args.fps)
            print(f"  extracted {len(vframes)} frames")
            frames.extend(vframes)
        else:
            img = Image.open(path)
            raw = image_to_raw(img)
            frames.append(raw)
            print(f"  {path}: 1 frame ({len(raw)} bytes)")

    if not frames:
        print("ERROR: no frames generated", file=sys.stderr)
        sys.exit(1)

    data = b"".join(frames)
    total = len(data) // FRAME_SIZE
    if len(data) > MAX_BYTES:
        print(f"WARNING: {len(data)} bytes exceeds {MAX_BYTES/1024:.0f}KB limit, truncating to {MAX_BYTES // FRAME_SIZE} frames")
        data = data[:MAX_BYTES]

    with open(args.output, "wb") as f:
        f.write(data)

    print(f"Written {args.output}: {total} frames, {len(data)} bytes")

if __name__ == "__main__":
    main()
