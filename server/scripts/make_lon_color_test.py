#!/usr/bin/env python3
"""経度ごとに色を変えた正距円筒(equirectangular)テストパターン動画を生成する。

球体表示で「画像幅(=経度 -180..+180°)の全域がサンプリングされているか」を
目視確認するための静止パターン。経度を N 等分し、各帯を色相環の順に塗る。
帯の順序 (赤→橙→黄→…) が経度の増加方向なので、左右鏡像の検出にも使える。

  ・全ての色が赤道一周に現れる            → 幅全域を使っている (正常)
  ・半分の色しか出ない / 同じ色が2回出る  → 経度が半分 (旧 sphereToUV 相当の不具合)
  ・色の並び順が逆                        → 左右鏡像 (atan2 の符号)

使い方:
    python make_lon_color_test.py                      # data/test_lon_colors12.mp4 (12帯=30°)
    python make_lon_color_test.py --bands 8            # 8帯=45°
    python make_lon_color_test.py --equator-line       # 赤道に白線 (上下の確認用)
"""
import argparse
import os

import cv2
import numpy as np

# 12色の名前 (色相 0,30,...,330°)。--bands 12 のとき凡例表示に使う
HUE_NAMES_12 = ["赤", "橙", "黄", "黄緑", "緑", "青緑", "シアン", "空色", "青", "紫", "マゼンタ", "ピンク"]


def band_colors_bgr(n):
    """色相環を n 等分した鮮やかな色 (BGR, uint8) を返す。"""
    hsv = np.zeros((1, n, 3), np.uint8)
    hsv[0, :, 0] = (np.arange(n) * 180 // n).astype(np.uint8)  # OpenCV の H は 0..179
    hsv[0, :, 1] = 255
    hsv[0, :, 2] = 255
    return cv2.cvtColor(hsv, cv2.COLOR_HSV2BGR)[0]


def build_frame(w, h, n, equator_line):
    img = np.empty((h, w, 3), np.uint8)
    cols = band_colors_bgr(n)
    x = np.arange(w)
    lon = -180.0 + 360.0 * (x + 0.5) / w                 # 各列の経度 (ファームの u→px と同じ向き)
    band = ((lon + 180.0) / 360.0 * n).astype(int) % n   # 経度 -180° から順に帯 0..n-1
    img[:] = cols[band][None, :, :]
    if equator_line:
        cy = h // 2
        img[cy - 1:cy + 1, :] = (255, 255, 255)
    return img, cols, lon


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--width", type=int, default=320)
    ap.add_argument("--height", type=int, default=160)
    ap.add_argument("--bands", type=int, default=12, help="経度の分割数 (帯の本数)")
    ap.add_argument("--seconds", type=float, default=5.0)
    ap.add_argument("--fps", type=float, default=5.0)
    ap.add_argument("--equator-line", action="store_true", help="赤道に白線を引く")
    ap.add_argument("--out", default=None)
    args = ap.parse_args()

    n = args.bands
    out = args.out or f"data/test_lon_colors{n}.mp4"
    frame, cols, lon = build_frame(args.width, args.height, n, args.equator_line)

    os.makedirs(os.path.dirname(out) or ".", exist_ok=True)
    vw = cv2.VideoWriter(out, cv2.VideoWriter_fourcc(*"mp4v"), args.fps, (args.width, args.height))
    if not vw.isOpened():
        raise SystemExit(f"VideoWriter を開けません: {out}")
    for _ in range(int(args.seconds * args.fps)):
        vw.write(frame)
    vw.release()
    cv2.imwrite(os.path.splitext(out)[0] + ".png", frame)

    print(f"生成完了: {out}  {args.width}x{args.height} {n}帯 ({360.0 / n:.1f}°/帯)")
    print(" 帯#  経度範囲          px範囲     色(BGR)        名前")
    step = 360.0 / n
    for i in range(n):
        lo, hi = -180.0 + i * step, -180.0 + (i + 1) * step
        px = np.nonzero(((lon + 180.0) / 360.0 * n).astype(int) % n == i)[0]
        name = HUE_NAMES_12[i] if n == 12 else ""
        print(f" {i:2d}  {lo:+7.1f}..{hi:+7.1f}   {px.min():3d}..{px.max():3d}   "
              f"{tuple(int(c) for c in cols[i])!s:14s} {name}")
    print("正面(+Y, 経度0°) は px 中央 = 帯 %d と %d の境目。+X 側(経度+90°)が帯 %d 付近。"
          % (n // 2 - 1, n // 2, int(n * 0.75)))


if __name__ == "__main__":
    main()
