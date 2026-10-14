# 시설 강화 검증 기록

검증 환경: Windows / Unreal Engine 5.7.4 / FestaFiestaEditor Win64 Development, 2026-09-08.

## 최종 결과

| 구분 | 결과 | 근거 |
|---|---|---|
| 에디터 C++ 빌드 | 성공 | `FlexibleAuthoringFinalBuildConsole.log` |
| 기능 관련 자동화 | 233건: 성공 205, 경고 동반 성공 28, 실패 0, 미실행 0 | `FlexibleAuthoringFinalAutomationReport/index.json` |
| 실제 RHI UI·3D 미리보기 | 12건 모두 성공, 일반·자유 설정 화면 2개 캡처 | `FlexibleAuthoringFinalVisualReport/index.json` |
| 시설별 설정 이관 | 기존 부스 7개 저장, 재실행 변경 0개 | `PerFacilityMigrationFirstReport.json`, `PerFacilityMigrationReport.json` |
| 기존 시설 콘텐츠 추가 검증 | 1건 실패: 기존 입구·대기열 배치 문제 | `PerFacilityAutomationReport/index.json`의 `AllDefinitionsAreProjectable` |

표의 로그·보고서 경로는 `Artifacts/FacilityUpgrade/` 기준이다. 전체 프로젝트 검증이 모두 통과했다는 의미는 아니다. 기존 콘텐츠 검증 실패는 아래에 구분했다. 시작 로그에는 기존 Surfing 머티리얼 누락, Crowd 구조체 초기화 진단 등이 있으며, 테스트 통과 여부는 프로세스 종료 코드 대신 JSON의 `failed`, `notRun`, 개별 결과로 확인했다.

최종 실행은 강화의 기획 제한을 제거한 코드 기준이다. 기존 시설 데이터 에셋은 이 변경에서 다시 저장하거나 기본값으로 덮어쓰지 않았다. 비용·수익·시간·크기의 숫자 상한과 이전 단계 비교, 최대 Lv.10, 레벨 번호 연속성과 배열 순서 제한을 제거했다. 숫자 형식과 의미, 고유 레벨 번호, 실제 배치·길찾기 조건은 유지했다.

자유 설정 1차 검증에서 새 테스트 2건은 기존 설치 정책과 축소 후 동선 간격을 위반했다. 무료 강화 테스트는 정상 유료 설치 후 잔액·현재 기준 가격이 0인 상태로 정리했고, 축소 테스트는 첫 슬롯 간격을 조정해 축소 후에도 필수 50cm보다 넓게 확보했다. 기존 설치·물리·동선 검사는 변경하지 않았으며 최종 전체 필터를 다시 통과했다.

첫 확장 검증에서는 테스트용 1.5배 시설이 기존 1,000,000cm² 예약 면적 한도를 넘어 3건이 실패했다. 테스트를 1.25배로 조정하고 충돌 이웃 위치를 맞춘 뒤 기능 관련 전체 필터를 재실행했다. 운영 코드의 면적 한도와 충돌 검사는 유지했다.

## 직접 확인한 동작

