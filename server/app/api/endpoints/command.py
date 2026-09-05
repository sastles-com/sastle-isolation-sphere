from fastapi import APIRouter, Request, HTTPException
from pydantic import BaseModel
from typing import Optional, Dict, Any
import logging
import json

# 宛先 core は StateManager が保持する操作対象に従う (command_prefix)。
# WebUI のセレクタで切り替えると、この経路のコマンドも同じ core に向く。

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
    mode: Optional[str] = None  # "sphere" | "pixels" | "off" | "test" (axis単独切替時は省略可)
    pixels: Optional[list] = None
    axis: Optional[bool] = None  # XYZ軸インジケータ ON/OFF
    pattern: Optional[str] = None  # test モード時のパターン: "strip_id" | "chase"
    width: Optional[int] = None  # test:chase の連続点灯LED数
    imu_comp: Optional[bool] = None  # IMU補正 ON/OFF (mode とは独立)
    imu_aux: Optional[bool] = None  # IMU補助データ(gyro/euler/accel)のI2C読み ON/OFF (診断用)
    imu_i2c_khz: Optional[int] = None  # IMU の I2C クロック [kHz] 10〜400 (配線切り分け用)
    imu_wordread: Optional[bool] = None  # quat を 2B×4 回に分割読み (多バイト転送不良の個体用)

class SystemCommand(BaseModel):
    action: str  # "restart" (ファーム CommandHandler::_handleSystem が処理)
    device: Optional[str] = None  # 宛先 core id。省略時は現在の操作対象


@router.post("/command/system")
async def send_system_command(command: SystemCommand, request: Request):
    """
    system コマンドを送信 (再起動など)。

    例: {"action": "restart", "device": "sphere002"}
    core 側は 1 秒後に ESP.restart() する。loop タスクが生きている場合のみ効く
    (完全ハング時は電源再投入が必要)。
    """
    if command.action not in ("restart",):
        raise HTTPException(status_code=400, detail=f"unsupported action: {command.action}")
    try:
        mqtt_service = request.app.state.mqtt_service
        payload = {"action": command.action}
        prefix = request.app.state.state_manager.command_prefix(command.device)
        mqtt_service.client.publish(f"{prefix}/system", _mqtt_json(payload), qos=1)
        logger.info(f"System command sent to {prefix}: {payload}")
        return {"status": "ok", "command": payload, "topic": f"{prefix}/system"}
    except HTTPException:
        raise
    except Exception as e:
        logger.error(f"Failed to send system command: {e}")
        raise HTTPException(status_code=500, detail=str(e))


@router.post("/command/playback")
async def send_playback_command(command: PlaybackCommand, request: Request):
    """
    Playback制御コマンドを送信
    
    例: {"action": "play", "playlist": "morning", "track": "demo01"}
    """
    try:
        mqtt_service = request.app.state.mqtt_service
        payload = command.model_dump(exclude_none=True)

        prefix = request.app.state.state_manager.command_prefix()
        mqtt_service.client.publish(
            f"{prefix}/playback",
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

        prefix = request.app.state.state_manager.command_prefix()
        mqtt_service.client.publish(
            f"{prefix}/params",
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

        prefix = request.app.state.state_manager.command_prefix()
        mqtt_service.client.publish(
            f"{prefix}/led",
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
