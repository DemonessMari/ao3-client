from pathlib import Path
import subprocess
import sys
import tempfile

project = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as directory:
    root = Path(directory)
    source = root / 'package'
    application = root / 'Applications/AO3 Client'
    local = root / 'user-local'
    (source / 'bin').mkdir(parents=True)
    (source / 'bin/ao3-client').write_text('#!/bin/sh\nexit 0\n')
    (source / 'share/applications').mkdir(parents=True)
    (source / 'share/applications/ao3-client.desktop').write_text('[Desktop Entry]\nType=Application\nName=AO3 Client\nExec=ao3-client\n')
    (source / 'share/icons/hicolor/256x256/apps').mkdir(parents=True)
    (source / 'share/icons/hicolor/256x256/apps/ao3-client.png').write_bytes(b'icon')
    application.mkdir(parents=True)
    (application / 'keep.txt').write_text('existing file')
    for _ in range(2):
        subprocess.run([sys.executable, str(project / 'scripts/install-package.py'), str(source), str(application), str(local)], check=True, capture_output=True)
    assert (application / 'keep.txt').read_text() == 'existing file'
    assert (application / 'ao3-client').stat().st_mode & 0o111
    assert (application / 'bin/ao3-client').is_symlink()
    assert (local / 'bin/ao3-client').is_symlink()
    assert (local / 'bin/ao3-client').resolve() == application / 'ao3-client'
    assert str(application / 'bin/ao3-client') in (local / 'share/applications/ao3-client.desktop').read_text()
    subprocess.run([str(local / 'bin/ao3-client')], cwd=root, check=True)
    missing = root / 'missing'
    result = subprocess.run([sys.executable, str(project / 'scripts/install-package.py'), str(missing), str(application), str(local)], capture_output=True)
    assert result.returncode != 0
    assert (application / 'ao3-client').read_text() == '#!/bin/sh\nexit 0\n'
print('Installer paths, upgrade, preservation and launcher passed')
