"""data/unreal/utility_coefficients_v0.csv 를 DT_UtilityCoefficients 에 다시 넣는다.

CSV 와 DataTable 이 갈라지면 코드 기본값이 아니라 표의 낡은 값이 이긴다
(UFF_CrowdSimulationSubsystem::OnWorldBeginPlay 가 표로 멤버를 덮는다).
CSV 만 고치고 이 스크립트를 안 돌리면 게임 동작은 그대로다.

    UnrealEditor-Cmd.exe FestaFiesta.uproject -run=pythonscript
        -script="Tools/DataTables/reimport_utility_coefficients.py"
"""
from pathlib import Path

import unreal

ROOT = Path(__file__).resolve().parents[2]
CSV = ROOT / "data/unreal/utility_coefficients_v0.csv"
DT = "/Game/FestaFiesta/Data/UtilityAI/DT_UtilityCoefficients"

# 값을 눈으로 확인하고 싶은 행. 갱신이 실제로 반영됐는지 여기서 갈린다.
WATCHED = ("reservation_return_buffer_minutes", "reservation_exit_buffer_minutes")


def main():
    table = unreal.load_asset(DT)
    assert isinstance(table, unreal.DataTable), DT

    text = CSV.read_text(encoding="utf-8-sig")
    if not unreal.DataTableFunctionLibrary.fill_data_table_from_csv_string(table, text):
        raise SystemExit("CSV 파싱 실패 — 열 구성이 FFF_UtilityCoefficientRow 와 맞는지 확인할 것")

    unreal.EditorAssetLibrary.save_loaded_asset(table)

    names = [str(n) for n in unreal.DataTableFunctionLibrary.get_data_table_row_names(table)]
    values = list(unreal.DataTableFunctionLibrary.get_data_table_column_as_string(table, "DefaultValue"))
    assert len(names) == len(values), (len(names), len(values))
    lookup = dict(zip(names, values))
    for row in WATCHED:
        assert row in lookup, "행이 사라졌다: %s" % row
        unreal.log("[DT] %s = %s" % (row, lookup[row]))
    unreal.log("[DT] %d 행 반영 완료" % len(names))


main()
