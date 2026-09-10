"""Audit the adapter's factual addresses against locally supplied symbol maps.

Download only the four .sym metadata files linked in docs/ROM_SUPPORT.md.
This tool does not read, generate or distribute game ROMs.
"""
from pathlib import Path
import argparse
import re

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--symbols', required=True, type=Path)
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
variants = ('pokefirered', 'pokefirered_rev1', 'pokeleafgreen', 'pokeleafgreen_rev1')
tables = []
for variant in variants:
    rows = {}
    for line in (args.symbols / (variant + '.sym')).read_text().splitlines():
        match = re.fullmatch(r'([0-9a-fA-F]{8}) [lg!] ([0-9a-fA-F]{8}) (.+)', line)
        if match:
            rows[match[3]] = (int(match[1], 16), int(match[2], 16))
    if not rows:
        raise SystemExit('Empty symbol table: ' + variant)
    tables.append(rows)

count = 0
mapped_addresses = set()
for line in (root / 'src/game/rom_layout.hpp').read_text().splitlines():
    match = re.fullmatch(r'\s*\{(0x[0-9a-f]{8}), \{(.+)\}\}, // (\S+)(?: \+ (0x[0-9a-f]+))?', line)
    if not match:
        continue
    canonical, values, name, offset = match.groups()
    offset = int(offset or '0', 16)
    actual = [int(value, 16) for value in values.split(',')]
    expected = [table[name][0] + offset for table in tables]
    if len(set(table[name][1] for table in tables)) != 1:
        raise SystemExit('Hook ABI requires review: ' + name)
    if actual != expected or actual[0] != int(canonical, 16):
        raise SystemExit('Incorrect ROM mapping: ' + name)
    mapped_addresses.add(int(canonical, 16))
    count += 1
if not count:
    raise SystemExit('No ROM mappings found.')

# Also audit direct calls, so a new hook cannot refer to a missing mapping.
for path in (root / 'src/game').glob('world*'):
    for value in re.findall(r'gameAddress\((0x[0-9a-fA-F]+)\)', path.read_text()):
        if int(value, 16) not in mapped_addresses:
            raise SystemExit('Unmapped game call in ' + path.name + ': ' + value)

ram_count = 0
files = list((root / 'src/game').glob('world*')) + [root / 'src/game/wager_wallet.inc']
for address in sorted({int(value, 16) for path in files for value in re.findall(r'0x([0-9a-fA-F]{8})(?![0-9a-fA-F])', path.read_text())}):
    if not 0x02000000 <= address < 0x03008000:
        continue
    candidates = [(name, value) for name, value in tables[0].items()
                  if value[0] <= address < value[0] + max(1, value[1]) and all(name in table for table in tables)]
    if not candidates:
        raise SystemExit(f'Unverified RAM address: {address:08x}')
    name, value = max(candidates, key=lambda row: row[1][0])
    if any(table[name] != value for table in tables):
        raise SystemExit('RAM layout differs: ' + name)
    ram_count += 1
print(f'PASS: {count} ROM mappings and {ram_count} RAM references across four cartridge layouts.')
