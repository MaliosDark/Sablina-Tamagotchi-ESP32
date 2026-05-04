#!/usr/bin/env python3

from __future__ import annotations

import argparse
import shutil
import subprocess
import sys
from pathlib import Path


FACE_CLIPS = [
    "sablina-idle",
    "sablina-close-up",
    "sablina-crying",
    "sablina-dance",
    "sablina-eating",
    "sablina-hackin",
    "sablina-sad-tired",
    "sablina-screaming",
    "sablina-shower",
    "sablina-sleep",
    "sablina-smiling",
]


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Convert Sablina MP4 face clips into SPIFFS-ready RGB565 frame blobs."
    )
    parser.add_argument(
        "--source-dir",
        type=Path,
        default=Path("/home/nexland/ESP32-DIV-KILAZ"),
        help="Directory that contains the sablina*.mp4 files.",
    )
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=Path(__file__).resolve().parents[1] / "data" / "faces",
        help="Destination directory for SPIFFS face blobs.",
    )
    parser.add_argument("--width", type=int, default=128, help="Output frame width.")
    parser.add_argument("--height", type=int, default=88, help="Output frame height.")
    parser.add_argument("--fps", type=int, default=5, help="Output frame rate.")
    parser.add_argument("--frames", type=int, default=30, help="Frames per clip.")
    parser.add_argument(
        "--ffmpeg",
        default="ffmpeg",
        help="Path to ffmpeg executable.",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Print planned conversions without invoking ffmpeg.",
    )
    return parser.parse_args()


def find_source_files(source_dir: Path) -> dict[str, Path]:
    files: dict[str, Path] = {}
    for path in source_dir.iterdir():
        if not path.is_file() or path.suffix.lower() != ".mp4":
            continue
        stem = path.stem.lower()
        if "sablina" not in stem:
            continue
        files[stem] = path
    return files


def build_ffmpeg_command(
    ffmpeg: str,
    source_path: Path,
    output_path: Path,
    width: int,
    height: int,
    fps: int,
    frames: int,
) -> list[str]:
    vf = f"fps={fps},scale={width}:{height}:force_original_aspect_ratio=increase,crop={width}:{height}"
    return [
        ffmpeg,
        "-y",
        "-i",
        str(source_path),
        "-vf",
        vf,
        "-frames:v",
        str(frames),
        "-f",
        "rawvideo",
        "-pix_fmt",
        "rgb565le",
        str(output_path),
    ]


def main() -> int:
    args = parse_args()

    if shutil.which(args.ffmpeg) is None:
      print(f"ffmpeg not found: {args.ffmpeg}", file=sys.stderr)
      return 1

    if not args.source_dir.exists():
        print(f"source dir not found: {args.source_dir}", file=sys.stderr)
        return 1

    args.output_dir.mkdir(parents=True, exist_ok=True)
    sources = find_source_files(args.source_dir)
    frame_bytes = args.width * args.height * 2
    clip_bytes = frame_bytes * args.frames

    missing = [stem for stem in FACE_CLIPS if stem not in sources]
    if missing:
        print("missing source clips:", file=sys.stderr)
        for stem in missing:
            print(f"  - {stem}.mp4", file=sys.stderr)
        return 1

    for stem in FACE_CLIPS:
        source_path = sources[stem]
        output_path = args.output_dir / f"{stem}.rgb565"
        cmd = build_ffmpeg_command(
            args.ffmpeg,
            source_path,
            output_path,
            args.width,
            args.height,
            args.fps,
            args.frames,
        )

        if args.dry_run:
            print("DRY RUN", source_path, "->", output_path)
            continue

        print(f"Converting {source_path.name} -> {output_path.name}")
        subprocess.run(cmd, check=True)

        actual_size = output_path.stat().st_size
        if actual_size != clip_bytes:
            print(
                f"unexpected output size for {output_path.name}: {actual_size} bytes, expected {clip_bytes}",
                file=sys.stderr,
            )
            return 1

    if args.dry_run:
        print(f"Would generate {len(FACE_CLIPS)} clips into {args.output_dir}")
        print(f"Per clip: {clip_bytes} bytes  Total: {clip_bytes * len(FACE_CLIPS)} bytes")
        return 0

    total_bytes = sum((args.output_dir / f"{stem}.rgb565").stat().st_size for stem in FACE_CLIPS)
    print(f"Generated {len(FACE_CLIPS)} clips into {args.output_dir}")
    print(f"Per clip: {clip_bytes} bytes  Total: {total_bytes} bytes")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())