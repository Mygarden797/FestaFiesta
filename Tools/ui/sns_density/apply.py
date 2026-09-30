"""SNS·밀집 패널 3개를 한 번 적용하고 바인딩·보호 에셋을 검증한다.

백업은 C++ 전용 생성기가 담당한다. 전체 HUD 생성기나 Save All을 호출하지 않는다.
"""

from datetime import datetime, timezone
from pathlib import Path
import hashlib
import json
import traceback

import unreal


ROOT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
OUT = ROOT / "Saved/SnsDensity"
REPORT_PATH = OUT / "apply-report.json"
COMMAND = "FF.ApplyLiveSocialPanelsDesign"
FOLDER = "/Game/FestaFiesta/UI/LiveHUD/Panels/"
TARGETS = {
    FOLDER + "WBP_FF_LiveSnsPostEntry": {
        "Text_Author": unreal.TextBlock,
        "Text_Body": unreal.TextBlock,
        "Text_Sentiment": unreal.TextBlock,
        "Text_SentimentSymbol": unreal.TextBlock,
        "Border_Sentiment": unreal.Border,
        "Border_Card": unreal.Border,
    },
    FOLDER + "WBP_FF_LivePanelSNS": {
        "Text_PanelTitle": unreal.TextBlock,
        "Button_Close": unreal.Button,
        "Button_FilterAll": unreal.Button,
        "Button_FilterPositive": unreal.Button,
        "Button_FilterNegative": unreal.Button,
        "Text_FilterAll": unreal.TextBlock,
        "Text_FilterPositive": unreal.TextBlock,
        "Text_FilterNegative": unreal.TextBlock,
        "Button_NewPosts": unreal.Button,
        "Text_NewPosts": unreal.TextBlock,
        "Text_Count": unreal.TextBlock,
        "Scroll_Posts": unreal.ScrollBox,
        "Box_Posts": unreal.VerticalBox,
        "Text_Empty": unreal.TextBlock,
        "Text_MissingNote": unreal.TextBlock,
    },
    FOLDER + "WBP_FF_LivePanelDensity": {
        "Text_PanelTitle": unreal.TextBlock,
        "Button_Close": unreal.Button,
        "Button_Toggle": unreal.Button,
        "Text_ToggleLabel": unreal.TextBlock,
        "Box_Legend": unreal.VerticalBox,
        "Text_Peak": unreal.TextBlock,
        "Text_PeakGrade": unreal.TextBlock,
        "Text_Congested": unreal.TextBlock,
        "Text_CellSize": unreal.TextBlock,
        "Text_MissingNote": unreal.TextBlock,
    },
}


def file_for(package):
    if not package.startswith("/Game/"):
        raise ValueError("프로젝트 밖 패키지: " + package)
    return ROOT / "Content" / (package[6:] + ".uasset")


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def protected_snapshot():
    """하단 HUD·공용 탭·다른 라이브 패널·준비 HUD 2종·공용 Styles를 보호한다."""
    excluded = {file_for(package) for package in TARGETS}
    paths = set((ROOT / "Content/FestaFiesta/UI/LiveHUD").rglob("*.uasset"))
    paths.update((ROOT / "Content/FestaFiesta/UI/Styles").rglob("*.uasset"))
    for name in ("WBP_FF_PrepHudBar", "WBP_FF_PrepHudTabButton"):
        paths.add(ROOT / "Content/FestaFiesta/Building/FF_WBP_Building/Shared" / (name + ".uasset"))
    return {
        path.relative_to(ROOT).as_posix(): sha256(path) if path.is_file() else None
        for path in sorted(paths - excluded)
    }


