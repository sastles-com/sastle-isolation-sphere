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
from fastapi.responses import FileResponse
from pydantic import BaseModel

from app.core.config import MEDIA_VIDEOS_DIR, MEDIA_THUMBNAILS_DIR

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


# ---- サムネイル (代表フレームを 2:1 JPEG で保存。カードは objectFit:cover で 1:1 表示) ----

def _thumb_path(uuid: str) -> str:
    """uuid からサムネイルファイルパスを導出 (DB に依存しない規約ベース)。"""
    return os.path.join(MEDIA_THUMBNAILS_DIR, f"{uuid}.jpg")


def _generate_thumbnail(video_path: str, out_path: str) -> bool:
    """動画の代表フレーム(先頭~10%位置)を 320x160 JPEG で書き出す。成功で True。"""
    try:
        import cv2
        cap = cv2.VideoCapture(video_path)
        if not cap.isOpened():
            return False
        total = cap.get(cv2.CAP_PROP_FRAME_COUNT) or 0
        if total > 0:
            cap.set(cv2.CAP_PROP_POS_FRAMES, int(total * 0.1))
        ok, frame = cap.read()
        if not ok or frame is None:  # シーク失敗時は先頭にフォールバック
            cap.set(cv2.CAP_PROP_POS_FRAMES, 0)
            ok, frame = cap.read()
        cap.release()
        if not ok or frame is None:
            return False
        frame = cv2.resize(frame, (320, 160))
        os.makedirs(os.path.dirname(out_path), exist_ok=True)
        return bool(cv2.imwrite(out_path, frame, [cv2.IMWRITE_JPEG_QUALITY, 82]))
    except Exception as e:
        logger.warning(f"thumbnail generate failed ({video_path}): {e}")
        return False


def _with_thumb_url(v: dict) -> dict:
    """動画 dict にサムネイル URL を付与して返す (フロントの <img src> 用)。"""
    if v and v.get("id") is not None:
        v["thumbnail_url"] = f"/api/playlist/videos/{v['id']}/thumbnail"
    return v


# ============================ 動画 (素材) ============================

@router.get("/videos")
async def get_videos(request: Request):
    return [_with_thumb_url(v) for v in request.app.state.db.get_videos()]


@router.get("/videos/{video_id}/thumbnail")
async def get_video_thumbnail(request: Request, video_id: int):
    """動画サムネイルを返す。未生成なら元動画から遅延生成する(既存動画のバックフィル)。"""
    v = request.app.state.db.get_video(video_id)
    if not v:
        raise HTTPException(status_code=404, detail="video not found")
    thumb = _thumb_path(v["uuid"])
    if not os.path.exists(thumb):
        src = v.get("converted_path")
        if not src or not os.path.exists(src) or not _generate_thumbnail(src, thumb):
            raise HTTPException(status_code=404, detail="thumbnail unavailable")
    return FileResponse(thumb, media_type="image/jpeg")


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
    # サムネイルを生成 (失敗しても致命ではない。GET 時にも遅延生成でフォールバックする)
    thumb = _thumb_path(vid)
    thumb_ok = _generate_thumbnail(dest, thumb)
    db = request.app.state.db
    row = db.create_video(
        uuid=vid,
        title=title or (file.filename or vid),
        filename=file.filename or f"{vid}{ext}",
        converted_path=dest,   # 元ファイル(再生時にライブ変換)
        thumbnail_path=thumb if thumb_ok else None,
        duration_ms=dur, width=w, height=h, fps=fps,
        size_bytes=size, codec=ext.lstrip("."),
    )
    logger.info(f"uploaded video {vid}: {size}B {w}x{h} {dur}ms thumb={thumb_ok}")
    return _with_thumb_url(row)


