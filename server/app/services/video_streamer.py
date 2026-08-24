"""動画をデバイスへUDPストリーミングするサービス。

別スレッドで OpenCV により動画をデコード → 320x160 にリサイズ → JPEG化 →
チャンク分割(プロトコルは docs/protocol_spec.md §4)→ UDP送出する。
play()/pause()/resume()/stop() で制御。送信先は config.json (sphere.static_ip /
wifi.udp_port)。ファーム ImageManager の UDPChunkHeader と一致させること。
"""
import asyncio
import base64
import json
import logging
import math
import os
import socket
import struct
import threading
import time

from app.core.config import CONFIG_SEARCH_PATHS

logger = logging.getLogger(__name__)

MAGIC = 0x4A504547        # "JPEG"
MAX_CHUNK = 1400          # ファーム MAX_CHUNK_DATA と一致
# 送出fpsの上限。デバイスのデコード能力(~20fps)に合わせ、かつ送出スレッドが
# 常に sleep を挟めるようにして uvicorn イベントループの starvation を防ぐ。
MAX_STREAM_FPS = 20.0
# UI デジタルツイン用プレビュー配信レート。デバイス宛 UDP とは別に、間引いた
# フレームを WebSocket で UI へ流す (30fps は UI プレビューに不要・WS を圧迫しない)。
PREVIEW_FPS = 5.0
PREVIEW_PERIOD = 1.0 / PREVIEW_FPS


def _load_device_target():
    """config.json から (device_ip, udp_port, width, height) を取得。"""
    for p in CONFIG_SEARCH_PATHS:
        if os.path.exists(p):
            try:
                with open(p) as f:
                    c = json.load(f)
                return (
                    c.get("sphere", {}).get("static_ip", "192.168.49.101"),
                    int(c.get("wifi", {}).get("udp_port", 8889)),
                    int(c.get("image", {}).get("width", 320)),
                    int(c.get("image", {}).get("height", 160)),
                )
            except Exception as e:
                logger.warning(f"config load failed ({p}): {e}")
    return ("192.168.49.101", 8889, 320, 160)


