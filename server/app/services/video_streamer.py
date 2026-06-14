"""動画をデバイスへUDPストリーミングするサービス。

別スレッドで OpenCV により動画をデコード → 320x160 にリサイズ → JPEG化 →
チャンク分割(プロトコルは docs/protocol_spec.md §4)→ UDP送出する。
play()/pause()/resume()/stop() で制御。送信先は config.json (sphere.static_ip /
wifi.udp_port)。ファーム ImageManager の UDPChunkHeader と一致させること。
"""
import json
import logging
import os
import socket
import struct
import threading
import time

from app.core.config import CONFIG_SEARCH_PATHS

logger = logging.getLogger(__name__)

MAGIC = 0x4A504547        # "JPEG"
MAX_CHUNK = 1400          # ファーム MAX_CHUNK_DATA と一致


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
        logger.info(f"VideoStreamer target={self._target} size={self._w}x{self._h}")

    def get_status(self):
        return {"status": self.status, "video_id": self.current_video_id,
                "playlist_id": self.current_playlist_id, "path": self.current_path,
                "target": list(self._target)}

    def play(self, path: str, fps=None, loop: bool = True, video_id=None):
        """単一動画を再生 (loop=True で繰り返し)。"""
        return self.play_entries(
            [{"path": path, "fps": fps, "video_id": video_id}], loop=loop)

    def play_entries(self, entries, loop: bool = True, playlist_id=None):
        """動画エントリ列 [{path, fps, video_id}] を順次再生する。

        各動画は最後まで再生してから次へ進む。末尾まで来たら loop なら先頭へ戻る。
        """
        self.stop()
        entries = [e for e in entries if e.get("path") and os.path.exists(e["path"])]
        if not entries:
            self.status = "error"
            logger.error("no playable entries")
            return False
        self._stop.clear()
        self._pause.clear()
        self.current_playlist_id = playlist_id
        self.current_video_id = entries[0].get("video_id")
        self.current_path = entries[0].get("path")
        self.status = "playing"
        self._thread = threading.Thread(target=self._run, args=(entries, loop), daemon=True)
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

    def stop(self):
        self._stop.set()
        if self._thread and self._thread.is_alive():
            self._thread.join(timeout=2.0)
        self._thread = None
        self.status = "stopped"
        self.current_path = None
        self.current_video_id = None
        self.current_playlist_id = None

    def _run(self, entries, loop):
        """エントリ列を順次再生。各動画を最後まで送出し、末尾で loop なら先頭へ。"""
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
                    period = 1.0 / max(1.0, src_fps)
                    logger.info(f"streaming {entry['path']} @ {src_fps:.1f}fps -> {self._target}")
                    next_t = time.time()
                    while not self._stop.is_set():
                        if self._pause.is_set():
                            time.sleep(0.05)
                            next_t = time.time()
                            continue
                        ok, frame = cap.read()
                        if not ok:
                            break  # この動画は終端
                        frame = cv2.resize(frame, (self._w, self._h))
                        ok2, jpg = cv2.imencode(".jpg", frame, [cv2.IMWRITE_JPEG_QUALITY, self._q])
                        if ok2:
                            self._send_frame(fid, jpg.tobytes())
                            fid += 1
                        next_t += period
                        sleep = next_t - time.time()
                        if sleep > 0:
                            time.sleep(sleep)
                        else:
                            next_t = time.time()  # 追いつけない場合リセット
                    cap.release()
                if self._stop.is_set():
                    break
                idx += 1
                if idx >= len(entries):
                    if loop:
                        idx = 0
                    else:
                        break
        finally:
            if not self._stop.is_set():
                self.status = "stopped"   # 自然終了 (loop=False)
        logger.info("streaming ended")

    def _send_frame(self, fid, jpeg):
        n = (len(jpeg) + MAX_CHUNK - 1) // MAX_CHUNK
        for ci in range(n):
            part = jpeg[ci * MAX_CHUNK:(ci + 1) * MAX_CHUNK]
            hdr = struct.pack("<IIHHHH", MAGIC, fid & 0xFFFFFFFF, ci, n, len(part), 0)
            self._sock.sendto(hdr + part, self._target)
