"""共有 config.json の読み書きサービス。

config.json は server↔device で共有する「事前設定の単一ソース」。
本サービスは運用に関わる `params`(明るさ等の既定値)と `playback`
(active playlist / loop / shuffle / autoplay)の取得・更新・永続化を担う。
プレイリストの中身は DB(playlist API)側で管理し、ここでは選択と運用オプションのみ保持。
"""
import json
import logging
import os
import threading

from app.core.config import CONFIG_SEARCH_PATHS

logger = logging.getLogger(__name__)

DEFAULT_PARAMS = {"brightness": 50, "speed": 50, "hue": 120, "saturation": 100}
DEFAULT_PLAYBACK = {"active_playlist": None, "loop": True, "shuffle": False, "autoplay": False}


class ConfigService:
    def __init__(self):
        self._lock = threading.Lock()
        self._path = self._find_path()
        self._data = self._load()
        logger.info(f"ConfigService using {self._path}")

    def _find_path(self):
        for p in CONFIG_SEARCH_PATHS:
            if os.path.exists(p):
                return p
        return CONFIG_SEARCH_PATHS[0]

    def _load(self):
        try:
            with open(self._path) as f:
                return json.load(f)
        except Exception as e:
            logger.warning(f"config load failed ({self._path}): {e}")
            return {}

    def get_params(self):
        return {**DEFAULT_PARAMS, **(self._data.get("params") or {})}

    def get_playback(self):
        return {**DEFAULT_PLAYBACK, **(self._data.get("playback") or {})}

    def get_public(self):
        return {"params": self.get_params(), "playback": self.get_playback()}

    def update(self, params: dict = None, playback: dict = None):
        """params / playback を部分更新して config.json に永続化する。"""
        with self._lock:
            if params:
                cur = dict(self._data.get("params") or {})
                cur.update({k: v for k, v in params.items() if k in DEFAULT_PARAMS})
                self._data["params"] = {**DEFAULT_PARAMS, **cur}
            if playback:
                cur = dict(self._data.get("playback") or {})
                cur.update({k: v for k, v in playback.items() if k in DEFAULT_PLAYBACK})
                self._data["playback"] = {**DEFAULT_PLAYBACK, **cur}
            self._save()
        return self.get_public()

    def _save(self):
        tmp = self._path + ".tmp"
        with open(tmp, "w") as f:
            json.dump(self._data, f, indent=2, ensure_ascii=False)
            f.write("\n")
        os.replace(tmp, self._path)
        logger.info(f"config saved to {self._path}")
