# プレイリストシステム - アーキテクチャ詳細

最終更新: 2025-12-02

## 1. データフロー全体像

```
┌─────────────────────────────────────────────────────────────────┐
│                    User Interaction Layer                        │
└─────────────────────────────────────────────────────────────────┘
                               │
                               ▼
┌─────────────────────────────────────────────────────────────────┐
│  Web UI (React)                                                  │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐          │
│  │VideoUploader │  │PlaylistEditor│  │PlaylistPlayer│          │
│  └──────┬───────┘  └──────┬───────┘  └──────┬───────┘          │
└─────────┼──────────────────┼──────────────────┼─────────────────┘
          │                  │                  │
          │ POST /upload     │ PUT /playlists   │ WebSocket
          │                  │                  │ (playback control)
          ▼                  ▼                  ▼
┌─────────────────────────────────────────────────────────────────┐
│  FastAPI Backend                                                 │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐          │
│  │VideoService  │  │PlaylistSrv   │  │StateManager  │          │
│  └──────┬───────┘  └──────┬───────┘  └──────┬───────┘          │
└─────────┼──────────────────┼──────────────────┼─────────────────┘
          │                  │                  │
          │ FFmpeg           │ CRUD             │ MQTT Publish
          │ conversion       │ operations       │ (state/command)
          ▼                  ▼                  ▼
┌─────────────────┐  ┌──────────────┐  ┌──────────────┐
│ File Storage    │  │ SQLite DB    │  │ MQTT Broker  │
│ /data/videos/   │  │ sphere.db    │  │ (mosquitto)  │
└─────────────────┘  └──────────────┘  └──────┬───────┘
                                               │
                                               │ Subscribe
                                               │ sphere/all/command/*
                                               ▼
                              ┌───────────────────────────┐
                              │   Video Daemon            │
                              │  ┌────────────────────┐   │
                              │  │ PlaybackController │   │
                              │  └─────────┬──────────┘   │
                              │            │              │
                              │  ┌─────────▼──────────┐   │
                              │  │   FrameReader      │   │
                              │  └─────────┬──────────┘   │
                              │            │              │
                              │  ┌─────────▼──────────┐   │
                              │  │   UDPStreamer      │   │
                              │  └─────────┬──────────┘   │
                              └────────────┼──────────────┘
                                           │ UDP:8889
                                           ▼
                              ┌───────────────────────────┐
                              │   ESP32 (M5Atom S3R)      │
                              │  ┌────────────────────┐   │
                              │  │  ImageManager      │   │
                              │  └─────────┬──────────┘   │
                              │            │              │
                              │  ┌─────────▼──────────┐   │
                              │  │  LEDManager        │   │
                              │  │  (800 LEDs)        │   │
                              │  └────────────────────┘   │
                              └───────────────────────────┘
```

## 2. コンポーネント詳細

### 2.1 VideoService

**責務**: 動画アップロード、変換、メタデータ管理

