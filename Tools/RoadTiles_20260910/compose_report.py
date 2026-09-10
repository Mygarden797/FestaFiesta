"""Arrange QA image copies using Pillow. Never modifies input/reference images."""
import argparse
import json
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont, ImageOps


ORDER = ('Sand', 'Asphalt', 'Wood', 'CreamTile', 'MintHerringbone', 'ConcreteDrain')
VIEWS = ('front', 'back', 'left', 'right', 'top', 'bottom', 'game')
BACKGROUND = '#edf4f1'
INK = '#153b38'


def _font(size):
    for candidate in ('C:/Windows/Fonts/segoeui.ttf', 'C:/Windows/Fonts/arial.ttf',
                      '/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf'):
        if Path(candidate).exists():
            return ImageFont.truetype(candidate, size)
    return ImageFont.load_default()


def _copy_image(canvas, path, rect):
    """Letterbox a copy; retain the complete source frame, never crop."""
    x, y, width, height = rect
    with Image.open(path) as source:
        thumb = ImageOps.contain(source.convert('RGB'), (width, height), Image.Resampling.LANCZOS)
    canvas.paste(thumb, (x + (width - thumb.width) // 2, y + (height - thumb.height) // 2))


def comparison(reference, render, outpath):
    canvas = Image.new('RGB', (2048, 820), BACKGROUND)
    draw = ImageDraw.Draw(canvas)
    draw.text((32, 20), 'ROAD TILES / Reference and actual Blender geometry', font=_font(32), fill=INK)
    draw.text((32, 77), 'Supplied reference: Road.png', font=_font(25), fill=INK)
    draw.text((1040, 77), 'Actual model render / Cycles / orthographic', font=_font(25), fill=INK)
    _copy_image(canvas, reference, (24, 125, 1000, 670))
    _copy_image(canvas, render, (1024, 125, 1000, 670))
    outpath = Path(outpath)
    outpath.parent.mkdir(parents=True, exist_ok=True)
    canvas.save(outpath)
    return str(outpath)


def view_sheet(asset, indir, outpath):
    indir, outpath = Path(indir), Path(outpath)
    cell, top, margin = 320, 100, 24
    canvas = Image.new('RGB', (4 * cell + 2 * margin, 2 * (cell + 36) + top + margin), BACKGROUND)
    draw = ImageDraw.Draw(canvas)
    draw.text((margin, 20), asset + ' / actual Blender mesh views', font=_font(30), fill=INK)
    draw.text((margin, 60), 'Source forward +Y; bottom has independent underside lighting.', font=_font(19), fill=INK)
    used = []
    for i, label in enumerate(VIEWS):
        path = indir / ('Blender_' + label + '.png')
        x, y = margin + i % 4 * cell, top + i // 4 * (cell + 36)
        draw.text((x + 8, y), label.upper(), font=_font(20), fill=INK)
        if path.exists():
            _copy_image(canvas, path, (x, y + 30, cell - 8, cell - 8))
            used.append(str(path))
        else:
            draw.text((x + 8, y + 70), 'Not rendered', font=_font(19), fill='#a03030')
    outpath.parent.mkdir(parents=True, exist_ok=True)
    canvas.save(outpath)
    return {'path': str(outpath), 'sources': used, 'complete': len(used) == len(VIEWS)}


def lod_sheet(assets, qa_root, outpath):
    qa_root, outpath = Path(qa_root), Path(outpath)
    size, label_width, header = 320, 225, 85
    canvas = Image.new('RGB', (label_width + 3 * size + 30, header + len(assets) * size + 24), BACKGROUND)
    draw = ImageDraw.Draw(canvas)
    draw.text((24, 15), 'ROAD TILES / Export LOD geometry comparison', font=_font(28), fill=INK)
    for i in range(3):
        draw.text((label_width + i * size + 12, 55), 'LOD' + str(i), font=_font(22), fill=INK)
    sources = []
    for row, asset in enumerate(assets):
        y = header + row * size
        draw.text((24, y + size // 2), asset, font=_font(21), fill=INK)
        for level in range(3):
            candidates = [qa_root / asset / ('Blender_LOD' + str(level) + '_game.png'),
                          qa_root / asset / 'qa' / ('Blender_LOD' + str(level) + '_game.png')]
            path = next((p for p in candidates if p.exists()), None)
            if path:
                _copy_image(canvas, path, (label_width + level * size, y, size - 8, size - 8))
                sources.append(str(path))
            else:
                draw.text((label_width + level * size + 15, y + 80), 'Not rendered', font=_font(20), fill='#a03030')
    outpath.parent.mkdir(parents=True, exist_ok=True)
    canvas.save(outpath)
    return {'path': str(outpath), 'sources': sources, 'complete': len(sources) == 3 * len(assets)}


def compose_all(root, reference=None, sheet=None):
    root = Path(root)
    reference = Path(reference) if reference else root / 'References' / 'Road.png'
    sheet = Path(sheet) if sheet else root / 'qa' / 'Road_Six_Tile_Sheet.png'
    qa = root / 'qa'
    result = {'reference_comparison': None, 'view_sheets': [], 'lod_sheet': None}
    if reference.exists() and sheet.exists():
        result['reference_comparison'] = comparison(reference, sheet, qa / 'Road_Reference_Comparison.png')
    assets = []
    for short in ORDER:
        candidates = [qa / short] + sorted(qa.glob('*' + short + '*'))
        indir = next((p for p in candidates if p.is_dir()), qa / short)
        result['view_sheets'].append(view_sheet(short, indir, qa / (short + '_Views.png')))
        assets.append(indir.name)
    result['lod_sheet'] = lod_sheet(assets, qa, qa / 'Road_LOD_Comparison.png')
    (qa / 'composition_manifest.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('root', help='SourceArt/RoadTiles_20260910 directory')
    parser.add_argument('--reference')
    parser.add_argument('--sheet')
    args = parser.parse_args()
    print(json.dumps(compose_all(args.root, args.reference, args.sheet), indent=2))
