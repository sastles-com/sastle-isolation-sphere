"""共有 config.json の読み書きサービス。

config.json は server↔device で共有する「事前設定の単一ソース」。
本サービスは運用に関わる `params`(明るさ等の既定値)と `playback`
(active playlist / loop / shuffle / autoplay)の取得・更新・永続化を担う。
プレイリストの中身は DB(playlist API)側で管理し、ここでは選択と運用オプションのみ保持。

`spheres`(接続する core の一覧)と `active_sphere`(既定の操作対象)もここで扱う。
core は同一の config.json を共有し、自機 MAC で spheres[] の自分のエントリを選ぶ
(core/src/ConfigManager.cpp)。サーバーはこの一覧をデバイスレジストリとして
WebUI に渡し、選択された core にだけコマンド/映像を送る。
"""
import json
import logging
import os
import threading

from app.core.config import CONFIG_SEARCH_PATHS, DEVICE_TARGET_ALL, MQTT_DEVICE_ID

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
        return {
            "params": self.get_params(),
            "playback": self.get_playback(),
            "spheres": self.get_spheres(),
            "active_sphere": self.get_active_sphere(),
        }

    # ===== spheres レジストリ =====

    def get_spheres(self):
        """接続対象 core の一覧を返す。

        新形式 `spheres`(配列)を優先し、無ければ旧形式の単一キー `sphere` を
        1要素のリストとして返す(移行前 config.json との後方互換)。
        """
        spheres = self._data.get("spheres")
        if isinstance(spheres, list) and spheres:
            return [s for s in spheres if isinstance(s, dict) and s.get("id")]
        legacy = self._data.get("sphere")
        if isinstance(legacy, dict) and legacy.get("id"):
            return [legacy]
        return []

    def get_sphere(self, device_id: str):
        """指定 id の core エントリを返す (無ければ None)。"""
        for s in self.get_spheres():
            if s.get("id") == device_id:
                return s
        return None

    def get_active_sphere(self):
        """既定の操作対象 core の id を返す。

        config.json の `active_sphere` を優先。未設定/不正なら spheres[0]、
        spheres[] 自体が空なら従来のハードコード値にフォールバックする。
        `DEVICE_TARGET_ALL` ("all") は「全 core にブロードキャスト」を意味する
        正当な値としてそのまま返す。
        """
        spheres = self.get_spheres()
        ids = [s.get("id") for s in spheres]
        active = self._data.get("active_sphere")
        if active == DEVICE_TARGET_ALL or (active and active in ids):
            return active
        if ids:
            return ids[0]
        return MQTT_DEVICE_ID

    def get_target_ips(self, device_id: str = None):
        """宛先 core の IP 一覧を返す (映像 UDP の送出先に使う)。

        device_id が "all" なら登録されている全 core の IP、それ以外はその core の
        IP を1件返す。static_ip が空のエントリは除外する。
        """
        target = device_id or self.get_active_sphere()
        spheres = self.get_spheres()
        if target == DEVICE_TARGET_ALL:
            return [s.get("static_ip") for s in spheres if s.get("static_ip")]
        entry = self.get_sphere(target)
        ip = (entry or {}).get("static_ip")
        return [ip] if ip else []

    def set_active_sphere(self, device_id: str):
        """操作対象 core を切り替えて config.json に永続化する。

        Returns: 確定した id
        Raises: ValueError (未知の id)
        """
        ids = [s.get("id") for s in self.get_spheres()]
        if device_id != DEVICE_TARGET_ALL and device_id not in ids:
            raise ValueError(f"unknown sphere id: {device_id}")
        with self._lock:
            self._data["active_sphere"] = device_id
            self._save()
        logger.info(f"active_sphere -> {device_id}")
        return device_id

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
