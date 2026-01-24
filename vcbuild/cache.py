"""Incremental build cache management."""

import json
import hashlib
from pathlib import Path
from dataclasses import dataclass

CACHE_FILE = ".vcbuild_cache.json"

@dataclass
class CacheEntry:
    hash: str
    mtime: float

class BuildCache:
    def __init__(self, project_root: Path):
        self.project_root = project_root
        self.cache_path = project_root / CACHE_FILE
        self.data = self._load()
    
    def _load(self) -> dict:
        if not self.cache_path.exists():
            return {"version": 1, "files": {}, "config_hash": ""}
        try:
            with open(self.cache_path, "r", encoding="utf-8") as f:
                return json.load(f)
        except (json.JSONDecodeError, IOError):
            return {"version": 1, "files": {}, "config_hash": ""}
    
    def save(self):
        with open(self.cache_path, "w", encoding="utf-8") as f:
            json.dump(self.data, f, indent=2)
    
    def file_changed(self, path: Path) -> bool:
        if not path.exists():
            return True
        
        key = str(path.resolve())
        current_hash = _hash_file(path)
        
        cached = self.data.get("files", {}).get(key)
        if cached is None:
            return True
        
        return cached.get("hash") != current_hash
    
    def update_file(self, path: Path):
        if not path.exists():
            return
        
        key = str(path.resolve())
        self.data.setdefault("files", {})[key] = {
            "hash": _hash_file(path),
            "mtime": path.stat().st_mtime
        }
    
    def config_changed(self, config_hash: str) -> bool:
        return self.data.get("config_hash") != config_hash
    
    def update_config_hash(self, config_hash: str):
        self.data["config_hash"] = config_hash
    
    def clear(self):
        self.data = {"version": 1, "files": {}, "config_hash": ""}
        if self.cache_path.exists():
            self.cache_path.unlink()

def _hash_file(path: Path, chunk_size: int = 8192) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as f:
        while chunk := f.read(chunk_size):
            h.update(chunk)
    return h.hexdigest()[:16]

def hash_config(config_data: dict) -> str:
    content = json.dumps(config_data, sort_keys=True)
    return hashlib.sha256(content.encode()).hexdigest()[:16]
