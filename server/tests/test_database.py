"""
Database テスト (TDD)

Phase 1: データベース基盤
期待される動作を定義し、Databaseクラスの実装をテストで駆動する
"""
import pytest
import os
import tempfile
from pathlib import Path
import sys

# Add parent directory to path
sys.path.insert(0, str(Path(__file__).parent.parent))

from app.db.database import Database


class TestDatabaseInitialization:
    """データベース初期化テスト"""
    
    def test_database_creates_file(self):
        """DBファイルが作成される"""
        with tempfile.TemporaryDirectory() as tmpdir:
            db_path = os.path.join(tmpdir, "test.db")
            db = Database(db_path)
            
            assert os.path.exists(db_path)
            db.close()
    
    def test_database_creates_tables(self):
        """全テーブルが作成される"""
        with tempfile.TemporaryDirectory() as tmpdir:
            db_path = os.path.join(tmpdir, "test.db")
            db = Database(db_path)
            
            # Check tables exist
            tables = db._get_tables()
            assert "videos" in tables
            assert "playlists" in tables
            assert "playlist_items" in tables
            assert "playback_state" in tables
            
            db.close()
    
    def test_database_initializes_playback_state(self):
        """playback_state が初期化される"""
        with tempfile.TemporaryDirectory() as tmpdir:
            db_path = os.path.join(tmpdir, "test.db")
            db = Database(db_path)
            
            state = db.get_playback_state()
            assert state is not None
            assert state["id"] == 1
            assert state["status"] == "stopped"
            
            db.close()


class TestVideoCRUD:
    """Video CRUD操作テスト"""
    
    @pytest.fixture
    def db(self):
        """テスト用DBインスタンス"""
        with tempfile.TemporaryDirectory() as tmpdir:
            db_path = os.path.join(tmpdir, "test.db")
            database = Database(db_path)
            yield database
            database.close()
    
    def test_create_video(self, db):
        """動画作成"""
        video = db.create_video(
            uuid="v_test_001",
            title="Test Video",
            filename="test.mp4",
            converted_path="/data/videos/v_test_001/converted.rgb565",
            duration_ms=120000,
            width=320,
            height=160,
            fps=30
        )
        
        assert video["id"] is not None
        assert video["uuid"] == "v_test_001"
        assert video["title"] == "Test Video"
        assert video["duration_ms"] == 120000
    
    def test_get_video(self, db):
        """動画取得"""
        # Create
        created = db.create_video(
            uuid="v_test_002",
            title="Test Video 2",
            filename="test2.mp4",
            converted_path="/data/videos/v_test_002/converted.rgb565",
            duration_ms=60000,
            width=320,
            height=160
        )
        
        # Get
        video = db.get_video(created["id"])
        assert video is not None
        assert video["uuid"] == "v_test_002"
        assert video["title"] == "Test Video 2"
    
    def test_get_videos_list(self, db):
        """動画一覧取得"""
        # Create multiple videos
        db.create_video(
            uuid="v_test_003",
            title="Video 1",
            filename="v1.mp4",
            converted_path="/data/videos/v_test_003/converted.rgb565",
            duration_ms=30000,
            width=320,
            height=160
        )
        db.create_video(
            uuid="v_test_004",
            title="Video 2",
            filename="v2.mp4",
            converted_path="/data/videos/v_test_004/converted.rgb565",
            duration_ms=45000,
            width=320,
            height=160
        )
        
        # Get list
        videos = db.get_videos(limit=10, offset=0)
        assert len(videos) >= 2
        assert any(v["uuid"] == "v_test_003" for v in videos)
        assert any(v["uuid"] == "v_test_004" for v in videos)
    
    def test_delete_video(self, db):
        """動画削除"""
        # Create
        video = db.create_video(
            uuid="v_test_005",
            title="To Delete",
            filename="delete.mp4",
            converted_path="/data/videos/v_test_005/converted.rgb565",
            duration_ms=10000,
            width=320,
            height=160
        )
        
        # Delete
        db.delete_video(video["id"])
        
        # Verify deleted
        deleted = db.get_video(video["id"])
        assert deleted is None


class TestPlaylistCRUD:
    """Playlist CRUD操作テスト"""
    
    @pytest.fixture
    def db(self):
        """テスト用DBインスタンス"""
        with tempfile.TemporaryDirectory() as tmpdir:
            db_path = os.path.join(tmpdir, "test.db")
            database = Database(db_path)
            yield database
            database.close()
    
    def test_create_playlist(self, db):
        """プレイリスト作成"""
        playlist = db.create_playlist(
            uuid="p_test_001",
            name="Test Playlist",
            description="Test description",
            loop=False,
            shuffle=False
        )
        
        assert playlist["id"] is not None
        assert playlist["uuid"] == "p_test_001"
        assert playlist["name"] == "Test Playlist"
        assert playlist["loop"] == 0  # SQLite stores bool as 0/1
    
    def test_get_playlist(self, db):
        """プレイリスト取得"""
        created = db.create_playlist(
            uuid="p_test_002",
            name="Test Playlist 2",
            loop=True
        )
        
        playlist = db.get_playlist(created["id"])
        assert playlist is not None
        assert playlist["uuid"] == "p_test_002"
        assert playlist["loop"] == 1
    
    def test_update_playlist(self, db):
        """プレイリスト更新"""
        created = db.create_playlist(
            uuid="p_test_003",
            name="Original Name",
            loop=False
        )
        
        db.update_playlist(created["id"], name="Updated Name", loop=True)
        
        updated = db.get_playlist(created["id"])
        assert updated["name"] == "Updated Name"
        assert updated["loop"] == 1
    
    def test_delete_playlist(self, db):
        """プレイリスト削除"""
        playlist = db.create_playlist(
            uuid="p_test_004",
            name="To Delete"
        )
        
        db.delete_playlist(playlist["id"])
        
        deleted = db.get_playlist(playlist["id"])
        assert deleted is None


