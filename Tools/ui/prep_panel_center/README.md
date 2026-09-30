# 셋업 패널 중앙 배치 (P1)

프로그램·인력·홍보·점검의 `Center` 기준을 실제 게임 화면 중앙에 맞춘다.
P1은 위치 계산과 편집값 보존 단계이며, 네 패널 내부 디자인은 이후 단계에서 변경한다.

## 에디터에서 위치·크기 수정

콘텐츠 브라우저에서 다음 에셋을 연다.

`/Game/FestaFiesta/Building/FF_WBP_Building/Shared/WBP_FF_PrepHudBar`

**Class Defaults → Panel Layouts → Program / Staff / Promotion / Check**에서 원하는 패널만 수정한다.

| 속성 | 의미 |
| --- | --- |
| `Dock = Center` | 실제 게임 화면 중앙을 기준점으로 사용 |
| `Offset X` | 0은 가로 중앙, 양수는 오른쪽, 음수는 왼쪽 |
| `Offset Y` | 0은 세로 중앙, 양수는 아래, 음수는 위 |
| `Size X` | 패널 폭 |
| `Size Y` | 패널 높이 |

Offset과 Size는 DPI 적용 전 Slate 단위다. 화면 해상도의 픽셀 수와 반드시 같지는 않다.
위치·크기를 변경한 뒤 **Compile → Save → PIE 재시작**으로 확인한다.
예를 들어 Program의 Offset을 `(80, -40)`으로 저장하면 프로그램 패널만 중앙에서 오른쪽 80, 위쪽 40만큼 이동한다.
창 크기가 바뀌면 중앙 기준점은 다시 계산하고, 편집한 Offset과 Size는 유지한다.

실행 중 드래그한 위치는 같은 실행에서 닫고 다시 열어도 유지한다. 새 실행은 에디터에 저장한 기본값으로 시작한다.
`ResetPanelPosition()`은 Offset을 `(0, 0)`으로 만든다. 에디터에서 저장한 임의의 Offset을 복원하는 기능은 아니다.

## 유지 범위

- 패널의 기존 크기와 내부 디자인, 배치 패널의 Bottom 배치, 피날레와 라이브 패널은 유지한다.
- 하단 HUD의 흰 버튼·하늘색 테두리와 현재 저장된 행 높이를 유지한다. 과거 요청은 메뉴 32 / 상태 36이었으나, P1 기준 `96e20389`의 저장된 셋업 HUD는 메뉴 32 / 상태 32로 확인했다. 현재 에디터 값을 보존하며 상태 높이를 36으로 강제 복원하지 않는다.
- 화면보다 큰 패널의 내부 스크롤·레이아웃 개선은 후속 디자인 단계에서 다룬다.
- P1 완료 보고 후 사용자 지시를 받기 전에는 P2 공통 틀·인력 디자인 작업을 시작하지 않는다.

## 최초 적용 도구

`apply.py`는 대상 네 항목의 저장된 Offset만 `(0, 0)`으로 맞춘다.
실행 전 에셋을 `Saved/PrepPanelCenter/Before-*`에 백업하고, Size·다른 탭·위젯 트리가 바뀌지 않았는지 검사한다.
성공한 적용 보고서가 있으면 재적용하지 않아 이후 에디터 수정값을 보호한다.
새 C++ 코드 빌드 후 에디터를 닫은 상태에서 실행한다.

```powershell
& Tools/ui/blender/bright_globe/run_editor.ps1 -Script Tools/ui/prep_panel_center/apply.py -Label prep_panel_center_apply -NullRHI
```

## P1 당시 검증 기록

P1 당시 상태: **중앙 배치·편집값 보존 완료. P2는 사용자 시작 지시 대기**였다.

| 항목 | 결과 |
| --- | --- |
| Development Editor 빌드 | UE 5.7.4 빌드 성공 (`Saved/PrepPanelCenter/build-final.log`) |
| 네 패널의 실제 화면 중앙 배치 | 실제 PIE 1280×720 / 1920×1080 / 2560×1080에서 요청·실제 크기 일치, 총 24회 좌표 측정 통과 |
| 개별 Offset·Size 및 재오픈 보존 | Staff `(80,-40)`, 크기 `(880,540)` 적용, Nudge 이동 후 닫기·재오픈, 다른 탭 설정 독립 확인 |
| 창 크기·DPI 변경과 첫 표시 | 좌표 변환·크기 변경·초기 geometry 무효 처리 자동화 2개 통과. DPI 0.666 / 1.0 PIE 좌표 확인 |
| 배치·피날레·라이브 및 기존 HUD 치수 | 배치 Bottom 계산 확인, 적용 전후 피날레 설정·위젯 트리 동일, 라이브 RenderedLayout 통과. 셋업 CTA 높이 검사 실패는 아래 별도 기록 |

최종 결과는 `Saved/PrepPanelCenter/report.json`의 `status=passed`이며 174.09초에 완료됐다.
검증 전후 보호 파일 286개 해시가 같고 PIE·일회용 에디터 종료를 확인했다. 기본값 적용·컴파일·저장 근거는 같은 폴더의 `apply-report.json`이다.
개인 Saved 설정 ini 2개는 검증 전 내용으로 복원하고 SHA256 일치를 확인했다(`settings-restored.json`).
Python의 `FGeometry`가 0으로 반환되는 문제는 Native C++ Slate 측정 명령으로 우회했다. 0 좌표를 성공으로 처리하지 않았다.

P1 당시 상태 행 높이 32 / `Box_Cta` 상하 여백 5에서 `Text_CtaReason`의 글자 높이 부족이 검출돼 셋업 RenderedLayout 검사가 세 해상도에서 실패했다.
당시 여백 보정은 미검증이었다. 중앙 배치 경로와 구분한 기존 HUD 항목으로 남겼으며 P1에서 수정하지 않았다. P1 기록은 전체 회귀 검사를 모두 통과한 결과가 아니다.

일괄 생성 PNG 12장은 실제 PIE가 아닌 다른 에디터 뷰포트(555×186)를 찍어 검증 근거에서 제외했다.
P1 완료 근거는 실제 PIE의 좌표·레이아웃 설정 측정이며, 당시 물리 마우스 입력·전체 화면 시각 수용·패키징은 미검증이었다.

## 2026-09-30 CTA 후속 검증

셋업의 저장된 메뉴/상태 높이 32/32를 유지하고 [HUD spacing v2](../hud_sky_outline/README.md)의
`Box_Cta` 상하 여백 5→2 보정을 적용했다. P1 때 기록한 CTA 글자 높이 부족은 자동 레이아웃 검사에서 해결됐다.
`FestaFiesta.UI.PrepHUD.BrightBottom.RenderedLayout`이 실제 GPU `-RenderOffscreen`으로
1280×720, 1920×1080, 2560×1080 세 해상도에서 통과했다.

`FF-Win64-Shipping-20260930-05`에서는 전체 자동화 1,030건(FestaFiesta 899 / FF_ZoneSystem 131)이 통과했고
실패·미실행·진행 중 항목은 모두 0건이었다. 두 테스트 그룹의 `Validation/<그룹>/Report/index.json`이 근거다.
이 후속 기록은 P1 당시 실패 이력을 보존하면서 현재 CTA 회귀가 해소됐음을 기록한다.
물리 마우스 입력·전체 화면 시각 수용과 실제 게임의 수동 플레이 완료는 이 자동 검증으로 판단하지 않는다.
