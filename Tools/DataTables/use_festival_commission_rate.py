"""DT_FacilityRevenues 의 행별 수수료율 덮어쓰기를 끈다. 다른 값(이름·1회 요금·레벨 배수)은 그대로 둔다.

모든 부스가 UFF_FestivalDefinition::BoothCommissionRate 하나를 따르게 해서 수수료율을 한 곳에서 맞춘다.
에디터에서 고친 값을 잃지 않도록 CSV 원본이 아니라 지금 표를 내보내서 그 칸만 바꿔 다시 넣는다.

    UnrealEditor-Cmd.exe FestaFiesta.uproject -run=pythonscript
        -script="<abs>/Tools/DataTables/use_festival_commission_rate.py"
"""
import csv
import io

import unreal

DT = "/Game/FestaFiesta/Data/Economy/DT_FacilityRevenues"


def main():
    table = unreal.load_asset(DT)
    assert isinstance(table, unreal.DataTable), DT
    text = unreal.DataTableFunctionLibrary.export_data_table_to_csv_string(table)
    rows = list(csv.reader(io.StringIO(text)))
    header = rows[0]
    column = header.index("bOverrideCommissionRate")
    changed = 0
    for row in rows[1:]:
        if row and row[column].strip().lower() == "true":
            row[column] = "False"
            changed += 1
    out = io.StringIO()
    csv.writer(out, lineterminator="\n").writerows(rows)
    if not unreal.DataTableFunctionLibrary.fill_data_table_from_csv_string(table, out.getvalue()):
        raise SystemExit("CSV 파싱 실패")
    unreal.EditorAssetLibrary.save_loaded_asset(table)
    unreal.log("[DT] 수수료율 덮어쓰기 해제 %d / %d 행" % (changed, len(rows) - 1))


main()
