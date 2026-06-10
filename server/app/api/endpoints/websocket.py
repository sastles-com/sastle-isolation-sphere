import logging

from fastapi import APIRouter, WebSocket, WebSocketDisconnect
from app.services.state_manager import StateManager

logger = logging.getLogger(__name__)

router = APIRouter()
state_manager = StateManager()

@router.websocket("/ws")
async def websocket_endpoint(websocket: WebSocket):
    await websocket.accept()
    state_manager.add_observer(websocket)
    
    # Send initial state
    await websocket.send_json({
        "type": "STATE_UPDATE", 
        "payload": state_manager.get_state()
    })
    
    try:
        while True:
            data = await websocket.receive_json()
            logger.info(f"Received from client: {data}")
            
            # StateManagerに処理を委譲
            await state_manager.handle_websocket_message(data)
            
    except WebSocketDisconnect:
        state_manager.remove_observer(websocket)
