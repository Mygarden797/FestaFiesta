"""인트로 프리비즈 렌더 — 별도 Blender 프로세스에서 실행해 켜진 Blender UI를 막지 않는다.

blender -b <FF_Intro_HouseDeparture_v001.blend> --python render_previs.py -- layout pilot

layout: 위에서 본 배치도·전체 조감·샷별 시작 구도 PNG (Previs/Layout)
pilot : 5초 파일럿의 시작·끝 PNG와 미리보기 MP4 (Previs/Pilot)
엔진은 Workbench(텍스처 색). 파일은 저장하지 않으므로 자세·해상도·카메라 변경은 이 프로세스에만 남는다.
"""
from pathlib import Path
import sys
import bpy

sys.path.insert(0, str(Path(__file__).resolve().parent))
import intro_previs_set as m  # noqa: E402

LAYOUT = m.ART / 'Previs/Layout'
PILOT = m.ART / 'Previs/Pilot'


def _prepare(sc, cam, res, paths=False, labels=False, backdrops=True, shadows=True, standin=True):
    sc.camera = bpy.data.objects[cam]
    sc.render.resolution_x, sc.render.resolution_y = res
    sc.render.resolution_percentage = 100
    cols = bpy.data.collections
    cols['GUIDE_Layout'].hide_render = not paths
    cols['GUIDE_Labels'].hide_render = not labels
    cols['CHR_Standin'].hide_render = not standin
    cols['PROP_Backpack'].hide_render = not standin
    # 문·창 너머 밝은 면은 샷용이다. 조감에서는 방을 가리므로 끈다.
    for ob in cols['SET_Room'].objects:
        if ob.name.startswith('SET_Outside'):
            ob.hide_render = not backdrops
    # 공중에 띄운 이름표의 그림자가 바닥에 찍히지 않게 배치도에서는 그림자를 끈다.
    sc.display.shading.show_shadows = shadows


def still(sc, cam, path, res=(1920, 1080), frame=1, **opts):
    _prepare(sc, cam, res, **opts)
    sc.frame_set(frame)
    im = sc.render.image_settings
    im.media_type = 'IMAGE'
    im.file_format = 'PNG'
    sc.render.filepath = str(path)
    bpy.ops.render.render(write_still=True, scene=sc.name)
    print('PNG:', path)


def video(sc, cam, path, **opts):
    _prepare(sc, cam, (1920, 1080), **opts)
    im = sc.render.image_settings
    im.media_type = 'VIDEO'
    im.file_format = 'FFMPEG'
    ff = sc.render.ffmpeg
    ff.format = 'MPEG4'
    ff.codec = 'H264'
    ff.constant_rate_factor = 'HIGH'
    ff.audio_codec = 'NONE'
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
    args = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else ['layout', 'pilot']
    sc = bpy.data.scenes[m.SCENE]
    if 'layout' in args:
        LAYOUT.mkdir(parents=True, exist_ok=True)
        m.pose('S03')
        still(sc, 'CAM_Layout_Top', LAYOUT / 'layout_top.png', res=(1600, 1600),
              paths=True, labels=True, shadows=False)
        still(sc, 'CAM_Layout_Overview', LAYOUT / 'layout_overview.png',
              paths=True, backdrops=False, shadows=False)
        for shot, cam, name in (('S01', 'CAM_S01_Enter', 'S01_enter_start.png'),
                                ('S02', 'CAM_S02_Spin', 'S02_spin_start.png'),
                                ('S03', 'CAM_S03_Pack', 'S03_pack_start.png')):
            m.pose(shot)
            still(sc, cam, LAYOUT / name)
    if 'pilot' in args:
        # S04 접근 구간 시험 — 캐릭터와 배낭은 이미 나간 뒤라 숨긴다.
        PILOT.mkdir(parents=True, exist_ok=True)
        f0, f1 = m.PILOT_FRAMES
        still(sc, 'CAM_P0_Pilot', PILOT / 'pilot_start.png', frame=f0, standin=False)
        # 0.5초 — 시작보다 서쪽 경도가 보이면 회전 방향(서→동)이 맞다.
        still(sc, 'CAM_P0_Pilot', PILOT / 'pilot_check_0p5s.png', frame=f0 + m.FPS // 2, standin=False)
        still(sc, 'CAM_P0_Pilot', PILOT / 'pilot_end.png', frame=f1, standin=False)
        video(sc, 'CAM_P0_Pilot', PILOT / 'pilot_previs.mp4', standin=False)


main()
