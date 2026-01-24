"""Terminal output formatting with clean, readable colors."""

import sys
import shutil
from dataclasses import dataclass
from enum import Enum
from typing import TextIO

class Color(Enum):
    RESET = "\033[0m"
    BOLD = "\033[1m"
    DIM = "\033[2m"
    
    RED = "\033[38;5;203m"
    GREEN = "\033[38;5;114m"
    YELLOW = "\033[38;5;221m"
    BLUE = "\033[38;5;110m"
    CYAN = "\033[38;5;116m"
    GRAY = "\033[38;5;245m"
    WHITE = "\033[38;5;252m"

@dataclass
class Theme:
    error: Color = Color.RED
    success: Color = Color.GREEN
    warning: Color = Color.YELLOW
    info: Color = Color.BLUE
    detail: Color = Color.GRAY
    highlight: Color = Color.CYAN
    text: Color = Color.WHITE

_theme = Theme()
_color_enabled = True

def _supports_color() -> bool:
    if not hasattr(sys.stdout, "isatty"):
        return False
    if not sys.stdout.isatty():
        return False
    if sys.platform == "win32":
        try:
            import ctypes
            kernel32 = ctypes.windll.kernel32
            kernel32.SetConsoleMode(kernel32.GetStdHandle(-11), 7)
            return True
        except Exception:
            return False
    return True

def init():
    global _color_enabled
    _color_enabled = _supports_color()

def _c(color: Color, text: str) -> str:
    if not _color_enabled:
        return text
    return f"{color.value}{text}{Color.RESET.value}"

def _bold(text: str) -> str:
    if not _color_enabled:
        return text
    return f"{Color.BOLD.value}{text}{Color.RESET.value}"

def error(msg: str, detail: str = ""):
    prefix = _c(_theme.error, "error")
    print(f"{prefix}: {msg}", file=sys.stderr)
    if detail:
        print(f"       {_c(_theme.detail, detail)}", file=sys.stderr)

def warning(msg: str):
    prefix = _c(_theme.warning, "warning")
    print(f"{prefix}: {msg}", file=sys.stderr)

def success(msg: str):
    print(_c(_theme.success, msg))

def info(msg: str):
    print(_c(_theme.info, msg))

def detail(msg: str):
    print(_c(_theme.detail, msg))

def step(msg: str):
    print(f"{_c(_theme.highlight, '>')} {msg}")

def header(title: str):
    width = min(shutil.get_terminal_size().columns, 60)
    line = _c(_theme.detail, "-" * width)
    print(f"\n{line}")
    print(_bold(title))
    print(f"{line}\n")

class Progress:
    def __init__(self, total: int, desc: str = "", width: int = 30):
        self.total = total
        self.current = 0
        self.desc = desc
        self.width = width
        self._last_line_len = 0
    
    def update(self, n: int = 1, status: str = ""):
        self.current = min(self.current + n, self.total)
        self._render(status)
    
    def _render(self, status: str = ""):
        if self.total == 0:
            pct = 100
            filled = self.width
        else:
            pct = int(100 * self.current / self.total)
            filled = int(self.width * self.current / self.total)
        
        bar_fill = _c(_theme.success, "=" * filled)
        bar_empty = _c(_theme.detail, "-" * (self.width - filled))
        bar = f"[{bar_fill}{bar_empty}]"
        
        pct_str = f"{pct:3d}%"
        count = f"{self.current}/{self.total}"
        
        if self.desc:
            line = f"\r{self.desc} {bar} {pct_str} {_c(_theme.detail, count)}"
        else:
            line = f"\r{bar} {pct_str} {_c(_theme.detail, count)}"
        
        if status:
            line += f" {_c(_theme.detail, status)}"
        
        padding = max(0, self._last_line_len - len(line))
        sys.stdout.write(line + " " * padding)
        sys.stdout.flush()
        self._last_line_len = len(line)
    
    def finish(self, msg: str = ""):
        self.current = self.total
        self._render()
        print()
        if msg:
            success(msg)

class Spinner:
    FRAMES = ["   ", ".  ", ".. ", "..."]
    
    def __init__(self, msg: str):
        self.msg = msg
        self.frame = 0
        self._active = False
    
    def __enter__(self):
        self._active = True
        self._render()
        return self
    
    def __exit__(self, *args):
        self._clear()
        self._active = False
    
    def tick(self):
        if not self._active:
            return
        self.frame = (self.frame + 1) % len(self.FRAMES)
        self._render()
    
    def _render(self):
        frame = _c(_theme.detail, self.FRAMES[self.frame])
        sys.stdout.write(f"\r{self.msg}{frame}")
        sys.stdout.flush()
    
    def _clear(self):
        sys.stdout.write("\r" + " " * (len(self.msg) + 4) + "\r")
        sys.stdout.flush()

def table(headers: list[str], rows: list[list[str]]):
    if not rows:
        return
    
    widths = [len(h) for h in headers]
    for row in rows:
        for i, cell in enumerate(row):
            if i < len(widths):
                widths[i] = max(widths[i], len(cell))
    
    header_line = "  ".join(
        _bold(h.ljust(widths[i])) for i, h in enumerate(headers)
    )
    print(header_line)
    print(_c(_theme.detail, "-" * sum(widths) + "-" * (2 * (len(widths) - 1))))
    
    for row in rows:
        cells = []
        for i, cell in enumerate(row):
            w = widths[i] if i < len(widths) else 0
            cells.append(cell.ljust(w))
        print("  ".join(cells))

init()
