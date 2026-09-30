# -*- coding: utf-8 -*-
"""
Dadaepo Office FBX -> /Game/Level/Demo_Matereal/Mesh/기본건축물 임포트

실행 방법
  언리얼 에디터에서 Output Log 패널 하단 드롭다운을 'Python' 으로 바꾸고
  아래 한 줄을 붙여넣고 엔터:

      exec(open(r"E:/Final_PJ/8th-Team13-Final-Project/Content/Python/import_dadaepo_office.py", encoding="utf-8").read())

  또는 Tools > Execute Python Script... 로 이 파일 선택.

무엇을 하나
  1. FBX 존재 확인
  2. 임포트 (스태틱 메시, UE 권장 설정)
  3. 임포트된 에셋을 다시 읽어서 검증 결과를 로그에 출력

설정은 전부 아래 상수로 빼뒀습니다. 크기를 바꾸려면 IMPORT_SCALE 만 고치세요.
"""

import os
import unreal

# ============================ 설정 ============================
FBX_PATH   = r"C:/Users/uinco/Desktop/SpaFainal/dddddd/SM_Dadaepo_Office_SilverRails.fbx"
DEST_PATH  = "/Game/Level/Demo_Matereal/Mesh/기본건축물"
ASSET_NAME = "SM_Dadaepo_Office_SilverRails"

# 배율. 1.0 이면 원본 그대로 약 105 x 63 x 37 uu (=1.05m) 로 들어옵니다.
# 건축물 실물 크기로 쓰려면 10~20 정도로 올리세요.
IMPORT_SCALE = 1.0

IMPORT_MATERIALS = True    # 45개 머티리얼 + 17개 텍스처가 같이 생성됩니다
IMPORT_TEXTURES  = True
GENERATE_LIGHTMAP_UVS = True   # UV0은 텍스처용이라 라이트맵은 자동 생성이 맞습니다
AUTO_GENERATE_COLLISION = True
REPLACE_EXISTING = True
SAVE_AFTER_IMPORT = True

# UE 5.5+ 는 Interchange 가 기본 FBX 임포터입니다.
# 아래 설정들을 확실히 먹이려면 레거시 임포터로 보내는 편이 안전합니다.
# 끝나면 원래대로 되돌립니다 (세션 한정, 에디터 재시작하면 어차피 초기화).
FORCE_LEGACY_FBX = True
# ==============================================================


def log(m):
    unreal.log("[ImportDadaepo] {}".format(m))


def warn(m):
    unreal.log_warning("[ImportDadaepo] {}".format(m))


def err(m):
    unreal.log_error("[ImportDadaepo] {}".format(m))


def set_cvar(name, value):
    try:
        unreal.SystemLibrary.execute_console_command(None, "{} {}".format(name, value))
        return True
    except Exception as e:
        warn("콘솔 명령 실패 ({} {}): {}".format(name, value, e))
        return False


def build_options():
    ui = unreal.FbxImportUI()
    ui.set_editor_property("import_mesh", True)
    ui.set_editor_property("import_as_skeletal", False)
    ui.set_editor_property("import_animations", False)
    ui.set_editor_property("import_materials", IMPORT_MATERIALS)
    ui.set_editor_property("import_textures", IMPORT_TEXTURES)
    try:
        ui.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)
    except Exception as e:
        warn("mesh_type_to_import 설정 실패: {}".format(e))

    sm = ui.static_mesh_import_data

    def prop(name, value):
        try:
            sm.set_editor_property(name, value)
        except Exception as e:
            warn("static_mesh_import_data.{} 설정 실패: {}".format(name, e))

    prop("combine_meshes", True)
    prop("generate_lightmap_u_vs", GENERATE_LIGHTMAP_UVS)
    prop("auto_generate_collision", AUTO_GENERATE_COLLISION)
    prop("remove_degenerates", True)
    prop("build_reversed_index_buffer", True)
    prop("build_nanite", False)

    # 커스텀 노멀이 FBX에 들어 있으므로 반드시 Import (Compute 금지)
    try:
        prop("normal_import_method",
             unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS_AND_TANGENTS)
    except Exception as e:
        warn("normal_import_method 설정 실패: {}".format(e))
    try:
        prop("normal_generation_method",
             unreal.FBXNormalGenerationMethod.MIKK_T_SPACE)
    except Exception:
        pass

    # 좌표계 / 스케일
    prop("convert_scene", True)        # Y-up FBX -> Z-up UE
    prop("force_front_x_axis", False)  # 켜면 90도 돌아갑니다
    prop("convert_scene_unit", False)
    prop("import_uniform_scale", float(IMPORT_SCALE))
    try:
        prop("import_translation", unreal.Vector(0.0, 0.0, 0.0))
        prop("import_rotation", unreal.Rotator(0.0, 0.0, 0.0))
    except Exception:
        pass

    return ui


