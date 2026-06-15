from fastapi import APIRouter, HTTPException, Request
from pydantic import BaseModel
from typing import Dict, Any, Optional

router = APIRouter()


# ===== 共有 config.json の運用設定 (params / playback) =====
# config = 事前設定の単一ソース。SPHERE ポータルや設定タブから参照・更新する。

class SettingsUpdate(BaseModel):
    params: Optional[Dict[str, Any]] = None
    playback: Optional[Dict[str, Any]] = None


@router.get("/settings")
async def get_settings(request: Request):
    """config.json の params / playback を返す。"""
    return request.app.state.config_service.get_public()


@router.put("/settings")
async def update_settings(request: Request, body: SettingsUpdate):
    """params / playback を更新し config.json に永続化、ライブにも反映する。"""
    cs = request.app.state.config_service
    updated = cs.update(params=body.params, playback=body.playback)
    # ライブ反映: params → StateManager、loop → streamer
    if body.params:
        request.app.state.state_manager.set_initial_params(updated["params"])
    if body.playback is not None and "loop" in body.playback:
        request.app.state.video_streamer.set_loop(updated["playback"]["loop"])
    return updated

# Mock Config Data (In reality, this would read/write config.json)
MOCK_CONFIG = {
  "network": {
    "ssid": "Isolation-Sphere",
    "ip": "192.168.1.100",
    "port": 8080
  },
  "system": {
    "version": "1.0.2",
    "debugMode": False,
    "logLevel": "info"
  },
  "motor": {
    "maxSpeed": 120,
    "acceleration": 50,
    "pid": { "p": 0.5, "i": 0.1, "d": 0.05 }
  }
}

class ConfigUpdate(BaseModel):
    section: str
    data: Dict[str, Any]

@router.get("/")
async def get_config():
    return MOCK_CONFIG

@router.post("/")
async def update_config(update: ConfigUpdate):
    if update.section not in MOCK_CONFIG:
        raise HTTPException(status_code=404, detail="Config section not found")
    
    MOCK_CONFIG[update.section].update(update.data)
    return {"status": "updated", "config": MOCK_CONFIG}
