# 건축모드 Clarity UI

기존 세이지·포레스트 그린 색감을 유지하면서 화면 중앙의 건축 공간을 확보하는 개선이다.
기존 Aurora 작업과 게임 데이터는 보존하고, 마이그레이션 스크립트에 명시한 대상 WBP와 신규 재질을 갱신한다.

## 사용

- **건축 목록:** 하단 중앙 6개 카드. 좌우 드래그, 마우스 휠, 페이지 버튼으로 탐색한다.
  필터 없는 `전체 시설`에서는 미설치 필수 시설, 설치 완료된 필수 시설, 일반 시설 순으로 표시한다.
  검색·카테고리·즐겨찾기 등 명시적인 필터의 결과와 기존 순서는 유지한다.
  이동한 포인터는 카드 클릭으로 처리하지 않는다. 즐겨찾기는 독립 버튼이다.
  카드 클릭을 놓으면 바로 배치 미리보기가 열린다. 별도의 `배치 시작` 단계는 없다.
  금액은 카드와 배치 요약에서 확인하며, 실제 설치 확정 시에만 차감한다.
  방향키 이동과 프로그램에서 요청한 시설 포커스는 미리보기를 시작하지 않는다.
  프로그램 시설은 라벤더·청색 펄 썸네일과 `프로그램` 배지로 구분한다.
  1일차 필수 시설은 같은 구성의 붉은 펄 카드와 필수 배지를 사용한다.
  필수/프로그램 배지는 썸네일 왼쪽 아래에 정렬하며, 필수 설치 수와 완료 상태는 유지한다.
  매출 순위와 도감 버튼은 동일한 112×40 크기·글꼴·여백을 사용한다.
- **배치:** 왼쪽에는 선택 시설·금액·상태, 오른쪽 아래에는 목록·확정만 유지한다.
  `시설 규격`은 크기/안전 여유/대기열을 펼친다.
- **직접 조정 T:** 현재 프리뷰를 고정하고 화면 크기가 일정한 월드 XY 화살표·회전 링을 표시한다.
  화살표를 드래그하면 선택한 1m/25cm/1cm 간격으로 이동하고, 링은 45° 회전 또는 Shift 자유 회전이다.
  직접 조정에서는 Enter 또는 `배치 확정`으로만 설치한다. `T`/`위치 다시 선택`은 커서 배치로 돌아간다.
  UI 진입·일시정지·포커스 상실로 끊긴 드래그는 취소하고, 목록 이탈 시 핸들을 정리한다.
- **배치 원인:** 시설/장애물 겹침, 안전 여유, 대기열, 보호 통로, 경계, 경사, 자금·한도를 구분한다.
  짧은 원인과 해결 안내를 함께 표시하고, 원인이 여러 개면 `다른 원인`으로 스크롤 없이 확인한다.
  실제 검증에서 얻은 최대 12개 방해 대상의 이름·윤곽과 실패 영역의 붉은 면·해칭을 표시한다.
- **가까운 빈자리:** 위치 이동으로 해결할 수 있는 경우에만 제공한다. 현재 방향과 스냅을 유지해
  6m 이내의 제한된 후보를 가까운 순서로 탐색하며, 프레임당 최대 4회/약 2ms 검사 예산을 사용한다.
  단일 검증의 실행 시간을 강제로 중단하지는 않는다. 모든 후보는 기존 지면·충돌·정책 검증을 통과해야 한다.
  찾은 위치는 고정된 미리보기로 표시한다. 시설 설치·비용 차감은 별도 확정에서만 수행한다.
  모든 좌표를 전수 탐색하지 않으며, 후보를 못 찾으면 회전 또는 위치 재선택을 안내한다.
  자금 부족·설치 한도·단계 제한은 이동으로 해결되지 않으므로 탐색하지 않는다.
- **도구 R 제거:** 요청에 따라 버튼·원형 메뉴·하위 정밀 패널과 R 단축키 진입을 제거했다.
  Q/E 회전, Shift 자유 회전, Ctrl/Alt 간격, G 격자 및 T 직접 조정은 계속 사용한다.
  과거 Blueprint의 `ToggleRadialMenu` 호출은 호환성을 위해 남지만 메뉴를 열지 않는다.