def inspect_target(package, bindings):
    asset = unreal.load_asset(package)
    assert isinstance(asset, unreal.WidgetBlueprint), "WidgetBlueprint 누락: " + package
    generated = unreal.EditorAssetLibrary.load_blueprint_class(package)
    assert generated, "GeneratedClass 누락: " + package
    expected_class_path = package + "." + package.rsplit("/", 1)[1] + "_C"
    assert generated.get_path_name() == expected_class_path, "GeneratedClass 경로 불일치: " + package
    defaults = unreal.get_default_object(generated)
    assert defaults, "CDO 누락: " + package
    tree = unreal.find_object(asset, "WidgetTree")
    assert tree, "WidgetTree 누락: " + package
    checked = {}
    for name, expected_type in bindings.items():
        widget = unreal.find_object(tree, name)
        assert widget, package + ": " + name + " 바인딩 누락"
        assert isinstance(widget, expected_type), package + ": " + name + " 위젯 타입 불일치"
        checked[name] = widget.get_class().get_name()
    path = file_for(package)
    assert path.is_file(), "저장된 uasset 누락: " + str(path)
    return {
        "asset": asset.get_path_name(),
        "generated_class": generated.get_path_name(),
        "bindings": checked,
        "file": path.relative_to(ROOT).as_posix(),
        "sha256": sha256(path),
    }


report = {
    "status": "running",
    "started_at": datetime.now(timezone.utc).isoformat(),
    "command": COMMAND,
    "targets": {},
}
write_report = True
before = None
try:
    OUT.mkdir(parents=True, exist_ok=True)
    if REPORT_PATH.exists():
        try:
            previous = json.loads(REPORT_PATH.read_text(encoding="utf-8-sig"))
        except (ValueError, OSError):
            write_report = False
            raise RuntimeError("기존 적용 기록을 읽지 못했다. 보호를 위해 재실행하지 않는다.")
        if previous.get("status") == "passed":
            # 성공 기록을 failed로 덮으면 다음 실행의 보호가 풀리므로 그대로 보존한다.
            write_report = False
            raise RuntimeError("이미 적용했다. 디자이너 편집 보존을 위해 재실행하지 않는다.")
    before = protected_snapshot()
    report["protected_before"] = before
    backup_root = ROOT / "Saved/UIBackups/LiveSocialPanels"
    previous_backups = {path.name for path in backup_root.iterdir()} if backup_root.is_dir() else set()
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    assert world, "에디터 월드를 찾을 수 없다."
    unreal.SystemLibrary.execute_console_command(world, COMMAND)
    report["backups"] = [
        str(path) for path in sorted(backup_root.iterdir())
        if path.is_dir() and path.name not in previous_backups
    ] if backup_root.is_dir() else []
    assert report["backups"], "전용 생성기의 실행 전 백업 디렉터리가 없다."
    for package, bindings in TARGETS.items():
        report["targets"][package] = inspect_target(package, bindings)
    sns_class = unreal.EditorAssetLibrary.load_blueprint_class(FOLDER + "WBP_FF_LivePanelSNS")
    entry_class = unreal.get_default_object(sns_class).get_editor_property("post_entry_class")
    expected_entry_class = report["targets"][FOLDER + "WBP_FF_LiveSnsPostEntry"]["generated_class"]
    assert entry_class and entry_class.get_path_name() == expected_entry_class, "SNS PostEntryClass 명시 참조가 없다."
    report["post_entry_class"] = entry_class.get_path_name()
    after = protected_snapshot()
    report["protected_after"] = after
    assert after == before, "보호 대상 HUD·탭·공용 Styles가 변경되었다."
    report["protected_assets_unchanged"] = True
    report["status"] = "passed"
except Exception:
    report["status"] = "failed"
    report["error"] = traceback.format_exc()
    unreal.log_error("FF_SNS_DENSITY_APPLY_FAIL\n" + report["error"])
finally:
    try:
        if before is not None:
            after = protected_snapshot()
            report["protected_after"] = after
            report["protected_assets_unchanged"] = after == before
            if after != before:
                report["status"] = "failed"
        report["finished_at"] = datetime.now(timezone.utc).isoformat()
        if write_report:
            REPORT_PATH.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
        unreal.log("FF_SNS_DENSITY_APPLY " + report["status"])
    finally:
        unreal.SystemLibrary.quit_editor()
