> **English** · [日本語](state.ja.md)

# State Diagrams

## Overall System State Transitions

```mermaid
stateDiagram-v2
    [*] --> Initializing: Power On / Reset
    
    Initializing --> InitializingM5: M5.begin()
    InitializingM5 --> InitializingFS: FileManager.begin()
    InitializingFS --> LoadingConfig: ConfigManager.loadConfig()
    LoadingConfig --> ConnectingWiFi: NetworkManager.begin()
    ConnectingWiFi --> ConnectingMQTT: MQTTManager.begin()
    ConnectingMQTT --> InitializingIMU: IMUManager.begin()
    InitializingIMU --> InitializingGesture: GestureManager.begin()
    InitializingGesture --> Running: Setup Complete
    
    Running --> Running: Main Loop
    Running --> Error: Critical Failure
    Running --> [*]: ESP.restart()
    
    Error --> Initializing: Auto Recovery
    Error --> [*]: Manual Reset
    
    note right of Initializing
        Serial output:
        "=== M5Atom S3R Starting ==="
    end note
    
    note right of Running
        Normal operation:
        - MQTT loop
        - IMU update
        - Gesture detection
        - UDP receive
    end note
```

## WiFi Connection State

```mermaid
stateDiagram-v2
    [*] --> Disconnected
    
    Disconnected --> Connecting: begin() / connectWiFi()
    
    Connecting --> ConfiguringIP: WiFi.begin(ssid, password)
    ConfiguringIP --> Waiting: WiFi.config(static_ip, ...)
    
    Waiting --> Connected: WiFi.status() == WL_CONNECTED
    Waiting --> Timeout: 10 seconds elapsed
    Waiting --> Waiting: Retry (delay 500ms)
    
    Timeout --> Disconnected: Connection failed
    
    Connected --> Disconnected: WiFi.status() != WL_CONNECTED
    Connected --> Connected: Normal operation
    
    note right of Connected
        IP: 192.168.49.101
        SSID: ESP32-P2P-Direct
        RSSI: Monitor signal
    end note
    
    note left of Timeout
        Max retries: 20
        Retry delay: 500ms
        Total: 10 seconds
    end note
```

## MQTT Connection State

```mermaid
stateDiagram-v2
    [*] --> Disconnected
    
    Disconnected --> Connecting: begin() / connect()
    Disconnected --> WaitingReconnect: Connection lost
    
    Connecting --> Subscribing: CONNACK received
    Subscribing --> Publishing: SUBSCRIBE command topic
    Publishing --> Connected: PUBLISH status "online"
    
    Connecting --> WaitingReconnect: Connection failed
    
    WaitingReconnect --> Connecting: 5 seconds elapsed
    WaitingReconnect --> WaitingReconnect: Timer not expired
    
    Connected --> ProcessingMessage: Message received
    ProcessingMessage --> Connected: Callback executed
    
    Connected --> Disconnected: Connection lost
    Connected --> Connected: loop() / Keep alive
    
    note right of Connected
        Subscribed: sphere/sphere001/command
        Publishing: 
        - sphere/sphere001/imu (10Hz)
        - sphere/sphere001/status
        - sphere/sphere001/response
    end note
    
    note left of WaitingReconnect
        Reconnect interval: 5s
        Auto-reconnect enabled
    end note
```

## IMU State Transitions

