from fastapi import APIRouter, WebSocket, WebSocketDisconnect
from app.services.state_manager import StateManager
from app.services.ros_bridge import ROSBridge

router = APIRouter()
state_manager = StateManager()
# Initialize bridge lazily or at startup
ros_bridge = None 

@router.websocket("/ws")
async def websocket_endpoint(websocket: WebSocket):
    global ros_bridge
    if ros_bridge is None:
        ros_bridge = ROSBridge()

    await websocket.accept()
    state_manager.add_observer(websocket)
    
    # Send initial state
    await websocket.send_json({"type": "STATE_UPDATE", "payload": state_manager.get_state()})
    
    try:
        while True:
            data = await websocket.receive_json()
            await ros_bridge.handle_frontend_message(data)
    except WebSocketDisconnect:
        state_manager.remove_observer(websocket)
