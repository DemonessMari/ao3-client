#!/usr/bin/env python3
"""Resolve a standard desktop launcher to this installation, independent of PATH."""
from pathlib import Path
import os
import sys

prefix = Path(sys.argv[1]).expanduser().absolute()
launcher = prefix / 'share/applications/ao3-client.desktop'
executable = str(prefix / 'bin/ao3-client').replace('%', '%%')
# Desktop Exec quoting is parsed after Desktop Entry string escapes.
escaped = ''.join('\\' + c if c in '\\"`$' else c for c in executable)
value = '/usr/bin/env "' + escaped.replace('\\', '\\\\') + '"'
lines = launcher.read_text().splitlines()
text = '\n'.join('Exec=' + value if line.startswith('Exec=') else line for line in lines) + '\n'
temporary = launcher.with_suffix('.desktop.tmp')
temporary.write_text(text)
temporary.chmod(0o644)
os.replace(temporary, launcher)