```mermaid
stateDiagram-v2
    [*] --> Uninitialized
    
    Uninitialized --> Initializing: begin(config, sda, scl)
    
    Initializing --> I2CSetup: Wire.begin(SDA, SCL, 400kHz)
    I2CSetup --> DetectingSensor: _bno.begin()
    
    DetectingSensor --> ConfiguringMode: Sensor detected
    DetectingSensor --> InitFailed: Sensor not found
    
    ConfiguringMode --> Calibrating: setMode(OPERATION_MODE_NDOF)
    
    Calibrating --> Ready: Sensor initialized
    
    Ready --> Updating: update() called
    Updating --> Ready: Data read complete
    
    Ready --> Publishing: getQuaternion() / getEuler() / etc.
    Publishing --> Ready: Data returned
    
    InitFailed --> Uninitialized: Retry / Reset
    
    note right of Calibrating
        Calibration status:
        - System: 0-3
        - Gyro: 0-3
        - Accel: 0-3
        - Mag: 0-3
        (0=uncalibrated, 3=fully)
    end note
    
    note right of Ready
        Update interval: 10ms (100Hz)
        Data available:
        - Quaternion (w,x,y,z)
        - Euler (heading,roll,pitch)
        - Acceleration (x,y,z)
        - Gyroscope (x,y,z)
    end note
```

## Gesture Detection State (planned)

```mermaid
stateDiagram-v2
    [*] --> Normal
    
    state Normal {
        [*] --> Monitoring
        Monitoring --> ShakeDetected: Accel > threshold
        ShakeDetected --> Monitoring: Record timestamp
        Monitoring --> Monitoring: Update accel data
    }
    
    Normal --> UIActive: Triple shake (3 shakes in 2s)
    
    state UIActive {
        [*] --> Idle
        Idle --> DetectingRotation: Rotation detected
        
        state DetectingRotation {
            [*] --> CheckingRoll
            CheckingRoll --> RollPositive: Roll > +45°
            CheckingRoll --> RollNegative: Roll < -45°
            CheckingRoll --> CheckingPitch: |Roll| < 45°
            
            CheckingPitch --> PitchPositive: Pitch > +45°
            CheckingPitch --> PitchNegative: Pitch < -45°
            CheckingPitch --> CheckingHeading: |Pitch| < 45°
            
            CheckingHeading --> HeadingPositive: Heading > +45°
            CheckingHeading --> HeadingNegative: Heading < -45°
            CheckingHeading --> [*]: |Heading| < 45°
            
            RollPositive --> [*]: Next image
            RollNegative --> [*]: Prev image
            PitchPositive --> [*]: Brightness up
            PitchNegative --> [*]: Brightness down
            HeadingPositive --> [*]: Next mode
            HeadingNegative --> [*]: Prev mode
        }
        
        DetectingRotation --> Selecting: Action identified
        
        state Selecting {
            [*] --> Holding
            Holding --> Confirmed: Return to neutral OR hold 1s
            Confirmed --> [*]: Execute action
        }
        
        Selecting --> Idle: Action executed
    }
    
    UIActive --> Normal: Timeout (10s)
    UIActive --> Normal: Action executed
    
    note right of Normal
        Monitoring accelerometer
        Shake detection active
        Mode: NORMAL
    end note
    
    note right of UIActive
        Rotation detection active
        Timeout: 10 seconds
        Mode: UI_ACTIVE / UI_SELECTING
        
        Feedback (pending):
        - LED indication
        - Audio beep
    end note
```

## UDP Reception State

```mermaid
stateDiagram-v2
    [*] --> Uninitialized
    
    Uninitialized --> Listening: beginUDP(port)
    
    Listening --> PacketAvailable: parsePacket() > 0
    Listening --> Listening: No packet
    
    PacketAvailable --> Reading: read(buffer, len)
    Reading --> Processing: Data in buffer
    
    Processing --> Listening: Process complete
    
    Listening --> Uninitialized: WiFi disconnected
    
    note right of Listening
        Port: 8889
        Receive-only mode
        No UDP sending
    end note
    
    note right of Processing
        Log received data:
        - Remote IP
        - Remote port
        - Payload
    end note
```

## File System State

