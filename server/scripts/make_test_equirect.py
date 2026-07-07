#!/usr/bin/env python3
"""正距円筒(equirectangular)のテストパターン動画を生成する。

デフォルトは「南極→北極に渡る黄色い縦帯が、経度 -180°→+180° を一周する」パターン。
球体ディスプレイのマッピング確認・ループ再生確認用。

■ シームレスループの原則 (これを守らないと最初と最後で帯が飛ぶ):
  経度 -180° と +180° は同一の子午線なので、両方を描くと 1 フレーム分重複して
  ループの継ぎ目でカクつく。そこで frame i の帯中心経度を
      lon(i) = -180 + 360 * i / N      (i = 0..N-1、+180° は含めない)
  とし、frame N が frame 0 に一致するようにする。さらに帯は経度方向に
  ラップさせ、継ぎ目 (画像左右端) をまたいでも途切れないようにする。

使い方:
    python make_test_equirect.py                       # data/test_meridian_sweep.mp4
    python make_test_equirect.py --frames 120 --fps 10 --band-deg 12 --out data/foo.mp4
    python make_test_equirect.py --axis lat            # 緯度方向に走査する横帯版
"""
import argparse
import os

import cv2
import numpy as np


def ang_delta(a, b):
    """角度差 a-b を [-180,180) へ正規化 (ラップ対応)。"""
    return (a - b + 180.0) % 360.0 - 180.0


def build_frame(w, h, center_lon, band_deg, axis, fg, bg, soft_deg):
    """1 フレームを生成。axis='lon': 経度走査の縦帯 / 'lat': 緯度走査の横帯。"""
    img = np.empty((h, w, 3), np.uint8)
    img[:] = bg
    half = band_deg / 2.0
    if axis == "lon":
        # 各列 x → 経度。帯は縦 (全緯度=全高)。
        x = np.arange(w)
        lon = -180.0 + 360.0 * (x + 0.5) / w
        d = np.abs((lon - center_lon + 180.0) % 360.0 - 180.0)   # ラップ距離
        # soft_deg でエッジをなめらかに (0 で硬いエッジ)
        alpha = np.clip((half + soft_deg - d) / max(soft_deg, 1e-6), 0.0, 1.0)
        col = (bg[None, :] * (1 - alpha[:, None]) + fg[None, :] * alpha[:, None])
        img[:] = col.astype(np.uint8)[None, :, :]
    else:
        # 各行 y → 緯度(-90..90)。帯は横 (全経度=全幅)。center_lon を緯度として流用。
        y = np.arange(h)
        lat = 90.0 - 180.0 * (y + 0.5) / h
        d = np.abs(lat - center_lon)     # 緯度はラップしない
        alpha = np.clip((half + soft_deg - d) / max(soft_deg, 1e-6), 0.0, 1.0)
        row = (np.array(bg)[None, :] * (1 - alpha[:, None]) + np.array(fg)[None, :] * alpha[:, None])
        img[:] = row.astype(np.uint8)[:, None, :]
    return img


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--width", type=int, default=320)
    ap.add_argument("--height", type=int, default=160)
    ap.add_argument("--frames", type=int, default=120, help="1周のフレーム数 (N)")
    ap.add_argument("--fps", type=float, default=10.0)
    ap.add_argument("--band-deg", type=float, default=12.0, help="帯の幅 (度)")
    ap.add_argument("--soft-deg", type=float, default=2.0, help="エッジのぼかし幅 (度, 0で硬い)")
    ap.add_argument("--axis", choices=["lon", "lat"], default="lon",
                    help="lon=経度走査の縦帯(既定) / lat=緯度走査の横帯")
    ap.add_argument("--direction", type=int, default=1, choices=[1, -1])
    ap.add_argument("--out", default="data/test_meridian_sweep.mp4")
    args = ap.parse_args()

    w, h, n = args.width, args.height, args.frames
    fg = np.array([0, 255, 255], np.float64)   # 黄 (BGR)
    bg = np.array([16, 16, 16], np.float64)     # ほぼ黒

    os.makedirs(os.path.dirname(args.out) or ".", exist_ok=True)
    fourcc = cv2.VideoWriter_fourcc(*"mp4v")
    vw = cv2.VideoWriter(args.out, fourcc, args.fps, (w, h))
    if not vw.isOpened():
        raise SystemExit(f"VideoWriter を開けません: {args.out}")

    span = 360.0 if args.axis == "lon" else 180.0   # lat は -90..90 を一往復せず片道
    base = -180.0 if args.axis == "lon" else 90.0
    for i in range(n):
        if args.axis == "lon":
            # +180 は含めない (frame N == frame 0 でシームレス)
            center = -180.0 + args.direction * 360.0 * (i / n)
            center = (center + 180.0) % 360.0 - 180.0
        else:
            # 緯度は端点(±90=極)を含めて往復させるとシームレス:
            #   0..N/2 で南→北、N/2..N で北→南。両端の折り返しで連続。
            t = i / n
            tri = 1.0 - abs(2.0 * t - 1.0)          # 0→1→0 の三角波
            center = -90.0 + 180.0 * tri
        frame = build_frame(w, h, center, args.band_deg, args.axis, fg, bg, args.soft_deg)
        vw.write(frame)
    vw.release()
    print(f"生成完了: {args.out}  {w}x{h} {n}frames @ {args.fps}fps axis={args.axis} "
          f"band={args.band_deg}deg (seamless loop)")


if __name__ == "__main__":
    main()
