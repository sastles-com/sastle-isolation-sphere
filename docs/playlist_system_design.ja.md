> [English](playlist_system_design.md) · **日本語**

# プレイリストシステム設計書

最終更新: 2025-12-02

## 1. 概要

PLAYLIST MODE機能の完全実装。動画アップロード、変換、データベース管理、プレイリスト編集、MQTT配信、UDP再生デーモンを含む。

## 2. システム全体像

```
┌─────────────────────────────────────────────────────────────┐
│                    PLAYLIST SYSTEM                           │
└─────────────────────────────────────────────────────────────┘
                               │
         ┌─────────────────────┼─────────────────────┐
         │                     │                     │
         ▼                     ▼                     ▼
  ┌─────────────┐      ┌──────────────┐      ┌──────────────┐
  │  Web UI     │      │   Backend    │      │ Video Daemon │
  │  (React)    │──────│  (FastAPI)   │──────│  (Python)    │
  └─────────────┘      └──────────────┘      └──────────────┘
         │                     │                     │
         │                     │                     │
         │              ┌──────┴──────┐              │
         │              │             │              │
         ▼              ▼             ▼              ▼
  ┌──────────┐   ┌──────────┐  ┌─────────┐   ┌──────────┐
  │WebSocket │   │Database  │  │  MQTT   │   │   UDP    │
  │(UI Sync) │   │(SQLite)  │  │(Control)│   │(Stream)  │
  └──────────┘   └──────────┘  └─────────┘   └──────────┘
                                      │              │
                                      └──────┬───────┘
                                             ▼
                                      ┌──────────┐
                                      │  ESP32   │
                                      │ (Sphere) │
                                      └──────────┘
```

## 3. 機能要件

### 3.1 動画管理 (Video Management)

#### アップロード機能
- **入力**: MP4, MOV, AVI, WebM (最大100MB)
- **処理**:
  1. ファイル検証 (形式、サイズ、コーデック)
  2. メタデータ抽出 (duration, resolution, codec)
  3. 動画変換 (FFmpeg)
     - 解像度: 320x160 (config.json準拠)
     - フォーマット: H.264/RGB565 frames
     - FPS: 30fps (設定可能)
  4. サムネイル生成 (first frame)
  5. データベース登録

#### データベーススキーマ (SQLite)

```sql
-- videos テーブル
CREATE TABLE videos (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    uuid TEXT UNIQUE NOT NULL,           -- v_{timestamp}_{random}
    title TEXT NOT NULL,
    description TEXT,
    filename TEXT NOT NULL,              -- original filename
    converted_path TEXT NOT NULL,        -- /data/videos/{uuid}/converted.rgb565
    thumbnail_path TEXT,                 -- /data/videos/{uuid}/thumb.jpg
    duration_ms INTEGER NOT NULL,
    width INTEGER NOT NULL,
    height INTEGER NOT NULL,
    fps INTEGER DEFAULT 30,
    size_bytes INTEGER,
    codec TEXT,
    uploaded_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- playlists テーブル
CREATE TABLE playlists (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    uuid TEXT UNIQUE NOT NULL,           -- p_{timestamp}_{random}
    name TEXT NOT NULL,
    description TEXT,
    loop BOOLEAN DEFAULT 0,              -- ループ再生
    shuffle BOOLEAN DEFAULT 0,           -- ランダム再生
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- playlist_items テーブル (多対多リレーション)
CREATE TABLE playlist_items (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    playlist_id INTEGER NOT NULL,
    video_id INTEGER NOT NULL,
    position INTEGER NOT NULL,           -- 再生順序
    FOREIGN KEY (playlist_id) REFERENCES playlists(id) ON DELETE CASCADE,
    FOREIGN KEY (video_id) REFERENCES videos(id) ON DELETE CASCADE,
    UNIQUE(playlist_id, position)
);

-- playback_state テーブル (現在の再生状態)
CREATE TABLE playback_state (
    id INTEGER PRIMARY KEY DEFAULT 1,
    current_playlist_id INTEGER,
    current_video_id INTEGER,
    position_ms INTEGER DEFAULT 0,
    status TEXT DEFAULT 'stopped',       -- playing/paused/stopped
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (current_playlist_id) REFERENCES playlists(id),
    FOREIGN KEY (current_video_id) REFERENCES videos(id),
    CHECK (id = 1)                       -- シングルトン
);
```

### 3.2 プレイリスト編集機能

#### UI Components
1. **PlaylistEditor** - プレイリスト編集画面
   - ドラッグ&ドロップで順序変更
   - 動画追加/削除
   - ループ/シャッフル設定