@router.delete("/videos/{video_id}")
async def delete_video(request: Request, video_id: int):
    db = request.app.state.db
    v = db.get_video(video_id)
    if not v:
        raise HTTPException(status_code=404, detail="video not found")

    # 再生中/参照中の動画を削除する場合はストリーミング停止 + playback_state の参照解除
    # (playback_state.current_video_id は FK 制約があり、参照が残ると削除が失敗する)
    streamer = request.app.state.video_streamer
    if streamer.current_video_id == video_id:
        streamer.stop()
    ps = db.get_playback_state() or {}
    if ps.get("current_video_id") == video_id:
        db.clear_playback_state()

    path = v.get("converted_path")
    if path and os.path.exists(path):
        try:
            os.remove(path)
        except Exception as e:
            logger.warning(f"file remove failed ({path}): {e}")
    thumb = _thumb_path(v["uuid"])
    if os.path.exists(thumb):
        try:
            os.remove(thumb)
        except Exception as e:
            logger.warning(f"thumbnail remove failed ({thumb}): {e}")
    db.delete_video(video_id)
    return {"status": "ok", "deleted": video_id}


# ============================ プレイリスト ============================

class PlaylistCreate(BaseModel):
    name: str
    description: Optional[str] = None
    loop: bool = False
    shuffle: bool = False


class PlaylistItemAdd(BaseModel):
    video_id: int


class PlaylistItemsSet(BaseModel):
    video_ids: list[int]


@router.get("/playlists")
async def get_playlists(request: Request):
    """プレイリスト一覧 (各々のアイテム数 item_count 付き)。"""
    db = request.app.state.db
    playlists = db.get_playlists()
    for p in playlists:
        p["item_count"] = len(db.get_playlist_items(p["id"]))
    return playlists


@router.get("/playlists/{playlist_id}")
async def get_playlist_detail(request: Request, playlist_id: int):
    """プレイリスト詳細 (動画情報込みのアイテム列)。"""
    db = request.app.state.db
    p = db.get_playlist(playlist_id)
    if not p:
        raise HTTPException(status_code=404, detail="playlist not found")
    p["items"] = [_with_thumb_url(it) for it in db.get_playlist_items_detailed(playlist_id)]
    return p


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


# ---------------------- プレイリストアイテム (追加/削除/並び替え) ----------------------

def _require_playlist(db, playlist_id):
    p = db.get_playlist(playlist_id)
    if not p:
        raise HTTPException(status_code=404, detail="playlist not found")
    return p


@router.post("/playlists/{playlist_id}/items")
async def add_playlist_item(request: Request, playlist_id: int, body: PlaylistItemAdd):
    """動画をプレイリスト末尾に追加する。"""
    db = request.app.state.db
    _require_playlist(db, playlist_id)
    if not db.get_video(body.video_id):
        raise HTTPException(status_code=404, detail="video not found")
    items = db.get_playlist_items(playlist_id)
    db.add_playlist_item(playlist_id, body.video_id, len(items))
    return {"status": "ok", "items": db.get_playlist_items_detailed(playlist_id)}


@router.delete("/playlists/{playlist_id}/items/{item_id}")
async def remove_playlist_item(request: Request, playlist_id: int, item_id: int):
    """アイテムを1件削除し、position を詰め直す。"""
    db = request.app.state.db
    _require_playlist(db, playlist_id)
    items = db.get_playlist_items(playlist_id)
    remaining = [it["video_id"] for it in items if it["id"] != item_id]
    if len(remaining) == len(items):
        raise HTTPException(status_code=404, detail="item not found")
    db.set_playlist_items(playlist_id, remaining)
    return {"status": "ok", "items": db.get_playlist_items_detailed(playlist_id)}


@router.put("/playlists/{playlist_id}/items")
async def set_playlist_items(request: Request, playlist_id: int, body: PlaylistItemsSet):
    """アイテムを指定順で総入れ替えする (並び替え/一括設定)。"""
    db = request.app.state.db
    _require_playlist(db, playlist_id)
    for vid in body.video_ids:
        if not db.get_video(vid):
            raise HTTPException(status_code=404, detail=f"video not found: {vid}")
    db.set_playlist_items(playlist_id, body.video_ids)
    return {"status": "ok", "items": db.get_playlist_items_detailed(playlist_id)}