class TestPlaylistItems:
    """Playlist Items操作テスト"""
    
    @pytest.fixture
    def db_with_data(self):
        """動画とプレイリストを持つDBインスタンス"""
        with tempfile.TemporaryDirectory() as tmpdir:
            db_path = os.path.join(tmpdir, "test.db")
            database = Database(db_path)
            
            # Create videos
            v1 = database.create_video(
                uuid="v_item_001",
                title="Video 1",
                filename="v1.mp4",
                converted_path="/data/v1.rgb565",
                duration_ms=30000,
                width=320,
                height=160
            )
            v2 = database.create_video(
                uuid="v_item_002",
                title="Video 2",
                filename="v2.mp4",
                converted_path="/data/v2.rgb565",
                duration_ms=40000,
                width=320,
                height=160
            )
            
            # Create playlist
            p = database.create_playlist(
                uuid="p_item_001",
                name="Test Playlist"
            )
            
            yield database, v1, v2, p
            database.close()
    
    def test_add_playlist_item(self, db_with_data):
        """プレイリストアイテム追加"""
        db, v1, v2, p = db_with_data
        
        db.add_playlist_item(p["id"], v1["id"], position=0)
        db.add_playlist_item(p["id"], v2["id"], position=1)
        
        items = db.get_playlist_items(p["id"])
        assert len(items) == 2
        assert items[0]["video_id"] == v1["id"]
        assert items[1]["video_id"] == v2["id"]
    
    def test_get_playlist_items_ordered(self, db_with_data):
        """プレイリストアイテムが順序通り取得される"""
        db, v1, v2, p = db_with_data
        
        # Add in reverse order
        db.add_playlist_item(p["id"], v2["id"], position=0)
        db.add_playlist_item(p["id"], v1["id"], position=1)
        
        items = db.get_playlist_items(p["id"])
        assert items[0]["position"] == 0
        assert items[1]["position"] == 1
        assert items[0]["video_id"] == v2["id"]
        assert items[1]["video_id"] == v1["id"]
    
    def test_delete_playlist_items(self, db_with_data):
        """プレイリストアイテム全削除"""
        db, v1, v2, p = db_with_data
        
        db.add_playlist_item(p["id"], v1["id"], position=0)
        db.add_playlist_item(p["id"], v2["id"], position=1)
        
        db.delete_playlist_items(p["id"])
        
        items = db.get_playlist_items(p["id"])
        assert len(items) == 0
    
    def test_cascade_delete_playlist(self, db_with_data):
        """プレイリスト削除時にアイテムも削除される (CASCADE)"""
        db, v1, v2, p = db_with_data
        
        db.add_playlist_item(p["id"], v1["id"], position=0)
        db.delete_playlist(p["id"])
        
        # Items should be deleted automatically
        items = db.get_playlist_items(p["id"])
        assert len(items) == 0


class TestPlaybackState:
    """Playback State操作テスト"""
    
    @pytest.fixture
    def db(self):
        """テスト用DBインスタンス"""
        with tempfile.TemporaryDirectory() as tmpdir:
            db_path = os.path.join(tmpdir, "test.db")
            database = Database(db_path)
            yield database
            database.close()
    
    def test_get_initial_playback_state(self, db):
        """初期状態取得"""
        state = db.get_playback_state()
        
        assert state["id"] == 1
        assert state["status"] == "stopped"
        assert state["current_playlist_id"] is None
        assert state["current_video_id"] is None
        assert state["position_ms"] == 0
    
    def test_update_playback_state(self, db):
        """再生状態更新"""
        # Create test video and playlist first
        video = db.create_video(
            uuid="v_playback_001",
            title="Playback Test Video",
            filename="test.mp4",
            converted_path="/data/test.rgb565",
            duration_ms=60000,
            width=320,
            height=160
        )
        playlist = db.create_playlist(
            uuid="p_playback_001",
            name="Playback Test Playlist"
        )
        
        # Update with valid IDs
        db.update_playback_state(
            status="playing",
            current_playlist_id=playlist["id"],
            current_video_id=video["id"],
            position_ms=30000
        )
        
        state = db.get_playback_state()
        assert state["status"] == "playing"
        assert state["current_playlist_id"] == playlist["id"]
        assert state["current_video_id"] == video["id"]
        assert state["position_ms"] == 30000
    
    def test_playback_state_is_singleton(self, db):
        """playback_stateは常に1レコードのみ"""
        # Update multiple times
        db.update_playback_state(status="playing")
        db.update_playback_state(status="paused")
        
        # Should still be only one record
        state = db.get_playback_state()
        assert state["id"] == 1
        assert state["status"] == "paused"


if __name__ == "__main__":
    pytest.main([__file__, "-v"])