- **조작 안내:** 시설 설치·이동에 진입하면 왼쪽 요약 바로 아래에 기본으로 표시한다.
  `안내 닫기`로 닫고 `조작 안내`로 다시 연다. 닫은 뒤 회전·금액·커서 갱신으로 다시 열리지 않으며,
  목록으로 돌아갔다가 새 배치를 시작하면 기본 안내를 다시 표시한다.
  도움말과 조합 미리보기는 서로 다른 영역을 사용한다.
- **매출 순위:** 폭 500, 시설 수에 따라 높이 378~600으로 조절한다.
  기간·총액·시설별 매출·비중을 간결하게 표시하고 선택 시설 정보로 바로 이동한다.
- **조합 도감:** 화면 중앙의 제한된 크기, 6열 고정. 조합 수가 늘면 세로 행이 늘어나며
  목록 영역만 스크롤된다. 상세는 조합/배치 조건/현재 효과 버튼으로 확인한다.
  도감이 열린 동안 건축 목록을 숨기며, 닫으면 기존 검색·페이지·선택 상태를 유지한다.
- **시설 정보:** 기본·조합·운영·강화·매출·프로그램 페이지를 버튼으로 직접 선택한다.
  일반 정보창은 논리 크기 456×620이며, 조합 정보는 이전/다음 버튼으로 한 항목씩 읽는다.
  프로그램 페이지에는 절제된 라벤더 펄 표면을 적용한다.
- **조명 편집:** 논리 크기 480×620으로 일반 정보창과 비슷한 면적을 사용한다.
  기본 화면은 190×190 색상 휠, 원래 색/청록/노을/보라 프리셋, HEX 입력, 밝기 슬라이더와
  50%/100%/150% 밝기 버튼을 제공한다. `정밀 값`은 같은 영역에 HSV·RGB 입력을 표시하고,
  `색상 휠`로 기본 화면에 돌아온다. 조명 편집 중에는 일반 정보 탭을 접어 조작 공간을 확보한다.
  색상과 밝기는 실시간으로 미리 보며 `적용`하면 확정하고 `취소`하면 편집 시작 상태로 복원한다.
  `장면 확인`/`접기`는 상세를 접고 시설명·색상·밝기·카메라 조작 안내를 남긴다.
  다시 펼쳐도 드래프트가 유지되며, 다른 시설을 선택하면 이전 시설의 미확정 변경을 되돌린다.
- **유료 프로그램 편성:** 일반 정보창과 같은 456×620 크기를 사용한다.
  목록의 작은 카드를 누르면 목록 대신 해당 프로그램의 시간·인력·정원·참가비·최대 매출·설명과
  구매 비용·구매 후 자원을 표시한다. `‹ 목록`은 선택을 해제하고 목록으로 돌아간다.
  호환 프로그램이 하나면 상세를 바로 열지만, 실제 구매는 금액이 적힌 별도 확정 버튼으로만 요청한다.
  당일 취소·환불 불가 및 편성 후 시설 조작 제한 안내와 구매 완료 영수증을 유지한다.

## 효과와 입력

- 얇은 광학 간섭 테두리, 낮은 투명도의 세이지 글라스 표면. 움직이는 무지개 효과나
  배경 전체 블러를 사용하지 않는다. 중요한 텍스트 아래는 높은 불투명도를 유지한다.
- 제거된 원형 도구의 SDF 재질은 과거 에셋 참조 호환성을 위해 보존하되 화면에서 사용하지 않는다.
  패널 모서리는 실제 화면 비율을 기준으로 계산한 SDF를 사용한다.
- 상황 마커는 배치 대상 옆의 빈 영역을 선택한다. 도구·요약·하단 HUD·조합 안내와 겹칠 수 있으면 숨기고,
  동일 상태는 요약 패널에서 계속 확인한다. 마커는 입력을 받지 않는다.
- 그리드는 화면에 투영된 간격에 따라 먼 잔선을 줄인다. 선택 시설 주변의 상세 격자와
  시설·대기열·안전 여유 표시는 유지하며 배치 단위나 충돌 판정은 바꾸지 않는다.
