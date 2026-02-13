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

def _dim(text: str) -> str:
    if not _color_enabled:
        return text
    return f"{Color.DIM.value}{text}{Color.RESET.value}"

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

def build_header(name: str, profile: str, arch: str, proj_type: str):
    """Print a professional build header with project info."""
    width = min(shutil.get_terminal_size().columns, 64)
    bar = _c(_theme.detail, "=" * width)
    print(f"\n{bar}")
    print(f"  {_bold(name)}")
    labels = []
    labels.append(f"{profile}")
    labels.append(f"{arch}")
    labels.append(f"{proj_type}")
    print(f"  {_c(_theme.detail, ' | '.join(labels))}")
    print(bar)

def build_summary(success: bool, duration: float, files: int,
                  warnings: int, errors: int, output_path: str = ""):
    """Print build result summary."""
    width = min(shutil.get_terminal_size().columns, 64)
    bar = _c(_theme.detail, "-" * width)
    print(f"\n{bar}")

    if success:
        status = _c(_theme.success, "BUILD SUCCEEDED")
    else:
        status = _c(_theme.error, "BUILD FAILED")

    print(f"  {status}")

    parts = []
    parts.append(f"{files} file(s)")
    if warnings > 0:
        parts.append(_c(_theme.warning, f"{warnings} warning(s)"))
    if errors > 0:
        parts.append(_c(_theme.error, f"{errors} error(s)"))
    parts.append(f"{_format_duration(duration)}")

    print(f"  {_c(_theme.detail, ' | '.join(parts))}")

    if success and output_path:
        print(f"  {_c(_theme.detail, '->')} {output_path}")

    print(bar)

def _format_duration(seconds: float) -> str:
    if seconds < 1.0:
        return f"{seconds*1000:.0f}ms"
    elif seconds < 60.0:
        return f"{seconds:.1f}s"
    else:
        m = int(seconds // 60)
        s = seconds % 60
        return f"{m}m {s:.1f}s"


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

        bar_char = "#"
        empty_char = "."
        bar_fill = _c(_theme.success, bar_char * filled)
        bar_empty = _c(_theme.detail, empty_char * (self.width - filled))
        bar = f"[{bar_fill}{bar_empty}]"

        pct_str = f"{pct:3d}%"
        count = f"{self.current}/{self.total}"

        if self.desc:
            line = f"\r  {self.desc} {bar} {pct_str} {_c(_theme.detail, count)}"
        else:
            line = f"\r  {bar} {pct_str} {_c(_theme.detail, count)}"

        if status:
            # Truncate status to fit terminal
            term_width = shutil.get_terminal_size().columns
            max_status = max(10, term_width - len(line) + _ansi_len_diff(line) - 2)
            if len(status) > max_status:
                status = status[:max_status - 3] + "..."
            line += f" {_c(_theme.detail, status)}"

        padding = max(0, self._last_line_len - _visible_len(line))
        sys.stdout.write(line + " " * padding)
        sys.stdout.flush()
        self._last_line_len = _visible_len(line)

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
        sys.stdout.write(f"\r  {self.msg}{frame}")
        sys.stdout.flush()

    def _clear(self):
        sys.stdout.write("\r" + " " * (len(self.msg) + 6) + "\r")
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
    print(f"  {header_line}")
    print(f"  {_c(_theme.detail, '-' * (sum(widths) + 2 * (len(widths) - 1)))}")

    for row in rows:
        cells = []
        for i, cell in enumerate(row):
            w = widths[i] if i < len(widths) else 0
            cells.append(cell.ljust(w))
        print(f"  {'  '.join(cells)}")


def _ansi_len_diff(text: str) -> int:
    """Return the difference between string length and visible length (ANSI codes)."""
    import re
    ansi_re = re.compile(r'\033\[[0-9;]*m')
    stripped = ansi_re.sub('', text)
    return len(text) - len(stripped)

def _visible_len(text: str) -> int:
    """Return the visible length of text (excluding ANSI codes)."""
    import re
    ansi_re = re.compile(r'\033\[[0-9;]*m')
    return len(ansi_re.sub('', text))

init()