```mermaid
stateDiagram-v2
    [*] --> Unmounted
    
    Unmounted --> Mounting: begin()
    
    Mounting --> CheckingSpace: LittleFS.begin(true)
    CheckingSpace --> Ready: Format if needed
    
    Ready --> Reading: readFile(path)
    Reading --> Ready: Return file content
    
    Ready --> Writing: writeFile(path, data)
    Writing --> Ready: Write complete
    
    Ready --> Listing: ls(path, levels)
    Listing --> Ready: Directory listed
    
    Ready --> Unmounted: LittleFS.end()
    
    Mounting --> MountFailed: Mount error
    MountFailed --> Unmounted: Error logged
    
    note right of Ready
        Total: 3.00 MB
        Used: 1.17 MB (39.1%)
        Free: 1.83 MB
        
        Files:
        - /config.json
        - /led_layouts-5strip.csv
        - /images/*.jpg (151 files)
    end note
```

## Command Processing State

```mermaid
stateDiagram-v2
    [*] --> Idle
    
    Idle --> Receiving: MQTT message on command topic
    
    Receiving --> Parsing: mqttCallback() invoked
    
    Parsing --> ExecutingStatus: Command == "status"
    Parsing --> ExecutingRestart: Command == "restart"
    Parsing --> ExecutingSetBrightness: Command starts with "set_brightness:"
    Parsing --> ExecutingShowImage: Command starts with "show_image:"
    Parsing --> ExecutingSetMode: Command starts with "set_mode:"
    Parsing --> Unknown: Unrecognized command
    
    ExecutingStatus --> BuildingResponse: Collect system info
    BuildingResponse --> Publishing: Format JSON
    
    ExecutingRestart --> Restarting: ESP.restart()
    Restarting --> [*]
    
    ExecutingSetBrightness --> Publishing: Parse value & set (pending)
    ExecutingShowImage --> Publishing: Load & display image (pending)
    ExecutingSetMode --> Publishing: Change display mode (pending)
    
    Unknown --> PublishingError: Log error
    PublishingError --> Idle: Error response sent
    
    Publishing --> Idle: Response published
    
    note right of ExecutingStatus
        Response includes:
        - Device name
        - Uptime
        - Free heap
        - IMU calibration
        - WiFi RSSI
    end note
    
    note left of ExecutingSetBrightness
        Pending: LEDManager
        Brightness: 0-255
    end note
    
    note left of ExecutingShowImage
        Pending: ImageManager
        Path: /images/.../*.jpg
    end note
```

## Overall Mode State (Integrated View)

```mermaid
stateDiagram-v2
    [*] --> Boot
    
    state Boot {
        [*] --> HardwareInit
        HardwareInit --> FilesystemInit
        FilesystemInit --> ConfigLoad
        ConfigLoad --> NetworkInit
        NetworkInit --> SensorInit
        SensorInit --> [*]
    }
    
    Boot --> Operational: All systems ready
    
    state Operational {
        [*] --> NormalMode
        
        state NormalMode {
            [*] --> MonitoringIMU
            MonitoringIMU --> PublishingIMU: 100ms timer
            PublishingIMU --> MonitoringIMU: Published
            
            MonitoringIMU --> CheckingUDP: Packet available
            CheckingUDP --> MonitoringIMU: Processed
            
            MonitoringIMU --> ProcessingCommand: MQTT command
            ProcessingCommand --> MonitoringIMU: Response sent
        }
        
        NormalMode --> GestureMode: Triple shake detected
        
        state GestureMode {
            [*] --> UIMenu
            UIMenu --> RotationDetection
            RotationDetection --> ActionExecution: Selection confirmed
            ActionExecution --> [*]
        }
        
        GestureMode --> NormalMode: Action executed OR timeout
    }
    
    Operational --> Recovery: Critical error
    Recovery --> Boot: Reset & restart
    
    note right of NormalMode
        - IMU publishing at 10Hz
        - MQTT keep-alive
        - UDP monitoring
        - Gesture shake detection
    end note
    
    note right of GestureMode
        - UI active (10s timeout)
        - Rotation detection
        - Action selection
        - Visual/audio feedback (pending)
    end note
```
