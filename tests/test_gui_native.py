"""Native-window smoke test. Requires an actual desktop or Xvfb; never substitutes headless."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys
ROOT = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser()
p.add_argument('--cc', default=shutil.which('cc') or shutil.which('gcc') or shutil.which('clang'))
args = p.parse_args()
tiny = ROOT / 'bin' / ('tiny.exe' if os.name == 'nt' else 'tiny')
if sys.platform.startswith('linux'):
    if not os.environ.get('DISPLAY'):
        raise SystemExit('Native X11 test requires DISPLAY (run with xvfb-run).')
    backend, fixture = 'x11', 'gui_x11.tc'
elif sys.platform == 'darwin':
    backend, fixture = 'cocoa', 'gui_cocoa.tc'
else:
    backend, fixture = 'win32', 'gui_x11.tc'  # Window instance requests Win32, not headless.
subprocess.run([str(tiny), 'run', str(ROOT / 'tests' / fixture), '--gui-backend', backend, '--cc', args.cc], check=True, timeout=30)
print('Native GUI create/present/close passed on ' + sys.platform)