- 시설별 강화 설정, 수익만 늘리는 단계, 레벨 없는 시설의 수익 표시와 강화 UI 숨김.
- 실제 Crowd 파티가 이용을 완료할 때 1.5배·2.25배 수익 정산. 무료 시설과 취소·이탈은 수익 없음.
- 수수료·인원·강화 배율을 적용한 뒤 한 번 반올림. 큰 금액도 잔액·누적 매출·원장에 같은 수용 금액 기록.
- 같은 종류의 다른 시설은 독립 상태 유지. 이동·schema 7 직렬화·복원에도 구매한 시간·수익·크기 배수 유지.
- 메시 교체, 미지정 단계의 Lv.1 메시 복귀, 실제 바닥 크기·높이·대기열·출구 변경, 원본 사양 보존.
- 공간 부족·잘못된 배율·오래된 레벨·중복 구매·운영 중 구매 거절. 정책 실패 시 금액과 대기열·레벨·크기·수익 배수 복구.
- schema 3~6 이전 저장 처리, 유료 효과 보존과 명령 영수증 호환.
- 데이터 에셋 3D 미리보기의 메시·크기 전환과 고정 카메라, 레벨 없는 시설의 기본 외형.
- 순서가 섞인 5·20·99 단계와 최대 정수 레벨, 레벨 사이 번호 누락, 효과가 같은 단계, 수익 0·감소·1000배, 느려지는 단계.
- 잔액 0에서 무료 강화, 기본 가격 0의 견적, 무료 명령의 정책 실패 복구·재전송, 기존 설치 지출 장부 유지.
- 0.75배 실제 바닥·메시 축소와 1.25→1 크기 전환. 대기열 간격·길이 조건은 유지.
- 미리보기는 실제 작성된 레벨만 나열해 매우 큰 레벨 번호 때문에 빈 번호 전체를 반복하지 않음.

## 실제 UI 확인

실제 `WBP_FF_FacilityInfoPanel`을 Direct3D 11에서 388×500으로 렌더링했다. 최종 이미지는 [일반 강화](../../Artifacts/FacilityUpgrade/FlexibleAuthoringStandardPreview.png)와 [무료·감소·축소 강화](../../Artifacts/FacilityUpgrade/FacilityFlexibleUpgradePreview.png)다.

현재 1인 예상 수익, 기본 대비 변화액, `10 → 15 Gold (기본 ×1.5)`, `변화 +5 Gold (+50%)`, 크기·시간·비용을 직접 확인했다. 이어 같은 실제 카드에서 Lv.5→Lv.20 무료 단계, `15 → 10 Gold`, `변화 -5 Gold (-33.3%)`, 시간 4→8초와 크기 4×4→3×3m를 확인했다. 두 화면 모두 버튼이 스크롤 가시 영역 안에 있으며 텍스트와 버튼이 잘리지 않는다. NullRHI의 이미지 생략과 별도로 실제 RHI 검증을 수행했다.

`FF.BuildFacilityInfoUMG`를 두 번 실행해 기존 위젯을 보존하며 `Text_FacilityRevenue`를 추가하고 재사용하는 것을 확인했다. 최종 실행에서는 저장된 WBP를 그대로 로드했다.

## 기존 콘텐츠 검증에서 남은 문제

`FestaFiesta.Content.FacilityDefinitions.AllDefinitionsAreProjectable`은 기존 13개 시설의 입장점이 예약 영역 밖으로 150cm 벗어나고, `Toilet_Accessible_WestGate`의 대기 경로가 필요한 길이보다 350cm 부족해 실패한다. 강화 대상 부스 6개는 같은 입구 문제를 Lv.2·Lv.3에서도 이어받는다. 신규 레벨에만 발생하는 Spec 유효성·Party Queue 변환·시각 크기 오류는 없었다.（10/14 기준: `Toilet_Accessible_WestGate` 정의는 현재 없다. 이후 기록에서는 같은 검사가 통과했다 — 9/20 시설 70종 검증의 `AllDefinitionsAreProjectable` Success, 9/23 전체 자동화 933/933. 현재 시점 재실행 결과는 없다.）

이관 스크립트는 강화 목록과 사용 여부만 변경했으며 원본 입구·대기열 좌표를 수정하지 않는다. 기존 기본 사양 검사와 해당 큐 검사 코드도 이번 변경으로 바뀌지 않았다. 콘텐츠 검증을 삭제하거나 조건을 완화하지 않았으며, 이 시설들의 입구 위치와 대기 공간은 별도 보정이 필요하다. [상세 진단](../../Artifacts/FacilityUpgrade/ExistingContentValidationIssues.md)과 `FacilityContentReadOnlyAudit.json`에 실제 18개 시설의 좌표와 근거를 기록했다.

