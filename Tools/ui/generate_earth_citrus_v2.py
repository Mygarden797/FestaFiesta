# -*- coding: utf-8 -*-
"""원본 픽셀 좌표를 고정한 Citrus Pop 소스 텍스처 제작·검증.

실사 원본과 같은 크기에서 색만 변환한다. AI 초안의 윤곽은 사용하지 않는다.
Pillow, NumPy만 필요. 기존 v1의 수치 함수와 구체 메시 생성기를 재사용한다.
"""
from pathlib import Path
import hashlib
import json
import numpy as np
from PIL import Image, ImageDraw
import generate_title_casual_relief_parts as relief

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'SourceArt/UI/Title/CasualReliefParts_v2'
EARTH = OUT / 'Earth'
REF = EARTH / 'Reference/Geography_Master_2048x1024.png'
PREVIEW = OUT / 'Preview'
W, H = 2048, 1024
PREFIX = 'T_FF_Earth_CitrusPop_v2_'


def classify(rgb):
    """원본의 파란 바다와 저채도 설빙을 분리한다. 좌표 변환은 없다."""
    r, g, b = rgb.astype(np.float32).transpose(2, 0, 1)
    mx = np.maximum.reduce([r, g, b])
    mn = np.minimum.reduce([r, g, b])
    water = (b > r + 16) & (b > g + 12) & (r < 145)
    signed_lat = (90 - (np.arange(H) + 0.5) * 180 / H)[:, None]
    lat = np.abs(signed_lat)
    # 설빙의 어두운 영역도 극지에서는 보존한다. 구름을 별도로 합성하지 않는다.
    ice = (~water) & ((mx-mn) < mx*0.18) & (mn > np.where(lat > 58, 112, 195))
    # 저위도 사막의 밝은 모래를 눈으로 오인하지 않도록 따뜻한 색을 제외한다.
    ice &= (lat > 58) | ((b >= r-6) & (g >= r-5))
    # 남극의 얼음 음영이 녹색 육지로 바뀌지 않게 한다. 바다 경계는 그대로 둔다.
    ice |= (~water) & (signed_lat < -60)
    return water, ice


def save(name, arr):
    path = EARTH / (PREFIX + name + '_2048x1024.png')
    Image.fromarray(arr).save(path)
    return path


def make_preview(albedo, heights, normal, masks):
    """구면 UV 방향과 극지 확인용 CPU 미리보기. UE 실기 캡처가 아니다."""
    pw, ph = 1440, 720
    canvas = np.full((ph, pw, 3), [239, 247, 234], dtype=np.float32) / 255
    views = [(35, 26), (128, 20)]
    for i, (longitude, latitude) in enumerate(views):
        size = 660
        yy, xx = np.mgrid[:size, :size]
        x, y = (xx - size/2) / 303, (yy - size/2) / 303
        inside = x*x+y*y <= 1
        z = np.sqrt(np.maximum(1-x*x-y*y, 0))
        lon0, lat0 = np.radians([longitude, latitude])
        forward = np.array([np.cos(lat0)*np.cos(lon0),np.cos(lat0)*np.sin(lon0),np.sin(lat0)])
        east0 = np.array([-np.sin(lon0),np.cos(lon0),0])
        up0 = np.cross(forward,east0)
        d = z[...,None]*forward+x[...,None]*east0-y[...,None]*up0
        d /= np.maximum(np.linalg.norm(d,axis=-1,keepdims=True),1e-6)
        lon = np.arctan2(d[...,1],d[...,0])
        lat = np.arcsin(np.clip(d[...,2],-1,1))
        u,v = (lon+np.pi)/(2*np.pi),(np.pi/2-lat)/np.pi
        color = relief.bilinear_sample(albedo/255.,u,v)
        n = relief.bilinear_sample(normal/255.,u,v)*2-1
        east = np.stack([-np.sin(lon),np.cos(lon),np.zeros_like(lon)],axis=-1)
        south = np.stack([np.sin(lat)*np.cos(lon),np.sin(lat)*np.sin(lon),-np.cos(lat)],axis=-1)
        world_n = east*n[...,0:1]+south*n[...,1:2]+d*n[...,2:3]
        world_n /= np.maximum(np.linalg.norm(world_n,axis=-1,keepdims=True),1e-6)
        light = forward*0.85-east0*0.35+up0*0.5
        light /= np.linalg.norm(light)
        diffuse = np.maximum(np.sum(world_n*light,axis=-1),0)
        ao = relief.bilinear_sample(masks[...,3],u,v)[...,0]
        lit = np.clip(color*(0.66+0.34*diffuse)[...,None]*ao[...,None],0,1)
        tile = canvas[30:690, i*720+30:i*720+690]
        tile[inside] = lit[inside]
    Image.fromarray(np.rint(canvas*255).astype(np.uint8)).save(PREVIEW/'CitrusPop_Globes.png')