```python
class VideoService:
    """動画管理サービス"""
    
    def __init__(self, db: Database, config: Config):
        self.db = db
        self.config = config
        self.upload_dir = Path("data/videos")
        self.ffmpeg = FFmpegWrapper()
    
    async def upload_video(
        self, 
        file: UploadFile,
        title: Optional[str] = None
    ) -> Video:
        """
        動画アップロード & 変換
        
        Steps:
        1. Validation (format, size, codec)
        2. Save to temp directory
        3. Extract metadata (ffprobe)
        4. Convert to RGB565 format (FFmpeg)
        5. Generate thumbnail (first frame)
        6. Save to database
        7. Clean up temp files
        
        Returns:
            Video: 作成された動画オブジェクト
        
        Raises:
            VideoValidationError: 無効な動画形式
            VideoConversionError: 変換失敗
        """
        # Implementation
        pass
    
    async def _validate_video(self, file: UploadFile) -> bool:
        """動画ファイル検証"""
        # MIME type check
        if file.content_type not in ALLOWED_VIDEO_TYPES:
            raise VideoValidationError(f"Unsupported format: {file.content_type}")
        
        # Size check
        if file.size > MAX_VIDEO_SIZE:
            raise VideoValidationError(f"File too large: {file.size} bytes")
        
        return True
    
    async def _extract_metadata(self, video_path: Path) -> Dict[str, Any]:
        """FFprobeでメタデータ抽出"""
        probe = ffmpeg.probe(str(video_path))
        
        video_stream = next(
            s for s in probe['streams'] if s['codec_type'] == 'video'
        )
        
        return {
            'duration_ms': int(float(probe['format']['duration']) * 1000),
            'width': video_stream['width'],
            'height': video_stream['height'],
            'codec': video_stream['codec_name'],
            'fps': eval(video_stream['r_frame_rate'])  # "30/1" -> 30
        }
    
    async def _convert_video(
        self,
        input_path: Path,
        output_dir: Path,
        width: int = 320,
        height: int = 160,
        fps: int = 30
    ) -> Path:
        """
        FFmpegで動画変換
        
        Output format: RGB565 raw frames
        Each frame: width * height * 2 bytes
        Total file size: frames * width * height * 2
        """
        output_path = output_dir / "converted.rgb565"
        
        # FFmpeg command
        stream = ffmpeg.input(str(input_path))
        stream = ffmpeg.filter(stream, 'scale', width, height)
        stream = ffmpeg.output(
            stream,
            str(output_path),
            format='rawvideo',
            pix_fmt='rgb565le',
            r=fps
        )
        
        # Run with progress callback
        await self._run_ffmpeg(stream, progress_callback=self._on_progress)
        
        return output_path
    
    async def _generate_thumbnail(
        self,
        video_path: Path,
        output_path: Path,
        timestamp: float = 0.0
    ) -> Path:
        """サムネイル生成 (指定時刻のフレーム)"""
        stream = ffmpeg.input(str(video_path), ss=timestamp)
        stream = ffmpeg.filter(stream, 'scale', 160, 90)
        stream = ffmpeg.output(stream, str(output_path), vframes=1)
        
        await self._run_ffmpeg(stream)
        return output_path
    
    async def delete_video(self, video_id: int):
        """動画削除 (ファイル + DB)"""
        video = self.db.get_video(video_id)
        
        # Delete files
        video_dir = Path(video.converted_path).parent
        shutil.rmtree(video_dir, ignore_errors=True)
        
        # Delete from DB
        self.db.delete_video(video_id)
    
    def get_videos(
        self,
        limit: int = 100,
        offset: int = 0,
        search: Optional[str] = None
    ) -> List[Video]:
        """動画一覧取得 (ページネーション対応)"""
        return self.db.get_videos(limit, offset, search)
```

### 2.2 PlaylistService

**責務**: プレイリストCRUD、順序管理

```python
class PlaylistService:
    """プレイリスト管理サービス"""
    
    def __init__(self, db: Database):
        self.db = db
    
    def create_playlist(
        self,
        name: str,
        description: Optional[str] = None,
        video_ids: List[int] = [],
        loop: bool = False,
        shuffle: bool = False
    ) -> Playlist:
        """プレイリスト作成"""
        playlist = self.db.create_playlist(
            name=name,
            description=description,
            loop=loop,
            shuffle=shuffle
        )
        
        # Add videos
        for position, video_id in enumerate(video_ids):
            self.db.add_playlist_item(
                playlist_id=playlist.id,
                video_id=video_id,
                position=position
            )
        
        return self._enrich_playlist(playlist)
    
    def get_playlists(self) -> List[Playlist]:
        """プレイリスト一覧 (メタデータ付加)"""
        playlists = self.db.get_playlists()
        return [self._enrich_playlist(p) for p in playlists]
    
    def _enrich_playlist(self, playlist: Playlist) -> Playlist:
        """
        プレイリストにメタデータ追加
        - video_count
        - total_duration_ms
        - videos (詳細)
        """
        items = self.db.get_playlist_items(playlist.id)
        videos = [self.db.get_video(item.video_id) for item in items]
        
        playlist.video_count = len(videos)
        playlist.total_duration_ms = sum(v.duration_ms for v in videos)
        playlist.videos = videos
        
        return playlist
    
    def update_playlist_items(
        self,
        playlist_id: int,
        video_ids: List[int]
    ):
        """プレイリスト順序更新 (ドラッグ&ドロップ対応)"""
        # Delete all items
        self.db.delete_playlist_items(playlist_id)
        
        # Re-add with new order
        for position, video_id in enumerate(video_ids):
            self.db.add_playlist_item(playlist_id, video_id, position)
    
    def toggle_loop(self, playlist_id: int) -> bool:
        """ループON/OFF切り替え"""
        playlist = self.db.get_playlist(playlist_id)
        new_loop = not playlist.loop
        self.db.update_playlist(playlist_id, loop=new_loop)
        return new_loop
    
    def toggle_shuffle(self, playlist_id: int) -> bool:
        """シャッフルON/OFF切り替え"""
        playlist = self.db.get_playlist(playlist_id)
        new_shuffle = not playlist.shuffle
        self.db.update_playlist(playlist_id, shuffle=new_shuffle)
        return new_shuffle
```

