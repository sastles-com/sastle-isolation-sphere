import json
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
            print(f"[WebSocket] Received from client: {data}")
            
            # Handle different message types
            msg_type = data.get("type")
            payload = data.get("payload", {})
            
            if msg_type == "COMMAND":
                # Forward command to MQTT via StateManager
                command_type = payload.get("command")
                params = payload.get("params", {})
                
                if command_type == "params":
                    # Publish to MQTT command topic
                    if state_manager._mqtt_client:
                        state_manager._mqtt_client.publish(
                            f"sphere/all/command/{command_type}",
                            json.dumps(params)
                        )
                        print(f"[WebSocket] Published MQTT command: sphere/all/command/{command_type} = {params}")
                    else:
                        print("[WebSocket] ERROR: MQTT client not available")
            
            # Also forward to ROS bridge
            await ros_bridge.handle_frontend_message(data)
    except WebSocketDisconnect:
        state_manager.remove_observer(websocket)
