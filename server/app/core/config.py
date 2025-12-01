from pydantic_settings import BaseSettings
from functools import lru_cache

class Settings(BaseSettings):
    PROJECT_NAME: str = "Isolation Sphere Server"
    VERSION: str = "0.1.0"
    API_V1_STR: str = "/api/v1"
    
    # ROS2 Settings
    ROS_DOMAIN_ID: int = 0
    NODE_NAME: str = "isolation_server_node"
    GEMINI_API_KEY: str = "YOUR_API_KEY"

    class Config:
        env_file = ".env"

@lru_cache()
def get_settings():
    return Settings()
