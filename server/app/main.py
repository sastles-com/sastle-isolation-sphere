import logging

from fastapi import FastAPI
from contextlib import asynccontextmanager
from app.core.config import get_settings, CORS_ORIGINS, DB_PATH
from app.services.state_manager import StateManager
from app.services.mqtt_service import MQTTService
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

    # アプリケーション状態に保存
    app.state.state_manager = state_manager
    app.state.mqtt_service = mqtt_service
    app.state.db = db

    yield

    # Shutdown
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
