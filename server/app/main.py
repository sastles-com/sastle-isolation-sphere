from fastapi import FastAPI
from contextlib import asynccontextmanager
from app.core.config import get_settings
from app.core.ros_manager import ROSManager
from app.services.mqtt_service import MQTTService
from app.api.router import api_router
from app.api.endpoints import websocket

settings = get_settings()

@asynccontextmanager
async def lifespan(app: FastAPI):
    # Startup
    ros_manager = ROSManager()
    ros_manager.start()
    
    # Start MQTT service for IMU data
    mqtt_service = MQTTService()
    mqtt_service.start()
    
    yield
    
    # Shutdown
    mqtt_service.stop()
    ros_manager.stop()

from fastapi.middleware.cors import CORSMiddleware

app = FastAPI(
    title=settings.PROJECT_NAME,
    version=settings.VERSION,
    lifespan=lifespan
)

# Configure CORS
app.add_middleware(
    CORSMiddleware,
    allow_origins=["http://localhost:5173", "http://localhost:3000", "*"], # Allow all for dev convenience
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