### 2.3 VideoDaemon

**責務**: プレイリスト再生、フレームストリーミング

```python
class VideoDaemon:
    """動画再生デーモン"""
    
    def __init__(self):
        self.db = Database("data/sphere.db")
        self.mqtt = MQTTClient()
        self.udp = UDPStreamer(port=8889)
        
        self.playback_state = PlaybackState()
        self.frame_reader = FrameReader()
        
        # Threading
        self.running = True
        self.playback_thread = None
    
    async def start(self):
        """デーモン起動"""
        logger.info("VideoDaemon starting...")
        
        # MQTT接続
        await self.mqtt.connect("192.168.49.1", 1883)
        self.mqtt.subscribe("sphere/all/command/playback", self._on_command)
        
        # Playback thread
        self.playback_thread = asyncio.create_task(self._playback_loop())
        
        logger.info("VideoDaemon started")
    
    async def _playback_loop(self):
        """メインプレイバックループ (30fps)"""
        while self.running:
            if self.playback_state.status == "playing":
                try:
                    await self._send_next_frame()
                except Exception as e:
                    logger.error(f"Frame send error: {e}")
            
            # 30fps = 33.3ms/frame
            await asyncio.sleep(1/30)
    
    async def _send_next_frame(self):
        """次のフレームをUDP送信"""
        state = self.playback_state
        
        # Read frame
        frame = self.frame_reader.read_frame(
            state.current_video_path,
            state.frame_index
        )
        
        if frame is None:
            # End of video
            await self._on_video_end()
            return
        
        # Send via UDP
        await self.udp.send_frame(
            frame,
            frame_index=state.frame_index,
            target=("192.168.49.101", 8889)
        )
        
        # Update state
        state.frame_index += 1
        state.position_ms = int(state.frame_index * 1000 / 30)  # 30fps
        
        # Publish state to MQTT
        await self._publish_state()
    
    async def _on_video_end(self):
        """動画終了時の処理"""
        state = self.playback_state
        playlist = self.db.get_playlist(state.current_playlist_id)
        
        # Get next video
        next_video = self._get_next_video(playlist, state.current_video_id)
        
        if next_video:
            await self._play_video(next_video)
        elif playlist.loop:
            # Loop playlist
            first_video = self._get_first_video(playlist)
            await self._play_video(first_video)
        else:
            # Stop playback
            state.status = "stopped"
            await self._publish_state()
    
    def _get_next_video(self, playlist: Playlist, current_video_id: int):
        """次の動画取得 (シャッフル対応)"""
        items = self.db.get_playlist_items(playlist.id)
        
        if playlist.shuffle:
            # Random next (excluding current)
            candidates = [i for i in items if i.video_id != current_video_id]
            return random.choice(candidates).video_id if candidates else None
        else:
            # Sequential next
            current_pos = next(i.position for i in items if i.video_id == current_video_id)
            next_item = next((i for i in items if i.position == current_pos + 1), None)
            return next_item.video_id if next_item else None
    
    async def _on_command(self, topic: str, payload: dict):
        """MQTTコマンド処理"""
        action = payload.get("action")
        
        if action == "select_playlist":
            playlist_id = payload.get("playlist_id")
            await self._load_playlist(playlist_id)
        
        elif action == "play":
            self.playback_state.status = "playing"
        
        elif action == "pause":
            self.playback_state.status = "paused"
        
        elif action == "stop":
            self.playback_state.status = "stopped"
            self.playback_state.frame_index = 0
        
        elif action == "toggle":
            if self.playback_state.status == "playing":
                self.playback_state.status = "paused"
            else:
                self.playback_state.status = "playing"
        
        await self._publish_state()
    
    async def _load_playlist(self, playlist_id: int):
        """プレイリスト読み込み & 最初の動画再生"""
        playlist = self.db.get_playlist(playlist_id)
        first_video = self._get_first_video(playlist)
        
        if first_video:
            self.playback_state.current_playlist_id = playlist_id
            await self._play_video(first_video)
    
    async def _play_video(self, video_id: int):
        """動画再生開始"""
        video = self.db.get_video(video_id)
        
        self.playback_state.current_video_id = video_id
        self.playback_state.current_video_path = video.converted_path
        self.playback_state.frame_index = 0
        self.playback_state.status = "playing"
        
        await self._publish_state()
    
    async def _publish_state(self):
        """再生状態をMQTT配信"""
        state = self.playback_state
        
        await self.mqtt.publish(
            "sphere/all/state/playback",
            {
                "status": state.status,
                "playlist_id": state.current_playlist_id,
                "video_id": state.current_video_id,
                "position_ms": state.position_ms,
                "frame_index": state.frame_index
            },
            retain=True
        )
```

