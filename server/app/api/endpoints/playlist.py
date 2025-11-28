from fastapi import APIRouter, HTTPException
from pydantic import BaseModel
from typing import List, Optional

router = APIRouter()

class Video(BaseModel):
    id: str
    title: str
    duration: str
    size: str

class Playlist(BaseModel):
    id: str
    name: str
    videos: List[str]

# Mock Data
VIDEOS = [
  { "id": "v1", "title": "Isolation Theme", "duration": "3:45", "size": "12MB" },
  { "id": "v2", "title": "Ambient Rain", "duration": "10:00", "size": "45MB" }
]

PLAYLISTS = [
  { "id": "p1", "name": "Morning Routine", "videos": ["v1"] }
]

@router.get("/videos", response_model=List[Video])
async def get_videos():
    return VIDEOS

@router.get("/playlists", response_model=List[Playlist])
async def get_playlists():
    return PLAYLISTS

@router.post("/playlists")
async def create_playlist(playlist: Playlist):
    PLAYLISTS.append(playlist.dict())
    return playlist
