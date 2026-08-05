from fastapi import APIRouter, Request, HTTPException
from pydantic import BaseModel
from typing import Optional, Dict, Any
import logging
import json

from app.core.config import MQTT_COMMAND_TOPIC_PREFIX

logger = logging.getLogger(__name__)

router = APIRouter()


def _mqtt_json(payload: dict) -> str:
    """デバイス向けコマンドを最小サイズで JSON 化する。

    ESP32 側の PubSubClient は送受信共用の固定バッファ (sastle::kMqttBufferSize)
    しか持たず、超過したメッセージを黙って破棄する。json.dumps の既定セパレータは
    区切りに空白を挟むため led コマンドの pixels 配列が約 18% 膨張し、
    50 要素で受信上限を超えていた。空白を落として上限まで使えるようにする。
    """
    return json.dumps(payload, separators=(",", ":"))

class PlaybackCommand(BaseModel):
    action: str  # "play" | "pause" | "stop" | "toggle"
    playlist: Optional[str] = None
    track: Optional[str] = None

class ParamsCommand(BaseModel):
    brightness: Optional[int] = None
    speed: Optional[int] = None
    hue: Optional[int] = None
    saturation: Optional[int] = None

class LEDCommand(BaseModel):
    mode: Optional[str] = None  # "sphere" | "pixels" | "off" (axis単独切替時は省略可)
    pixels: Optional[list] = None
    axis: Optional[bool] = None  # XYZ軸インジケータ ON/OFF

@router.post("/command/playback")
async def send_playback_command(command: PlaybackCommand, request: Request):
    """
    Playback制御コマンドを送信
    
    例: {"action": "play", "playlist": "morning", "track": "demo01"}
    """
    try:
        mqtt_service = request.app.state.mqtt_service
        payload = command.model_dump(exclude_none=True)

        mqtt_service.client.publish(
            f"{MQTT_COMMAND_TOPIC_PREFIX}/playback",
            _mqtt_json(payload),
            qos=1
        )
        
        logger.info(f"Playback command sent: {payload}")
        return {"status": "ok", "command": payload}
    except Exception as e:
        logger.error(f"Failed to send playback command: {e}")
        raise HTTPException(status_code=500, detail=str(e))

@router.post("/command/params")
async def send_params_command(command: ParamsCommand, request: Request):
    """
    パラメータ変更コマンドを送信
    
    例: {"brightness": 85}
    """
    try:
        mqtt_service = request.app.state.mqtt_service
        payload = command.model_dump(exclude_none=True)

        if not payload:
            raise HTTPException(status_code=400, detail="No parameters provided")

        mqtt_service.client.publish(
            f"{MQTT_COMMAND_TOPIC_PREFIX}/params",
            _mqtt_json(payload),
            qos=1
        )

        logger.info(f"Params command sent: {payload}")
        return {"status": "ok", "command": payload}
    except Exception as e:
        logger.error(f"Failed to send params command: {e}")
        raise HTTPException(status_code=500, detail=str(e))

@router.post("/command/led")
async def send_led_command(command: LEDCommand, request: Request):
    """
    LED制御コマンドを送信
    
    例: {"mode": "sphere"}
    """
    try:
        mqtt_service = request.app.state.mqtt_service
        payload = command.model_dump(exclude_none=True)

        mqtt_service.client.publish(
            f"{MQTT_COMMAND_TOPIC_PREFIX}/led",
            _mqtt_json(payload),
            qos=1
        )
        
        logger.info(f"LED command sent: {payload}")
        return {"status": "ok", "command": payload}
    except Exception as e:
        logger.error(f"Failed to send LED command: {e}")
        raise HTTPException(status_code=500, detail=str(e))

@router.get("/state")
async def get_current_state(request: Request):
    """現在の状態を取得"""
    state_manager = request.app.state.state_manager
    return state_manager.get_state()