2. **VideoManager** - 動画ライブラリ
   - アップロードボタン
   - 動画一覧 (サムネイル、タイトル、メタデータ)
   - プレビュー機能
   - 削除機能

3. **PlaylistManager** - プレイリスト一覧
   - プレイリスト作成/削除
   - 再生/一時停止/停止
   - 編集ボタン

### 3.3 動画再生デーモン

#### Video Daemon (`server/video/daemon.py`)

**責務**:
- プレイリストの再生管理
- 動画フレームの読み込み
- UDPストリーミング (ESP32へ)
- MQTT経由での制御受信

**構成**:
```python
class VideoDaemon:
    def __init__(self):
        self.db = Database("data/sphere.db")
        self.mqtt_client = MQTTClient()
        self.udp_socket = UDPSocket(port=8889)
        self.current_playlist = None
        self.current_video = None
        self.frame_index = 0
        self.status = "stopped"
    
    async def run(self):
        """メインループ"""
        while True:
            if self.status == "playing":
                await self._send_frame()
            await asyncio.sleep(1/30)  # 30fps
    
    async def _send_frame(self):
        """1フレーム送信"""
        frame = self._read_frame(self.current_video, self.frame_index)
        self.udp_socket.send(frame, ("192.168.49.101", 8889))
        self.frame_index += 1
    
    async def _handle_mqtt_command(self, topic, payload):
        """MQTT制御コマンド処理"""
        # sphere/all/command/playback を購読
        pass
```

**フレームフォーマット (UDP)**:
```
┌─────────────┬─────────────┬──────────────┐
│ Header (8B) │ Frame Data  │ Checksum (4B)│
└─────────────┴─────────────┴──────────────┘

Header:
- magic: 0xDEADBEEF (4B)
- frame_index: uint32 (4B)

Frame Data:
- RGB565 pixels: 320x160x2 = 102,400 bytes
- または JPEG compressed (実装による)

Checksum:
- CRC32 (4B)
```

## 4. クラス設計

### 4.1 Backend (FastAPI)

```python
# app/services/video_service.py
class VideoService:
    def __init__(self, db: Database, config: Config):
        self.db = db
        self.config = config
        self.upload_dir = Path("data/videos")
    
    async def upload_video(self, file: UploadFile) -> Video:
        """動画アップロード & 変換"""
        # 1. ファイル保存
        video_uuid = self._generate_uuid()
        video_dir = self.upload_dir / video_uuid
        video_dir.mkdir(parents=True, exist_ok=True)
        
        # 2. メタデータ抽出
        metadata = await self._extract_metadata(file)
        
        # 3. 動画変換 (FFmpeg)
        converted_path = await self._convert_video(
            file, 
            video_dir,
            width=self.config.image.width,
            height=self.config.image.height
        )
        
        # 4. サムネイル生成
        thumb_path = await self._generate_thumbnail(converted_path)
        
        # 5. DB登録
        video = self.db.create_video(
            uuid=video_uuid,
            title=file.filename,
            converted_path=str(converted_path),
            thumbnail_path=str(thumb_path),
            **metadata
        )
        
        return video
    
    async def _convert_video(self, input_path, output_dir, width, height):
        """FFmpegで動画変換"""
        output_path = output_dir / "converted.rgb565"
        
        # FFmpeg コマンド
        cmd = [
            "ffmpeg", "-i", str(input_path),
            "-vf", f"scale={width}:{height}",
            "-pix_fmt", "rgb565le",
            "-f", "rawvideo",
            str(output_path)
        ]
        
        await asyncio.create_subprocess_exec(*cmd)
        return output_path

# app/services/playlist_service.py
class PlaylistService:
    def __init__(self, db: Database):
        self.db = db
    
    def create_playlist(self, name: str, video_ids: List[int]) -> Playlist:
        """プレイリスト作成"""
        playlist = self.db.create_playlist(name=name)
        
        for position, video_id in enumerate(video_ids):
            self.db.add_playlist_item(
                playlist_id=playlist.id,
                video_id=video_id,
                position=position
            )
        
        return playlist
    
    def update_playlist_order(self, playlist_id: int, video_ids: List[int]):
        """プレイリスト順序更新"""
        self.db.delete_playlist_items(playlist_id)
        
        for position, video_id in enumerate(video_ids):
            self.db.add_playlist_item(playlist_id, video_id, position)

# app/db/database.py
class Database:
    def __init__(self, db_path: str):
        self.db_path = db_path
        self._init_db()
    
    def _init_db(self):
        """DB初期化 & マイグレーション"""
        conn = sqlite3.connect(self.db_path)
        # CREATE TABLE文実行
        conn.close()
    
    def create_video(self, **kwargs) -> Video:
        """動画レコード作成"""
        pass
    
    def get_videos(self, limit=100, offset=0) -> List[Video]:
        """動画一覧取得"""
        pass
    
    def create_playlist(self, name: str) -> Playlist:
        """プレイリスト作成"""
        pass
    
    def get_playlists(self) -> List[Playlist]:
        """プレイリスト一覧"""
        pass
```

