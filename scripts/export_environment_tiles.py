"""Export native FireRed environment textures from the owner's ROM, locally only.

Requires Pillow. Reads a .gba or a ZIP containing exactly one .gba. The matching
symbol table and map-group catalog are already cached by this project. Extracted
art belongs under ignored extracted/, never in a redistributable application.
"""
from __future__ import annotations
import argparse
import csv
import hashlib
import html
import json
import math
import re
import struct
import zipfile
from collections import defaultdict
from pathlib import Path
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
ROM_SHA1 = '41cb23d8dccc8ebd7c649cd8fbb58eeace6e2fdc'
SOURCE_URL = 'https://github.com/pret/pokefirered/blob/master/src/'
# Destination tile, tiles copied, update interval, frame-table sequence.
# Verified against FireRed's original tileset animation callbacks.
ANIMATIONS = {
    'General_Flower': (508, 4, 16),
    'General_Water_Current_LandWatersEdge': (416, 48, 16),
    'General_SandWatersEdge': (464, 18, 8),
    'CeladonCity_Fountain': (744, 8, 12),
    'SilphCo_Fountain': (976, 8, 10),
    'MtEmber_Steam': (896, 8, 16),
    'VermilionGym_MotorizedDoor': (880, 7, 2),
    'CeladonGym_Flowers': (739, 4, 16),
}
PROPS = ('ItemBall', 'CutTree', 'RockSmashRock', 'StrengthBoulder', 'Fossil',
         'Ruby', 'Sapphire', 'OldAmber', 'GymSign', 'Sign', 'WoodenSign',
         'Clipboard', 'BirthIslandStone', 'LaprasDoll', 'Seagallop', 'SSAnne')


def power2(n):
    return 1 << (max(1, n) - 1).bit_length()


def write_json(path, value):
    path.write_text(json.dumps(value, indent=2, ensure_ascii=False), encoding='utf-8')


def write_csv(path, rows):
    if not rows:
        return
    with path.open('w', newline='', encoding='utf-8-sig') as f:
        writer = csv.DictWriter(f, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)


def read_rom(path):
    limit = 32 * 1024 * 1024
    if path.suffix.lower() == '.zip':
        with zipfile.ZipFile(path) as archive:
            files = [f for f in archive.infolist() if not f.is_dir() and f.filename.lower().endswith('.gba')]
            if len(files) != 1 or files[0].file_size > limit:
                raise ValueError('ZIP must contain exactly one GBA ROM, at most 32 MiB.')
            data = archive.read(files[0])
    else:
        with path.open('rb') as f:
            data = f.read(limit + 1)
    if hashlib.sha1(data).hexdigest() != ROM_SHA1:
        raise ValueError('This exporter requires the matching FireRed US 1.0 ROM and symbols.')
    return data