- 연결된 컨트롤러에는 메뉴 선택/설치 확정/무효 배치 시 짧은 진동을 보낸다.
  플레이어의 Force Feedback 설정을 존중하며 `FF.UI.BuildHaptics 0`으로 비활성화할 수 있다.
  진동 호출은 120ms 간격으로 제한한다. 실제 하드웨어 진동은 별도 기기 확인이 필요하다.
- 카탈로그 11개 선택지에는 부스·공연·휴게·편의·안전 등을 나타내는 원본 SDF 픽토그램을 사용한다.
  한글 이름과 기존 분류 ID·키보드 이동·6개 카드 가상화는 유지한다.
- 배치 고스트에는 원본 재질 위에 유효/불가 상태색을 얹는다. 이 임시 컴포넌트만 Nanite fallback을
  사용하며, 설치된 시설이나 원본 메시·재질·맵·전역 포스트프로세스는 변경하지 않는다.
  깊이에 가려지지 않는 핸들·장애물 모서리와 투명 고스트로 가려진 배치 위치도 확인할 수 있다.
  현재 권위 모델은 시설의 평면 이동·회전이다. 벽 돌출·방 자동 감지·소켓 부착 편집기를 추가한 것은 아니다.

## 영상 참고

[아일의 심즈4 집짓기 영상](https://www.youtube.com/watch?v=Ye5bAENV-EE)의 약 0:26, 17:17,
27:39, 31:06 장면을 직접 확인했다. 하단의 썸네일 중심 목록, 카테고리 탐색, 선택 직후의
배치 미리보기, 선택 대상 가까이에 나타나는 색상 도구, 넓게 비워진 중앙 작업 공간을
이번 구성에 반영했다. 게임의 세이지·포레스트 색상과 기존 기능은 유지한다.

## 반영 및 검증

1. UE 5.7 `FestaFiestaEditor`를 빌드한다.
2. Entry 맵에서 `Tools/BuildModeClarity/apply_and_verify.py`를 실행한다.
3. 스크립트는 변경 전 현재 에셋을 `Saved/BuildModeClarity/before_assets`에 보관하고,
   재질 생성 후 `apply_and_verify.py`의 `COMMANDS` 순서로 저장 WBP를 갱신한다.
4. 대상 외 UI 해시와 두 번째 실행의 멱등성을 검사한다.
5. 관련 자동 검사는 `FestaFiesta.Building.BuildTools`, `FestaFiesta.UI.PrepBuild`,
   `FestaFiesta.UI.FacilityInfo`, `FestaFiesta.FacilityCodex.UI`에 있다.

### 전체 목록 필수 시설 우선·도구 R 제거 검증 · 2026-09-21

- `build-editor-required-first.log`, `build-game-required-first.log`: 에디터·게임 빌드 성공.
- `Tests-RequiredFirst/index.json`: 관련 검사 **100개 통과**, 실패/미실행 0개.
  경고 없는 통과 93개, 경고 포함 통과 7개(기존 경고 21건)다.
  필수 시설의 완료 상태별 우선순위, 필터·카드 객체 유지, 제거된 메뉴 재진입 차단,
  도움말·규격·배치 보조와 720p/1080p 레이아웃을 검증했다.
- `migration-required-first.log`: 멱등성 검사 통과. 이전 마이그레이션 결과와 비교해
  대상 중 `WBP_FF_BuildTools`만 변경됐으며, 대상 외 UI 432개는 동일하다.
  제거 직전 UI는 `Saved/BuildModeClarity/before-required-first`에 보관했다.
- `runtime-20260921-104057/report.json`: 실제 전체 목록 첫 카드가 미설치 필수 시설임을 확인.
  메뉴가 없는 배치 화면과 예전 메뉴 API를 호출해도 복원되지 않는 화면을 직접 검토했다.
  캡처 전후 맵·데이터·UI·설정 파일 해시는 동일하며, 시설 설치·에셋 저장은 호출하지 않았다.
- [전체 목록 첫 화면](../../Saved/BuildModeClarity/runtime-20260921-104057/01-catalog-Test_Main.png)
- [도구 R을 제거한 배치 화면](../../Saved/BuildModeClarity/runtime-20260921-104057/03-placement-default-help-Test_Main.png)

아래 검증 기록은 도구 R 제거 이전 개정의 기록이다.

### 필수 시설·배치 보조 최종 검증 · 2026-09-21

- UE 5.7 에디터와 게임 빌드 성공: `build-editor-assistance-final.log`, `build-game-assistance.log`.
- `Tests-Assistance/index.json`의 **118개 검사 모두 통과**: 경고 없는 통과 97개,
  경고 포함 통과 21개, 실패 0개. 기즈모 투영/드래그 계산/취소, 근처 후보의 경계와 스냅,
  월드 변경·일시정지·미확정 비용/시설 보존, 장애물 진단과 표시 정리, 카탈로그 가상화,
  필수 배지 재사용, 기본 도움말 및 720p/1080p 배치 HUD를 포함한다.
- `migration-assistance.log`의 `FF_BUILD_CLARITY_APPLY_VERIFY_PASS` 확인.
  대상 WBP 15개를 검증했고, 재실행 멱등성과 대상 외 UI 432개의 해시를 보존했다.
- 실제 `Test_Main` 캡처: `Saved/BuildModeClarity/runtime-20260921-094219`.
  준비용 이미지를 제외한 **13장 모두 직접 검토**했다. 실제 장애물을 향해 프리뷰를 이동하고,
  게임 카메라의 포커스 기능으로 붉은 고스트·실패 영역·윤곽과 원인 안내를 확인했다.
  가까운 유효 후보를 찾은 뒤 초록색으로 복원되며, 확정 전 월드 revision이 유지됐다.
  도움말 닫기/재열기/다음 배치의 기본 열림과 필수·프로그램 카드의 색상 구분도 확인했다.
- 캡처 전후 맵·데이터·UI·설정의 해시는 동일하다. 캡처는 공개 게임 액션을 사용하므로
  물리 마우스 드래그·컨트롤러 진동을 검증했다고 간주하지 않는다. 입력 계산과 Slate 드래그는 자동 검사로 확인했다.
- 결과 집계: `Saved/BuildModeClarity/assistance-validation.json`.
  앞선 캡처의 카메라 밖 프리뷰와 Python 카메라 API 오류 기록은 보존했으며, 최종 결과로 사용하지 않는다.

자동 검사에는 기존 LiveHUD 아이콘 5개 누락에서 10건, 테스트 월드의 Queue 부재 14건,
PlayerInput 부재 4건, 정산 참가자 부재 1건, Navigation/Crowd 설정·로그 4건,
빈 프로그램 스케줄 2건으로 총 35개 경고가 남아 있다.
실제 맵 시작 시 기존 6개 존의 출구 경로 연결 오류도 기록됐다. 해당 맵 내비게이션을
수정한 작업은 아니며, UI 검증 성공이 모든 존의 배치 가능성을 보증하지 않는다.

- [필수 시설의 붉은 카드](../../Saved/BuildModeClarity/runtime-20260921-094219/02a-required-catalog-Test_Main.png)
- [배치 원인과 방해 영역](../../Saved/BuildModeClarity/runtime-20260921-094219/04b-placement-problem-Test_Main.png)
- [가까운 유효 위치와 조정 핸들](../../Saved/BuildModeClarity/runtime-20260921-094219/04c-nearby-placement-Test_Main.png)
- [복수 원인과 원형 도구](../../Saved/BuildModeClarity/runtime-20260921-094219/05-placement-radial-Test_Main.png)

### 두 번째 개정 · 시설 정보/조명/프로그램

`FF.ApplyBuildModeClarityFacilityV2`는 기존 트리와 기능 바인딩을 보존하는 명시적 1회 보정이다.
이미 적용한 V2 표식이 있으면 같은 레이아웃을 재생성하지 않는다. 대상은
`/Game/FestaFiesta/UI/Facility/` 아래의 다음 4개 WBP다.

- `WBP_FF_FacilityInfoPanel`: `ClarityV2_Info` 표식, 프로그램 강조와 조명 편집 크기/탐색 전환.
- `WBP_FF_FacilityLightingControls`: `ClarityV2_Lighting` 표식,
  `Button_LightingPrecision`, `Panel_LightingBasic`, `Panel_LightingPrecision` 및
  `Button_LightingBrightnessLow/Normal/High` 선택적 바인딩.
- `WBP_FF_FacilityPaidProgramCard`: 작은 카드의 서체·여백과 프로그램 표면.
- `WBP_FF_FacilityPaidProgramPicker`: `Button_BackToPrograms` 바인딩,
  기존 `Size_ProgramList`·`Border_Preview`·`Text_Instruction`을 이용한 목록/상세 전환.

관련 자동 테스트는 `FestaFiesta.UI.FacilityInfo.` 접두사 아래에 있다.

| 테스트 | 확인하는 조건 |
| --- | --- |
| `AuroraLightingDraft` | 정밀/기본 전환만으로 조명 값을 쓰지 않음, 드래프트 유지, 밝기 프리셋, 적용·취소·선택 변경·잘못된 HEX 처리 |
| `AuroraLightingNavigation` | V2 조명 폭 480/높이 620, 접기·펼치기, 조명 진입 시 일반 탭 숨김 및 이탈 시 복원 |
| `LightingVisualPreview` | 실제 WBP의 기본·정밀 화면 렌더링, 190px 휠, 720p/1080p 기본 화면과 720p 정밀 화면의 상세 스크롤 초과 1px 이하 |
| `ClarityNavigation` | 매출/프로그램 직접 전환, 조합 이전/다음 경계와 항목 재사용, 사용할 수 없는 탭 숨김 |
| `InspectorV2PagesPreview` | 720p/1080p 일반 페이지의 스크롤 초과 1px 이하 및 헤더·본문·하단 동작 영역의 배치 |
| `PaidProgramWbpContract` | V2 우측 456×620 도킹, 기존 구매·자원·영수증 위젯 계약 유지 |
| `PaidProgramPurchaseFlow` | 목록 복귀 시 선택 해제, 별도 확정 전 구매 없음, 단일 프로그램 자동 선택, 중복 구매 방지와 영수증 유지 |
| `PaidProgramRenderedPreview` | 프로젝트 DPI의 720p/1080p 목록·상세·완료 화면, 카드 높이 128 이하, 상세 스크롤 초과 1px 이하, 버튼과 하단 HUD 여유 공간 |

렌더링 테스트에는 실제 RHI와 `-RenderOffscreen`이 필요하다. `-nullrhi` 실행에서는 화면 검사가
건너뛰어지므로 레이아웃 검증으로 간주하지 않는다. 위 항목은 각 테스트의 검사 조건이며,
실행 결과는 아래 기록을 따른다.

### 두 번째 개정의 최종 검증 · 2026-09-20

- UE 5.7 에디터와 게임 빌드 성공: `build-editor-v2-title.log`, `build-game-v2-title.log`.
- 자동 검사 **89개 모두 통과**: 경고 없는 통과 70개, 경고 포함 통과 19개, 최신 실패 0개.
  `Tests-V2` → `Tests-V2Fit` → `Tests-V2Header` → `Tests-V2Title` 순서로 테스트별 최신
  결과를 집계했다. 이전 실패 기록은 그대로 보존했다. 최신 집계는
  `Saved/BuildModeClarity/validation-summary.json`에 있다.
- 렌더 검사에서 발견한 카드 하단 잘림, 적은 시설 수의 매출 행 잘림, 헤더 글자 높이,
  도감의 긴 두 줄 제목을 수정한 뒤 해당 검사를 재실행했다. 선택 탭의 낮은 대비와
  프로그램 금액 줄바꿈도 보정했다.
- 저장 WBP 14개를 대상으로 루트 연결·필수 바인딩·재실행 멱등성을 검사했다.
  대상 외 UI 에셋 **433개**의 해시가 유지됐다. 최종 마이그레이션 로그는
  `migration-v2-title.log`이며 오류·ensure 없이 `FF_BUILD_CLARITY_APPLY_VERIFY_PASS`를 기록했다.
- 실제 `Test_Main`의 최종 캡처 8장:
  `Saved/BuildModeClarity/runtime-20260920-224113`. 모든 PNG를 직접 확인했고,
  캡처 전후 맵·데이터·UI·설정 파일 해시가 동일하다. 요청 1600×900, 실제 게임 viewport
  1598×890, 창 테두리를 포함한 PNG 1604×928이다.
- 조명 기본/정밀, 정보 기본/운영/강화/프로그램, 프로그램 구매 목록/상세/영수증,
  시설이 있는 매출 목록과 도감의 긴 제목은 저장된 실제 WBP의 렌더 검증 화면으로 확인했다.
  실제 맵 캡처에는 시설을 설치하지 않았으므로 매출 화면은 빈 상태다.

남은 경고 32건은 기존 LiveHUD 아이콘 에셋 5개 누락에서 10건, 테스트 입력 초기화에서 6건,
Queue가 없는 Editor 검증 월드에서 14건, 참가자 없이 프로그램 정산을 직접 호출하는 검사에서
2건이다. 아이콘 누락은 실제 공용 콘텐츠 문제이며 이번 변경에서 해결하지 않았다.
실제 컨트롤러의 진동 감각은 확인하지 않았다.

- [프로그램 시설 카탈로그](../../Saved/BuildModeClarity/runtime-20260920-224113/02-program-catalog-Test_Main.png)
- [왼쪽 조작 안내](../../Saved/BuildModeClarity/runtime-20260920-224113/04-left-placement-help-Test_Main.png)
- [원형 도구](../../Saved/BuildModeClarity/runtime-20260920-224113/05-placement-radial-Test_Main.png)
- [6열 조합 도감](../../Saved/BuildModeClarity/runtime-20260920-224113/06-combination-codex-Test_Main.png)
- [축소한 매출 순위](../../Saved/BuildModeClarity/runtime-20260920-224113/08-revenue-ranking-Test_Main.png)
- [조명 기본 화면](../../Saved/Automation/FacilityLightingPanelClarityV2_720.png)
- [조명 정밀 입력](../../Saved/Automation/FacilityLightingPrecisionClarityV2_720.png)

### 첫 번째 개정에서 확인된 결과 · 2026-09-20

아래 기록은 두 번째 개정 이전의 검증 결과다. 새 조명·프로그램 변경의 통과 근거로 사용하지 않는다.

- UE 5.7 **FestaFiestaEditor 빌드 성공**: `Saved/BuildModeClarity/build-editor-occlusion.log`.
- UE 5.7 **FestaFiesta 게임 빌드 성공**: `Saved/BuildModeClarity/build-game-occlusion.log`.
- 자동 검사 **65개 고유 테스트 모두 통과**. `Tests-Final/index.json`의 63개 결과에
  `Tests-LayoutRerun/index.json`의 2개 결과, `Tests-Occlusion/index.json`의 23개 결과를
  순서대로 적용해 테스트별 최신 상태를 선택한 집계다. 마지막 23개 실행은 모두 통과했으며,
  조합 안내와 도감 presenter 검사 2종이 새로 포함됐다. 최초 최종 실행의
  실패 2개는 도감 `AuthoredWidgets`와 정보창 `InspectorV2PagesPreview`였으며,
  수정 후 재실행에서 둘 다 통과했다. 원래 실패 기록을 삭제하거나 성공으로 바꾸지 않았다.
- 첫 번째 집계 결과는 `Saved/BuildModeClarity/validation-summary-first-revision.json`에 있다.
  **경고 없는 통과 61개, 경고를 포함한 통과 4개, 최신 실패 0개**다.
  드래그 입력, 배치 도구, 카탈로그 가상화·레이아웃, 도감 6열 및 정보창 페이지 검증을 포함한다.
- `migration-result.json`은 **PASS**, 재실행 멱등성은 **true**다.
  대상 외 **UI 에셋 440개**의 해시가 유지됐으며, 대상 WBP는 조합 안내를 포함한 7개다.

남아 있는 경고 16건은 다음과 같다. 테스트 통과가 이 경고의 원인까지 수정했다는 뜻은 아니다.

- `FacilityInspection`, `PlacementGridOverview`, `PresenterLifecycle`: 테스트 LocalPlayer에
  유효한 PlayerInput이 없어 Enhanced Input 사용자 설정을 읽지 못했다는 경고가 각 2건,
  총 6건이다.
- `ClarityCatalogDrag`: 기존 LiveHUD 탐색 아이콘 중 Build·Program·Promotion·Finale·Check의
  `T_FF_LH_Icon_*_White_256` 패키지가 없다. 각 아이콘의 패키지 로드·객체 조회 경고가
  2건씩, 총 10건이다. 누락 경로는 `UI/LiveHUD/Themes/LightTeal/Icons/Navigation`이며,
  해당 아이콘 누락을 이번 변경에서 해결했다고 보고하지 않는다.

### 실제 게임 화면 캡처

`capture_runtime.py`는 실제 `Test_Main` 맵에서 Festival Atlas와 `UIFlow.StartFestival`을 거쳐
PrepHUD를 연다. 별도 검증용 월드나 시설 데이터를 만들지 않는다. 카탈로그, 프로그램 시설,
간결한 배치 HUD, 왼쪽 조작 안내, 도구 R 제거 상태, 조합 도감, 목록 복원, 매출 순위의
`SHOWUI` PNG와 상태 JSON을 남긴다.
배치 프리뷰는 실제 게임 명령으로 열고 취소하며, 설치 확정과 에셋 저장을 호출하지 않는다.

마이그레이션 이후, 기존 PIE 세션이 없는 별도의 에디터에서 렌더링을 켜고 실행한다.
프로젝트 루트의 PowerShell 예시는 다음과 같다.

```powershell
& 'C:\Unreal\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' `
  "$PWD\FestaFiesta.uproject" /Engine/Maps/Entry `
  "-ExecutePythonScript=$PWD\Tools\BuildModeClarity\capture_runtime.py" `
  -unattended -nosplash -nosound -NoSourceControl -RenderOffscreen -DisablePlugins=RiderLink
```

결과 위치는 `Saved/BuildModeClarity/runtime-<실행시각>/report.json`과 같은 폴더의 PNG다.
선택 설정 파일 `Saved/BuildModeClarity/runtime-capture-request.json`으로
`output`(새 `runtime-` 접두사 폴더명), `width`, `height`, `timeout_seconds`를 지정할 수 있다.
이미 `report.json`이 있는 출력 폴더에는 덮어쓰지 않는다.

캡처는 공개 UI 액션을 호출하므로 실제 마우스 클릭·드래그 조작 검증을 대체하지 않는다.
커서 위치는 PlayerController의 실제 viewport 크기에서 계산해 지정하고, 다시 읽은 좌표와
배치 상태를 보고서에 기록한다. 화면 밖 렌더링에서 위젯 geometry 또는 유효한 배치 대상이
없으면 그 한계를 기록하며, 마커를 임의로 만들어 넣지 않는다. 종료 후 에셋 해시도 비교한다.
첫 번째 개정의 실제 게임 캡처는 `Saved/BuildModeClarity/runtime-20260920-214133`에 있다.
카탈로그 → 후보 선택 → 배치 → 원형 메뉴 → 도감 → 목록 복원의 6개 PNG를 직접 검토했다.
조합 안내의 긴 문장 표시, 도감 뒤 목록 숨김, 닫은 뒤 선택 시설 유지가 확인됐다.
실제 도감에는 22개 조합이 표시됐고, 캡처 전후 맵·데이터·UI·설정 파일 해시가 동일하다.
요청 크기는 1600×900이며 실제 게임 viewport는 1598×890, 창 테두리를 포함한 PNG는 1604×928이다.
드래그 입력은 별도 `ClarityCatalogDrag`의 실제 Slate 포인터 경로로 검증했다.

- [건축 카탈로그](../../Saved/BuildModeClarity/runtime-20260920-214133/01-catalog-Test_Main.png)
- [배치 HUD](../../Saved/BuildModeClarity/runtime-20260920-214133/03-placement-compact-Test_Main.png)
- [원형 도구](../../Saved/BuildModeClarity/runtime-20260920-214133/04-placement-radial-Test_Main.png)
- [조합 도감](../../Saved/BuildModeClarity/runtime-20260920-214133/05-combination-codex-Test_Main.png)

공식 API 참고: [Epic Force Feedback](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Engine/APlayerController/PlayDynamicForceFeedback).
프로젝트의 UE 5.7 로컬 헤더를 기준으로 구현했다.
