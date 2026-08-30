# 미니게임 사운드 교체 가이드

미니게임 사운드는 각 미니게임의 Definition Data Asset에서 관리한다.

- `Content/FestaFiesta/Minigame/Data/DA_Minigame_TargetShooting`
- `Content/FestaFiesta/Minigame/Data/DA_Minigame_Surfing`
- `Content/FestaFiesta/Minigame/Data/DA_Minigame_ClawMachine`

에디터에서 Data Asset을 열고 `사운드 > 미니게임 사운드`를 편집한다.

- `배경 음악`: `Sound Wave`, `Sound Cue`, `MetaSound Source` 중 하나를 지정한다.
- `게임 중 반복 재생`: 기본값은 켜짐이다. 일반 Sound Wave처럼 자체 루프가 없는 음원도 미니게임 세션이 끝날 때까지 자동 재시작한다.
- `이벤트별 효과음`: 이벤트 행의 `사운드 에셋`만 교체한다. 코드 수정은 필요 없다.
- `볼륨`, `최소/최대 피치`, `최소 재생 간격`: 이벤트별 믹싱과 반복감을 조정한다.
- `동시 재생 정책`: 같은 효과음의 보이스 수 제한이 필요하면 `Sound Concurrency` 에셋을 지정한다.
- `내장 폴백 톤`: 최종 에셋이 없는 동안 실제 재생되는 임시 톤이다. 최종 납품 뒤 전체 또는 이벤트별로 끌 수 있다.

런타임은 게임 시작 시 프로필의 soft reference를 한 번 미리 로드한다. 배경 음악 컴포넌트는 오디오 서브시스템이 세션 전체에 걸쳐 소유하고, 음원이 끝나면 다시 시작하며, 완료·취소·화면 종료 시에만 페이드아웃 후 해제한다. Sound Cue를 사용할 때는 일반 `Looping` 노드로 Wave Player를 감싸지 말고 **Wave Player 노드의 `Looping` 옵션을 직접 켠다.** 이것이 Unreal에서 무한 배경음악을 끊김 없이 반복하는 방식이다. MetaSound는 Wave Player의 `Loop` 입력 또는 명시적인 루프 구간을 사용한다. 화면 코드는 `발사`, `명중`, `코인 획득` 같은 의미 이벤트만 전달하며 에셋 경로, 볼륨, 피치, 동시재생 정책을 알지 않는다.

새 미니게임 이벤트를 추가할 때는 다음 순서를 지킨다.

1. `EFF_MinigameAudioEvent` 끝의 예약된 숫자 구간에 새 값을 추가한다. 기존 숫자는 변경하지 않는다.
2. 새 Definition 생성자에서 `AudioProfile.EventSounds.FindOrAdd(...)`로 기본 행을 노출한다.
3. 런타임 스냅샷의 단조 증가 이벤트 시리얼을 감지한 시점에 `PlayMinigameSound(...)`를 한 번 호출한다.
4. 필요하면 `GetToneSpec`에 폴백 톤만 추가한다.

게임 규칙과 결과 판정에서는 직접 `PlaySound2D`를 호출하지 않는다. 세션/BGM 수명은 `UFF_MinigameAudioSubsystem`, 이벤트 감지는 각 화면, 실제 음색은 Definition Data Asset이 각각 담당한다.