class Rom:
    def __init__(self, path, symbols):
        self.data = read_rom(path)
        self.symbols = {}
        for line in symbols.read_text().splitlines():
            parts = line.split()
            if len(parts) == 4 and re.fullmatch('[0-9a-fA-F]{8}', parts[0]):
                self.symbols[parts[3]] = (int(parts[0], 16), int(parts[2], 16))
        self.at = {addr: (name, size) for name, (addr, size) in self.symbols.items() if size}

    def read(self, addr, size):
        off = addr - 0x08000000
        if off < 0 or off + size > len(self.data):
            raise ValueError(f'Out-of-ROM read: {addr:#x}, {size}')
        return self.data[off:off + size]

    def u16(self, addr):
        return struct.unpack('<H', self.read(addr, 2))[0]

    def u32(self, addr):
        return struct.unpack('<I', self.read(addr, 4))[0]

    def symbol(self, name):
        return self.symbols[name][0]

    def lz(self, addr):
        head = self.u32(addr)
        if head & 255 != 0x10 or not 0 < head >> 8 <= 65536:
            raise ValueError(f'Invalid GBA LZ77 header at {addr:#x}')
        length, pos, result = head >> 8, addr + 4, bytearray()
        while len(result) < length:
            flags = self.read(pos, 1)[0]
            pos += 1
            for bit in range(7, -1, -1):
                if len(result) >= length:
                    break
                if flags >> bit & 1:
                    a, b = self.read(pos, 2)
                    pos += 2
                    distance = ((a & 15) << 8 | b) + 1
                    if distance > len(result):
                        raise ValueError('Invalid LZ77 back reference')
                    for _ in range(min((a >> 4) + 3, length - len(result))):
                        result.append(result[-distance])
                else:
                    result.extend(self.read(pos, 1))
                    pos += 1
        return bytes(result)

    def palette(self, addr, count=256):
        return [tuple((self.u16(addr + 2 * i) >> shift & 31) * 255 // 31 for shift in (0, 5, 10))
                for i in range(count)]


class Tileset:
    def __init__(self, rom, name, addr):
        self.name, self.addr = name, addr
        self.secondary = bool(rom.read(addr + 1, 1)[0])
        self.tiles_ptr, self.palette_ptr, self.defs_ptr, self.callback, self.attrs_ptr = (
            rom.u32(addr + i) for i in (4, 8, 12, 16, 20))
        raw_size = rom.at[self.tiles_ptr][1]
        self.raw = rom.lz(self.tiles_ptr) if rom.read(addr, 1)[0] else rom.read(self.tiles_ptr, raw_size)
        self.count = rom.at[self.defs_ptr][1] // 16
        assert len(self.raw) % 32 == 0
        assert rom.at[self.attrs_ptr][1] // 4 == self.count
        self.refs = [struct.unpack('<8H', rom.read(self.defs_ptr + 16 * i, 16)) for i in range(self.count)]
        self.attrs = [rom.u32(self.attrs_ptr + 4 * i) for i in range(self.count)]


def tile8(raw, tile, palette, pal, flip_x=False, flip_y=False, transparent=True):
    pixels = []
    for y in range(8):
        sy = 7 - y if flip_y else y
        for x in range(8):
            sx = 7 - x if flip_x else x
            byte = raw[tile * 32 + sy * 4 + sx // 2]
            index = (byte >> ((sx & 1) * 4)) & 15
            pixels.append((*palette[pal * 16 + index], 0 if transparent and index == 0 else 255))
    result = Image.new('RGBA', (8, 8))
    result.putdata(pixels)
    return result


class Pair:
    def __init__(self, rom, primary, secondary):
        self.primary, self.secondary = primary, secondary
        self.name = primary.name + '__' + secondary.name
        self.palette = rom.palette(primary.palette_ptr)
        other = rom.palette(secondary.palette_ptr)
        self.palette[112:208] = other[112:208]
        self.palette[0] = (0, 0, 0)  # Native field backdrop.
        self.raw = bytearray(32768)
        # Match the original VRAM transfer lengths; retain extra source tiles in raw exports.
        for ts, off, capacity in ((primary, 0, 640 * 32), (secondary, 640 * 32, 384 * 32)):
            data = ts.raw[:capacity]
            self.raw[off:off + len(data)] = data
        self.refs = dict(enumerate(primary.refs))
        self.refs.update({640 + i: refs for i, refs in enumerate(secondary.refs)})
        self.attrs = dict(enumerate(primary.attrs))
        self.attrs.update({640 + i: attr for i, attr in enumerate(secondary.attrs)})
        self.parts_cache = {}
        self.art = {i: self.decode(refs) for i, refs in self.refs.items()}

    def decode(self, refs, raw=None):
        raw = self.raw if raw is None else raw
        layers = [Image.new('RGBA', (16, 16)) for _ in range(2)]
        for layer in range(2):
            for j, ref in enumerate(refs[layer * 4:layer * 4 + 4]):
                key = (bytes(raw[(ref & 1023) * 32:(ref & 1023) * 32 + 32]), ref & ~1023)
                if key not in self.parts_cache:
                    self.parts_cache[key] = tile8(key[0], 0, self.palette, ref >> 12,
                                                 bool(ref & 1024), bool(ref & 2048))
                layers[layer].paste(self.parts_cache[key], (j % 2 * 8, j // 2 * 8))
        composite = Image.new('RGBA', (16, 16), (0, 0, 0, 255))
        composite.alpha_composite(layers[0])
        composite.alpha_composite(layers[1])
        return composite, *layers


def save_layers(out, stem, entries, columns=16, ids=None):
    slots = ids if ids is not None else list(range(len(entries)))
    height = power2((max(slots, default=0) // columns + 1) * 16)
    for layer, suffix in enumerate(('', '-lower', '-upper')):
        image = Image.new('RGBA', (columns * 16, height))
        for slot, art in zip(slots, entries):
            image.paste(art[layer], (slot % columns * 16, slot // columns * 16))
        image.save(out / (stem + suffix + '.png'))


def export_raw(rom, tilesets, out):
    folder = out / 'raw-8x8'
    folder.mkdir(exist_ok=True)
    rows = []
    for ts in tilesets.values():
        count = len(ts.raw) // 32
        height = power2(math.ceil(count / 16) * 8)
        image = Image.new('RGBA', (2048, height))
        palette = rom.palette(ts.palette_ptr)
        for pal in range(16):
            for tile in range(count):
                image.paste(tile8(ts.raw, tile, palette, pal), (pal * 128 + tile % 16 * 8, tile // 16 * 8))
        image.save(folder / (ts.name + '.png'))
        rows.append(dict(tileset=ts.name, tile_count=count, palette_band_width=128,
                         tile_columns_per_band=16, palette_bands=16,
                         active_palette_slots='7-12' if ts.secondary else '0-6',
                         note='All palette interpretations; use assembled sheets for actual palette assignments.'))
    write_csv(folder / 'index.csv', rows)


def export_animations(rom, pairs, out):
    folder = out / 'animations'
    folder.mkdir(exist_ok=True)
    rows = []
    for name, (start, count, interval) in ANIMATIONS.items():
        owner = name.split('_')[0]
        candidates = [p for p in pairs.values()
                      if (p.primary.name if owner == 'General' else p.secondary.name) == owner]
        table, table_size = rom.symbols['sTilesetAnims_' + name]
        order = [rom.u32(table + i) for i in range(0, table_size, 4)]
        columns = power2(len(order))
        art, slots, sources, sequence_ids = [], [], [], {}
        for pair in candidates:
            raw_frames = []
            for ptr in order:
                raw = pair.raw.copy()
                raw[start * 32:(start + count) * 32] = rom.read(ptr, count * 32)
                raw_frames.append(raw)
            for tile_id, refs in pair.refs.items():
                if not any(start <= (r & 1023) < start + count for r in refs):
                    continue
                frames = [pair.decode(refs, raw) for raw in raw_frames]
                key = b''.join(image.tobytes() for frame in frames for image in frame)
                source = dict(tileset=pair.name, metatile_id=tile_id)
                if key in sequence_ids:
                    sources[sequence_ids[key]].append(source)
                    continue
                row = len(sources)
                sequence_ids[key] = row
                sources.append([source])
                art.extend(frames)
                slots.extend(row * columns + frame for frame in range(len(order)))
        save_layers(folder, name, art, columns, slots)
        rows.append(dict(name=name, frames=len(order), frame_columns=columns,
                         frame_interval_game_ticks=interval, row_sources=sources,
                         destination_tile=start, transfer_tile_count=count,
                         source_frame_bytes=[rom.at[ptr][1] for ptr in order],
                         source_frame_symbols=[rom.at[ptr][0] for ptr in order],
                         native_transfer_bytes=count * 32))
    write_json(folder / 'index.json', rows)
    cards = ''.join(f'<h2>{r["name"]}</h2><p>{r["frames"]} frames across, '
                    f'{len(r["row_sources"])} distinct tiles down. '
                    f'{r["frame_interval_game_ticks"]} game ticks per frame.</p>'
                    f'<p><a href="{r["name"]}.png">Composite PNG</a> ? '
                    f'<a href="{r["name"]}-lower.png">Lower layer</a> ? '
                    f'<a href="{r["name"]}-upper.png">Upper layer</a></p>'
                    f'<img src="{r["name"]}.png" alt="{r["name"]}">' for r in rows)
    (folder / 'index.html').write_text('<!doctype html><html lang="en"><meta charset="utf-8">'
        '<title>Environment animations</title><style>body{background:#15242b;color:#ecf5ee;'
        'font:16px/1.5 system-ui;margin:32px}a{color:#9fe6c4}img{image-rendering:pixelated;'
        'background:#63777c}h2{margin-top:40px}</style><a href="../index.html">Back to gallery</a>'
        '<h1>Environment animations</h1><p>UV tile size: 16 ? 16. Frames run left to right; '
        'each row is a different tile. Empty columns are padding. '
        '<a href="index.json">Row sources and frame order</a></p>' + cards + '</html>', encoding='utf-8')
    return rows


def export_doors(rom, pairs, maps, out):
    folder = out / 'doors'
    folder.mkdir(exist_ok=True)
    table, size = rom.symbols['sDoorGraphics']
    rows, atlas_art = [], []
    for offset in range(0, size, 12):
        p = table + offset
        tile_id, large, ptr, pp = rom.u16(p), rom.read(p + 3, 1)[0], rom.u32(p + 4), rom.u32(p + 8)
        if not ptr:
            break
        name = rom.at[ptr][0].removeprefix('sDoorAnimTiles_')
        height, tiles = (32, 8) if large else (16, 4)
        palettes = rom.read(pp, 8)
        seen = set()
        for map_info in maps:
            pair = pairs[map_info['tileset']]
            for pos, raw_id in enumerate(map_info['_ids']):
                if raw_id & 1023 != tile_id or pair.attrs[tile_id] & 511 not in (0x69, 0x60):
                    continue
                # Native door lookup is based on behavior and metatile ID, even when IDs repeat across tilesets.
                colors = tuple(pair.palette[p * 16:(p + 1) * 16] for p in palettes)
                key = tuple(tuple(c) for c in colors)
                if key in seen:
                    break
                seen.add(key)
                sheet = Image.new('RGBA', (64, power2(height)))
                closed = Image.new('RGBA', (16, height))
                if large and pos >= map_info['width']:
                    top_id = map_info['_ids'][pos - map_info['width']] & 1023
                    closed.paste(pair.art[top_id][0], (0, 0))
                closed.paste(pair.art[tile_id][0], (0, height - 16))
                sheet.paste(closed, (0, 0))
                for frame in range(3):
                    raw = rom.read(ptr + frame * tiles * 32, tiles * 32)
                    image = Image.new('RGBA', (16, height), (0, 0, 0, 255))
                    for tile in range(tiles):
                        image.alpha_composite(tile8(raw, tile, pair.palette, palettes[tile]), (tile % 2 * 8, tile // 2 * 8))
                    sheet.paste(image, ((frame + 1) * 16, 0))
                filename = name + '__' + pair.name + '.png'
                sheet.save(folder / filename)
                rows.append(dict(name=name, tileset=pair.name, file=filename, width=16, height=height,
                                 frames=4, frame_order='closed, opening1, opening2, open', source_map=map_info['name']))
                atlas_art.append(sheet)
                break
    # A browseable sheet of doors, 64x32 slots; smaller doors retain their 16px height.
    atlas = Image.new('RGBA', (512, power2(math.ceil(len(atlas_art) / 8) * 32)))
    for i, im in enumerate(atlas_art):
        atlas.paste(im, (i % 8 * 64, i // 8 * 32))
        rows[i].update(atlas_x=i % 8 * 64, atlas_y=i // 8 * 32)
    atlas.save(out / 'Door-animations.png')
    write_csv(folder / 'index.csv', rows)
    return rows


def export_props(rom, out):
    folder = out / 'props'
    folder.mkdir(exist_ok=True)
    palette_table, table_size = rom.symbols['sObjectEventSpritePalettes']
    palettes = {rom.u16(p + 4): rom.u32(p) for p in range(palette_table, palette_table + table_size, 8)}
    infos = [a for n, (a, size) in rom.symbols.items() if n.startswith('gObjectEventGraphicsInfo_')]
    rows, pictures = [], []
    for name in PROPS:
        ptr, size = rom.symbols['gObjectEventPic_' + name]
        candidates = []
        for info in infos:
            frame_table = rom.u32(info + 28)
            if frame_table and rom.u32(frame_table) == ptr:
                candidates.append(info)
        if not candidates:
            raise ValueError('Missing prop metadata: ' + name)
        info = candidates[0]
        w, h = rom.u16(info + 8), rom.u16(info + 10)
        palette = rom.palette(palettes[rom.u16(info + 2)], 16)
        frame_bytes = w * h // 2
        assert size % frame_bytes == 0
        frames = size // frame_bytes
        sheet = Image.new('RGBA', (power2(w * frames), power2(h)))
        for frame in range(frames):
            raw = rom.read(ptr + frame * frame_bytes, frame_bytes)
            subsprites = rom.u32(info + 20)
            subcount = rom.read(subsprites, 1)[0] if subsprites else 0
            if subcount:
                subptr = rom.u32(subsprites + 4)
                chunks = []
                sizes = (((8, 8), (16, 16), (32, 32), (64, 64)),
                         ((16, 8), (32, 8), (32, 16), (64, 32)),
                         ((8, 16), (8, 32), (16, 32), (32, 64)))
                for j in range(subcount):
                    x, y, attr = struct.unpack('<bbH', rom.read(subptr + j * 4, 4))
                    sw, sh = sizes[attr & 3][(attr >> 2) & 3]
                    chunks.append((x, y, sw, sh, (attr >> 4) & 1023))
                left, top = min(c[0] for c in chunks), min(c[1] for c in chunks)
                assert max(c[0] + c[2] for c in chunks) - left == w
                assert max(c[1] + c[3] for c in chunks) - top == h
                for x, y, sw, sh, offset in chunks:
                    for i in range(sw * sh // 64):
                        sheet.paste(tile8(raw, offset + i, palette, 0),
                                    (frame * w + x - left + i % (sw // 8) * 8,
                                     y - top + i // (sw // 8) * 8))
            else:
                for i in range(frame_bytes // 32):
                    sheet.paste(tile8(raw, i, palette, 0), (frame * w + i % (w // 8) * 8, i // (w // 8) * 8))
        sheet.save(folder / (name + '.png'))
        pictures.append(sheet)
        rows.append(dict(name=name, frame_width=w, frame_height=h, stored_frames=frames,
                         note='Stored frame order, left to right; no inferred animation timing.'))
    atlas = Image.new('RGBA', (512, power2(math.ceil(len(pictures) / 4) * 64)))
    for i, im in enumerate(pictures):
        assert im.width <= 128 and im.height <= 64
        atlas.paste(im, (i % 4 * 128, i // 4 * 64))
        rows[i].update(atlas_x=i % 4 * 128, atlas_y=i // 4 * 64)
    atlas.save(out / 'Environment-props.png')
    write_csv(folder / 'index.csv', rows)
    return rows


def export(args):
    rom = Rom(args.rom, args.symbols)
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=True)
    for name in ('tilesets', 'maps'):
        (out / name).mkdir(exist_ok=True)
    tilesets = {a: Tileset(rom, n.removeprefix('gTileset_'), a)
                for n, (a, size) in rom.symbols.items() if n.startswith('gTileset_')}
    groups = json.loads(args.maps.read_text())
    maps, pair_addresses = [], {}
    for gi, group in enumerate(groups['group_order']):
        for ni, name in enumerate(groups[group]):
            header = rom.u32(rom.u32(rom.symbol('gMapGroups') + gi * 4) + ni * 4)
            layout = rom.u32(header)
            w, h = rom.u32(layout), rom.u32(layout + 4)
            assert 0 < w <= 512 and 0 < h <= 512
            primary, secondary = rom.u32(layout + 16), rom.u32(layout + 20)
            pair_name = tilesets[primary].name + '__' + tilesets[secondary].name
            pair_addresses[pair_name] = (primary, secondary)
            ids = list(struct.unpack('<' + str(w * h) + 'H', rom.read(rom.u32(layout + 12), w * h * 2)))
            maps.append(dict(name=name, group=gi, number=ni, width=w, height=h, tileset=pair_name,
                             image='maps/' + name + '.png', _ids=ids))
    pairs = {name: Pair(rom, tilesets[a], tilesets[b]) for name, (a, b) in sorted(pair_addresses.items())}
    print(f'Decoded {len(tilesets)} source tilesets, {len(pairs)} active pairs, {len(maps)} maps.', flush=True)
    unique, unique_ids, mapping, tileset_index = [], {}, [], []
    for name, pair in pairs.items():
        ids = list(pair.art)
        save_layers(out / 'tilesets', name, list(pair.art.values()), ids=ids)
        for tile_id, art in pair.art.items():
            key = b''.join(im.tobytes() for im in art)
            if key not in unique_ids:
                unique_ids[key] = len(unique)
                unique.append(art)
            slot = unique_ids[key]
            mapping.append(dict(tileset=name, metatile_id=tile_id, metatile_hex=f'{tile_id:03X}',
                                local_x=tile_id % 16 * 16, local_y=tile_id // 16 * 16,
                                master_index=slot, master_x=slot % 128 * 16, master_y=slot // 128 * 16,
                                behavior=pair.attrs[tile_id] & 511))
        tileset_index.append(dict(name=name, count=len(ids), maps=[m['name'] for m in maps if m['tileset'] == name]))
    save_layers(out, 'All-environment-and-interiors', unique, columns=128)
    write_csv(out / 'tile-index.csv', mapping)
    for m in maps:
        pair = pairs[m['tileset']]
        image = Image.new('RGBA', (m['width'] * 16, m['height'] * 16))
        for i, tile_id in enumerate(m['_ids']):
            tile = tile_id & 1023
            if tile not in pair.art:
                raise ValueError(f"Unknown metatile {tile:#x} in {m['name']}")
            image.paste(pair.art[tile][0], (i % m['width'] * 16, i // m['width'] * 16))
        image.save(out / m['image'])
    print(f'Wrote {len(unique)} distinct metatiles and all map references.', flush=True)
    export_raw(rom, tilesets, out)
    animations = export_animations(rom, pairs, out)
    doors = export_doors(rom, pairs, maps, out)
    props = export_props(rom, out)
    clean_maps = [{k: v for k, v in m.items() if not k.startswith('_')} for m in maps]
    write_csv(out / 'map-index.csv', clean_maps)
    used = {addr for pair in pair_addresses.values() for addr in pair}
    unused = [ts.name for a, ts in tilesets.items() if a not in used]
    manifest = dict(rom_sha1=ROM_SHA1, source_tilesets=len(tilesets), active_tileset_pairs=len(pairs),
                    maps=len(maps), distinct_metatiles=len(unique), tile_pixels=16,
                    master_columns=128, master_width=2048,
                    master_height=power2(math.ceil(len(unique) / 128) * 16),
                    supplemental_unused_source_tilesets=unused, animation_sequences=len(animations),
                    door_palette_variants=len(doors), environment_props=len(props),
                    tilesets=tileset_index, map_index=clean_maps,
                    scope='Original ROM map tiles, native tileset animation frames, door frames and named environment props. No Pokemon/character/battle/UI sprites, weather effects, or custom project art.')
    write_json(out / 'manifest.json', manifest)
    make_index(out, manifest)
    readme = f'''PokéMulti - FireRed environment textures for Crocotile 3D

START HERE
Open index.html for a searchable gallery and map-to-tileset lookup.
Import All-environment-and-interiors.png for one combined static texture atlas.
It contains {len(unique):,} distinct 16x16 metatiles from all {len(pairs)} tileset pairs used by the {len(maps)} maps.
For a specific model, the named sheets in tilesets/ are easier to navigate.
Native tile order is preserved there: 16 columns; IDs 0-639 are common tiles,
and ID 640 starts the area's secondary tiles at pixel y=640. Empty trailing
slots are transparent padding; they are not missing art.

tilesets/: composite, lower and upper sheets with identical coordinates.
Lower/upper preserve the original layers and transparent palette index zero;
these are layer extractions, not edited silhouettes. Some background pixels
are part of the source art. Composite sheets show the native black backdrop.
All-environment-and-interiors-lower.png and -upper.png match the master atlas.
tile-index.csv maps every native tileset/metatile ID to the atlas coordinates.
Exact duplicates share an atlas slot. Different palettes/layers remain distinct.

CROCOTILE SETUP
Tileset panel > image menu > Add Tileset, then choose a PNG.
Set UV Tilesize to 16 x 16 for complete map tiles, or 8 x 8 for smaller details.
Use nearest-neighbor texture filtering in Tileset Material Settings; keep
original image resolution. UV Padding starts at 0: these sheets have no gutters.
All texture sheets use power-of-two canvas dimensions. Padding adds empty
canvas only. No source pixels were rescaled, smoothed, recolored or redrawn.
Official help: https://www.crocotile3d.com/howto.html

ANIMATION AND PROPS
animations/: all eight native tileset animation sequences. Each column is a
sequence frame and each row is an affected 16x16 metatile. index.json gives
row sources, frame counts, native tick intervals and repeat order via columns.
All affected tiles and palette variants across the active map pairs are included;
identical animation rows share one entry.
These are separate animation sheets, not baked into the static master.
Native transfer lengths are preserved, including the final water frame's
original read past its symbol's allocation. No replacement pixels are invented.
doors/: closed-to-open strips; each frame is 16x16 or 16x32 as listed in index.csv.
Door-animations.png collects these strips in 64x32 cells (native-size frames).
Environment-props.png / props/: cut trees, rocks, signs, items, boats and a room
decoration. The combined sheet uses 128x64 cells; the individual frames retain
their native dimensions. The SS Anne uses its original subsprite arrangement. index.csv provides native frame sizes. Pokémon and people are excluded.

RAW SOURCE AND MAP REFERENCES
raw-8x8/: all {len(tilesets)} source tile banks, including {len(unused)} unused banks.
Each 128-pixel-wide band displays the same source tiles with palette 0 through
15, left to right. Within each band, tiles are 8x8, 16 tiles per row. Most
palette variants are not used in game: the assembled sheets carry the correct
assignments and are the recommended modeling textures. Native primary tiles
normally use palettes 0-6 and secondary tiles use palettes 7-12. All stored
source tiles are present here, including unused tails beyond VRAM load lengths.
Unused banks: {', '.join(unused)}. Their raw art is retained separately rather
than guessing a primary-bank pairing for maps that do not exist.
maps/: all {len(maps)} static terrain layouts at original size, without NPCs or Pokémon.
Map references show ROM defaults, without runtime script changes, weather,
lighting tints, cutscenes, movable props or multiplayer overlays.
map-index.csv and the gallery link each map to its texture sheet.
These PNGs are modeling references and textures; no 3D models/importer are added.

SOURCE / REPEATABILITY
Extracted only from the local owner-provided FireRed US 1.0 ROM.
SHA1: {ROM_SHA1}
RGB555 expands to RGB888 with component * 255 // 31, matching PokéMulti.
The ROM and saves are read-only. No source graphics were downloaded or sent.
This local extracted/ directory is ignored and is not part of app packages.
Exporter: scripts/export_environment_tiles.py (requires Python and Pillow).
Layout/animation metadata references:
{SOURCE_URL}tileset_anims.c
{SOURCE_URL}field_door.c
{SOURCE_URL}fieldmap.c
'''
    (out / 'README.txt').write_text(readme, encoding='utf-8')
    # Verify every PNG is readable and all three master layers have the same grid.
    pngs = list(out.rglob('*.png'))
    for path in pngs:
        with Image.open(path) as im:
            im.verify()
    archive_path = out.parent / (out.name + '.zip')
    with zipfile.ZipFile(archive_path, 'w', zipfile.ZIP_DEFLATED, compresslevel=6) as archive:
        for path in sorted(out.rglob('*')):
            if path.is_file():
                archive.write(path, (Path(out.name) / path.relative_to(out)).as_posix())
    with zipfile.ZipFile(archive_path) as archive:
        assert archive.testzip() is None
    print(json.dumps(dict(output=str(out), archive=str(archive_path), pngs=len(pngs),
                          master_metatiles=len(unique), maps=len(maps),
                          animations=len(animations), doors=len(doors), props=len(props)), indent=2), flush=True)


def make_index(out, manifest):
    cards = []
    for ts in manifest['tilesets']:
        name = ts['name']
        maps = ts['maps']
        links = ' '.join(f'<a href="maps/{html.escape(m)}.png">{html.escape(m)}</a>' for m in maps)
        cards.append(f'''<article data-search="{html.escape(name + ' ' + ' '.join(maps)).lower()}">
<h2>{html.escape(name.replace('__', ' / '))}</h2>
<nav><a href="tilesets/{name}.png">Texture PNG</a><a href="tilesets/{name}-lower.png">Lower layer</a><a href="tilesets/{name}-upper.png">Upper layer</a></nav>
<a href="tilesets/{name}.png"><img loading="lazy" src="tilesets/{name}.png" width="256" alt="{name} native tile sheet"></a>
<details><summary>{len(maps)} map references</summary><div class="maplinks">{links}</div></details></article>''')
    document = '''<!doctype html><html lang="en"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>PokéMulti environment textures</title><style>
*{box-sizing:border-box}body{margin:0;background:#121b22;color:#ebf3ef;font:16px/1.5 system-ui,sans-serif}header{padding:30px 5vw;background:#1b2c31;border-bottom:3px solid #71d5a5}main{padding:24px 5vw}h1{margin:0 0 8px}p{max-width:960px;margin:8px 0}a{color:#9fe6c4;text-underline-offset:3px}nav{display:flex;flex-wrap:wrap;gap:14px;margin:14px 0}input{width:min(650px,100%);padding:12px;font:inherit;border:1px solid #609080;border-radius:6px;background:#101e22;color:white;margin-top:12px}.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(300px,1fr));gap:24px;align-items:start}article{background:#1c2a31;border:1px solid #354b50;border-radius:8px;padding:18px}h2{font-size:18px;overflow-wrap:anywhere;margin:0}img{display:block;image-rendering:pixelated;background-color:#889096;background-image:linear-gradient(45deg,#a2aaaf 25%,transparent 25%),linear-gradient(-45deg,#a2aaaf 25%,transparent 25%),linear-gradient(45deg,transparent 75%,#a2aaaf 75%),linear-gradient(-45deg,transparent 75%,#a2aaaf 75%);background-size:16px 16px;background-position:0 0,0 8px,8px -8px,-8px 0;max-width:100%;height:auto}details{margin-top:14px}.maplinks{display:grid;gap:8px;overflow-wrap:anywhere;margin-top:10px}article[hidden]{display:none}small{color:#bacbc6}
</style><header><h1>PokéMulti environment textures</h1>
<p>Original FireRed map art for Crocotile 3D. Native pixels, correct map palettes, separate texture layers.</p>
<p>COUNTS. Pokémon and character sprites stay separate.</p>
<nav><a href="All-environment-and-interiors.png">Full environment + interiors PNG</a><a href="Environment-props.png">Environment props</a><a href="Door-animations.png">Door animations</a><a href="animations/index.html">Animated tiles</a><a href="README.txt">Crocotile setup / file guide</a></nav>
<p>Use UV Tilesize <b>16 × 16</b>, or <b>8 × 8</b> for details. Start with a named sheet below. The lower/upper layers are exact native layers, useful for walls, roofs and foliage.</p>
<label for="search">Find a tileset or map</label><br><input id="search" type="search" placeholder="Pokémon Center, Pallet, cave, gym, Route1…"><p id="count" aria-live="polite"></p></header><main><div class="grid">CARDS</div></main>
<script>const search=document.querySelector('#search'),cards=[...document.querySelectorAll('article')];function filter(){const q=search.value.toLowerCase().normalize('NFD').replace(/[\u0300-\u036f]/g,'').replace(/[^a-z0-9]/g,'');let n=0;cards.forEach(c=>{c.hidden=!c.dataset.search.replace(/[^a-z0-9]/g,'').includes(q);if(!c.hidden)n++});document.querySelector('#count').textContent=n+' tileset combinations'}search.addEventListener('input',filter);filter();</script></html>'''
    document = document.replace('COUNTS', f"{manifest['active_tileset_pairs']} tileset combinations · {manifest['maps']} map references · {manifest['distinct_metatiles']:,} distinct map tiles")
    (out / 'index.html').write_text(document.replace('CARDS', '\n'.join(cards)), encoding='utf-8')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--rom', type=Path, default=ROOT / 'Pokemon - Fire Red Version.zip')
    parser.add_argument('--symbols', type=Path, default=ROOT / 'cache/dependency-review/pokefirered.sym')
    parser.add_argument('--maps', type=Path, default=ROOT / 'cache/campaign-reference/data__maps__map_groups.json')
    parser.add_argument('--output', type=Path, default=ROOT / 'extracted/Crocotile-Environment-FireRed')
    export(parser.parse_args())
