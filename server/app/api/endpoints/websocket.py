import logging

from fastapi import APIRouter, WebSocket, WebSocketDisconnect

logger = logging.getLogger(__name__)

router = APIRouter()

@router.websocket("/ws")
async def websocket_endpoint(websocket: WebSocket):
    # lifespan で生成・登録された StateManager を参照する
    state_manager = websocket.app.state.state_manager
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
