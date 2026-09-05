#!/usr/bin/env python3
"""2本の動画を左右に連結して横幅2倍の動画を作る (OpenCV のみ、ffmpeg 不要)。

用途: 16:9 の平面素材を球体 (経度360°) に貼ると横に間延びするので、
2本 (同じ動画×2 でもよい) を横に並べて 32:9 にし、1本あたり経度180°分にする。

  ・高さは --height に揃える (未指定なら A の高さ)。幅はアスペクト維持で個別に決まる
  ・尺は --mode shortest (短い方で打ち切り) / loop (短い方を繰り返して長い方に合わせる)
  ・fps は A に合わせ、B は最近傍フレームで追従

使い方:
    python hstack_videos.py A.mp4 B.mp4 --out data/AB.mp4
    python hstack_videos.py A.mp4 A.mp4 --out data/AA.mp4          # 同じ動画を2枚並べる
    python hstack_videos.py A.mp4 B.mp4 --height 1080 --mode loop
"""
import argparse
import os

import cv2
import numpy as np


def open_cap(path):
    cap = cv2.VideoCapture(path)
    if not cap.isOpened():
        raise SystemExit(f"開けません: {path}")
    n = int(cap.get(cv2.CAP_PROP_FRAME_COUNT))
    fps = cap.get(cv2.CAP_PROP_FPS) or 30.0
    w = int(cap.get(cv2.CAP_PROP_FRAME_WIDTH))
    h = int(cap.get(cv2.CAP_PROP_FRAME_HEIGHT))
    return cap, n, fps, w, h


def read_all(cap, n, height):
    """全フレームを読み込み、指定高さにアスペクト維持でリサイズして返す (メモリに載る前提の短尺用)。"""
    frames = []
    while True:
        ok, f = cap.read()
        if not ok:
            break
        h, w = f.shape[:2]
        if h != height:
            f = cv2.resize(f, (int(round(w * height / h)), height), interpolation=cv2.INTER_AREA)
        frames.append(f)
    return frames


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("a")
    ap.add_argument("b")
    ap.add_argument("--out", required=True)
    ap.add_argument("--height", type=int, default=None, help="連結後の高さ (既定: A の高さ)")
    ap.add_argument("--mode", choices=["shortest", "loop"], default="loop")
    args = ap.parse_args()

    cap_a, na, fps_a, wa, ha = open_cap(args.a)
    cap_b, nb, fps_b, wb, hb = open_cap(args.b)
    height = args.height or ha
    fa = read_all(cap_a, na, height)
    fb = read_all(cap_b, nb, height)
    cap_a.release(); cap_b.release()
    if not fa or not fb:
        raise SystemExit("フレームが読めませんでした")

    dur_a = len(fa) / fps_a
    dur_b = len(fb) / fps_b
    dur = min(dur_a, dur_b) if args.mode == "shortest" else max(dur_a, dur_b)
    n_out = int(round(dur * fps_a))
    w_out = fa[0].shape[1] + fb[0].shape[1]

    os.makedirs(os.path.dirname(args.out) or ".", exist_ok=True)
    vw = cv2.VideoWriter(args.out, cv2.VideoWriter_fourcc(*"mp4v"), fps_a, (w_out, height))
    if not vw.isOpened():
        raise SystemExit(f"VideoWriter を開けません: {args.out}")
    for i in range(n_out):
        t = i / fps_a
        ia = int(t * fps_a) % len(fa)            # loop モードでは短い方を繰り返す
        ib = int(round(t * fps_b)) % len(fb)
        vw.write(np.hstack([fa[ia], fb[ib]]))
    vw.release()
    print(f"生成完了: {args.out}  {w_out}x{height} {n_out}frames @ {fps_a:.2f}fps "
          f"(A {wa}x{ha} {dur_a:.1f}s + B {wb}x{hb} {dur_b:.1f}s, mode={args.mode})")


if __name__ == "__main__":
    main()