### 4.2 Frontend (React)

```javascript
// VideoUploader.jsx
export const VideoUploader = ({ onUploadComplete }) => {
    const [uploading, setUploading] = useState(false);
    const [progress, setProgress] = useState(0);
    
    const handleUpload = async (file) => {
        setUploading(true);
        
        const formData = new FormData();
        formData.append('file', file);
        
        const response = await fetch('/api/playlist/videos/upload', {
            method: 'POST',
            body: formData,
            onUploadProgress: (e) => {
                setProgress(Math.round((e.loaded * 100) / e.total));
            }
        });
        
        const video = await response.json();
        onUploadComplete(video);
        setUploading(false);
    };
    
    return <DropZone onDrop={handleUpload} />;
};

// PlaylistEditor.jsx
export const PlaylistEditor = ({ playlistId }) => {
    const [items, setItems] = useState([]);
    
    const handleReorder = (startIndex, endIndex) => {
        const newItems = Array.from(items);
        const [removed] = newItems.splice(startIndex, 1);
        newItems.splice(endIndex, 0, removed);
        
        setItems(newItems);
        
        // Save to backend
        fetch(`/api/playlist/playlists/${playlistId}/items`, {
            method: 'PUT',
            body: JSON.stringify({
                video_ids: newItems.map(i => i.video_id)
            })
        });
    };
    
    return (
        <DragDropContext onDragEnd={handleReorder}>
            <Droppable droppableId="playlist">
                {(provided) => (
                    <div ref={provided.innerRef}>
                        {items.map((item, index) => (
                            <Draggable key={item.id} draggableId={item.id} index={index}>
                                <VideoItem item={item} />
                            </Draggable>
                        ))}
                    </div>
                )}
            </Droppable>
        </DragDropContext>
    );
};
```

## 5. タスク分解

### Phase 1: データベース基盤 (2-3日)
- [ ] SQLiteスキーマ作成
- [ ] Database クラス実装
- [ ] マイグレーション機能
- [ ] ユニットテスト

### Phase 2: 動画アップロード & 変換 (3-4日)
- [ ] VideoService実装
- [ ] FFmpeg統合
- [ ] アップロードAPI (`POST /api/playlist/videos/upload`)
- [ ] メタデータ抽出
- [ ] サムネイル生成
- [ ] ユニットテスト

### Phase 3: プレイリスト管理API (2-3日)
- [ ] PlaylistService実装
- [ ] CRUD API実装
  - `GET /api/playlist/playlists`
  - `POST /api/playlist/playlists`
  - `PUT /api/playlist/playlists/{id}`
  - `DELETE /api/playlist/playlists/{id}`
  - `PUT /api/playlist/playlists/{id}/items` (順序更新)
- [ ] ユニットテスト

### Phase 4: Frontend UI実装 (4-5日)
- [ ] VideoUploader コンポーネント
  - ドラッグ&ドロップ
  - プログレスバー
  - エラーハンドリング
- [ ] VideoManager 拡張
  - 実際のAPIと接続
  - サムネイル表示
  - プレビュー機能
- [ ] PlaylistEditor 新規作成
  - ドラッグ&ドロップ順序変更 (react-beautiful-dnd)
  - 動画追加/削除
  - ループ/シャッフル設定
- [ ] PlaylistManager 拡張
  - 実際のAPIと接続
  - 編集画面遷移

### Phase 5: Video Daemon実装 (3-4日)
- [ ] VideoDaemon基本構造
- [ ] MQTT購読 (`sphere/all/command/playback`)
- [ ] フレーム読み込み
- [ ] UDPストリーミング
- [ ] プレイリスト管理
- [ ] ループ/シャッフル機能
- [ ] systemdサービス化

### Phase 6: StateManager統合 (2日)
- [ ] playback状態をDBと同期
- [ ] プレイリスト選択をMQTT配信
- [ ] Video Daemon連携

### Phase 7: テスト & デバッグ (2-3日)
- [ ] 統合テスト
- [ ] E2Eテスト (UI → Backend → Daemon → ESP32)
- [ ] パフォーマンステスト
- [ ] ドキュメント作成

## 6. API仕様

### 6.1 動画管理

