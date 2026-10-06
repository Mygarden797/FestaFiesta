"""프로그램 시설 버튼·SNS 필터의 글자 칸만 버튼 폭 전체로 바꾼다. 다른 위젯·HUD 에셋은 저장하지 않는다."""
import time
import unreal

try:
    unreal.SystemLibrary.execute_console_command(None, 'FF.FixButtonLabelWrap')
except Exception as error:
    unreal.log_error('FF_BUTTON_LABEL_WRAP_SCRIPT_FAIL %s' % error)

exit_after = time.monotonic() + 2.0


def finish(_delta):
    if time.monotonic() < exit_after:
        return
    unreal.unregister_slate_post_tick_callback(handle)
    unreal.SystemLibrary.quit_editor()


handle = unreal.register_slate_post_tick_callback(finish)