### 2.4 FrameReader

**責務**: RGB565フレーム読み込み

```python
class FrameReader:
    """RGB565フレーム読み込み"""
    
    FRAME_WIDTH = 320
    FRAME_HEIGHT = 160
    BYTES_PER_PIXEL = 2  # RGB565
    FRAME_SIZE = FRAME_WIDTH * FRAME_HEIGHT * BYTES_PER_PIXEL  # 102,400 bytes
    
    def __init__(self):
        self.cache = {}  # video_path -> mmap object
    
    def read_frame(self, video_path: str, frame_index: int) -> Optional[bytes]:
        """指定フレーム読み込み"""
        # Memory-mapped file for efficient random access
        if video_path not in self.cache:
            self.cache[video_path] = self._open_mmap(video_path)
        
        mmap_file = self.cache[video_path]
        
        # Calculate offset
        offset = frame_index * self.FRAME_SIZE
        
        # Check bounds
        if offset + self.FRAME_SIZE > len(mmap_file):
            return None  # End of file
        
        # Read frame
        mmap_file.seek(offset)
        frame_data = mmap_file.read(self.FRAME_SIZE)
        
        return frame_data
    
    def _open_mmap(self, video_path: str):
        """Memory-mapped fileオープン"""
        import mmap
        
        f = open(video_path, 'rb')
        return mmap.mmap(f.fileno(), 0, access=mmap.ACCESS_READ)
    
    def close_all(self):
        """全キャッシュクローズ"""
        for mmap_file in self.cache.values():
            mmap_file.close()
        self.cache.clear()
```

### 2.5 UDPStreamer

**責務**: UDPフレーム送信

```python
class UDPStreamer:
    """UDPフレームストリーミング"""
    
    MAGIC = 0xDEADBEEF
    MTU = 1400  # Safe UDP payload size
    
    def __init__(self, port: int = 8889):
        self.socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.socket.setsockopt(socket.SOL_SOCKET, socket.SO_SNDBUF, 512 * 1024)
    
    async def send_frame(
        self,
        frame_data: bytes,
        frame_index: int,
        target: Tuple[str, int]
    ):
        """
        フレーム送信 (分割パケット対応)
        
        Packet format:
        ┌────────┬────────┬────────┬────────┬─────────┐
        │ Magic  │FrameID │PacketID│ Total  │  Data   │
        │  (4B)  │  (4B)  │  (2B)  │  (2B)  │ (<=MTU) │
        └────────┴────────┴────────┴────────┴─────────┘
        """
        chunks = self._split_frame(frame_data)
        total_packets = len(chunks)
        
        for packet_id, chunk in enumerate(chunks):
            packet = self._build_packet(
                frame_index,
                packet_id,
                total_packets,
                chunk
            )
            
            self.socket.sendto(packet, target)
            
            # Small delay to avoid packet loss
            await asyncio.sleep(0.001)  # 1ms
    
    def _split_frame(self, frame_data: bytes) -> List[bytes]:
        """フレームを分割 (MTUサイズ)"""
        data_per_packet = self.MTU - 12  # Header size
        chunks = []
        
        for i in range(0, len(frame_data), data_per_packet):
            chunks.append(frame_data[i:i+data_per_packet])
        
        return chunks
    
    def _build_packet(
        self,
        frame_index: int,
        packet_id: int,
        total_packets: int,
        data: bytes
    ) -> bytes:
        """パケット構築"""
        import struct
        
        header = struct.pack(
            '!IIHH',  # Big-endian: uint32, uint32, uint16, uint16
            self.MAGIC,
            frame_index,
            packet_id,
            total_packets
        )
        
        return header + data
```

## 3. データベース詳細設計

### ERD (Entity Relationship Diagram)

