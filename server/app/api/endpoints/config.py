import csv
import os

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
    """config.json の params / playback / spheres を返す。"""
    return request.app.state.config_service.get_public()


# ===== 操作対象 core (spheres レジストリ) =====
# core は同一 config.json を共有し、自機 MAC で spheres[] の自分のエントリを選ぶ。
# サーバーは選択された core にだけコマンド (MQTT) と映像 (UDP) を送る。

class ActiveSphereUpdate(BaseModel):
    id: str  # spheres[].id または "all" (全 core へブロードキャスト)


@router.get("/spheres")
async def get_spheres(request: Request):
    """登録されている core の一覧と現在の操作対象を返す。"""
    cs = request.app.state.config_service
    return {"spheres": cs.get_spheres(), "active": cs.get_active_sphere()}


@router.put("/spheres/active")
async def set_active_sphere(request: Request, body: ActiveSphereUpdate):
    """操作対象 core を切り替える。

    コマンド (MQTT sphere/<id>/command/*) と映像 (UDP) の宛先を同時に切り替え、
    config.json の active_sphere に永続化する。
    """
    cs = request.app.state.config_service
    try:
        active = cs.set_active_sphere(body.id)
    except ValueError as e:
        raise HTTPException(status_code=404, detail=str(e))

    # 宛先はオンラインの core だけに絞る (到達不能な core を混ぜると、その IP の
    # ARP 未解決が送信キューを埋めて生存 core の映像まで壊す)。実処理は lifespan の
    # retarget_streamer が持ち、死活変化時と同じ経路を通す。
    request.app.state.retarget_streamer()
    await request.app.state.state_manager.set_target(active)
    return {
        "active": active,
        "targets": request.app.state.video_streamer.get_targets(),
        "requested": cs.get_target_ips(active),
    }


# ===== LED 物理レイアウト配信 (実機と同一の core/data CSV が単一ソース) =====

# CONFIG_SEARCH_PATHS と同じ流儀 (サーバーは server/ から起動される前提)
LED_LAYOUT_SEARCH_PATHS = [
    "../core/data/led_layouts-5strip.csv",
    "data/led_layouts-5strip.csv",
]


@router.get("/led-layout")
async def get_led_layout():
    """LED 物理レイアウトをフラット配列で返す。

    返り値: {count, strips, positions: [x0,y0,z0, x1,y1,z1, ...], strip: [s0, s1, ...]}
    positions はそのまま Float32Array に流し込める形式。
    """
    for path in LED_LAYOUT_SEARCH_PATHS:
        if not os.path.exists(path):
            continue
        positions, strip_ids = [], []
        with open(path, newline="") as f:
            for row in csv.DictReader(f):
                positions.extend((float(row["x"]), float(row["y"]), float(row["z"])))
                strip_ids.append(int(row["strip"]))
        return {
            "count": len(strip_ids),
            "strips": (max(strip_ids) + 1) if strip_ids else 0,
            "positions": positions,
            "strip": strip_ids,
        }
    raise HTTPException(status_code=404, detail="LED layout CSV not found")


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
