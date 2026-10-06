"""SNS 새 글 자동 표시 정리: 글 행에 새 글 빨간 점(`Image_NewDot`)을 넣고 `새 글 보기` 버튼을 접는다.

`FF.ApplyLiveSnsNewDot` 만 실행한다. 이미 점이 있으면 건너뛰어 Designer 편집을 지킨다.
"""
import time
import unreal

try:
    unreal.SystemLibrary.execute_console_command(None, 'FF.ApplyLiveSnsNewDot')
except Exception as error:
    unreal.log_error('FF_LIVE_SNS_NEWDOT_SCRIPT_FAIL %s' % error)

exit_after = time.monotonic() + 2.0


def finish(_delta):
    if time.monotonic() < exit_after:
        return
    unreal.unregister_slate_post_tick_callback(handle)
    unreal.SystemLibrary.quit_editor()


handle = unreal.register_slate_post_tick_callback(finish)
