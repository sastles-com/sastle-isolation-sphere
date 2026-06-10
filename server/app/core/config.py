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
