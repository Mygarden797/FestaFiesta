"""라이브 스태프 정원 막대 2개만 칸 전체로 늘린다. 다른 위젯·HUD·셋업 에셋은 저장하지 않는다."""
import time
import unreal

try:
    unreal.SystemLibrary.execute_console_command(None, 'FF.FixLiveStaffCapacityBar')
except Exception as error:
    unreal.log_error('FF_LIVE_STAFF_BAR_SCRIPT_FAIL %s' % error)

exit_after = time.monotonic() + 2.0


def finish(_delta):
    if time.monotonic() < exit_after:
        return
    unreal.unregister_slate_post_tick_callback(handle)
    unreal.SystemLibrary.quit_editor()


handle = unreal.register_slate_post_tick_callback(finish)
