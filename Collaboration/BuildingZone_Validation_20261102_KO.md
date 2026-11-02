# 건설·구역 코드 정리 최종 검증 — 2026-11-02

**관련 Unreal 자동화 578개 전부 통과. 실패·미실행·실행 중 항목 0개.**

## 최종 결과

| 검증 | 결과 |
| --- | --- |
| Unreal 5.7.4 Win64 Development Editor | 성공, 15.35초 |
| Win64 Development Game | 성공, 24.13초 |
| Python 빌드·모듈 경계 도구 테스트 | 41/41 통과 |
| Unreal 자동화 | 544개 통과 + 경고가 있는 34개 통과 = 578개 |
| FF_MapOverview | 21/21 통과 |
| FF_ZoneAuthoring | 102/102 통과 |
| FF_ZoneSystem | 168/168 통과 |
| 원래 보고한 실패 25개 | 모두 최종 리포트에서 Success |
| 파일명 Codex 제거 | 소스·도구 63개 + 에셋 34개 = 97개, 관리 대상 파일명 잔여 0개 |

경고가 있는 34개 테스트도 Success 상태다. 자세한 경고와 각 테스트 결과는 아래 리포트에 보존한다.

## 해결한 코드·테스트·콘텐츠 계약

- 시설 검사 시 카탈로그가 함께 열리는 실제 상태에 이동·드래그·편집 허용 조건을 맞췄다. 월드 드래그·단축키의 입력 포커스도 월드 뷰포트로 돌려준다. 미리보기 중 동작 제한과 철거 확인은 유지한다.
- 건축 도움말은 현재의 오른쪽 카드와 실제 중첩 아이콘을 대상으로 입력 영역·반응형 배치·본문·닫기·스크롤을 검증한다. 제거된 확인 도크나 숨겨진 구형 버튼을 역참조하지 않는다.
- 도감의 현재 2열 병렬 목록·상세 화면, 검색 Overlay, 미리보기 뒤로가기와 입력 소유권을 검증한다. 맞닿는 외부 타일 슬롯은 허용하면서 실제 겹침과 잘못된 줄 배치는 계속 거부한다.
- 시설 상세 화면은 실제 Designer 계약, 미지원 탭의 표시·비활성 정책, 고정 헤더·행동 영역, 스크롤 도달성을 확인한다. 수익 안내 3줄은 장식 Border 대신 실제 텍스트 전체가 첫 화면 안에 있는지 검증한다.
- 스태프 마이그레이션은 현재 UniformGrid 탐색을 보존한다. 구형 구조 복구 테스트도 위젯 이름 변경과 Blueprint GUID 대응표를 함께 갱신해 반복 적용·GUID 보존을 확인한다.
- 관객 전용 메인 무대의 콘텐츠 검증은 공용 UsesServiceQueue() 규칙을 사용한다. 유효한 프로젝트 소분류를 테스트 시설 ID에 적용하고, 배치 실패 알림은 실제 로컬 플레이어가 없는 테스트 환경에 맞춰 먼저 준비한다.
- 누락된 카탈로그 5열×2행 아이콘 atlas·음식 시설 기본 이미지·배경 텍스처 3개를 현재 저작된 HUD 아이콘과 팔레트를 재사용해 복구했다. 생성기는 기존 에셋을 덮어쓰지 않는다.

## 애니메이션 실행 순서 문제

UE 5.7에서는 플랫폼 압축 데이터가 비어 있어도 IsCompressedDataValid()가 참일 수 있다. 이 상태에서는 압축 코덱이 없어 참조 포즈가 반환됐다. 단독 실행과 전체 실행의 차이는 그래프나 원본 클립의 손상이 아니라 압축 결과 준비·게시 시점이었다.

테스트는 실제 그래프가 사용하는 모든 Sequence·BlendSpace 샘플의 현재 플랫폼 데이터를 CacheDerivedDataForCurrentPlatform()으로 준비하고, 압축 트랙·코덱·데이터가 실제로 존재하는지 확인한 뒤 측정한다. 메시 컴파일 대기, 초기 10프레임 준비, 프레임 카운터 복원도 수행한다. 29개 외형 × 정지·이동 58개 경우의 실제 압축 애니메이션 움직임을 기존 0.5cm 기준으로 확인하며, raw 데이터로 통과시키거나 기준을 낮추지 않았다.

## 파일명과 호환성

파일명에서 Codex 문자열만 제거하고 중복·끝 구분자를 정리했다. include와 생성 헤더 이름, 코드·설정·제작 도구의 파일·에셋 경로도 갱신했다. 조립형 머티리얼 이름의 구분자는 별도로 확인했다.

에셋 34개는 AssetTools로 동일 객체를 이름 변경하고 관련 패키지를 재저장했다. 각 대상의 클래스·최종 경로를 확인했으며 참조가 없는 리다이렉터 34개만 제거했다. 기존 반영 타입명·시설 ID·프로필 키·자동화 이름은 유지한다. 변경 전 파일과 참조 패키지 백업은 Saved/SourceOrganization/FixContracts에 있다.

자동화 등록 1,348개는 정리 전·후 동일하다. 파일 이동·이름 변경에는 코드 소유권 목록의 현재 경로와 이동 목적지도 함께 갱신했다.

## 실행 근거

- [최종 자동화 리포트](../Saved/SourceOrganization/Validation-63186fa925224bc6a96e80100ba50f93/Report/index.json)
- [최종 자동화 로그](../Saved/SourceOrganization/Validation-63186fa925224bc6a96e80100ba50f93/Automation.log)
- [에디터 빌드](../Saved/BuildDiagnostics/EditorBuild.log)
- [게임 빌드](../Saved/SourceOrganization/FixContracts/GameBuildFinal.log)
- [파일명 검증](../Saved/SourceOrganization/FixContracts/final_filename_audit.json)
- [에셋 읽기 재검증](../Saved/SourceOrganization/FixContracts/asset_readback.json)
- [참조 저장·리다이렉터 검증](../Saved/SourceOrganization/FixContracts/AssetFilenameRename/fixup-result.json)
