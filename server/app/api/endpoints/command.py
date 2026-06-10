from fastapi import APIRouter, Request, HTTPException
from pydantic import BaseModel
from typing import Optional, Dict, Any
import logging
import json

from app.core.config import MQTT_COMMAND_TOPIC_PREFIX

logger = logging.getLogger(__name__)

router = APIRouter()

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
    mode: str  # "sphere" | "pixels" | "off"
    pixels: Optional[list] = None

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
            json.dumps(payload),
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
            json.dumps(payload),
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
            json.dumps(payload),
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
