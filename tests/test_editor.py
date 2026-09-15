"""Replay real editor events, including saving, Unicode and unsaved changes."""
from pathlib import Path
import os
import subprocess
import uuid

ROOT = Path(__file__).resolve().parents[1]
folder = ROOT / 'build' / 'tests' / ('editor-' + uuid.uuid4().hex)
folder.mkdir(parents=True)
tiny = ROOT / 'bin' / ('tiny.exe' if os.name == 'nt' else 'tiny')
editor = folder / ('tinyedit.exe' if os.name == 'nt' else 'tinyedit')
subprocess.run([str(tiny), 'build', str(ROOT / 'apps' / 'tinyedit.tc'), '-o', str(editor)], check=True)
path = folder / 'document.txt'
keys = folder / 'keys.txt'
screen = folder / 'screen.txt'

def replay(initial, events, expected):
    path.write_text(initial, encoding='utf-8')
    keys.write_bytes(events.encode('utf-8'))
    result = subprocess.run([str(editor), str(path), '--replay', str(keys), '--snapshot', str(screen)], capture_output=True, text=True, timeout=10)
    assert result.returncode == 0, (result.stdout, result.stderr)
    assert path.read_text(encoding='utf-8') == expected
    snapshot = screen.read_text(encoding='utf-8')
    assert str(path)[:60] in snapshot and len(snapshot.splitlines()) == 24
    assert not list(folder.glob('*.tmp'))
    return snapshot

snapshot = replay('world\nsecond\n', 'Hello ñ日\x13\x11', 'Hello ñ日world\nsecond\n')
assert 'Saved.' in snapshot and 'Hello ñ日world' in snapshot
# Search second, cut its line, go to first, paste, save and quit.
replay('first\nsecond\n', '\x06second\r\x18\x071\r\x15\x13\x11', 'second\nfirst\n')
# Double quit discards the dirty buffer without touching the disk.
replay('keep\n', 'discard\x11\x11', 'keep\n')
# Backspace deletes a complete multibyte character.
replay('', 'ñ日\x08\x13\x11', 'ñ')
print('TinyEdit verified: open/edit/atomic save/quit, Unicode, search, goto, cut/paste, discard confirmation')
