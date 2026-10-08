"""DT_FacilityRevenues 의 1회 매출(RevenuePerUseGold)에 배수를 곱한다. 0(무료 시설)은 그대로 둔다.

에디터에서 고친 값을 잃지 않도록 CSV 원본이 아니라 지금 표를 내보내서 그 칸만 바꿔 다시 넣는다.
같은 배수를 두 번 돌리면 두 번 곱해진다 — 로그의 전후 값을 확인할 것.

    UnrealEditor-Cmd.exe FestaFiesta.uproject -run=pythonscript
        -script="<abs>/Tools/DataTables/scale_booth_revenue.py 1.5"
"""
import csv
import io
import sys

import unreal

DT = "/Game/FestaFiesta/Data/Economy/DT_FacilityRevenues"


def main():
    factor = float(sys.argv[1]) if len(sys.argv) > 1 else 1.0
    table = unreal.load_asset(DT)
    assert isinstance(table, unreal.DataTable), DT
    rows = list(csv.reader(io.StringIO(unreal.DataTableFunctionLibrary.export_data_table_to_csv_string(table))))
    column = rows[0].index("RevenuePerUseGold")
    before = after = 0
    for row in rows[1:]:
        if not row:
            continue
        value = int(row[column])
        scaled = int(round(value * factor))
        before += value
        after += scaled
        row[column] = str(scaled)
    out = io.StringIO()
    csv.writer(out, lineterminator="\n").writerows(rows)
    if not unreal.DataTableFunctionLibrary.fill_data_table_from_csv_string(table, out.getvalue()):
        raise SystemExit("CSV 파싱 실패")
    unreal.EditorAssetLibrary.save_loaded_asset(table)
    unreal.log("[DT] 1회 매출 x%.2f — 합계 %d -> %d (%d 행)" % (factor, before, after, len(rows) - 1))


main()
