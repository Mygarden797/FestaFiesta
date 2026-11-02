# 프로젝트 구역 연동

`Zone`은 프로젝트 시스템과 `FF_ZoneSystem`을 연결하는 어댑터 계층이다. 시설·구역 상태의 권위는 플러그인에 있으며, 프로젝트의 Stage·Party Queue·Crowd 데이터를 이 계층에서 변환한다.

| 공개 진입점 | Private 구현과 테스트 | 책임 |
| --- | --- | --- |
| `FF_ZoneSystemStageBridgeSubsystem.h` | `Zone/Stage`, `Zone/Stage/Tests` | 축제 단계에 따른 배치 권한과 시설 자동 개폐 |
| `FF_ZoneSystemQueueBridgeSubsystem.h` | `Zone/Queue`, `Zone/Queue/Tests` | 시설 ID·대기열 동선 투영과 점유 정책 |
| `FF_ZonePopulationBridgeSubsystem.h` | `Zone/Population`, `Zone/Population/Tests` | 시뮬레이션 시계를 기준으로 인원을 집계·보고 |
| `FF_ZoneLayoutAuthoringLibrary.h`, `FF_ZoneLayoutSaveGame.h` | `Zone/Layout` | 반복 측정용 배치 저장과 에디터 제작 요청 |
| 구역 접근 계약 | `Zone/Access/Tests` | 프로젝트 스폰·출구와 플러그인 접근 API의 연결 검증 |
| 여러 브리지를 함께 사용하는 계약 | `Zone/Tests` | 실제 테스트 월드에서 단계·대기열 통합 검증 |

공개 헤더 경로와 리플렉션 이름은 Blueprint·다른 모듈의 계약이므로 유지한다. 테스트는 관련 구현 아래 `Tests`에 두고, 런타임 디버그 도구와 자동화 테스트를 구분한다. 시간 HUD의 hit-test 검증은 구역 로직이 아니므로 `Private/Time/Tests`에서 관리한다.

변경 시 확인할 불변식:

- 시설 수명주기는 ZoneSystem, 실제 일행의 대기·서비스는 프로젝트 Queue가 관리한다. 한 시설에 두 대기열 참가 API를 섞어 쓰지 않는다.
- Snapshot 복원은 시설 액터를 교체할 수 있으므로 Queue 등록과 Stage 권한을 동기화한다.
- 인구가 0인 등록 구역도 보고해야 기존 혼잡 값이 남지 않는다. 보고 경과 시간은 실시간이 아닌 Crowd 시뮬레이션 시계에서 구한다.
- 제작용 배치 슬롯은 플레이어 진행 저장과 별개다. `WITH_EDITOR` 제작 함수와 비 Shipping 콘솔 명령을 배포용 게임 흐름에 연결하지 않는다.
