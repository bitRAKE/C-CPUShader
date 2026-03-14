from __future__ import annotations

import argparse
import glob
from fractions import Fraction
from pathlib import Path
import struct
import sys
import zlib


PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"
ANIMATION_CHUNKS = {b"acTL", b"fcTL", b"fdAT"}


class ParsedPng:
    def __init__(self, path: Path, ihdr: bytes, pre_idat: list[tuple[bytes, bytes]], idat: list[bytes], post_idat: list[tuple[bytes, bytes]]):
        self.path = path
        self.ihdr = ihdr
        self.pre_idat = pre_idat
        self.idat = idat
        self.post_idat = post_idat
        self.fields = struct.unpack(">IIBBBBB", ihdr)


def png_chunk(chunk_type: bytes, chunk_data: bytes) -> bytes:
    crc = zlib.crc32(chunk_type)
    crc = zlib.crc32(chunk_data, crc) & 0xFFFFFFFF
    return struct.pack(">I", len(chunk_data)) + chunk_type + chunk_data + struct.pack(">I", crc)


def parse_png(path: Path) -> ParsedPng:
    data = path.read_bytes()
    if not data.startswith(PNG_SIGNATURE):
        raise ValueError(f"{path} is not a PNG file.")

    offset = len(PNG_SIGNATURE)
    ihdr = None
    pre_idat: list[tuple[bytes, bytes]] = []
    idat: list[bytes] = []
    post_idat: list[tuple[bytes, bytes]] = []
    saw_idat = False

    while offset < len(data):
        if offset + 12 > len(data):
            raise ValueError(f"{path} is truncated near byte {offset}.")

        length = struct.unpack(">I", data[offset : offset + 4])[0]
        chunk_type = data[offset + 4 : offset + 8]
        chunk_start = offset + 8
        chunk_end = chunk_start + length
        crc_offset = chunk_end
        next_offset = crc_offset + 4

        if next_offset > len(data):
            raise ValueError(f"{path} is truncated in chunk {chunk_type!r}.")

        chunk_data = data[chunk_start:chunk_end]
        stored_crc = struct.unpack(">I", data[crc_offset:next_offset])[0]
        actual_crc = zlib.crc32(chunk_type)
        actual_crc = zlib.crc32(chunk_data, actual_crc) & 0xFFFFFFFF
        if actual_crc != stored_crc:
            raise ValueError(f"{path} has a CRC mismatch in chunk {chunk_type.decode('ascii', 'replace')}.")

        offset = next_offset

        if chunk_type == b"IHDR":
            if ihdr is not None:
                raise ValueError(f"{path} contains multiple IHDR chunks.")
            if length != 13:
                raise ValueError(f"{path} has an invalid IHDR size.")
            ihdr = chunk_data
            continue

        if chunk_type == b"IDAT":
            saw_idat = True
            idat.append(chunk_data)
            continue

        if chunk_type == b"IEND":
            break

        if chunk_type in ANIMATION_CHUNKS:
            raise ValueError(f"{path} is already an animated PNG.")

        if not saw_idat:
            pre_idat.append((chunk_type, chunk_data))
        else:
            post_idat.append((chunk_type, chunk_data))

    if ihdr is None:
        raise ValueError(f"{path} is missing IHDR.")
    if not idat:
        raise ValueError(f"{path} is missing IDAT data.")

    return ParsedPng(path, ihdr, pre_idat, idat, post_idat)


def choose_delay(fps: float) -> tuple[int, int]:
    if fps <= 0.0:
        raise ValueError("FPS must be greater than zero.")

    fraction = Fraction(1.0 / fps).limit_denominator(1000)
    if fraction.numerator > 65535 or fraction.denominator > 65535:
        fraction = Fraction(round(1000.0 / fps), 1000)

    return fraction.numerator, fraction.denominator


def load_frames(pattern: str, take_last: int | None) -> list[ParsedPng]:
    paths = [Path(match) for match in sorted(glob.glob(pattern))]
    if take_last is not None and take_last > 0:
        paths = paths[-take_last:]

    if len(paths) < 2:
        raise ValueError("Need at least two PNG frames to build an animation.")

    frames = [parse_png(path) for path in paths]
    first = frames[0]

    for frame in frames[1:]:
        if frame.fields != first.fields:
            raise ValueError(
                f"{frame.path} does not match the first frame's width/height/bit depth/color type.")
        if frame.pre_idat != first.pre_idat:
            raise ValueError(f"{frame.path} does not match the first frame's pre-IDAT metadata.")
        if frame.post_idat != first.post_idat:
            raise ValueError(f"{frame.path} does not match the first frame's post-IDAT metadata.")

    return frames


def write_apng(frames: list[ParsedPng], output_path: Path, fps: float, plays: int) -> None:
    width, height, bit_depth, color_type, compression, filtering, interlace = frames[0].fields
    delay_num, delay_den = choose_delay(fps)
    sequence = 0

    with output_path.open("wb") as stream:
        stream.write(PNG_SIGNATURE)
        stream.write(png_chunk(b"IHDR", frames[0].ihdr))

        for chunk_type, chunk_data in frames[0].pre_idat:
            stream.write(png_chunk(chunk_type, chunk_data))

        stream.write(png_chunk(b"acTL", struct.pack(">II", len(frames), plays)))
        stream.write(
            png_chunk(
                b"fcTL",
                struct.pack(">IIIIIHHBB", sequence, width, height, 0, 0, delay_num, delay_den, 0, 0),
            )
        )
        sequence += 1

        for idat_chunk in frames[0].idat:
            stream.write(png_chunk(b"IDAT", idat_chunk))

        for frame in frames[1:]:
            stream.write(
                png_chunk(
                    b"fcTL",
                    struct.pack(">IIIIIHHBB", sequence, width, height, 0, 0, delay_num, delay_den, 0, 0),
                )
            )
            sequence += 1

            for idat_chunk in frame.idat:
                fd_data = struct.pack(">I", sequence) + idat_chunk
                stream.write(png_chunk(b"fdAT", fd_data))
                sequence += 1

        for chunk_type, chunk_data in frames[0].post_idat:
            stream.write(png_chunk(chunk_type, chunk_data))

        stream.write(png_chunk(b"IEND", b""))

    print(
        f"Wrote {output_path} with {len(frames)} frames "
        f"({width}x{height}, bit depth {bit_depth}, color type {color_type}, "
        f"{delay_num}/{delay_den} seconds per frame, plays={plays})."
    )


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Merge captured PNG frames into an APNG without external Python packages.")
    parser.add_argument("--input-glob", required=True, help="Glob pattern for the PNG frame sequence.")
    parser.add_argument("--output", required=True, help="Output APNG path.")
    parser.add_argument("--fps", type=float, default=12.0, help="Playback rate for the output animation.")
    parser.add_argument("--last", type=int, default=None, help="Use only the last N lexicographically sorted frames.")
    parser.add_argument("--plays", type=int, default=0, help="Animation play count. 0 means loop forever.")
    return parser.parse_args()


def main() -> int:
    args = parse_args()

    try:
        frames = load_frames(args.input_glob, args.last)
        output_path = Path(args.output)
        output_path.parent.mkdir(parents=True, exist_ok=True)
        write_apng(frames, output_path, args.fps, args.plays)
    except Exception as exc:  # noqa: BLE001
        print(f"error: {exc}", file=sys.stderr)
        return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