def main():
    EARTH.mkdir(parents=True,exist_ok=True)
    PREVIEW.mkdir(parents=True,exist_ok=True)
    source = np.asarray(Image.open(REF).convert('RGB'))
    assert source.shape == (H,W,3), '지리 원본은 리사이즈하지 않는다'
    water, ice = classify(source)
    land = ~water
    # 지형 가이드는 동일 위치의 공식 원본 노멀에서 얻는다. 절대 고도는 아니다.
    r,g,b = source.astype(np.float32).transpose(2,0,1)
    greenness = np.clip((g-r+8)/42,0,1)
    sand = np.clip((r-g+8)/38,0,1)*np.clip((r-b-5)/48,0,1)
    sand = relief.blur_scalar(sand,1.4)
    green_smooth = relief.blur_scalar(greenness,1.0)
    forest = np.array([99,177,37],np.float32)
    lime = np.array([189,226,47],np.float32)
    gold = np.array([255,213,61],np.float32)
    ocean = np.array([7,185,205],np.float32)
    snow = np.array([247,249,245],np.float32)
    vegetation = forest[None,None,:]*green_smooth[...,None]+lime[None,None,:]*(1-green_smooth[...,None])
    desert_weight = np.clip(sand*1.7,0,1)
    albedo = vegetation*(1-desert_weight[...,None])+gold*desert_weight[...,None]
    # 위성사진의 미세한 명암을 굽지 않고 저주파 색 변화만 남긴다.
    albedo[water] = ocean
    albedo[ice] = snow
    # 同じ分類の継ぎ目だけ色を一致させる。異なる分類の海岸を移動しない。
    labels = np.where(water,0,np.where(ice,2,1)).astype(np.uint8)
    seam_same = labels[:,0] == labels[:,-1]
    seam_mean = (albedo[:,0]+albedo[:,-1])/2
    albedo[seam_same,0] = seam_mean[seam_same]
    albedo[seam_same,-1] = seam_mean[seam_same]
    albedo = np.rint(np.clip(albedo,0,255)).astype(np.uint8)

    guide = relief.load_rgb(relief.SOURCE_NORMAL).astype(np.float32)
    slope = np.sqrt(np.sum(((guide[...,:2]-128)/127)**2,axis=-1))*land
    mountain = relief.blur_scalar(np.clip(slope/0.22,0,1),4.0)*land
    mountain /= max(float(np.percentile(mountain[land],99.5)),0.01)
    mountain = np.clip(mountain,0,1)
    # 베벨은 육지 안쪽에만 만든다. 바다는 정확히 높이 0이다.
    coast = relief.blur_scalar(land.astype(np.float32),3.0)
    bevel = np.clip((coast-0.45)/0.50,0,1)
    bevel = bevel*bevel*(3-2*bevel)*land
    noise = relief.spherical_noise()
    heights = bevel*(0.54+0.07*noise+0.29*mountain)
    heights[ice] = bevel[ice]*(0.55+0.025*noise[ice]+0.10*mountain[ice])
    # 모든 경도가 한 점으로 모이는 극점은 높이·노멀을 안정화한다.
    for top in (True,False):
        for row in range(12):
            y = row if top else H-1-row
            t = (row/11)**2*(3-2*row/11)
            if np.all(ice[y]):
                heights[y] = 0.565*(1-t)+heights[y]*t
    heights[water]=0
    same_land = land[:,0] & land[:,-1]
    mean_h=(heights[:,0]+heights[:,-1])/2
    heights[same_land,0]=mean_h[same_land]
    heights[same_land,-1]=mean_h[same_land]
    normals = relief.build_normal_dx(heights,land.astype(np.float32))
    normals[water]=[128,128,255]
    normals[0]=[128,128,255]
    normals[-1]=[128,128,255]
    rough = np.where(water,0.23,np.where(ice,0.56,0.70)).astype(np.float32)
    ao = np.clip(1-0.15*(1-bevel)*land-0.035*mountain,0,1)
    masks = np.stack([heights,land.astype(np.float32),rough,ao],axis=-1)
    files = [save('Albedo',albedo),save('Height16',np.rint(heights*65535).astype(np.uint16)),save('Normal_DX',normals),save('Masks',np.rint(masks*255).astype(np.uint8))]
    for name,mask in [('Land',land),('Ice',ice)]:
        Image.fromarray(mask.astype(np.uint8)*255).save(EARTH/'Reference'/f'{name}Mask.png')
    # 저장 후 재독출하여 영역·채널 계약을 검증한다.
    final = np.asarray(Image.open(files[0]))
    final_i = final.astype(np.int16)
    final_water = (final_i[...,2]>final_i[...,0]+16) & (final_i[...,0]<145)
    final_ice = np.min(final,axis=-1)>235
    packed = np.asarray(Image.open(files[3]))
    assert np.array_equal(final_water,water)
    assert np.array_equal(final_ice,ice)
    assert np.array_equal(packed[...,1],land.astype(np.uint8)*255)
    assert np.all(packed[...,0][water]==0)
    assert np.all(normals[water]==[128,128,255])
    relief.MESH_DIR=OUT/'Mesh'
    relief.OUT_SPHERE=relief.MESH_DIR/'SM_FF_Earth_UVSphere_R50cm_256x128.obj'
    relief.generate_uv_sphere()
    make_preview(albedo,heights,normals,masks)
    # 원본·결과를 같은 픽셀 좌표에서 비교할 수 있는 미리보기.
    compare=Image.new('RGB',(W,H*2))
    compare.paste(Image.fromarray(source),(0,0))
    compare.paste(Image.fromarray(albedo),(0,H))
    compare.save(PREVIEW/'Original_vs_CitrusPop.png')
    report = {
      'status':'source_art_verified_unreal_not_imported',
      'size':[W,H], 'projection':'equirectangular',
      'source_sha256':hashlib.sha256(REF.read_bytes()).hexdigest(),
      'geography_transform':'none; no resize, translation, rotation, warp or generated geometry',
      'classification_note':'분류 마스크 기준의 일치. 실측 해안/설빙의 정답 판정은 아님.',
      'land_mask_mismatched_pixels':int(np.count_nonzero(final_water!=water)),
      'ice_mask_mismatched_pixels':int(np.count_nonzero(final_ice!=ice)),
      'original_classified_ice_pixels':int(ice.sum()),
      'output_ice_pixels':int(final_ice.sum()),
      'ocean_nonzero_height_pixels':int(np.count_nonzero(packed[...,0][water])),
      'seam_classification_mismatch_rows':int(np.count_nonzero(~seam_same)),
      'seam_max_rgb_difference':int(np.abs(final[:,0].astype(int)-final[:,-1].astype(int)).max()),
      'normal_encoding':'DirectX; tangent convention requires Unreal mesh verification',
      'mask_channels':{'R':'height','G':'land_or_ice','B':'roughness','A':'AO'},
      'files':{p.name:{'sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'mode':Image.open(p).mode} for p in files},
      'preview':'CPU 구면 미리보기. 실제 WPO 실루엣·UE 조명·핀 정렬 검증 아님.',
    }
    (OUT/'asset-manifest.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    print(json.dumps(report,ensure_ascii=False,indent=2))


if __name__=='__main__':
    main()
