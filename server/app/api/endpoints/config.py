from fastapi import APIRouter, HTTPException
from pydantic import BaseModel
from typing import Dict, Any

router = APIRouter()

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
