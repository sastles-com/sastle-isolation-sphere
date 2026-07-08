"""
Database クラス - SQLite データベース管理

Phase 1: データベース基盤
- SQLite接続管理
- テーブル作成
- CRUD操作 (videos, playlists, playlist_items, playback_state)
"""
import sqlite3
import logging
from typing import List, Dict, Any, Optional
from datetime import datetime
from pathlib import Path

logger = logging.getLogger(__name__)


class Database:
    """SQLite データベース管理クラス"""
    
    def __init__(self, db_path: str):
        """
        データベース初期化
        
        Args:
            db_path: SQLiteデータベースファイルパス
        """
        self.db_path = db_path
        self.conn = None
        self._connect()
        self._create_tables()
        self._initialize_playback_state()
        logger.info(f"Database initialized: {db_path}")
    
    def _connect(self):
        """データベース接続"""
        # Create directory if not exists
        Path(self.db_path).parent.mkdir(parents=True, exist_ok=True)
        
        self.conn = sqlite3.connect(self.db_path, check_same_thread=False)
        self.conn.row_factory = sqlite3.Row  # Dict-like access
        
        # Enable foreign keys
        self.conn.execute("PRAGMA foreign_keys = ON")
    
    def _create_tables(self):
        """テーブル作成"""
        cursor = self.conn.cursor()
        
        # videos テーブル
        cursor.execute("""
            CREATE TABLE IF NOT EXISTS videos (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                uuid TEXT UNIQUE NOT NULL,
                title TEXT NOT NULL,
                description TEXT,
                filename TEXT NOT NULL,
                converted_path TEXT NOT NULL,
                thumbnail_path TEXT,
                duration_ms INTEGER NOT NULL,
                width INTEGER NOT NULL,
                height INTEGER NOT NULL,
                fps INTEGER DEFAULT 30,
                size_bytes INTEGER,
                codec TEXT,
                kind TEXT NOT NULL DEFAULT 'video',
                uploaded_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
                updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
            )
        """)
        
        # playlists テーブル
        cursor.execute("""
            CREATE TABLE IF NOT EXISTS playlists (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                uuid TEXT UNIQUE NOT NULL,
                name TEXT NOT NULL,
                description TEXT,
                loop BOOLEAN DEFAULT 0,
                shuffle BOOLEAN DEFAULT 0,
                created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
                updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
            )
        """)
        
        # playlist_items テーブル
        cursor.execute("""
            CREATE TABLE IF NOT EXISTS playlist_items (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                playlist_id INTEGER NOT NULL,
                video_id INTEGER NOT NULL,
                position INTEGER NOT NULL,
                FOREIGN KEY (playlist_id) REFERENCES playlists(id) ON DELETE CASCADE,
                FOREIGN KEY (video_id) REFERENCES videos(id) ON DELETE CASCADE,
                UNIQUE(playlist_id, position)
            )
        """)
        
        # playback_state テーブル (シングルトン)
        cursor.execute("""
            CREATE TABLE IF NOT EXISTS playback_state (
                id INTEGER PRIMARY KEY DEFAULT 1,
                current_playlist_id INTEGER,
                current_video_id INTEGER,
                position_ms INTEGER DEFAULT 0,
                status TEXT DEFAULT 'stopped',
                updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
                FOREIGN KEY (current_playlist_id) REFERENCES playlists(id),
                FOREIGN KEY (current_video_id) REFERENCES videos(id),
                CHECK (id = 1)
            )
        """)
        
        # インデックス作成
        cursor.execute("CREATE INDEX IF NOT EXISTS idx_videos_uuid ON videos(uuid)")
        cursor.execute("CREATE INDEX IF NOT EXISTS idx_playlists_uuid ON playlists(uuid)")
        cursor.execute("CREATE INDEX IF NOT EXISTS idx_playlist_items_position ON playlist_items(playlist_id, position)")
        cursor.execute("CREATE INDEX IF NOT EXISTS idx_videos_uploaded_at ON videos(uploaded_at DESC)")
        
        self.conn.commit()
        self._migrate_schema()

    def _migrate_schema(self):
        """既存DBへの後方互換マイグレーション (欠けている列を追加する)。"""
        cursor = self.conn.cursor()
        cursor.execute("PRAGMA table_info(videos)")
        video_cols = {row[1] for row in cursor.fetchall()}
        if "kind" not in video_cols:
            # 既存の全動画は素材('video')として扱う
            cursor.execute("ALTER TABLE videos ADD COLUMN kind TEXT NOT NULL DEFAULT 'video'")
            logger.info("migrated: added videos.kind (default 'video')")
        self.conn.commit()

    def _initialize_playback_state(self):
        """playback_state初期化 (シングルトンレコード作成)"""
        cursor = self.conn.cursor()
        cursor.execute("SELECT id FROM playback_state WHERE id = 1")
        
        if cursor.fetchone() is None:
            cursor.execute("""
                INSERT INTO playback_state (id, status, position_ms)
                VALUES (1, 'stopped', 0)
            """)
            self.conn.commit()
    
    def _get_tables(self) -> List[str]:
        """テーブル一覧取得 (テスト用)"""
        cursor = self.conn.cursor()
        cursor.execute("SELECT name FROM sqlite_master WHERE type='table'")
        return [row[0] for row in cursor.fetchall()]
    
    def close(self):
        """データベース接続クローズ"""
        if self.conn:
            self.conn.close()
            logger.info("Database connection closed")
    
    # ==================== Video CRUD ====================
    
    def create_video(
        self,
        uuid: str,
        title: str,
        filename: str,
        converted_path: str,
        duration_ms: int,
        width: int,
        height: int,
        description: Optional[str] = None,
        thumbnail_path: Optional[str] = None,
        fps: int = 30,
        size_bytes: Optional[int] = None,
        codec: Optional[str] = None,
        kind: str = "video"
    ) -> Dict[str, Any]:
        """
        動画作成

        Args:
            kind: 種別 ('video' 素材動画 | 'pattern' パターン動画)

        Returns:
            作成された動画レコード (dict)
        """
        cursor = self.conn.cursor()
        cursor.execute("""
            INSERT INTO videos (
                uuid, title, description, filename, converted_path, thumbnail_path,
                duration_ms, width, height, fps, size_bytes, codec, kind
            ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        """, (
            uuid, title, description, filename, converted_path, thumbnail_path,
            duration_ms, width, height, fps, size_bytes, codec, kind
        ))
        self.conn.commit()
        
        return self.get_video(cursor.lastrowid)
    
    def get_video(self, video_id: int) -> Optional[Dict[str, Any]]:
        """動画取得 (ID指定)"""
        cursor = self.conn.cursor()
        cursor.execute("SELECT * FROM videos WHERE id = ?", (video_id,))
        row = cursor.fetchone()
        
        return dict(row) if row else None
    
    def get_videos(
        self,
        limit: int = 100,
        offset: int = 0,
        search: Optional[str] = None,
        kind: Optional[str] = None
    ) -> List[Dict[str, Any]]:
        """
        動画一覧取得

        Args:
            limit: 取得件数
            offset: オフセット
            search: タイトル検索 (LIKE検索)
            kind: 種別フィルタ ('video' | 'pattern'。None なら全件)
        """
        cursor = self.conn.cursor()

        where = []
        params: list = []
        if search:
            where.append("title LIKE ?")
            params.append(f"%{search}%")
        if kind:
            where.append("kind = ?")
            params.append(kind)

        sql = "SELECT * FROM videos"
        if where:
            sql += " WHERE " + " AND ".join(where)
        sql += " ORDER BY uploaded_at DESC LIMIT ? OFFSET ?"
        params.extend([limit, offset])

        cursor.execute(sql, params)
        return [dict(row) for row in cursor.fetchall()]
    
    def delete_video(self, video_id: int):
        """動画削除"""
        cursor = self.conn.cursor()
        cursor.execute("DELETE FROM videos WHERE id = ?", (video_id,))
        self.conn.commit()
    
    # ==================== Playlist CRUD ====================
    
    def create_playlist(
        self,
        uuid: str,
        name: str,
        description: Optional[str] = None,
        loop: bool = False,
        shuffle: bool = False
    ) -> Dict[str, Any]:
        """プレイリスト作成"""
        cursor = self.conn.cursor()
        cursor.execute("""
            INSERT INTO playlists (uuid, name, description, loop, shuffle)
            VALUES (?, ?, ?, ?, ?)
        """, (uuid, name, description, int(loop), int(shuffle)))
        self.conn.commit()
        
        return self.get_playlist(cursor.lastrowid)
    
    def get_playlist(self, playlist_id: int) -> Optional[Dict[str, Any]]:
        """プレイリスト取得"""
        cursor = self.conn.cursor()
        cursor.execute("SELECT * FROM playlists WHERE id = ?", (playlist_id,))
        row = cursor.fetchone()
        
        return dict(row) if row else None
    
    def get_playlists(self) -> List[Dict[str, Any]]:
        """プレイリスト一覧取得"""
        cursor = self.conn.cursor()
        cursor.execute("SELECT * FROM playlists ORDER BY created_at DESC")
        return [dict(row) for row in cursor.fetchall()]
    
    def update_playlist(
        self,
        playlist_id: int,
        name: Optional[str] = None,
        description: Optional[str] = None,
        loop: Optional[bool] = None,
        shuffle: Optional[bool] = None
    ):
        """プレイリスト更新"""
        updates = []
        params = []
        
        if name is not None:
            updates.append("name = ?")
            params.append(name)
        if description is not None:
            updates.append("description = ?")
            params.append(description)
        if loop is not None:
            updates.append("loop = ?")
            params.append(int(loop))
        if shuffle is not None:
            updates.append("shuffle = ?")
            params.append(int(shuffle))
        
        if not updates:
            return
        
        updates.append("updated_at = CURRENT_TIMESTAMP")
        params.append(playlist_id)
        
        cursor = self.conn.cursor()
        sql = f"UPDATE playlists SET {', '.join(updates)} WHERE id = ?"
        cursor.execute(sql, params)
        self.conn.commit()
    
    def delete_playlist(self, playlist_id: int):
        """プレイリスト削除 (CASCADE: playlist_itemsも自動削除)"""
        cursor = self.conn.cursor()
        cursor.execute("DELETE FROM playlists WHERE id = ?", (playlist_id,))
        self.conn.commit()
    
    # ==================== Playlist Items ====================
    
    def add_playlist_item(
        self,
        playlist_id: int,
        video_id: int,
        position: int
    ):
        """プレイリストアイテム追加"""
        cursor = self.conn.cursor()
        cursor.execute("""
            INSERT INTO playlist_items (playlist_id, video_id, position)
            VALUES (?, ?, ?)
        """, (playlist_id, video_id, position))
        self.conn.commit()
    
    def get_playlist_items(self, playlist_id: int) -> List[Dict[str, Any]]:
        """プレイリストアイテム一覧取得 (position順)"""
        cursor = self.conn.cursor()
        cursor.execute("""
            SELECT * FROM playlist_items
            WHERE playlist_id = ?
            ORDER BY position
        """, (playlist_id,))
        return [dict(row) for row in cursor.fetchall()]
    
    def delete_playlist_items(self, playlist_id: int):
        """プレイリストアイテム全削除"""
        cursor = self.conn.cursor()
        cursor.execute("DELETE FROM playlist_items WHERE playlist_id = ?", (playlist_id,))
        self.conn.commit()

    def get_playlist_items_detailed(self, playlist_id: int) -> List[Dict[str, Any]]:
        """アイテムを動画情報込みで position 順に取得 (item_id / position + videos.*)。"""
        cursor = self.conn.cursor()
        cursor.execute("""
            SELECT pi.id AS item_id, pi.position, v.*
            FROM playlist_items pi
            JOIN videos v ON v.id = pi.video_id
            WHERE pi.playlist_id = ?
            ORDER BY pi.position
        """, (playlist_id,))
        return [dict(row) for row in cursor.fetchall()]

    def set_playlist_items(self, playlist_id: int, video_ids: List[int]):
        """アイテムを指定順で総入れ替え (追加/削除/並び替えの共通処理)。position は 0..n。"""
        cursor = self.conn.cursor()
        cursor.execute("DELETE FROM playlist_items WHERE playlist_id = ?", (playlist_id,))
        cursor.executemany(
            "INSERT INTO playlist_items (playlist_id, video_id, position) VALUES (?, ?, ?)",
            [(playlist_id, vid, i) for i, vid in enumerate(video_ids)],
        )
        self.conn.commit()
    
    # ==================== Playback State ====================
    
    def get_playback_state(self) -> Dict[str, Any]:
        """再生状態取得 (シングルトン)"""
        cursor = self.conn.cursor()
        cursor.execute("SELECT * FROM playback_state WHERE id = 1")
        row = cursor.fetchone()
        return dict(row) if row else None
    
    def update_playback_state(
        self,
        status: Optional[str] = None,
        current_playlist_id: Optional[int] = None,
        current_video_id: Optional[int] = None,
        position_ms: Optional[int] = None
    ):
        """再生状態更新"""
        updates = []
        params = []
        
        if status is not None:
            updates.append("status = ?")
            params.append(status)
        if current_playlist_id is not None:
            updates.append("current_playlist_id = ?")
            params.append(current_playlist_id)
        if current_video_id is not None:
            updates.append("current_video_id = ?")
            params.append(current_video_id)
        if position_ms is not None:
            updates.append("position_ms = ?")
            params.append(position_ms)
        
        if not updates:
            return
        
        updates.append("updated_at = CURRENT_TIMESTAMP")

        cursor = self.conn.cursor()
        sql = f"UPDATE playback_state SET {', '.join(updates)} WHERE id = 1"
        cursor.execute(sql, params)
        self.conn.commit()

    def clear_playback_state(self):
        """再生状態を停止・参照クリア (動画削除時に FK 参照を外すため)。"""
        cursor = self.conn.cursor()
        cursor.execute(
            "UPDATE playback_state SET status = 'stopped', "
            "current_video_id = NULL, current_playlist_id = NULL, "
            "updated_at = CURRENT_TIMESTAMP WHERE id = 1"
        )
        self.conn.commit()