class VideoStreamer:
    def __init__(self, quality: int = 80):
        ip, port, w, h = _load_device_target()
        self._target = (ip, port)
        self._w, self._h, self._q = w, h, quality
        self._sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self._thread = None
        self._stop = threading.Event()
        self._pause = threading.Event()
        self.status = "stopped"          # playing/paused/stopped/error
        self.current_path = None
        self.current_video_id = None
        self.current_playlist_id = None
        self._loop = True                # ループ再生フラグ (再生中も変更可能)
        self._blank_fid = 0              # 黒フレーム用 frame_id (本編の連番と衝突させない)
        # UI プレビュー配信 (デジタルツイン用)。main.py で set_preview_broadcaster() 済み。
        self._preview_sm = None          # StateManager
        self._preview_loop = None        # uvicorn イベントループ (別スレッドから投入する)
        logger.info(f"VideoStreamer target={self._target} size={self._w}x{self._h}")

    def set_preview_broadcaster(self, state_manager, loop):
        """UI へのプレビュー配信先 (StateManager) とイベントループを登録する。

        送出は別スレッド (_run) から run_coroutine_threadsafe でループへ投入する
        (mqtt_service._submit_coroutine と同じ流儀)。
        """
        self._preview_sm = state_manager
        self._preview_loop = loop

    def _emit_preview(self, seq: int, jpeg_bytes: bytes):
        """デコード済み JPEG 1枚を UI へ WebSocket 配信する (5fps に間引き済み)。"""
        sm, loop = self._preview_sm, self._preview_loop
        if not sm or not loop:
            return
        try:
            b64 = base64.b64encode(jpeg_bytes).decode("ascii")
            asyncio.run_coroutine_threadsafe(
                sm.broadcast_frame_preview(b64, self._w, self._h, seq), loop)
        except Exception as e:
            logger.debug(f"preview emit failed: {e}")

    def get_status(self):
        return {"status": self.status, "video_id": self.current_video_id,
                "playlist_id": self.current_playlist_id, "path": self.current_path,
                "loop": self._loop, "target": list(self._target)}

    def set_loop(self, loop: bool):
        """ループ再生フラグを切り替える (再生中に変更すると末尾到達時の挙動に反映)。"""
        self._loop = bool(loop)
        return self._loop

    def play(self, path: str, fps=None, loop: bool = True, video_id=None):
        """単一動画を再生 (loop=True で繰り返し)。"""
        return self.play_entries(
            [{"path": path, "fps": fps, "video_id": video_id}], loop=loop)

    def play_entries(self, entries, loop: bool = True, playlist_id=None):
        """動画エントリ列 [{path, fps, video_id}] を順次再生する。

        各動画は最後まで再生してから次へ進む。末尾まで来たら loop なら先頭へ戻る。
        """
        self.stop(blank=False)  # 直後に新しい映像を流すので消灯は不要
        entries = [e for e in entries if e.get("path") and os.path.exists(e["path"])]
        if not entries:
            self.status = "error"
            logger.error("no playable entries")
            self._send_black_frame()  # 再生しないので前の映像を残さない
            return False
        self._stop.clear()
        self._pause.clear()
        self._loop = bool(loop)
        self.current_playlist_id = playlist_id
        self.current_video_id = entries[0].get("video_id")
        self.current_path = entries[0].get("path")
        self.status = "playing"
        self._thread = threading.Thread(target=self._run, args=(entries,), daemon=True)
        self._thread.start()
        return True

    def pause(self):
        if self.status == "playing":
            self._pause.set()
            self.status = "paused"

    def resume(self):
        if self.status == "paused":
            self._pause.clear()
            self.status = "playing"

    def toggle(self):
        if self.status == "playing":
            self.pause()
        elif self.status == "paused":
            self.resume()

    def stop(self, blank: bool = True):
        """再生を停止する。

        blank=False は play_entries() が「直前の再生を畳む」ために呼ぶ場合に使う。
        直後に新しい映像を流すので、消灯は無駄な 60ms の遅延と黒画面の一瞬の
        点滅にしかならない。
        """
        self._stop.set()
        if self._thread and self._thread.is_alive():
            self._thread.join(timeout=2.0)
        self._thread = None
        self.status = "stopped"
        self.current_path = None
        self.current_video_id = None
        self.current_playlist_id = None
        if blank:
            self._send_black_frame()

    def _send_black_frame(self):
        """停止時に黒フレームを送る。デバイスは最後に受信したフレームを
        表示し続けるため、これが無いと停止後も残像が出続ける。

        UDP は取りこぼすので数回送る。1枚落ちただけで残像が残るのを避ける。
        frame_id は 0xFFFFFFFF を避けること: 旧ファームの FrameReassembler は
        その値を「未受信」センチネルとして使っており、黒フレームが新フレームと
        判定されず丸ごと無視されていた (このバグの原因)。
        """
        try:
            import cv2
            import numpy as np
            black = np.zeros((self._h, self._w, 3), np.uint8)
            ok, jpeg = cv2.imencode(".jpg", black, [cv2.IMWRITE_JPEG_QUALITY, 80])
            if not ok:
                logger.warning("black frame encode failed")
                return
            data = jpeg.tobytes()
            # 3回送るが間隔は詰める: stop() は同期呼び出しなので、ここでの sleep が
            # そのまま uvicorn のイベントループを止める。合計 20ms に抑える。
            for _ in range(3):
                self._blank_fid = (self._blank_fid + 1) & 0x7FFFFFFF
                self._send_frame(self._blank_fid, data)
                time.sleep(0.01)
            # UI プレビューも同時に黒へ (再生中しか送っていないため、
            # これが無いと WebUI 側に最後のフレームが残る)
            self._emit_preview(self._blank_fid, data)
        except Exception as e:  # 停止処理は失敗しても継続
            logger.warning(f"black frame send failed: {e}")

    def _run(self, entries):
        """エントリ列を順次再生。各動画を最後まで送出し、末尾で self._loop なら先頭へ。"""
        import cv2
        fid = 0
        idx = 0
        try:
            while not self._stop.is_set():
                entry = entries[idx]
                self.current_video_id = entry.get("video_id")
                self.current_path = entry.get("path")
                cap = cv2.VideoCapture(entry["path"])
                if not cap.isOpened():
                    logger.error(f"cannot open video, skipping: {entry['path']}")
                else:
                    src_fps = float(entry.get("fps") or cap.get(cv2.CAP_PROP_FPS) or 15.0)
                    # 送出fpsを上限化。速度維持のため skip フレームずつ読み進めて1枚送る。
                    skip = max(1, math.ceil(src_fps / MAX_STREAM_FPS))
                    eff_fps = src_fps / skip
                    period = 1.0 / max(1.0, eff_fps)
                    logger.info(f"streaming {entry['path']} src={src_fps:.0f}fps "
                                f"-> {eff_fps:.1f}fps (skip {skip}) -> {self._target}")
                    next_t = time.time()
                    next_preview = 0.0   # 次に UI プレビューを送る時刻 (即送出から開始)
                    while not self._stop.is_set():
                        if self._pause.is_set():
                            time.sleep(0.05)
                            next_t = time.time()
                            continue
                        # skip フレーム読み進め、最後の1枚を送る (再生速度を維持)
                        frame = None
                        for _ in range(skip):
                            ok, f = cap.read()
                            if not ok:
                                break
                            frame = f
                        if frame is None:
                            break  # この動画は終端
                        frame = cv2.resize(frame, (self._w, self._h))
                        ok2, jpg = cv2.imencode(".jpg", frame, [cv2.IMWRITE_JPEG_QUALITY, self._q])
                        if ok2:
                            data = jpg.tobytes()
                            self._send_frame(fid, data)
                            # UI プレビューを PREVIEW_FPS に間引いて配信 (再生中のみ)
                            now = time.time()
                            if now >= next_preview:
                                self._emit_preview(fid, data)
                                next_preview = now + PREVIEW_PERIOD
                            fid += 1
                        next_t += period
                        sleep = next_t - time.time()
                        # 追いついていても最低 3ms は必ず sleep して GIL を譲る
                        # (これがないと CPU を握り続けイベントループが応答しなくなる)
                        time.sleep(sleep if sleep > 0.003 else 0.003)
                        if sleep <= 0:
                            next_t = time.time()  # 追いつけない場合リセット
                    cap.release()
                if self._stop.is_set():
                    break
                idx += 1
                if idx >= len(entries):
                    if self._loop:
                        idx = 0
                    else:
                        break
        finally:
            if not self._stop.is_set():
                # 自然終了 (loop=False でプレイリスト末尾に到達)。stop() を経由しない
                # ためここで消灯する。これが無いと最後のフレームが残り続けていた。
                self.status = "stopped"
                self.current_path = None
                self.current_video_id = None
                self._send_black_frame()
        logger.info("streaming ended")

    def _send_frame(self, fid, jpeg):
        n = (len(jpeg) + MAX_CHUNK - 1) // MAX_CHUNK
        for ci in range(n):
            part = jpeg[ci * MAX_CHUNK:(ci + 1) * MAX_CHUNK]
            hdr = struct.pack("<IIHHHH", MAGIC, fid & 0xFFFFFFFF, ci, n, len(part), 0)
            self._sock.sendto(hdr + part, self._target)
