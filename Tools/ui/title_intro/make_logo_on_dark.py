"""검은 시작 화면용 꽃 로고. 진한 초록 꽃잎만 밝은 크림색으로 바꾼다(가운데 원·점은 그대로).

원본: SourceArt/UI/Title/CasualBrightParts_v1/Brand/GenerationSource/T_FF_Title_CasualBright_Logo_1254_alpha.png
출력: SourceArt/UI/Title/Intro_v2/T_FF_Title_Logo_OnDark_1024.png  (UE 임포트는 import_intro_assets.py)
"""
from pathlib import Path
import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parents[3]
SRC = ROOT / 'SourceArt/UI/Title/CasualBrightParts_v1/Brand/GenerationSource/T_FF_Title_CasualBright_Logo_1254_alpha.png'
OUT = ROOT / 'SourceArt/UI/Title/Intro_v2/T_FF_Title_Logo_OnDark_1024.png'
CREAM = np.array([246, 241, 228], dtype=np.float32)

img = np.asarray(Image.open(SRC).convert('RGBA')).astype(np.float32)
rgb, alpha = img[..., :3], img[..., 3:]
lum = (rgb @ np.array([0.2126, 0.7152, 0.0722], dtype=np.float32)) / 255.0
# 꽃잎(진한 초록, 밝기 0.25 이하)만 크림색으로. 경계의 중간 밝기는 비율로 섞어 계단을 막는다.
weight = np.clip((0.32 - lum) / 0.12, 0.0, 1.0)[..., None]
rgb = rgb * (1.0 - weight) + CREAM * weight
# 무채색 회색(반짝이·회색 점)은 검은 바탕에서 묻히므로 밝게 올린다.
sat = (rgb.max(axis=-1) - rgb.min(axis=-1)) / 255.0
gray = ((sat < 0.08) & (lum > 0.35) & (lum < 0.9))[..., None]
rgb = np.where(gray, rgb * 0.35 + 255.0 * 0.65, rgb)
out = np.concatenate([rgb, alpha], axis=-1).clip(0, 255).astype(np.uint8)
OUT.parent.mkdir(parents=True, exist_ok=True)
Image.fromarray(out, 'RGBA').resize((1024, 1024), Image.LANCZOS).save(OUT)
print('저장:', OUT)
