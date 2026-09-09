"""Select a local native test fixture without reusing one from another ROM."""
from pathlib import Path
import hashlib
import zipfile


def local_field_state(root: Path, rom: Path, selected: Path | None) -> Path:
    if selected is not None:
        state = selected.resolve()
    else:
        if rom.suffix.lower() == '.zip':
            with zipfile.ZipFile(rom) as archive:
                data = archive.read(next(name for name in archive.namelist() if name.lower().endswith('.gba')))
        else:
            data = rom.read_bytes()
        if hashlib.sha1(data).hexdigest() != '41cb23d8dccc8ebd7c649cd8fbb58eeace6e2fdc':
            raise ValueError('Pass --state with a field savestate created using this exact ROM.')
        state = root / 'cache/runtime-check/cable-a.state'
    if not state.is_file():
        raise ValueError('Local test savestate does not exist: ' + str(state))
    return state
