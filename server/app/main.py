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

    # 時刻同期ビーコンを1秒周期でブロードキャスト (複数コアの共通タイムベース)
    clock_task = asyncio.create_task(mqtt_service.clock_publisher_loop())

    # SQLite データベース初期化 (動画/プレイリスト)
    db = Database(DB_PATH)

    # 動画ストリーマ初期化 (再生時に動画→JPEGチャンク→UDPでデバイスへ送出)
    video_streamer = VideoStreamer()
    # 再生フレームを間引いて UI へも配信 (デジタルツイン用 FRAME_PREVIEW)
    video_streamer.set_preview_broadcaster(state_manager, loop)

    # 共有 config.json の運用設定 (params / playback)
    config_service = ConfigService()

    # アプリケーション状態に保存
    app.state.state_manager = state_manager
    app.state.mqtt_service = mqtt_service
    app.state.db = db
    app.state.video_streamer = video_streamer
    app.state.config_service = config_service

    def retarget_streamer(online_ids=None):
        """映像 (UDP) の宛先を「操作対象 ∩ オンライン」に絞る。

        到達不能な core を宛先に含めると、その IP の ARP が解決できないまま
        カーネルの未解決近傍キューと wlp1s0 の送信キューを埋め、生きている core
        へのチャンクまで EWOULDBLOCK で落ちる (実測 9%欠損 = フレームの過半が
        再構成不能)。宛先から外すのが唯一の構造的な解。

        online_ids が None のときは StateManager の現在値を使う。
        オンラインが1台も無い場合は絞り込まず操作対象そのままに戻す:
        死活判定の一時的な取りこぼしや、起動直後でまだ publish していない core
        で映像が出なくなるのを防ぐ (フェイルオープン)。
        """
        active = config_service.get_active_sphere()
        wanted = config_service.get_target_ips(active)
        if online_ids is None:
            online_ids = state_manager.online_ids()
        live_ips = {
            sp.get("static_ip") for sp in config_service.get_spheres()
            if sp.get("id") in set(online_ids) and sp.get("static_ip")
        }
        effective = [ip for ip in wanted if ip in live_ips] or wanted
        video_streamer.set_targets(effective)

    app.state.retarget_streamer = retarget_streamer

    # core の死活監視。publish が途切れた core を online から外して UI に伝え、
    # 同時に映像の宛先からも外す (落ちた core が生存 core の映像を妨害する)
    presence_task = asyncio.create_task(
        state_manager.presence_sweep_loop(on_change=retarget_streamer))

    # config の事前設定を反映: spheres / 操作対象 / params 既定値 / loop / autoplay
    try:
        # デバイスレジストリ (config.json の spheres[]) と既定の操作対象を反映。
        # コマンドは MQTT sphere/<id>/command/*、映像は UDP でこの core に送られる。
        state_manager.set_spheres(config_service.get_spheres())
        active_sphere = config_service.get_active_sphere()
        state_manager.set_initial_target(active_sphere)
        # 起動直後はまだ誰も publish していないので、フェイルオープンで
        # 操作対象そのまま (= 従来と同じ宛先) になる
        retarget_streamer()

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
    clock_task.cancel()
    presence_task.cancel()
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

# Pattern Studio (別 Vite アプリ) を /studio に mount。catch-all より前に置く (spec §8)。
if os.path.exists("frontend-studio/dist/assets"):
    app.mount("/studio/assets", StaticFiles(directory="frontend-studio/dist/assets"), name="studio-assets")

# WebSocket route (no /api prefix)
app.include_router(websocket.router, tags=["websocket"])

# API routes (with /api prefix)
app.include_router(api_router, prefix="/api")

@app.get("/health")
async def health():
    return {"status": "ok"}

# Pattern Studio の index (catch-all より前)。/studio と /studio/ の両方に対応。
@app.get("/studio")
@app.get("/studio/")
async def serve_studio():
    studio_index = "frontend-studio/dist/index.html"
    if os.path.exists(studio_index):
        return FileResponse(studio_index)
    return {"message": "Pattern Studio not built. Run 'npm run build' in frontend-studio."}

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
