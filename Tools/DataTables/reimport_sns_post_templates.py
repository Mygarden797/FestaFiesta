"""data/unreal/sns_post_templates_v0.csv 를 DT_SnsPostTemplates 에 다시 넣는다.

CSV 만 고치고 이 스크립트를 안 돌리면 게임에 새 문장이 안 나온다.

    UnrealEditor-Cmd.exe FestaFiesta.uproject -run=pythonscript
        -script="Tools/DataTables/reimport_sns_post_templates.py"
"""
from pathlib import Path

import unreal

ROOT = Path(__file__).resolve().parents[2]
CSV = ROOT / "data/unreal/sns_post_templates_v0.csv"
DT = "/Game/FestaFiesta/Data/SNS/DT_SnsPostTemplates"

# 반영됐는지 눈으로 확인할 행.
WATCHED = ("sns_fee_high_01", "sns_fee_low_01", "sns_finale_hype_01", "sns_finale_s_22")


def main():
    table = unreal.load_asset(DT)
    assert isinstance(table, unreal.DataTable), DT

    text = CSV.read_text(encoding="utf-8-sig")
    if not unreal.DataTableFunctionLibrary.fill_data_table_from_csv_string(table, text):
        raise SystemExit("CSV 파싱 실패 — 열 구성이 FFF_SnsPostTemplateRow 와 맞는지 확인할 것")

    unreal.EditorAssetLibrary.save_loaded_asset(table)

    names = [str(n) for n in unreal.DataTableFunctionLibrary.get_data_table_row_names(table)]
    for row in WATCHED:
        assert row in names, "행이 없다: %s" % row
    unreal.log("[DT] SNS 문장 %d 행 반영 완료" % len(names))


main()
