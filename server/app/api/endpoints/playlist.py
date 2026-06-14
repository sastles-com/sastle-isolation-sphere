"""動画・プレイリスト API (SQLite DB バックエンド)。

素材動画(videos) と プレイリスト(playlists/playlist_items) を管理する。
設計: docs/playlist_system_design.md。DB: app/db/database.py。
動画は再生時に 320x160 JPEG にライブ変換して送出するため、ここでは元ファイルを
そのまま保存する(事前RGB565変換はしない)。
"""
import logging
import os
import time
import uuid as uuidlib
from typing import Optional

from fastapi import APIRouter, HTTPException, Request, UploadFile, File, Form
from pydantic import BaseModel

from app.core.config import MEDIA_VIDEOS_DIR

logger = logging.getLogger(__name__)
router = APIRouter()

ALLOWED_EXT = {".mp4", ".mov", ".avi", ".webm", ".mkv"}


def _extract_metadata(path: str):
    """OpenCV があれば (duration_ms, width, height, fps) を返す。無ければ既定値。"""
    try:
        import cv2
        cap = cv2.VideoCapture(path)
        if not cap.isOpened():
            return (0, 0, 0, 30)
        fps = cap.get(cv2.CAP_PROP_FPS) or 30.0
        frames = cap.get(cv2.CAP_PROP_FRAME_COUNT) or 0
        w = int(cap.get(cv2.CAP_PROP_FRAME_WIDTH) or 0)
        h = int(cap.get(cv2.CAP_PROP_FRAME_HEIGHT) or 0)
        cap.release()
        dur = int(frames / fps * 1000) if fps else 0
        return (dur, w, h, int(round(fps)) or 30)
    except Exception as e:
        logger.warning(f"metadata extract failed ({path}): {e}")
        return (0, 0, 0, 30)


# ============================ 動画 (素材) ============================

@router.get("/videos")
async def get_videos(request: Request):
    return request.app.state.db.get_videos()


@router.post("/videos")
async def upload_video(
    request: Request,
    file: UploadFile = File(...),
    title: Optional[str] = Form(None),
):
    """動画をアップロードして保存・DB登録する。"""
    ext = os.path.splitext(file.filename or "")[1].lower()
    if ext not in ALLOWED_EXT:
        raise HTTPException(status_code=400, detail=f"unsupported format: {ext or '(none)'}")

    os.makedirs(MEDIA_VIDEOS_DIR, exist_ok=True)
    vid = f"v_{int(time.time())}_{uuidlib.uuid4().hex[:6]}"
    dest = os.path.join(MEDIA_VIDEOS_DIR, f"{vid}{ext}")

    size = 0
    try:
        with open(dest, "wb") as out:
            while True:
                chunk = await file.read(1 << 20)  # 1MBずつ
                if not chunk:
                    break
                out.write(chunk)
                size += len(chunk)
    except Exception as e:
        if os.path.exists(dest):
            os.remove(dest)
        raise HTTPException(status_code=500, detail=f"save failed: {e}")

    dur, w, h, fps = _extract_metadata(dest)
    db = request.app.state.db
    row = db.create_video(
        uuid=vid,
        title=title or (file.filename or vid),
        filename=file.filename or f"{vid}{ext}",
        converted_path=dest,   # 元ファイル(再生時にライブ変換)
        duration_ms=dur, width=w, height=h, fps=fps,
        size_bytes=size, codec=ext.lstrip("."),
    )
    logger.info(f"uploaded video {vid}: {size}B {w}x{h} {dur}ms")
    return row


@router.delete("/videos/{video_id}")
async def delete_video(request: Request, video_id: int):
    db = request.app.state.db
    v = db.get_video(video_id)
    if not v:
        raise HTTPException(status_code=404, detail="video not found")
    path = v.get("converted_path")
    if path and os.path.exists(path):
        try:
            os.remove(path)
        except Exception as e:
            logger.warning(f"file remove failed ({path}): {e}")
    db.delete_video(video_id)
    return {"status": "ok", "deleted": video_id}


# ============================ プレイリスト ============================

class PlaylistCreate(BaseModel):
    name: str
    description: Optional[str] = None
    loop: bool = False
    shuffle: bool = False


@router.get("/playlists")
async def get_playlists(request: Request):
    return request.app.state.db.get_playlists()


@router.post("/playlists")
async def create_playlist(request: Request, body: PlaylistCreate):
    pid = f"p_{int(time.time())}_{uuidlib.uuid4().hex[:6]}"
    return request.app.state.db.create_playlist(
        uuid=pid, name=body.name, description=body.description,
        loop=body.loop, shuffle=body.shuffle,
    )


@router.delete("/playlists/{playlist_id}")
async def delete_playlist(request: Request, playlist_id: int):
    db = request.app.state.db
    if not db.get_playlist(playlist_id):
        raise HTTPException(status_code=404, detail="playlist not found")
    db.delete_playlist(playlist_id)
    return {"status": "ok", "deleted": playlist_id}


# ============================ 再生制御 (ストリーミング) ============================
# 動画を選択 → サーバーが OpenCV でデコードし 320x160 JPEG チャンクとして UDP 送出。
# 制御は VideoStreamer (別スレッド)。状態は playback_state DB にも反映する。

def _sync_playback_state(db, streamer):
    """ストリーマ状態を playback_state DB に反映 (Web UI 表示・永続化用)。"""
    try:
        db.update_playback_state(
            status=streamer.status,
            current_video_id=streamer.current_video_id,
        )
    except Exception as e:  # DB スキーマ差異等で落とさない
        logger.warning(f"playback_state sync skipped: {e}")


@router.post("/play/{video_id}")
async def play_video(request: Request, video_id: int):
    """指定した素材動画の再生(デバイスへのストリーミング)を開始する。"""
    db = request.app.state.db
    streamer = request.app.state.video_streamer
    v = db.get_video(video_id)
    if not v:
        raise HTTPException(status_code=404, detail="video not found")
    path = v.get("converted_path")
    if not path or not os.path.exists(path):
        raise HTTPException(status_code=410, detail="video file missing on disk")
    ok = streamer.play(path, fps=v.get("fps") or None, loop=True, video_id=video_id)
    if not ok:
        raise HTTPException(status_code=500, detail="failed to start streaming")
    _sync_playback_state(db, streamer)
    logger.info(f"play video {video_id}: {path}")
    return {"status": "ok", "playback": streamer.get_status()}


@router.post("/playback/pause")
async def pause_playback(request: Request):
    streamer = request.app.state.video_streamer
    streamer.toggle()
    _sync_playback_state(request.app.state.db, streamer)
    return {"status": "ok", "playback": streamer.get_status()}


@router.post("/playback/stop")
async def stop_playback(request: Request):
    streamer = request.app.state.video_streamer
    streamer.stop()
    _sync_playback_state(request.app.state.db, streamer)
    return {"status": "ok", "playback": streamer.get_status()}


@router.get("/playback")
async def get_playback(request: Request):
    return request.app.state.video_streamer.get_status()
