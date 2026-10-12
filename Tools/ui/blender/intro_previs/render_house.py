"""인트로 프리비즈(집) 렌더 — 별도 Blender 프로세스에서 실행해 켜진 Blender UI를 막지 않는다.

blender -b <FF_Intro_HouseDeparture_v003.blend> --python render_house.py -- layout shots pilot

layout: Workbench 배치도(위·거실 조감·침실 조감). 천장·남쪽 벽·바깥·안개는 숨긴다.
shots : EEVEE 밤 무드 — 거실·침실 기준 이미지(캐릭터 없음)와 S01·S01b·S02·S03 시작 구도.
pilot : EEVEE 5초 파일럿 시작·0.5초·끝 PNG와 MP4. 캐릭터·배낭은 이미 나간 뒤라 숨긴다.
출력은 Previs/<VERSION>/. 파일은 저장하지 않으므로 자세·엔진·해상도 변경은 이 프로세스에만 남는다.
"""
from pathlib import Path
import sys
import bpy

sys.path.insert(0, str(Path(__file__).resolve().parent))
import intro_house as h  # noqa: E402

OUT = h.ART / 'Previs' / h.VERSION
TOGGLED = ('GUIDE_Layout', 'GUIDE_Labels', 'CHR_Standin', 'PROP_Backpack', 'SET_Ceiling', 'SET_WallSouth',
           'SET_Outside', 'FX_Volume')


def _prepare(sc, cam, engine, res=(1920, 1080), hide=(), show=()):
    sc.camera = bpy.data.objects[cam]
    sc.render.engine = engine
    sc.render.resolution_x, sc.render.resolution_y = res
    sc.render.resolution_percentage = 100
    for name in TOGGLED:
        bpy.data.collections[name].hide_render = name in hide or (name.startswith('GUIDE') and name not in show)


def still(sc, cam, path, engine='BLENDER_EEVEE', frame=1, **opts):
    _prepare(sc, cam, engine, **opts)
    sc.frame_set(frame)
    im = sc.render.image_settings
    im.media_type = 'IMAGE'
    im.file_format = 'PNG'
    sc.render.filepath = str(path)
    bpy.ops.render.render(write_still=True, scene=sc.name)
    print('PNG:', path)


def video(sc, cam, path, **opts):
    _prepare(sc, cam, 'BLENDER_EEVEE', **opts)
    im = sc.render.image_settings
    im.media_type = 'VIDEO'
    im.file_format = 'FFMPEG'
    ff = sc.render.ffmpeg
    ff.format, ff.codec, ff.constant_rate_factor, ff.audio_codec = 'MPEG4', 'H264', 'HIGH', 'NONE'
    before = set(path.parent.glob('*.mp4'))
    sc.render.filepath = str(path.with_suffix(''))
    bpy.ops.render.render(animation=True, scene=sc.name)
    made = sorted(set(path.parent.glob('*.mp4')) - before, key=lambda p: p.stat().st_mtime)
    if made and made[-1] != path:
        if path.exists():
            path.unlink()
        made[-1].rename(path)
    print('MP4:', path, path.exists())


def main():
    args = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else ['layout', 'shots', 'pilot']
    sc = bpy.data.scenes[h.SCENE]
    if 'layout' in args:
        d = OUT / 'Layout'
        d.mkdir(parents=True, exist_ok=True)
        h.pose('S03')
        roofless = ('SET_Ceiling', 'FX_Volume')
        still(sc, 'CAM_Layout_Top', d / 'layout_top.png', 'BLENDER_WORKBENCH', res=(1600, 1600),
              hide=roofless, show=('GUIDE_Layout', 'GUIDE_Labels'))
        still(sc, 'CAM_Layout_Overview', d / 'layout_overview.png', 'BLENDER_WORKBENCH',
              hide=roofless + ('SET_WallSouth', 'SET_Outside'), show=('GUIDE_Layout',))
        still(sc, 'CAM_Layout_Bedroom', d / 'layout_bedroom.png', 'BLENDER_WORKBENCH',
              hide=roofless + ('SET_Outside',), show=('GUIDE_Layout',))
    if 'shots' in args:
        d = OUT / 'Shots'
        d.mkdir(parents=True, exist_ok=True)
        h.pose('S01')
        still(sc, 'CAM_S01_Enter', d / 'room_reference_living_night.png', hide=('CHR_Standin',))
        still(sc, 'CAM_S03_Pack', d / 'room_reference_bedroom_night.png', hide=('CHR_Standin',))
        for shot, cam, name in (('S01', 'CAM_S01_Enter', 'S01_enter_start.png'),
                                ('S01b', 'CAM_S01_Enter', 'S01_cross_to_bedroom.png'),
                                ('S02', 'CAM_S02_Spin', 'S02_spin_start.png'),
                                ('S03', 'CAM_S03_Pack', 'S03_pack_start.png')):
            h.pose(shot)
            still(sc, cam, d / name)
    if 'pilot' in args:
        d = OUT / 'Pilot'
        d.mkdir(parents=True, exist_ok=True)
        gone = ('CHR_Standin', 'PROP_Backpack')
        f0, f1 = h.PILOT_FRAMES
        still(sc, 'CAM_P0_Pilot', d / 'pilot_start.png', frame=f0, hide=gone)
        still(sc, 'CAM_P0_Pilot', d / 'pilot_check_0p5s.png', frame=f0 + h.FPS // 2, hide=gone)
        still(sc, 'CAM_P0_Pilot', d / 'pilot_end.png', frame=f1, hide=gone)
        if 'nomp4' not in args:
            video(sc, 'CAM_P0_Pilot', d / 'pilot_previs.mp4', hide=gone)


main()
