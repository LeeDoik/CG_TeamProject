"""Rebuild game PNGs and review images; requires Pillow. Originals are read-only."""
import argparse
import hashlib
import json
from pathlib import Path
from PIL import Image, ImageDraw

parser = argparse.ArgumentParser()
parser.add_argument('--source', type=Path, required=True)
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
dest = root / 'src/Default_Project/Assets/Characters/PaperChild'
preview = root / 'art_preview/sprite_resize'
assert args.source.resolve() != dest.resolve(), 'Never overwrite originals'
dest.mkdir(parents=True, exist_ok=True)
preview.mkdir(parents=True, exist_ok=True)
digest = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
files = sorted(args.source.glob('paper_child_*_right.png'))
assert len(files) == 9
rows, frames = [], {}
for source in files:
    before = digest(source)
    with Image.open(source) as im:
        assert im.mode == 'RGBA' and im.size == (1086, 1448)
        # Premultiplied alpha prevents dark/colored halos during filtering.
        small = im.convert('RGBa').resize((372, 496), Image.Resampling.LANCZOS).convert('RGBA')
    out = Image.new('RGBA', (384, 512))
    out.paste(small, (6, 8))
    path = dest / source.name
    out.save(path, optimize=True)
    with Image.open(path) as check:
        assert check.mode == 'RGBA' and check.size == (384, 512)
        assert check.getchannel('A').getextrema() == (0, 255)
        assert check.getbbox()[0] >= 6 and check.getbbox()[2] <= 378
    assert digest(source) == before
    rows.append(dict(file=source.name, source_size=[1086, 1448], output_size=[384, 512],
                     source_sha256=before, output_sha256=digest(path), alpha_bbox=out.getbbox()))
    frames[source.stem] = out
manifest = dict(format='RGBA8 PNG, straight alpha', canvas=[384, 512],
                resample='premultiplied-alpha Lanczos', content_rectangle=[6, 8, 372, 496],
                pivot_source_top_left=[530, 1389],
                pivot_output_top_left=[6 + 530 * 372 / 1086, 8 + 1389 * 496 / 1448],
                generated_images=0, resized_images=9, frames=rows)
(dest / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')
sheet = Image.new('RGB', (384 * 3, 544 * 3), '#dadada')
draw = ImageDraw.Draw(sheet)
for i, (name, frame) in enumerate(frames.items()):
    x, y = (i % 3) * 384, (i // 3) * 544
    for cy in range(0, 512, 16):
        for cx in range(0, 384, 16):
            draw.rectangle((x+cx, y+cy, x+cx+15, y+cy+15), fill='#b3b3b3' if (cx//16+cy//16)%2 else '#eeeeee')
    sheet.paste(frame, (x, y), frame)
    draw.text((x+8, y+520), name.replace('paper_child_', ''), fill='black')
sheet.save(preview / 'contact_sheet.png')
# RGB animated WebP is a review artifact, never a game texture.
walk = []
for i in range(1, 5):
    bg = Image.new('RGBA', (384, 512), '#909898')
    bg.alpha_composite(frames[f'paper_child_walk_{i:02}_right'])
    walk.append(bg.convert('RGB'))
walk[0].save(preview / 'walk_8fps.webp', save_all=True, append_images=walk[1:], duration=125, loop=0, lossless=True)
print(f'Validated {len(rows)} RGBA PNGs; original hashes unchanged.')
