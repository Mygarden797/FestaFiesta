# compose_unreal.py
# 역할: 도로·타일 제작 원본과 납품 작업에서 검증 이미지와 정보를 비교 가능한 형태로 묶는다.

"""Arrange unmodified UE GPU captures into labeled evidence sheets."""
import json
from pathlib import Path
import shutil

from PIL import Image, ImageDraw, ImageFont, ImageOps

TOOLS = Path(__file__).resolve().parent
SOURCE_QA = TOOLS.parent.parent / 'SourceArt/RoadTiles_20260910/qa'
OUT = TOOLS / 'qa'
RENDERS = OUT / 'unreal_renders'
NAMES = ['Sand', 'Asphalt', 'Wood', 'CreamTile', 'MintHerringbone', 'ConcreteDrain']


def font(size):
    return ImageFont.truetype('C:/Windows/Fonts/segoeui.ttf', size)


def paste(canvas, source, box):
    with Image.open(source) as image:
        thumbnail = ImageOps.contain(image.convert('RGB'), (box[2], box[3]), Image.Resampling.LANCZOS)
    canvas.paste(thumbnail, (box[0] + (box[2] - thumbnail.width) // 2,
                             box[1] + (box[3] - thumbnail.height) // 2))


def main():
    report = json.loads((OUT / 'unreal_capture_report.json').read_text(encoding='utf-8'))
    if report['status'] != 'PASS':
        raise RuntimeError('Capture must pass before composing evidence')
    cell, label, header = 360, 210, 105
    sheet = Image.new('RGB', (label + cell * 3 + 25, header + cell * 6 + 20), '#edf4f1')
    draw = ImageDraw.Draw(sheet)
    draw.text((24, 18), 'ROAD TILES / Actual Unreal Engine 5.7.4 materials and LODs', font=font(28), fill='#153b38')
    for lod in range(3):
        draw.text((label + cell * lod + 12, 66), 'LOD' + str(lod), font=font(23), fill='#153b38')
    for row, name in enumerate(NAMES):
        draw.text((24, header + row * cell + cell // 2), name, font=font(22), fill='#153b38')
        for lod in range(3):
            paste(sheet, RENDERS / ('SM_Road_' + name + '_LOD' + str(lod) + '.png'),
                  (label + cell * lod, header + cell * row, cell - 8, cell - 8))
    sheet.save(OUT / 'Road_Unreal_LOD_Comparison.png')
    top = Image.new('RGB', (1530, 1120), '#edf4f1')
    draw = ImageDraw.Draw(top)
    draw.text((24, 18), 'ROAD TILES / Actual Unreal Engine top surfaces', font=font(30), fill='#153b38')
    for index, name in enumerate(NAMES):
        x, y = 15 + index % 3 * 505, 75 + index // 3 * 510
        draw.text((x + 10, y), name, font=font(23), fill='#153b38')
        paste(top, RENDERS / ('SM_Road_' + name + '_Top.png'), (x, y + 35, 495, 470))
    top.save(OUT / 'Road_Unreal_Top_Comparison.png')
    SOURCE_QA.mkdir(parents=True, exist_ok=True)
    for path in OUT.glob('unreal_*report.json'):
        shutil.copy2(path, SOURCE_QA / path.name)
    for name in ['Road_Unreal_LOD_Comparison.png', 'Road_Unreal_Top_Comparison.png']:
        shutil.copy2(OUT / name, SOURCE_QA / name)
    destination = SOURCE_QA / 'unreal_renders'
    destination.mkdir(exist_ok=True)
    for path in RENDERS.glob('*.png'):
        shutil.copy2(path, destination / path.name)
    print(json.dumps({'status': 'PASS', 'gallery': report['gallery_map'], 'output': str(SOURCE_QA),
                      'captures': len(report['captures'])}, indent=2))


if __name__ == '__main__':
    main()