```
POST /api/playlist/videos/upload
Content-Type: multipart/form-data

Request:
- file: video file (max 100MB)
- title: (optional) custom title

Response:
{
  "id": 123,
  "uuid": "v_1701234567_abc123",
  "title": "My Video",
  "duration_ms": 125000,
  "width": 320,
  "height": 160,
  "fps": 30,
  "thumbnail_url": "/data/videos/v_1701234567_abc123/thumb.jpg"
}

GET /api/playlist/videos
Response:
[
  { "id": 1, "title": "...", ... },
  ...
]

DELETE /api/playlist/videos/{id}
Response: 204 No Content
```

### 6.2 プレイリスト管理

```
POST /api/playlist/playlists
{
  "name": "Morning Routine",
  "loop": false,
  "shuffle": false,
  "video_ids": [1, 3, 5]
}

Response:
{
  "id": 1,
  "uuid": "p_1701234567_xyz789",
  "name": "Morning Routine",
  "video_count": 3,
  "total_duration_ms": 450000
}

PUT /api/playlist/playlists/{id}/items
{
  "video_ids": [3, 1, 5]  // 新しい順序
}

Response: 200 OK
```

## 7. MQTT連携

### トピック

```
# プレイリスト選択
sphere/all/command/playback
{
  "action": "select_playlist",
  "playlist_id": "p_1701234567_xyz789"
}

# 再生コントロール (既存)
sphere/all/command/playback
{
  "action": "play" | "pause" | "stop" | "toggle"
}

# 状態配信 (Video Daemon → StateManager)
sphere/all/state/playback
{
  "status": "playing",
  "playlist_id": "p_1701234567_xyz789",
  "current_video_id": "v_1701234567_abc123",
  "position_ms": 45000,
  "duration_ms": 125000,
  "frame_index": 1350
}
```

## 8. ファイル構成

```
server/
├── app/
│   ├── db/
│   │   ├── __init__.py
│   │   ├── database.py          # Database クラス
│   │   └── models.py            # SQLAlchemy Models (optional)
│   ├── services/
│   │   ├── video_service.py     # VideoService
│   │   ├── playlist_service.py  # PlaylistService
│   │   └── state_manager.py     # 既存 (拡張)
│   └── api/endpoints/
│       └── playlist.py          # 拡張
├── video/
│   ├── __init__.py
│   ├── daemon.py                # VideoDaemon
│   ├── frame_reader.py          # フレーム読み込み
│   └── udp_sender.py            # UDP送信
├── data/
│   ├── sphere.db                # SQLite DB
│   └── videos/                  # 動画ストレージ
│       ├── v_1701234567_abc123/
│       │   ├── converted.rgb565
│       │   └── thumb.jpg
│       └── ...
└── frontend/src/components/playlist/
    ├── VideoUploader.jsx        # 新規
    ├── PlaylistEditor.jsx       # 新規
    ├── VideoManager.jsx         # 拡張
    └── PlaylistManager.jsx      # 拡張
```

## 9. 技術スタック

### Backend
- **Database**: SQLite3 (簡易性、ファイルベース)
- **ORM**: 生SQL or SQLAlchemy (検討)
- **Video Processing**: FFmpeg (Python wrapper: `ffmpeg-python`)
- **Async**: asyncio, aiofiles

### Frontend
- **Drag & Drop**: `react-beautiful-dnd`
- **File Upload**: `react-dropzone`
- **Progress**: MUI LinearProgress

### Video Daemon
- **Frame Reading**: NumPy, OpenCV (cv2)
- **UDP**: asyncio sockets
- **MQTT**: paho-mqtt

## 10. パフォーマンス考慮

### 動画変換
- バックグラウンドタスク (Celery or asyncio.create_task)
- 進捗通知 (WebSocket)
- キャンセル機能

### フレームストリーミング
- 30fps = 33.3ms/frame
- UDP MTU: 1500 bytes → 分割送信必要 (102KB frame)
- 圧縮検討: JPEG (品質80%) → 約10-20KB/frame

### データベース
- インデックス: videos.uuid, playlists.uuid
- position順のソート最適化

## 11. セキュリティ

- **ファイルアップロード**: 
  - MIME type検証
  - ファイルサイズ制限 (100MB)
  - ウイルススキャン (optional: ClamAV)
- **パス traversal 防止**: uuid使用
- **SQL injection**: パラメータバインド

## 12. まとめ

総推定工数: **18-25日**

優先実装順:
1. Database基盤 (Phase 1)
2. 動画アップロード (Phase 2)
3. Frontend UI (Phase 4) - 並行可能
4. プレイリストAPI (Phase 3)
5. Video Daemon (Phase 5)
6. 統合 (Phase 6, 7)
