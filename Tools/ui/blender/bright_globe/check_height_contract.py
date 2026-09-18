"""실제 원본 UV/Height 샘플로 두께·산 높이 독립 변경 시 Top/Coast 접합을 검사."""
from collections import defaultdict
import json
import math
import numpy as np
from PIL import Image
from prepare_roundtrip import ROOT, ART, OUT, parse_obj


def sample_height(uv, height):
    h, w = height.shape
    x, y = uv[0] * w - .5, (1.-uv[1]) * h - .5
    ix, iy = math.floor(x), math.floor(y)
    tx, ty = x-ix, y-iy
    ix %= w; iy = max(0, min(h-1, iy))
    jx, jy = (ix+1)%w, min(iy+1, h-1)
    return height[iy,ix]*(1-tx)*(1-ty)+height[iy,jx]*tx*(1-ty)+height[jy,ix]*(1-tx)*ty+height[jy,jx]*tx*ty


def run():
    height = np.asarray(Image.open(ART/'P02/Inputs/T_Diorama_Height.png'), dtype=float)/255.
    cases = [(40.,350.), (100.,800.), (40.,650.)]
    results = []
    for tile in (3,4,5):
        tv,tu,tf = parse_obj(ART/f'P02/Inputs/SM_Diorama_{tile:02d}_Top.obj')
        cv,cu,cf = parse_obj(ART/f'P02/Inputs/SM_Diorama_{tile:02d}_Coast.obj')
        top_at = {tv[v]: tu[u] for f in tf for v,u in f}
        matches = [(cv[v], top_at[cv[v]], cu[u]) for v,u in {corner for f in cf for corner in f} if cv[v] in top_at]
        assert len(matches)>100
        for thick,mountain in cases:
            gap = 0.
            for pos,top_uv,coast_uv in matches:
                ht = sample_height(top_uv,height)
                hc,weight = coast_uv[0],1.-coast_uv[1]
                assert weight==1.
                top_delta = thick-100.+ht*(mountain-350.)
                coast_delta = (thick-100.+hc*(mountain-350.))*weight
                gap=max(gap,abs(top_delta-coast_delta))
            assert gap < .01, (tile,thick,mountain,gap)
            results.append({'tile':tile,'thickness':thick,'mountain':mountain,'joined_vertices':len(matches),'max_gap_uu':gap})
    report={'status':'passed','method':'CPU: actual texture bilinear samples and exported Coast UV; GPU render not yet verified','cases':results}
    (OUT/'p02_height_contract.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    print('HEIGHT_CONTRACT_PASSED',max(r['max_gap_uu'] for r in results))


if __name__=='__main__':run()
