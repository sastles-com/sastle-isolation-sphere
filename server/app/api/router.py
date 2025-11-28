from fastapi import APIRouter
from app.api.endpoints import websocket, config, playlist

api_router = APIRouter()
api_router.include_router(websocket.router, tags=["websocket"])
api_router.include_router(config.router, prefix="/config", tags=["config"])
api_router.include_router(playlist.router, prefix="/playlist", tags=["playlist"])