# ============================ 再生制御 (ストリーミング) ============================
# 動画を選択 → サーバーが OpenCV でデコードし 320x160 JPEG チャンクとして UDP 送出。
# 制御は VideoStreamer (別スレッド)。状態は playback_state DB にも反映する。

def _sync_playback_state(db, streamer):
    """ストリーマ状態を playback_state DB に反映 (Web UI 表示・永続化用)。"""
    try:
        db.update_playback_state(
            status=streamer.status,
            current_video_id=streamer.current_video_id,
            current_playlist_id=streamer.current_playlist_id,
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


@router.post("/playlists/{playlist_id}/play")
async def play_playlist(request: Request, playlist_id: int):
    """プレイリストを順次ストリーミング再生する (loop/shuffle はプレイリスト設定に従う)。"""
    db = request.app.state.db
    streamer = request.app.state.video_streamer
    p = _require_playlist(db, playlist_id)
    items = db.get_playlist_items_detailed(playlist_id)
    entries = [
        {"path": it.get("converted_path"), "fps": it.get("fps") or None, "video_id": it.get("id")}
        for it in items
        if it.get("converted_path") and os.path.exists(it["converted_path"])
    ]
    if not entries:
        raise HTTPException(status_code=400, detail="playlist has no playable videos")
    if p.get("shuffle"):
        import random
        random.shuffle(entries)
    ok = streamer.play_entries(entries, loop=bool(p.get("loop")), playlist_id=playlist_id)
    if not ok:
        raise HTTPException(status_code=500, detail="failed to start streaming")
    _sync_playback_state(db, streamer)
    logger.info(f"play playlist {playlist_id}: {len(entries)} videos loop={p.get('loop')}")
    return {"status": "ok", "playback": streamer.get_status()}


def start_configured_playback(db, streamer, config_service):
    """config.playback の active_playlist を loop/shuffle 設定どおりに再生開始する。

    SPHERE ポータルの「再生」と起動時 autoplay の共通処理。
    戻り値: (ok: bool, detail: str)
    """
    pb = config_service.get_playback()
    pid = pb.get("active_playlist")
    if not pid:
        return False, "no active playlist configured"
    p = db.get_playlist(pid)
    if not p:
        return False, f"active playlist {pid} not found"
    items = db.get_playlist_items_detailed(pid)
    entries = [
        {"path": it.get("converted_path"), "fps": it.get("fps") or None, "video_id": it.get("id")}
        for it in items
        if it.get("converted_path") and os.path.exists(it["converted_path"])
    ]
    if not entries:
        return False, "active playlist has no playable videos"
    if pb.get("shuffle"):
        import random
        random.shuffle(entries)
    streamer.play_entries(entries, loop=bool(pb.get("loop")), playlist_id=pid)
    logger.info(f"start configured playback: playlist {pid}, {len(entries)} videos")
    return True, "ok"


@router.post("/playback/start")
async def start_playback(request: Request):
    """config の active playlist を再生開始する (SPHERE ポータルの主要操作)。"""
    db = request.app.state.db
    streamer = request.app.state.video_streamer
    config_service = request.app.state.config_service
    ok, detail = start_configured_playback(db, streamer, config_service)
    if not ok:
        raise HTTPException(status_code=400, detail=detail)
    _sync_playback_state(db, streamer)
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


class LoopSet(BaseModel):
    loop: bool


@router.post("/playback/loop")
async def set_playback_loop(request: Request, body: LoopSet):
    """ループ再生のON/OFFを切り替える (再生中も即時反映)。"""
    streamer = request.app.state.video_streamer
    streamer.set_loop(body.loop)
    return {"status": "ok", "playback": streamer.get_status()}


@router.get("/playback")
async def get_playback(request: Request):
    return request.app.state.video_streamer.get_status()
