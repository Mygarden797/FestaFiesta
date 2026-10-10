# 통합 소식

- 에디터 콘솔: `FF.ApplyUnifiedNews`
- 일회용 에디터 적용/바인딩 보존 검사: `apply.py`
- 통합 Python 실행에서 `runpy.run_path(path, init_globals={'FF_BATCH_APPLY': True})`로 부르면 에디터 종료를 요청하지 않는다.
- 원본 백업: `Saved/UIBackups/UnifiedNews/`; 결과: `Saved/UnifiedNews/apply-report.json`.

대상은 기존 소식 도크·SNS 펼침 창·SNS 펼침 행 3개다. 원본 트리를 갈지 않고 제목, 필터 스타일/라벨, 유형 배지만 수정한다. 이미 `FF_UnifiedNewsVersion=1`인 에셋은 다시 덮지 않는다.

표시 문구·글꼴·크기·위치·행 간격은 각 WBP Designer에서 수정한다. `WBP_FF_LiveSnsFlyout` Class Defaults의 `Unified News`가 켜지면 기존 버튼 바인딩 `Positive`/`Negative`는 각각 SNS/운영 알림 필터로 동작한다. 기존 하단 SNS 패널은 이 옵션이 꺼져 있어 긍정/아쉬움 필터가 유지된다.

두 원본은 공통 타임스탬프를 제공하지 않는다. 도크 구독 후 실시간 수신 순서를 보존하며, 도크 생성 전에 있던 기록은 과거 목록으로 복원한다. 가짜 분 단위 시각은 표시하지 않는다. 필터 밖의 새 소식은 해당 유형의 미읽음 수와 도크 숫자로 남는다. 새 글 자동 표시·빨간 점·스크롤 위치 보정은 기존 동작을 사용한다.

오른쪽 운영 알림은 통합 소식 도크가 실제로 생성되었을 때 지난 알림 진입점/목록을 숨기고 짧은 토스트만 유지한다. 도크가 없는 기존 화면은 기존 지난 알림을 유지한다. 출동 사건 UI와 운영 판정 시스템은 수정하지 않는다.
