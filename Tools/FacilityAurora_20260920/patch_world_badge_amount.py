# patch_world_badge_amount.py
# 역할: 시설 정보·수익 화면의 테마 작업에서 관련 도구가 공유하는 설정과 처리 함수를 제공한다.

"""Root-run editor patch: constrain large V7 world-badge amounts, preserving its theme."""
import hashlib
import json
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
PACKAGE = '/Game/FestaFiesta/Building/FF_WBP_Building/FacilityRevenue/WBP_FF_FacilityWorldBadge'
FILE = ROOT / 'Content/FestaFiesta/Building/FF_WBP_Building/FacilityRevenue/WBP_FF_FacilityWorldBadge.uasset'
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world and world.get_path_name().startswith('/Engine/Maps/Entry.'), 'Use the Entry world.'

asset = unreal.EditorAssetLibrary.load_asset(PACKAGE)
assert asset, 'The saved V7 world badge must exist.'

def widgets():
    result = {}
    for widget in unreal.ObjectIterator(unreal.Widget):
        outer = widget.get_outer()
        if outer and outer.get_class().get_name() == 'WidgetTree' and outer.get_outer() == asset:
            result[widget.get_name()] = widget
    return result

def material_paths():
    return {
        name: widget.get_editor_property('background').get_editor_property('resource_object').get_path_name()
        for name, widget in widgets().items()
        if isinstance(widget, unreal.Border)
        and widget.get_editor_property('background').get_editor_property('resource_object')
    }

before_materials = material_paths()
unreal.SystemLibrary.execute_console_command(world, 'FF.ApplyFacilityAuroraWorldBadgeAmountFit')
after = widgets()
assert 'Scale_AuroraWorldRevenueAmount' in after, 'Amount-fitting widget was not authored.'
assert after['Text_RevenueValue'].get_parent() == after['Scale_AuroraWorldRevenueAmount']
assert after['Scale_AuroraWorldRevenueAmount'].get_parent() == after['Horizontal_Revenue']
assert before_materials == material_paths(), 'The amount patch must preserve the existing green materials.'
first_digest = hashlib.sha256(FILE.read_bytes()).hexdigest()
unreal.SystemLibrary.execute_console_command(world, 'FF.ApplyFacilityAuroraWorldBadgeAmountFit')
assert hashlib.sha256(FILE.read_bytes()).hexdigest() == first_digest, 'Patch must be idempotent.'
out = ROOT / 'Saved/FacilityAurora_20260920'
out.mkdir(parents=True, exist_ok=True)
(out / 'world-badge-amount-patch.json').write_text(json.dumps({
    'package': PACKAGE, 'amount_fitting_widget': True, 'materials_preserved': True,
    'idempotent': True, 'sha256': first_digest,
}, ensure_ascii=False, indent=2), encoding='utf-8')
unreal.log('FF_AURORA_WORLD_BADGE_AMOUNT_PATCH_PASS')
