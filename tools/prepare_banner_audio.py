#!/usr/bin/env python3
import argparse
import wave


def main() -> int:
    parser = argparse.ArgumentParser(description="Prepare 2s loop-friendly WAV for 3DS banner audio.")
    parser.add_argument("input_wav")
    parser.add_argument("output_wav")
    parser.add_argument("--seconds", type=float, default=2.0)
    parser.add_argument("--fade-ms", type=int, default=20)
    args = parser.parse_args()

    if args.seconds <= 0:
        raise ValueError("seconds must be > 0")

    with wave.open(args.input_wav, "rb") as wf:
        channels = wf.getnchannels()
        sampwidth = wf.getsampwidth()
        rate = wf.getframerate()
        nframes = wf.getnframes()
        comptype = wf.getcomptype()
        compname = wf.getcompname()

        if comptype != "NONE":
            raise RuntimeError(f"Unsupported WAV compression: {comptype}")
        if sampwidth not in (1, 2):
            raise RuntimeError(f"Unsupported sample width: {sampwidth}")
        if channels not in (1, 2):
            raise RuntimeError(f"Unsupported channel count: {channels}")

        keep_frames = int(args.seconds * rate)
        if keep_frames < 1:
            keep_frames = 1
        if keep_frames > nframes:
            keep_frames = nframes
        raw = wf.readframes(keep_frames)

    # Optional tiny fade-in/out to reduce click at loop boundary.
    if sampwidth == 2 and args.fade_ms > 0:
        import struct
        total_frames = len(raw) // (channels * sampwidth)
        fade_frames = int(rate * (args.fade_ms / 1000.0))
        fade_frames = max(0, min(fade_frames, total_frames // 2))
        if fade_frames > 0:
            samples = list(struct.unpack("<" + "h" * (len(raw) // 2), raw))
            for i in range(fade_frames):
                g_in = i / float(fade_frames)
                g_out = (fade_frames - i) / float(fade_frames)
                for c in range(channels):
                    idx_in = i * channels + c
                    idx_out = (total_frames - 1 - i) * channels + c
                    samples[idx_in] = int(samples[idx_in] * g_in)
                    samples[idx_out] = int(samples[idx_out] * g_out)
            raw = struct.pack("<" + "h" * len(samples), *samples)

    with wave.open(args.output_wav, "wb") as out:
        out.setnchannels(channels)
        out.setsampwidth(sampwidth)
        out.setframerate(rate)
        out.setcomptype(comptype, compname)
        out.writeframes(raw)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
