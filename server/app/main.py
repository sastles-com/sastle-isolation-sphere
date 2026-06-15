import logging

from fastapi import FastAPI
from contextlib import asynccontextmanager
from app.core.config import get_settings, CORS_ORIGINS, DB_PATH
from app.services.state_manager import StateManager
from app.services.mqtt_service import MQTTService
from app.services.video_streamer import VideoStreamer
from app.services.config_service import ConfigService
from app.db import Database
from app.api.router import api_router
from app.api.endpoints import websocket

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s %(levelname)s %(name)s: %(message)s",
)

settings = get_settings()

@asynccontextmanager
async def lifespan(app: FastAPI):
    # Startup
    import asyncio
    loop = asyncio.get_event_loop()
    
    # StateManager初期化
    state_manager = StateManager()
    
    # MQTTService初期化
    mqtt_service = MQTTService()
    
    # StateManager ↔ MQTTService 連携
    mqtt_service.state_manager = state_manager
    mqtt_service.set_event_loop(loop)
    state_manager.set_mqtt_client(mqtt_service.client)
    
    # MQTT接続開始
    mqtt_service.start()
    
    # SQLite データベース初期化 (動画/プレイリスト)
    db = Database(DB_PATH)

    # 動画ストリーマ初期化 (再生時に動画→JPEGチャンク→UDPでデバイスへ送出)
    video_streamer = VideoStreamer()

    # 共有 config.json の運用設定 (params / playback)
    config_service = ConfigService()

    # アプリケーション状態に保存
    app.state.state_manager = state_manager
    app.state.mqtt_service = mqtt_service
    app.state.db = db
    app.state.video_streamer = video_streamer
    app.state.config_service = config_service

    # config の事前設定を反映: params 既定値 / loop / autoplay
    try:
        params = config_service.get_params()
        state_manager.set_initial_params(params)
        playback_cfg = config_service.get_playback()
        video_streamer.set_loop(bool(playback_cfg.get("loop", True)))
        if playback_cfg.get("autoplay") and playback_cfg.get("active_playlist"):
            from app.api.endpoints.playlist import start_configured_playback
            ok, detail = start_configured_playback(db, video_streamer, config_service)
            logging.getLogger("app.main").info(f"autoplay: {ok} ({detail})")
    except Exception as e:
        logging.getLogger("app.main").warning(f"config apply failed: {e}")

    yield

    # Shutdown
    video_streamer.stop()
    mqtt_service.stop()
    db.close()

from fastapi.middleware.cors import CORSMiddleware

app = FastAPI(
    title=settings.PROJECT_NAME,
    version=settings.VERSION,
    lifespan=lifespan
)

# Configure CORS
app.add_middleware(
    CORSMiddleware,
    allow_origins=CORS_ORIGINS,
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

# Serve frontend static files
import os
from fastapi.staticfiles import StaticFiles
from fastapi.responses import FileResponse

# Mount assets folder BEFORE other routes
if os.path.exists("frontend/dist/assets"):
    app.mount("/assets", StaticFiles(directory="frontend/dist/assets"), name="assets")

# WebSocket route (no /api prefix)
app.include_router(websocket.router, tags=["websocket"])

# API routes (with /api prefix)
app.include_router(api_router, prefix="/api")

@app.get("/health")
async def health():
    return {"status": "ok"}

# Catch-all route for SPA (must be LAST)
@app.get("/{full_path:path}")
async def serve_frontend(full_path: str):
    # If API path not matched above, serve index.html
    if full_path.startswith("api"):
        return {"error": "API endpoint not found"}
        
    index_path = "frontend/dist/index.html"
    if os.path.exists(index_path):
        return FileResponse(index_path)
    return {"message": "Frontend not built. Please run 'npm run build' in frontend directory."}
