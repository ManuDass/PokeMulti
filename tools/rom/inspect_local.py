"""Inspect a user-owned GBA or ZIP locally; never extract, download, or transmit it."""
import argparse
import pathlib
import subprocess
import zipfile

LIMIT = 32 * 1024 * 1024

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("rom", type=pathlib.Path)
    parser.add_argument("--tool", type=pathlib.Path, required=True)
    parser.add_argument("--probe", action="store_true")
    args = parser.parse_args()
    try:
        if args.rom.suffix.lower() == ".zip":
            with zipfile.ZipFile(args.rom) as archive:
                candidates = [item for item in archive.infolist() if not item.is_dir() and item.filename.lower().endswith(".gba")]
                if len(candidates) != 1:
                    raise ValueError("Select an archive containing exactly one GBA ROM.")
                if candidates[0].file_size > LIMIT:
                    raise ValueError("ROM exceeds the 32 MiB input limit.")
                with archive.open(candidates[0]) as stream:
                    data = stream.read(LIMIT + 1)
        else:
            with args.rom.open("rb") as stream:
                data = stream.read(LIMIT + 1)
        if len(data) > LIMIT:
            raise ValueError("ROM exceeds the 32 MiB input limit.")
        # Binary data goes only to the local validator's stdin, never the terminal or network.
        mode = "--probe-stdin" if args.probe else "--validate-stdin"
        result = subprocess.run([str(args.tool.resolve()), mode], input=data, check=False)
        return result.returncode
    except (OSError, ValueError, zipfile.BadZipFile, RuntimeError) as error:
        parser.exit(3, f"[ROM] {error}\n")

if __name__ == "__main__":
    raise SystemExit(main())