## 다시 실행하기

프로젝트 루트 PowerShell에서 설치 경로에 맞춰 실행한다.

```powershell
& 'C:/Program Files/Epic Games/UE_5.7/Engine/Build/BatchFiles/Build.bat' `
  FestaFiestaEditor Win64 Development `
  '-Project=C:/GitHub/FestaTeam/8th-Team13-Final-Project/FestaFiesta.uproject' `
  -WaitMutex -NoHotReloadFromIDE -NoUBA -NoUBALocal

Set-Item -LiteralPath 'Env:UE-LocalDataCachePath' `
  -Value 'C:/GitHub/FestaTeam/8th-Team13-Final-Project/DerivedDataCache'

& 'C:/Program Files/Epic Games/UE_5.7/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' `
  'C:/GitHub/FestaTeam/8th-Team13-Final-Project/FestaFiesta.uproject' /Engine/Maps/Entry `
  -unattended -nop4 -nosplash -nullrhi -nosound -stdout -FullStdOutLogOutput `
  -DDC=InstalledNoZenLocalFallback -NoZenAutoLaunch -SharedDataCachePath=None `
  '-ReportExportPath=C:/GitHub/FestaTeam/8th-Team13-Final-Project/Artifacts/FacilityUpgrade/FlexibleAuthoringFinalAutomationReport' `
  '-TestExit=Automation Test Queue Empty' `
  '-ExecCmds=Automation RunTests FF_ZoneSystem+FestaFiesta.Building.FacilityUpgrade+FestaFiesta.Economy+FestaFiesta.ZoneIntegration+FestaFiesta.UI.FacilityInfo'
```

실제 화면 검증은 `-nullrhi`를 `-RenderOffscreen -d3d11 -NoRemoteShaderCompile`로 교체하고, `-ShaderWorkingDir`를 프로젝트의 `Saved/ShaderWorking/FacilityUpgradePreview`로 지정한다. 필터는 `FestaFiesta.UI.FacilityInfo+FF_ZoneSystem.Editor.FacilityLevelPreview`, 보고서 폴더는 `FlexibleAuthoringFinalVisualReport`로 바꾼다. 기존 콘텐츠 검증을 다시 확인하려면 `FestaFiesta.Content.FacilityDefinitions`를 별도 필터로 실행한다.

## 보존과 범위

- 원래 WBP 백업: `Artifacts/FacilityUpgrade/WBP_FF_FacilityInfoPanel.before-upgrade.uasset`.
- 부스 이관 전 원본: `Artifacts/FacilityUpgrade/BeforePerFacilityMigration/`.
- 이전 전역 비용·시간 추출본: `Artifacts/FacilityUpgrade/LegacyUpgradeConfig.json`.
- 앞선 빌드에서 기존 `FF_FestivalSettingsScreen.cpp`의 단독 숫자 `4`가 컴파일을 막아 제거했다. 수정 전 파일은 `Artifacts/FacilityUpgrade/FF_FestivalSettingsScreen.before-build-fix.cpp`에 보관했다.

강화 항목의 기획상 상한과 증가·감소 조건은 제거했다. 비용·수익은 0 이상, 시간·크기는 양수, 레벨은 2 이상의 고유 정수로 작성한다. 실제 배치 공간·인원 간격·대기열 길이·길찾기·수치 표현 가능 여부는 계속 검사한다. 축소 후 동선이 현재 NavMesh에서 연결되지 않으면 기존 동기 배치 검증이 거절할 수 있다. 축소의 비동기 길찾기 갱신, 전체 게임의 Gold·날짜를 함께 저장하는 새 통합 저장 기능, 네트워크 동기화는 추가하지 않았다. 테스트는 시설 snapshot과 기존 경제·대기열 연결 범위이며 실제 축제 장기 플레이 밸런스나 특정 기기의 FPS 향상을 측정한 것은 아니다.
