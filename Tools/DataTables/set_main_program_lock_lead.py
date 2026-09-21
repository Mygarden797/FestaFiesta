"""DT_TimeSettings 의 MainProgramLockLeadSeconds 를 바꾼다.

이 값이 곧 **관객 집결 리드타임**이다. `IsMainProgramLocked()` 가 공연 시각에서 이만큼을
빼서 잠금 구간을 잡고, 그 진입에서 `OnMainProgramStarted` 가 나간다. Crowd 는 그 신호에서
Audience 정원만큼을 무대로 보낸다 — 즉 NPC 가 걸어올 시간이 이 값이다.

보행 1.5 m/s 기준 대략적인 도달 반경: 10초 15m / 30초 45m / 60초 90m.
`IsMainProgramLocked` 은 표의 첫 행만 읽지만 GetAllRows 순서는 보장되지 않으므로 전 행을 맞춘다.

    UnrealEditor-Cmd.exe FestaFiesta.uproject -run=pythonscript
        -script="Tools/DataTables/set_main_program_lock_lead.py"
"""
import unreal

DT = "/Game/FestaFiesta/Data/Time/DT_TimeSettings"
NEW_LEAD_SECONDS = 60.0

COLUMNS = ("TimeScaleValue", "MainProgramLockLeadSeconds", "NightLightingTriggerSeconds")


def main():
    table = unreal.load_asset(DT)
    assert isinstance(table, unreal.DataTable), DT

    names = [str(n) for n in unreal.DataTableFunctionLibrary.get_data_table_row_names(table)]
    columns = {
        c: list(unreal.DataTableFunctionLibrary.get_data_table_column_as_string(table, c))
        for c in COLUMNS
    }
    for c in COLUMNS:
        assert len(columns[c]) == len(names), (c, len(columns[c]), len(names))

    lines = ["Name," + ",".join(COLUMNS)]
    for index, name in enumerate(names):
        values = [columns[c][index] for c in COLUMNS]
        before = values[1]
        values[1] = "%.6f" % NEW_LEAD_SECONDS
        unreal.log_warning("[TimeSettings] %s: MainProgramLockLeadSeconds %s -> %s"
                           % (name, before, values[1]))
        lines.append(name + "," + ",".join(values))

    if not unreal.DataTableFunctionLibrary.fill_data_table_from_csv_string(table, "\n".join(lines)):
        raise SystemExit("DT_TimeSettings 갱신 실패 — 열 구성을 확인할 것")
    assert unreal.EditorAssetLibrary.save_loaded_asset(table, only_if_is_dirty=False), "저장 실패"

    after = list(unreal.DataTableFunctionLibrary.get_data_table_column_as_string(
        table, "MainProgramLockLeadSeconds"))
    assert all(abs(float(v) - NEW_LEAD_SECONDS) < 1e-3 for v in after), after
    unreal.log_warning("[TimeSettings] %d 행 반영 완료" % len(names))


main()
