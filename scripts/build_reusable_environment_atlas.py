"""Build a curated, reusable Crocotile texture kit from the local ROM export.

This is an editorial selection of model/material families, not a dump of map
metatiles. Crops and packing preserve native pixels. No art is redrawn.
"""
from pathlib import Path
import csv
import hashlib
import html
import json
import math
import zipfile
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'extracted/Crocotile-Environment-FireRed'
OUT = ROOT / 'extracted/Crocotile-Reusable-Kit'
entries = []


def add(name, category, family, image, source, note=''):
    entries.append(dict(name=name, category=category, model_family=family,
                        image=image.convert('RGBA'), source=source, note=note))


def crop(name, category, family, filename, box, note=''):
    with Image.open(SOURCE / filename) as im:
        assert 0 <= box[0] < box[2] <= im.width and 0 <= box[1] < box[3] <= im.height
        picture = im.crop(box)
    add(name, category, family, picture, dict(file=filename, crop=list(box)), note)


def tile(name, category, family, tileset, ids, columns=1, layer='', note=''):
    filename = 'tilesets/' + tileset + ('-' + layer if layer else '') + '.png'
    with Image.open(SOURCE / filename) as im:
        picture = Image.new('RGBA', (columns * 16, math.ceil(len(ids) / columns) * 16))
        for j, tile_id in enumerate(ids):
            x, y = tile_id % 16 * 16, tile_id // 16 * 16
            picture.paste(im.crop((x, y, x + 16, y + 16)), (j % columns * 16, j // columns * 16))
    add(name, category, family, picture, dict(file=filename, metatiles=ids, columns=columns), note)


def place(name, category, family, map_name, x, y, w, h, note=''):
    crop(name, category, family, 'maps/' + map_name + '.png', (x, y, x + w, y + h), note)


def select():
    g = 'General__PalletTown'
    b = 'Building__GenericBuilding1'
    tile('Grass surface', 'Ground', 'Ground plane', g, [0x001], note='Repeat this surface. Grass placements do not need individual models.')
    tile('Sand surface', 'Ground', 'Ground plane', g, [0x0dc])
    tile('Dirt surface', 'Ground', 'Ground plane', g, [0x0d9])
    place('Paved path surface', 'Ground', 'Ground plane', 'ViridianCity', 352, 240, 16, 16)
    place('Water surface', 'Ground', 'Ground plane', 'ViridianCity', 240, 432, 16, 16, 'One surface example; animate the material separately if desired.')
    tile('Cave floor', 'Ground', 'Ground plane', 'General__Cave', [0x280])
    tile('Stone floor', 'Ground', 'Ground plane', b, [0x078])
    tile('Wood floor', 'Ground', 'Ground plane', b, [0x001])
    tile('Carpet', 'Ground', 'Ground plane', b, [0x054])
    place('Checkered floor', 'Ground', 'Ground plane', 'CeladonCity_GameCorner', 160, 192, 16, 16)
    place('Office floor', 'Ground', 'Ground plane', 'SilphCo_1F', 144, 272, 16, 16)
    place('Shop floor', 'Ground', 'Ground plane', 'ViridianCity_Mart', 80, 80, 16, 16)
    tile('Clinic floor', 'Ground', 'Ground plane', 'Building__PokemonCenter', [0x281])
    place('Pool tile', 'Ground', 'Ground plane', 'CeruleanCity_Gym', 32, 48, 16, 16)
    tile('Cliff face', 'Terrain shapes', 'Cliff segment', g, [0x083])
    tile('Cliff top', 'Terrain shapes', 'Cliff segment', g, [0x069])
    tile('Rocky cliff base', 'Terrain shapes', 'Cliff segment', g, [0x0b0])
    tile('Low ledge', 'Terrain shapes', 'Ledge segment', g, [0x0b2])
    tile('Cave wall', 'Terrain shapes', 'Cave wall segment', 'General__Cave', [0x293])
    tile('Cave wall top', 'Terrain shapes', 'Cave wall segment', 'General__Cave', [0x29b])
    tile('Outdoor rock steps', 'Terrain shapes', 'Stair segment', g, [0x088, 0x089], 2)
    tile('Cave ladder', 'Terrain shapes', 'Ladder', 'General__Cave', [0x28e, 0x2ae])
    tile('Tall grass clump', 'Plants', 'Tall grass clump', g, [0x00a], note='Reuse or instance this clump; no different mesh for each grass tile.')
    tile('Flower clump', 'Plants', 'Flower clump', g, [0x004], layer='upper')
    tile('Round bush', 'Plants', 'Bush', g, [0x005], layer='upper')
    tile('Tree', 'Plants', 'Tree', g, [0x00e, 0x00f, 0x01e, 0x01f, 0x026, 0x027], 2,
         note='One complete source tree. Repeat the finished model throughout the map.')
    tile('Picket fence', 'Outdoor props', 'Fence segment', g, [0x0e7], layer='upper')
    tile('Wood fence', 'Outdoor props', 'Fence segment', g, [0x0e6], layer='upper', note='Material/style variant of the same fence kit.')
    tile('Fence corner', 'Outdoor props', 'Fence segment', g, [0x0ec], layer='upper')
    tile('Town sign', 'Outdoor props', 'Signpost', g, [0x003], layer='upper')
    for name, source, w, h, family in [
        ('Cuttable tree', 'CutTree', 16, 16, 'Cuttable tree'),
        ('Breakable rock', 'RockSmashRock', 16, 16, 'Rock'),
        ('Boulder', 'StrengthBoulder', 16, 16, 'Rock'),
        ('Gym sign', 'GymSign', 16, 32, 'Signpost'),
        ('Small sign', 'Sign', 16, 16, 'Signpost'),
        ('Wooden sign', 'WoodenSign', 16, 16, 'Signpost'),
        ('Ferry', 'Seagallop', 64, 64, 'Boat'),
        ('Ship', 'SSAnne', 128, 64, 'Boat')]:
        crop(name, 'Outdoor props', family, 'props/' + source + '.png', (0, 0, w, h), 'One example; stored duplicate/destruction frames are omitted.')
    # One reference per building form. Roof, wall, window and door areas in these
    # references are directly selectable with Crocotile's 8px or 16px UV grid.
    for name, map_name, x, y, w, h, family in [
        ('House', 'PalletTown', 5, 3, 5, 5, 'Small building shell'),
        ('Pokemon Center', 'ViridianCity', 24, 22, 5, 5, 'Clinic shell'),
        ('Poke Mart', 'ViridianCity', 34, 16, 4, 4, 'Shop shell'),
        ('Gym', 'ViridianCity', 33, 6, 6, 5, 'Large building shell'),
        ('Laboratory', 'PalletTown', 13, 9, 7, 5, 'Large building shell'),
        ('Apartment', 'CeladonCity', 4, 4, 5, 8, 'Stackable building shell'),
        ('Gatehouse', 'SaffronCity', 32, 0, 6, 6, 'Gatehouse shell')]:
        place(name, 'Building kits', family, map_name, x * 16, y * 16, w * 16, h * 16,
              'Representative source building. Reuse roof, wall and window modules; width, height and color variations are not separate required models.')
    tile('Red roof material', 'Building materials', 'Roof module', g, [0x049])
    tile('Blue roof material', 'Building materials', 'Roof module', g, [0x02a])
    place('Green roof material', 'Building materials', 'Roof module', 'ViridianCity', 400, 144, 16, 16)
    place('Brown roof material', 'Building materials', 'Roof module', 'ViridianCity', 560, 112, 16, 16)
    tile('Brick wall material', 'Building materials', 'Wall module', g, [0x05d])
    tile('Metal wall material', 'Building materials', 'Wall module', g, [0x046])
    tile('Window pair', 'Building materials', 'Window module', g, [0x006, 0x007], 2)
    tile('Door and frame', 'Building materials', 'Door module', g, [0x03d],
         note='Native door texture; also select door and frame areas from the building references.')
    # Interior material families and furnishings, each represented only once.
    place('Wall and window', 'Interior kit', 'Interior wall module', 'PalletTown_PlayersHouse_1F', 112, 0, 32, 32)
    place('Interior stairs', 'Interior kit', 'Stair segment', 'PalletTown_PlayersHouse_2F', 128, 24, 48, 40)
    place('Interior doorway', 'Interior kit', 'Door module', 'PalletTown_PlayersHouse_1F', 56, 128, 32, 16)
    place('Bookshelf', 'Furniture', 'Bookcase', 'PalletTown_ProfessorOaksLab', 0, 120, 32, 32)
    place('Bed', 'Furniture', 'Bed', 'PalletTown_PlayersHouse_2F', 24, 72, 32, 48)
    place('Dining table', 'Furniture', 'Table', 'PalletTown_PlayersHouse_1F', 96, 64, 32, 32)
    place('Chair', 'Furniture', 'Chair', 'PalletTown_PlayersHouse_1F', 80, 64, 16, 16)
    place('TV and console', 'Furniture', 'Monitor', 'PalletTown_PlayersHouse_2F', 96, 56, 16, 48)
    place('Computer desk', 'Furniture', 'Computer desk', 'PalletTown_PlayersHouse_2F', 16, 8, 40, 40)
    place('Kitchen sink', 'Furniture', 'Counter segment', 'PalletTown_PlayersHouse_1F', 16, 8, 32, 32)
    place('Potted plant', 'Furniture', 'Potted plant', 'PalletTown_PlayersHouse_1F', 16, 104, 16, 32)
    place('Large table', 'Furniture', 'Table', 'PalletTown_ProfessorOaksLab', 128, 64, 48, 32)
    place('Lab machine', 'Machines', 'Lab machine', 'PalletTown_ProfessorOaksLab', 16, 56, 32, 40)
    place('Lab workstation', 'Machines', 'Computer desk', 'PalletTown_ProfessorOaksLab', 32, 8, 32, 40)
    place('Clinic counter', 'Machines', 'Counter segment', 'ViridianCity_PokemonCenter_1F', 64, 40, 32, 24)
    place('Healing machine', 'Machines', 'Healing machine', 'ViridianCity_PokemonCenter_1F', 80, 16, 32, 32)
    place('Clinic computer', 'Machines', 'Computer terminal', 'ViridianCity_PokemonCenter_1F', 176, 8, 16, 32)
    place('Glass table', 'Furniture', 'Table', 'ViridianCity_PokemonCenter_1F', 176, 96, 32, 32)
    place('Shop display', 'Furniture', 'Display shelf', 'ViridianCity_Mart', 112, 56, 16, 64)
    place('Shop wall shelf', 'Furniture', 'Display shelf', 'ViridianCity_Mart', 112, 16, 48, 32)
    place('Shop counter', 'Furniture', 'Counter segment', 'ViridianCity_Mart', 8, 56, 40, 24)
    place('Slot machine', 'Machines', 'Slot machine', 'CeladonCity_GameCorner', 80, 80, 32, 48)
    place('Museum display', 'Furniture', 'Display case', 'PewterCity_Museum_1F', 48, 56, 64, 40)
    place('Museum pillar', 'Interior kit', 'Column', 'PewterCity_Museum_1F', 0, 0, 32, 48)
    place('Gym statue', 'Interior kit', 'Statue', 'ViridianCity_Gym', 232, 296, 32, 40)
    place('Gravestone', 'Interior kit', 'Gravestone', 'PokemonTower_3F', 96, 96, 16, 16)
    place('Office bench', 'Furniture', 'Bench', 'SilphCo_1F', 16, 192, 32, 32)
    place('Wall poster', 'Interior kit', 'Wall decoration', 'PalletTown_PlayersHouse_2F', 176, 8, 16, 16)


def pack(items, width=512):
    skyline = [0] * (width // 8)
    for entry in sorted(items, key=lambda e: (-e['image'].height, -e['image'].width)):
        w, h = entry['image'].size
        cells = math.ceil((w + 8) / 8)
        x = min(range(len(skyline) - cells + 1), key=lambda i: (max(skyline[i:i + cells]), i))
        y = max(skyline[x:x + cells])
        entry.update(x=x * 8, y=y, width=w, height=h)
        skyline[x:x + cells] = [y + h + 8] * cells
    height = 1 << (max(skyline) - 1).bit_length()
    atlas = Image.new('RGBA', (width, height))
    for entry in items:
        atlas.paste(entry['image'], (entry['x'], entry['y']))
    return atlas


def build():
    select()
    OUT.mkdir(parents=True, exist_ok=True)
    # Repeated materials share a texture region, regardless of where used.
    unique = {}
    for entry in entries:
        assert entry['image'].getchannel('A').getbbox(), 'Empty region: ' + entry['name']
        key = (entry['image'].size, entry['image'].tobytes())
        if key in unique:
            unique[key].setdefault('also_used_as', []).append(entry['name'])
        else:
            unique[key] = entry
    items = list(unique.values())
    atlas = pack(items)
    atlas_path = OUT / 'Reusable-Environment-Atlas.png'
    atlas.save(atlas_path)
    rows = []
    for i, entry in enumerate(items, 1):
        row = {k: v for k, v in entry.items() if k != 'image'}
        row.update(id=i, pixel_sha256=hashlib.sha256(entry['image'].tobytes()).hexdigest())
        rows.append(row)
        assert atlas.crop((entry['x'], entry['y'], entry['x'] + entry['width'], entry['y'] + entry['height'])).tobytes() == entry['image'].tobytes()
    manifest = dict(purpose='Compact curated examples of reusable modeling and material families; not every map tile or every one-off landmark.',
                    atlas=atlas_path.name, width=atlas.width, height=atlas.height, native_pixel_scale=1,
                    recommended_uv_grid=8, entries=rows,
                    mesh_note='Entries are texture/example regions, not a count of required models. Model families and material variants can share geometry.')
    (OUT / 'atlas.json').write_text(json.dumps(manifest, indent=2), encoding='utf-8')
    with (OUT / 'model-guide.csv').open('w', newline='', encoding='utf-8-sig') as f:
        writer = csv.DictWriter(f, fieldnames=['id', 'name', 'category', 'model_family', 'x', 'y', 'width', 'height', 'note'])
        writer.writeheader()
        writer.writerows({key: row[key] for key in writer.fieldnames} for row in rows)
    # Labels live on the guide, never over the importable texture pixels.
    background = Image.new('RGBA', atlas.size, '#303b42')
    checker = ImageDraw.Draw(background)
    for y in range(0, atlas.height, 8):
        for x in range(0, atlas.width, 8):
            if (x // 8 + y // 8) % 2:
                checker.rectangle((x, y, x + 7, y + 7), fill='#39464e')
    background.alpha_composite(atlas)
    guide = background.resize((atlas.width * 2, atlas.height * 2), Image.Resampling.NEAREST).convert('RGB')
    d = ImageDraw.Draw(guide)
    for row in rows:
        x, y, w, h = [row[k] * 2 for k in ('x', 'y', 'width', 'height')]
        d.rectangle((x, y, x + w, y + h), outline='#efbe54', width=1)
        d.text((x + 2, y + h + 1), str(row['id']), fill='#ffffff')
    guide.save(OUT / 'Numbered-Guide.png')
    cards = []
    for row in rows:
        w, h = row['width'], row['height']
        preview = f"width:{w * 2}px;height:{h * 2}px;background-image:url('Reusable-Environment-Atlas.png');background-size:{atlas.width * 2}px {atlas.height * 2}px;background-position:-{row['x'] * 2}px -{row['y'] * 2}px"
        cards.append(f'''<article data-category="{html.escape(row['category'])}"><h3>{row['id']}. {html.escape(row['name'])}</h3><div class="preview" style="{preview}"></div><p><b>{html.escape(row['model_family'])}</b><br>{html.escape(row['note'])}</p><small>UV pixels: {row['x']}, {row['y']} · {w} × {h}</small></article>''')
    categories = dict.fromkeys(row['category'] for row in rows)
    options = ''.join(f'<option>{html.escape(category)}</option>' for category in categories)
    page = '''<!doctype html><html lang="en"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>PokéMulti reusable modeling kit</title><style>
*{box-sizing:border-box}body{background:#152128;color:#edf6f0;font:16px/1.5 system-ui;margin:0}header,main{padding:26px 5vw}header{background:#21343c;border-bottom:3px solid #8ed2ae}h1{margin:0}p{max-width:950px}a{color:#a0e0bc}nav{display:flex;flex-wrap:wrap;gap:22px;margin:18px 0}.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(285px,1fr));gap:18px}article{background:#1f3038;padding:18px;border-radius:8px;border:1px solid #3a5158}h3{font-size:17px;margin:0 0 14px}.preview{image-rendering:pixelated;max-width:100%;background-color:#45545a;background-repeat:no-repeat}small{color:#bfd0c8}select{font:inherit;background:#17282f;color:white;padding:8px;border:1px solid #668c7b}article[hidden]{display:none}
</style><header><h1>Reusable environment modeling kit</h1><p>One repeatable grass surface, representative scenery and furniture, and modular building/material examples. These are reusable texture regions, not individual map placements.</p><nav><a href="Reusable-Environment-Atlas.png">Texture atlas PNG</a><a href="Numbered-Guide.png">Numbered guide</a><a href="model-guide.csv">Model families / UV coordinates</a><a href="README.txt">Using the kit</a></nav><p>Import only <b>Reusable-Environment-Atlas.png</b> into Crocotile. Start with an <b>8 × 8 UV grid</b>; full map tiles use 16 × 16. The original pixels are unchanged.</p><p><b>Reuse geometry:</b> grass and floors share a plane, roof colors share roof modules, repeated houses share a building shell, and every tree placement can instance one tree model. One-off landmarks can be added later.</p><label>Show <select id="category"><option>All categories</option>OPTIONS</select></label></header><main><div class="grid">CARDS</div></main><script>document.querySelector('select').onchange=e=>{document.querySelectorAll('article').forEach(a=>a.hidden=e.target.selectedIndex>0&&a.dataset.category!==e.target.value)}</script></html>'''
    (OUT / 'index.html').write_text(page.replace('OPTIONS', options).replace('CARDS', ''.join(cards)), encoding='utf-8')
    (OUT / 'README.txt').write_text(f'''PokéMulti reusable environment modeling kit

IMPORT: Reusable-Environment-Atlas.png ({atlas.width} x {atlas.height} pixels)
BROWSE: index.html or Numbered-Guide.png

This compact atlas selects {len(rows)} source-art regions as examples of reusable
model and material families. It is not a map dump or a requirement to create
{len(rows)} separate meshes. For example:
- Ground: one plane reused for grass, sand, water and floor materials.
- Trees, bushes and tall grass: reuse / instance the chosen model.
- Buildings: reuse roofs, wall panels, doors and windows. Change width, height
  and texture/material without making each placed building from scratch.
- Fences, cliffs and stairs: repeat segments and join them at corners.
- Furniture: one representative of each selected furnishing family.

In Crocotile use Add Tileset and import the atlas PNG. Start at UV Tilesize
8 x 8 for details, or 16 x 16 for whole map tiles. Keep nearest filtering.
Each region starts on an 8px boundary. There is an 8px empty gap between
regions, not between the native 8px/16px tiles inside a region. Coordinates
and model families are in model-guide.csv and atlas.json.

The large building regions are representative assembled source drawings so
you can take roof / wall / window / door textures from a coherent example.
They are not proposed billboard models. Build their volume in Crocotile.
Roof colors are material variants, not separate required geometry.

Every texture is copied from the owner's native ROM export at 1:1 resolution.
No pixels are resized, recolored, smoothed, repainted or AI-generated.
Native upper layers provide transparency where available; baked ground in
other source drawings remains original. Only the numbered guide/browser
previews are enlarged with nearest sampling. No Pokemon or player art is used.

This is a curated modeling kit, not an exhaustive collection of unique
landmarks, alternate palettes, decorative variants or animation frames.
The earlier complete extraction is available separately if a later model
needs a specific additional texture. No map images are included in this kit.

Exporter: scripts/build_reusable_environment_atlas.py
Crocotile reference: https://www.crocotile3d.com/howto.html
''', encoding='utf-8')
    for path in OUT.glob('*.png'):
        with Image.open(path) as im:
            im.verify()
    archive_path = OUT.parent / (OUT.name + '.zip')
    with zipfile.ZipFile(archive_path, 'w', zipfile.ZIP_DEFLATED) as archive:
        for path in sorted(OUT.iterdir()):
            if path.is_file():
                archive.write(path, (Path(OUT.name) / path.name).as_posix())
    with zipfile.ZipFile(archive_path) as archive:
        assert archive.testzip() is None
    print(json.dumps(dict(atlas=str(atlas_path), size=atlas.size, texture_regions=len(rows),
                          categories=len(categories), pack=str(archive_path)), indent=2))


if __name__ == '__main__':
    build()
