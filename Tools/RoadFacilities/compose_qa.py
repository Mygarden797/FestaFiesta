"""Arrange unmodified reference and actual Blender renders for visual review."""
from pathlib import Path
import json
from PIL import Image, ImageDraw, ImageFont

ROOT=Path(__file__).resolve().parents[2]/'SourceArt/Roads_20260910'
NAMES=['Sand','Asphalt','Wood','Stone','Herringbone','Concrete']
LABELS=['모래길','아스팔트길','나무 데크길','석재 타일길','헤링본 벽돌길','콘크리트길']
BG=(185,219,210)
FONT=ImageFont.truetype('C:/Windows/Fonts/malgun.ttf',22)
SMALL=ImageFont.truetype('C:/Windows/Fonts/malgun.ttf',17)

def put(canvas,path,xy,size):
    src=Image.open(path).convert('RGBA')
    assert src.size==(512,512),(path,src.size)
    box=src.getchannel('A').getbbox();assert box and box[2]-box[0]>100,(path,box)
    src.thumbnail((size,size),Image.Resampling.LANCZOS)
    canvas.paste(src,xy,src)

out=Image.new('RGB',(1152,860),BG);draw=ImageDraw.Draw(out)
draw.text((28,18),'길 6종 · 실제 Blender 모델',font=FONT,fill=(31,63,60))
for i,(name,label) in enumerate(zip(NAMES,LABELS)):
    x=(i%3)*384;y=60+(i//3)*396
    put(out,ROOT/'qa'/('SM_Road_'+name+'_Hero.png'),(x,y),384)
    draw.text((x+192,y+365),label,font=FONT,fill=(31,63,60),anchor='mt')
out.save(ROOT/'qa/Roads_Overview.png')
ref=Image.open(ROOT/'references/Road.png').convert('RGB')
ref.thumbnail((760,620),Image.Resampling.LANCZOS)
comparison=Image.new('RGB',(1560,670),BG);d=ImageDraw.Draw(comparison)
d.text((20,20),'참고 이미지',font=FONT,fill=(31,63,60));d.text((800,20),'실제 모델 렌더 · 4m × 4m × 8cm',font=FONT,fill=(31,63,60))
comparison.paste(ref,(20,115))
shown=out.copy();shown.thumbnail((750,590),Image.Resampling.LANCZOS);comparison.paste(shown,(790,65))
comparison.save(ROOT/'qa/Reference_Comparison.png')
records=[]
for name in NAMES:
    views=['Hero','Front','Rear','Left','Right','Top','Bottom','Game','LOD0','LOD1','LOD2']
    sheet=Image.new('RGB',(1000,850),BG);d=ImageDraw.Draw(sheet)
    d.text((20,15),'SM_Road_'+name+' · 다각도 / LOD 검수',font=FONT,fill=(31,63,60))
    for i,view in enumerate(views):
        x=(i%4)*250;y=50+(i//4)*263
        put(sheet,ROOT/'qa'/('SM_Road_'+name+'_'+view+'.png'),(x,y),250)
        d.text((x+125,y+239),view,font=SMALL,fill=(31,63,60),anchor='mt')
    sheet.save(ROOT/'qa'/('SM_Road_'+name+'_ContactSheet.png'))
    records.append({'asset':name,'render_count':len(views),'dimensions':[512,512],'alpha_content':'PASS'})
(ROOT/'qa/render_validation.json').write_text(json.dumps({'status':'PASS','assets':records},indent=2),encoding='utf-8')
print('ROAD_RENDER_COMPOSITION_PASS')
