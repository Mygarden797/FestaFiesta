"""피날레 카드·확인 창 그림 브러시 크기(0×0)를 원본 크기로 바로잡고, 카드를 임시로 만들어 확인한다."""
import time
import unreal

try:
    unreal.SystemLibrary.execute_console_command(None, 'FF.FixFinalePreviewBrushSize')
    unreal.SystemLibrary.execute_console_command(None, 'FF.DiagnoseFinaleCardImage')
except Exception as error:
    unreal.log_error('FF_FINALE_PREVIEW_SCRIPT_FAIL %s' % error)

exit_after = time.monotonic() + 2.0


def finish(_delta):
    if time.monotonic() < exit_after:
        return
    unreal.unregister_slate_post_tick_callback(handle)
    unreal.SystemLibrary.quit_editor()


handle = unreal.register_slate_post_tick_callback(finish)
