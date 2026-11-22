#!/usr/bin/env python3
"""Install a staged Linux package without moving source or application data."""
from pathlib import Path
import os
import shutil
import subprocess
import sys
import tempfile

source = Path(sys.argv[1]).absolute()
application = Path(sys.argv[2]).expanduser().absolute() if len(sys.argv) > 2 else Path.home() / 'Applications/ao3-client'
user_prefix = Path(sys.argv[3]).expanduser().absolute() if len(sys.argv) > 3 else Path.home() / '.local'
executable = source / 'bin/ao3-client'
if not executable.is_file():
    raise SystemExit('The package executable is missing.')
with executable.open('rb') as handle:
    is_elf = handle.read(4) == b'\x7fELF'
if is_elf:
    result = subprocess.run(['ldd', str(executable)], text=True, capture_output=True)
    if result.returncode or 'not found' in result.stdout:
        raise SystemExit('Required system libraries are missing or incompatible.\n' + result.stdout + result.stderr)
application.mkdir(parents=True, exist_ok=True)
fd, temporary = tempfile.mkstemp(prefix='.ao3-client-', dir=application)
os.close(fd)
try:
    shutil.copyfile(executable, temporary)
    Path(temporary).chmod(0o755)
    shutil.copytree(source / 'share', application / 'share', dirs_exist_ok=True)
    os.replace(temporary, application / 'ao3-client')
finally:
    Path(temporary).unlink(missing_ok=True)

def link(target, destination):
    destination.parent.mkdir(parents=True, exist_ok=True)
    temporary = destination.with_name(destination.name + '.new-' + str(os.getpid()))
    temporary.unlink(missing_ok=True)
    temporary.symlink_to(target)
    os.replace(temporary, destination)

# Keep standard prefix-style paths usable without copying the executable.
link(Path('../ao3-client'), application / 'bin/ao3-client')
writer = Path(__file__).with_name('write-launcher.py')
subprocess.run([sys.executable, str(writer), str(application)], check=True)
local_binary = user_prefix / 'bin/ao3-client'
if local_binary.exists() and not local_binary.is_symlink() and local_binary.resolve() != (application / 'ao3-client').resolve():
    previous = application / 'ao3-client.previous'
    if previous.exists():
        raise SystemExit('A previous executable already exists; the existing launcher was left intact.')
    shutil.move(str(local_binary), previous)
link(application / 'ao3-client', local_binary)
for relative in ['applications/ao3-client.desktop', 'icons/hicolor/256x256/apps/ao3-client.png']:
    destination = user_prefix / 'share' / relative
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(application / 'share' / relative, destination)
print('Installed AO3 Client in ' + str(application))
