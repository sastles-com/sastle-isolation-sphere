from fastapi import APIRouter
from app.api.endpoints import websocket, config, playlist, command

api_router = APIRouter()
# WebSocket should NOT have /api prefix - it's a special route
# api_router.include_router(websocket.router, tags=["websocket"]) 
api_router.include_router(config.router, prefix="/config", tags=["config"])
api_router.include_router(playlist.router, prefix="/playlist", tags=["playlist"])
api_router.include_router(command.router, tags=["command"])