def do_import():
    if not os.path.exists(FBX_PATH):
        err("FBX 파일이 없습니다: {}".format(FBX_PATH))
        return None

    size_mb = os.path.getsize(FBX_PATH) / 1048576.0
    log("소스: {} ({:.1f} MB)".format(FBX_PATH, size_mb))
    log("대상: {}/{}".format(DEST_PATH, ASSET_NAME))
    log("배율: {}".format(IMPORT_SCALE))

    task = unreal.AssetImportTask()
    task.set_editor_property("filename", FBX_PATH)
    task.set_editor_property("destination_path", DEST_PATH)
    task.set_editor_property("destination_name", ASSET_NAME)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", REPLACE_EXISTING)
    task.set_editor_property("save", SAVE_AFTER_IMPORT)
    task.set_editor_property("options", build_options())

    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

    paths = []
    try:
        paths = list(task.get_editor_property("imported_object_paths"))
    except Exception as e:
        warn("imported_object_paths 읽기 실패: {}".format(e))

    if not paths:
        err("임포트된 에셋이 없습니다. 위쪽 로그에서 FBX 임포터 에러를 확인하세요.")
        return None

    log("생성된 에셋 {}건".format(len(paths)))
    for p in paths[:10]:
        log("  {}".format(p))
    if len(paths) > 10:
        log("  ... 외 {}건".format(len(paths) - 10))
    return paths


def verify():
    """임포트 결과를 다시 읽어서 확인."""
    asset_path = "{}/{}".format(DEST_PATH, ASSET_NAME)
    mesh = unreal.load_asset(asset_path)
    if mesh is None:
        err("검증 실패 — 에셋을 못 찾습니다: {}".format(asset_path))
        return False

    log("=" * 58)
    log("검증 — {}".format(asset_path))
    log("  클래스            : {}".format(type(mesh).__name__))

    def show(label, fn):
        try:
            log("  {:<18}: {}".format(label, fn()))
        except Exception as e:
            warn("  {:<18}: 읽기 실패 ({})".format(label, e))

    show("LOD 수", lambda: mesh.get_num_lods())
    show("머티리얼 슬롯", lambda: len(mesh.static_materials))
    show("섹션 수 (LOD0)", lambda: mesh.get_num_sections(0))

    try:
        sub = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
        show("UV 채널 수", lambda: sub.get_num_uv_channels(mesh, 0))
        show("삼각형 수", lambda: sub.get_number_verts(mesh, 0))
        show("콜리전 프리미티브", lambda: sub.get_simple_collision_count(mesh))
    except Exception as e:
        warn("  StaticMeshEditorSubsystem 사용 불가: {}".format(e))

    try:
        bb = mesh.get_bounding_box()
        mn, mx = bb.min, bb.max
        log("  바운딩박스 min    : [{:.2f}, {:.2f}, {:.2f}]".format(mn.x, mn.y, mn.z))
        log("  바운딩박스 max    : [{:.2f}, {:.2f}, {:.2f}]".format(mx.x, mx.y, mx.z))
        log("  크기 (uu)         : [{:.2f}, {:.2f}, {:.2f}]".format(
            mx.x - mn.x, mx.y - mn.y, mx.z - mn.z))
        log("  크기 (m)          : [{:.2f}, {:.2f}, {:.2f}]".format(
            (mx.x - mn.x) / 100.0, (mx.y - mn.y) / 100.0, (mx.z - mn.z) / 100.0))
    except Exception as e:
        warn("  바운딩박스 읽기 실패: {}".format(e))

    log("  기대값 (배율 {}) : 약 [{:.1f}, {:.1f}, {:.1f}] uu".format(
        IMPORT_SCALE, 105.12 * IMPORT_SCALE, 63.02 * IMPORT_SCALE, 37.40 * IMPORT_SCALE))
    log("=" * 58)
    return True


def main():
    log("=" * 58)
    restore_interchange = False
    if FORCE_LEGACY_FBX:
        if set_cvar("Interchange.FeatureFlags.Import.FBX", 0):
            restore_interchange = True
            log("레거시 FBX 임포터로 전환 (임포트 후 되돌립니다)")

    try:
        paths = do_import()
    finally:
        if restore_interchange:
            set_cvar("Interchange.FeatureFlags.Import.FBX", 1)
            log("Interchange FBX 임포터 복구")

    if paths:
        verify()
    log("끝.")


main()
