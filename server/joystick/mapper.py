class InputMapper:
    def __init__(self):
        # Default mapping for generic gamepad (Xbox/PS style)
        self.BTN_MAP = {
            304: "BTN_A",      # A / Cross
            305: "BTN_B",      # B / Circle
            307: "BTN_X",      # X / Square
            308: "BTN_Y",      # Y / Triangle
            310: "BTN_LB",     # L1
            311: "BTN_RB",     # R1
            314: "BTN_SELECT", # Select / Share
            315: "BTN_START",  # Start / Options
        }
        
        self.ABS_MAP = {
            0: "ABS_X",  # Left Stick X
            1: "ABS_Y",  # Left Stick Y
            3: "ABS_RX", # Right Stick X
            4: "ABS_RY", # Right Stick Y
            16: "ABS_HAT0X", # D-Pad X
            17: "ABS_HAT0Y", # D-Pad Y
        }

    def map_event(self, event):
        """
        Maps a raw event to a logical command.
        Returns (command_type, payload) or None.
        """
        # EV_KEY (Buttons)
        if event.type == 1: 
            btn_name = self.BTN_MAP.get(event.code)
            if btn_name:
                if event.value == 1: # Press
                    if btn_name == "BTN_A":
                        return "SET_PLAYBACK", {"isPlaying": True}
                    elif btn_name == "BTN_B":
                        return "SET_PLAYBACK", {"isPlaying": False}
                    elif btn_name == "BTN_START":
                        return "SYSTEM_RESET", {}
                return None

        # EV_ABS (Sticks)
        elif event.type == 3:
            axis_name = self.ABS_MAP.get(event.code)
            if axis_name:
                # Normalize value (assuming 16-bit signed or similar, depends on device)
                # For now, just passing raw or simple scaling. 
                # Ideally, we need calibration data.
                
                # Example: Left Stick -> IMU Offset
                if axis_name == "ABS_Y":
                    # Invert Y for intuitive pitch control
                    return "SET_OFFSET", {"pitch_delta": -event.value / 32768.0} 
                elif axis_name == "ABS_X":
                    return "SET_OFFSET", {"roll_delta": event.value / 32768.0}
                    
                # Example: Right Stick -> Params
                elif axis_name == "ABS_RY":
                    return "SET_PARAMS", {"brightness_delta": -event.value / 32768.0}
                    
        return None
