from pydantic_settings import BaseSettings
from functools import lru_cache

# MQTT 接続設定(従来のハードコード値を集約。値は変更しない)
MQTT_BROKER_PORT = 1883
MQTT_DEVICE_ID = "sphere001"
MQTT_CLIENT_ID = "isolation-server"

# MQTT トピック
MQTT_STATE_TOPIC = "sphere/all/state"
MQTT_COMMAND_TOPIC_PREFIX = "sphere/all/command"
MQTT_COMMAND_TOPIC_WILDCARD = "sphere/all/command/#"

# 操作対象 core を選んだときの宛先。ファーム側は sphere/<id>/command/# を購読する
# (core/src/MQTTManager.cpp)。"all" を選んだ場合は MQTT_COMMAND_TOPIC_PREFIX を使う。
MQTT_DEVICE_COMMAND_PREFIX_FMT = "sphere/{device_id}/command"
DEVICE_TARGET_ALL = "all"

# ===== core の死活判定 =====
# core は IMU を約8.5Hz で publish し続けるため、これが途切れたことを
# オフラインの判定に使う。タイムアウトは取りこぼし数発を許容する値にする
# (MQTT の再接続やブローカーの一時的な詰まりで点滅させない)。
DEVICE_OFFLINE_TIMEOUT_SEC = 3.0
# online 集合の変化を監視する周期。変化が無ければ配信しないので実質無負荷。
DEVICE_PRESENCE_SWEEP_SEC = 1.0

# サーバーが購読するワイルドカード。"sphere/+/command/#" は sphere/all/... も含むため、
# MQTT_COMMAND_TOPIC_WILDCARD と併用しない (同一クライアントで購読が重複すると
# ブローカーがマッチした購読ごとに配信し、コマンドが二重処理される)。
MQTT_ANY_COMMAND_TOPIC_WILDCARD = "sphere/+/command/#"
MQTT_ANY_STATUS_TOPIC_WILDCARD = "sphere/+/status"
MQTT_ANY_LOG_TOPIC_WILDCARD = "sphere/+/log"

# 時刻同期ビーコン (複数コアの共通タイムベース。設計: core/doc/time_sync_show.md)
# 1秒周期・QoS0・非retain でブロードキャストする。
MQTT_CLOCK_TOPIC = "sphere/all/clock"
MQTT_CLOCK_INTERVAL_SEC = 1.0

# プレイリスト/動画システムの保存先 (server/ 起動前提の相対パス)
DB_PATH = "data/sphere.db"            # SQLite データベース
MEDIA_VIDEOS_DIR = "data/videos"      # アップロード動画の保存先
MEDIA_THUMBNAILS_DIR = "data/thumbnails"  # 動画サムネイルの保存先

# MQTT ブローカー設定 (config.json) の探索パス。
# サーバーは server/ ディレクトリから起動される前提の相対パス(順序も既存実装と同一)
CONFIG_SEARCH_PATHS = [
    "../core/data/config.json",
    "data/config.json",
    "../data/config.json",
]

# CORS 許可オリジン
# NOTE: "*" は開発用の全許可。本番では具体的なオリジンに絞るべきだが、
# 削除するとレスポンスヘッダーが変わるため現状維持している。
CORS_ORIGINS = ["http://localhost:5173", "http://localhost:3000", "*"]

class Settings(BaseSettings):
    PROJECT_NAME: str = "Isolation Sphere Server"
    VERSION: str = "0.1.0"

    class Config:
        env_file = ".env"

@lru_cache()
def get_settings():
    return Settings()