```
┌─────────────────┐
│     videos      │
├─────────────────┤
│ id (PK)         │
│ uuid            │◄────────┐
│ title           │         │
│ description     │         │
│ converted_path  │         │
│ thumbnail_path  │         │
│ duration_ms     │         │
│ width, height   │         │
│ fps             │         │
│ uploaded_at     │         │
└─────────────────┘         │
                            │
                            │ (N)
                            │
                ┌───────────┴─────────────┐
                │   playlist_items        │
                ├─────────────────────────┤
                │ id (PK)                 │
                │ playlist_id (FK)        │
                │ video_id (FK)           │
                │ position                │
                └───────────┬─────────────┘
                            │
                            │ (N)
                            │
┌─────────────────┐         │
│   playlists     │◄────────┘
├─────────────────┤
│ id (PK)         │
│ uuid            │
│ name            │
│ description     │
│ loop            │
│ shuffle         │
│ created_at      │
└─────────────────┘
        ▲
        │ (1)
        │
┌───────┴─────────────┐
│  playback_state     │
├─────────────────────┤
│ id (PK) = 1         │
│ current_playlist_id │
│ current_video_id    │
│ position_ms         │
│ status              │
└─────────────────────┘
```

### インデックス戦略

```sql
-- UUID検索の高速化
CREATE INDEX idx_videos_uuid ON videos(uuid);
CREATE INDEX idx_playlists_uuid ON playlists(uuid);

-- プレイリストアイテムの順序ソート
CREATE INDEX idx_playlist_items_position ON playlist_items(playlist_id, position);

-- 作成日時でのソート
CREATE INDEX idx_videos_uploaded_at ON videos(uploaded_at DESC);
CREATE INDEX idx_playlists_created_at ON playlists(created_at DESC);

-- タイトル検索 (LIKE検索用)
CREATE INDEX idx_videos_title ON videos(title);
```

## 4. エラーハンドリング

### 例外クラス階層

```python
class PlaylistError(Exception):
    """プレイリストシステム基底例外"""
    pass

class VideoError(PlaylistError):
    """動画関連エラー"""
    pass

class VideoValidationError(VideoError):
    """動画検証エラー (無効な形式、サイズ超過)"""
    pass

class VideoConversionError(VideoError):
    """動画変換エラー (FFmpeg失敗)"""
    pass

class VideoNotFoundError(VideoError):
    """動画が見つからない"""
    pass

class PlaylistNotFoundError(PlaylistError):
    """プレイリストが見つからない"""
    pass

class PlaybackError(PlaylistError):
    """再生エラー"""
    pass
```

### エラーレスポンス

```json
{
  "error": {
    "code": "VIDEO_VALIDATION_ERROR",
    "message": "Unsupported video format: video/x-matroska",
    "details": {
      "allowed_formats": ["video/mp4", "video/quicktime", "video/x-msvideo"],
      "max_size_mb": 100
    }
  }
}
```

## 5. パフォーマンス最適化

### 5.1 動画変換の並列化

```python
# TaskQueue (Celery風)
from concurrent.futures import ThreadPoolExecutor

class ConversionQueue:
    def __init__(self, max_workers=2):
        self.executor = ThreadPoolExecutor(max_workers=max_workers)
        self.tasks = {}  # task_id -> Future
    
    def submit(self, video_id: int, input_path: str) -> str:
        """変換タスク投入"""
        task_id = f"task_{uuid.uuid4().hex[:8]}"
        
        future = self.executor.submit(
            self._convert_worker,
            video_id,
            input_path
        )
        
        self.tasks[task_id] = future
        return task_id
    
    def _convert_worker(self, video_id, input_path):
        """変換ワーカー (別スレッド)"""
        # FFmpeg実行
        # 進捗をWebSocket経由で通知
        pass
```

### 5.2 フレーム読み込みキャッシュ

```python
# LRU Cache for recently read frames
from functools import lru_cache

class CachedFrameReader(FrameReader):
    @lru_cache(maxsize=90)  # 3秒分 (30fps)
    def read_frame(self, video_path: str, frame_index: int):
        return super().read_frame(video_path, frame_index)
```

### 5.3 UDP送信バッファリング

```python
# Burst send with pacing
async def send_frame_buffered(self, frames: List[bytes]):
    """複数フレームをバッファリング送信"""
    for i, frame in enumerate(frames):
        await self.send_frame(frame, i, self.target)
        
        # Adaptive pacing
        if i % 10 == 0:
            await asyncio.sleep(0.01)  # 10フレームごとに10ms休止
```

## 6. まとめ

このアーキテクチャにより：

✅ **スケーラビリティ**: データベース、ファイルストレージの分離
✅ **保守性**: 責務の明確な分離 (Service層、Daemon層)
✅ **拡張性**: 新しい動画形式、プレイリスト機能の追加が容易
✅ **パフォーマンス**: 非同期処理、キャッシング、最適化された送信
✅ **テスタビリティ**: 各コンポーネントの単体テストが可能

次のステップ: Phase 1 (データベース基盤) の実装開始
