from pydantic_settings import BaseSettings
from functools import lru_cache

class Settings(BaseSettings):
    PROJECT_NAME: str = "Isolation Sphere Server"
    VERSION: str = "0.1.0"

    class Config:
        env_file = ".env"

@lru_cache()
def get_settings():
    return Settings()
