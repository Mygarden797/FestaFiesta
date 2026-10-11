# UI 1단계 문구 수정

홍보의 오래된 실제 유입 미연결 안내, 점검·시설의 수수료 용어, 병목 사건 선택지 `1`을 현재 동작에 맞는 표시로 바꾼다. C++·빌더의 문구 수정과 함께 적용한다.

- 에디터를 닫고 Development Editor 빌드 후 `run.ps1` 실행.
- WBP 4개는 알려진 이전 문구만 교체하고 구조·폰트·색을 보존한다. 전체 HUD를 재생성하지 않는다.
- `FF_ID_NPCCrowded.Options[0].Label`만 `스태프 현장 출동`으로 변경한다. 비용·인원·시간·보상 필드는 그대로 둔다.
- 백업은 `Saved/UIBackups/CopyPhase1`, 적용 결과는 `Saved/UiCopyPhase1/apply-report.json`에 남긴다.
- 개인 설정·세이브는 실행 전 백업하고 종료 후 복원한다. 실제 표시·줄바꿈은 플레이 화면에서 확인한다.
