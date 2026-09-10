# FestaFiesta 오류·경고 해결 가이드 — 한광훈 작업 범위

<callout icon="✅" color="green_bg">
	**한광훈 작업 범위의 수정과 검증을 완료했습니다.**
	UE 5.7.4 에디터 빌드와 관련 자동 검사 9개가 통과했습니다. 서핑의 불필요한 누락 에셋 로드, 옛 타코 시설의 카탈로그 노출, 누락된 건축 안내 위젯을 처리했고, 구역 진단의 잘못된 경고와 보고 방식을 개선했습니다.
	**프로젝트 전체의 오류가 모두 사라졌다는 뜻은 아닙니다.** 팀원 소유 코드·에셋과 공유 맵 문제는 수정하지 않고 아래에 원인과 해결 절차를 남겼습니다.
</callout>

작성일: 2026-09-10 · 대상: FestaFiesta / UE 5.7.4 · 읽는 사람: 기획자, 디자이너, 비전공 팀원, 담당 개발자

# 1. 수정 범위를 어떻게 정했나요?

현재 Git 작성자 설정인 Asera, 파일별 작성 이력, 기존 팀 인계 문서를 대조했습니다. 한광훈이 작성한 서핑·시설 카탈로그·FF_ZoneSystem 파일만 수정했습니다. 한광훈의 수정 이력이 있어도 다른 담당자의 영역인 공유 파일은 제외했습니다.

- 백종태의 Atlas·LiveHUD, 강민서의 상호작용, 이정원의 Crowd·애니메이션, 김시리의 경제·NPC 코드는 변경하지 않았습니다.
- 범위가 좁혀지기 전에 잠시 수정했던 Crowd·경제 파일 3개는 줄바꿈과 파일 끝까지 복구했습니다. 해당 파일의 Git 변경 목록이 비어 있는 것을 확인했습니다.
- 공유 설정인 Config 파일은 변경하지 않았습니다.
- 기존 작업 중이던 Test_Main 맵과 M_Flower 재질은 작업 시작 때의 파일과 SHA-256 해시가 같습니다. 이번 작업에서 저장하지 않았습니다.

# 2. 이번에 해결하거나 개선한 것

<table header-row="true" fit-page-width="true">
<tr><td>항목</td><td>쉽게 설명한 원인</td><td>이번 결과</td></tr>
<tr><td>서핑 Mi_Sea_Inst 누락 오류</td><td>지금 쓰지도 않는 옛 바다 재질을 시작할 때마다 찾고 있었습니다.</td><td>미사용 참조와 로드를 제거했습니다. 새 에디터 세션에서 해당 경로의 로그가 0건임을 확인했습니다.</td></tr>
<tr><td>FoodTruck_Taco 분류 경고</td><td>옛 타코 시설과 새 타코 시설이 동시에 남아 있었고, 옛 이름은 현재 분류 규칙에 맞지 않았습니다.</td><td>옛 시설은 보존하고 신규 건설 목록에서 제외했습니다. 새 FoodStall_Taco는 계속 표시됩니다.</td></tr>
<tr><td>건축 안내 위젯 누락</td><td>안내창을 만드는 코드가 있는데 실제 안내창 에셋이 생성되지 않은 상태였습니다.</td><td>기존 전용 생성기로 WBP_FF_BuildModeGuide를 생성·컴파일·저장했습니다. 새 세션에서 정상 로드를 확인했습니다.</td></tr>
<tr><td>구역·출구 진단</td><td>구역이 없는 시작 화면도 검사했고, 제한된 지점만 조사한 결과를 구역 전체 단절로 표현했습니다. 일부 실패 구역은 총수에서도 빠졌습니다.</td><td>구역 없는 화면은 검사하지 않습니다. 전체 구역을 분모로 쓰고, 경로 없음과 NavMesh 투영 실패를 구분합니다. 실제 길을 연결하는 지형 수정은 하지 않았습니다.</td></tr>
</table>

## A. 서핑: 안 쓰는 옛 주소를 지웠습니다

**비유:** 이미 새 가게를 이용하고 있는데 프로그램이 매번 폐업한 옛 가게에도 전화를 걸던 상황입니다.

실제 바다 화면은 BasicShapeMaterial로 만들어지고 있었습니다. 삭제한 ProjectSeaMaterial은 화면을 만드는 데 사용되지 않았습니다. 그래서 새 바다 에셋을 만들거나 임의의 재질을 연결할 필요 없이, 사용하지 않는 경로 상수·생성자 로드·멤버만 제거했습니다.

효과는 불필요한 파일 검색과 초기화 오류 제거입니다. FPS가 얼마나 올랐는지는 측정하지 않았습니다. 기존 3D 서핑 장면의 생성·설정·정리 계약 테스트는 통과했습니다.

## B. 타코 시설: 옛 번호는 남기고 새 목록만 정리했습니다

**비유:** 옛 메뉴를 메뉴판에서 내리되, 과거 주문서의 메뉴 번호는 지우지 않는 방식입니다.

현재 두 시설 정의가 존재합니다.

- 신규 건설에 사용할 정식 항목: `Booth_FoodStall_Taco`
- 과거 항목으로 남길 정의: `Booth_FoodTruck_Taco`

두 이름을 강제로 같게 바꾸면 고유 번호가 중복되고, 과거 저장 데이터가 다른 시설을 가리킬 수 있습니다. 따라서 시설 정의에 **Show in Build Catalog** 설정을 추가했습니다. 기본값은 켜짐이며, 옛 Taco 에셋 하나만 껐습니다.

기존 시설의 ID·크기·대기열·설치 사양은 그대로입니다. AssetManager에서도 옛 정의를 조회할 수 있습니다. 자동 검사에서 전체 Spec과 Primary Asset ID가 바뀌지 않는 것을 확인했습니다.

**팀 운영 방법:** 이후 시설을 신규 판매 목록에서 내릴 때는 에셋 삭제나 번호 변경 전에 이 설정을 사용합니다. 이 설정은 에셋 완전 삭제나 저장 데이터 마이그레이션을 대신하지 않습니다.

## C. 건축 안내: 준비된 설계도로 빠진 안내창을 만들었습니다

생성된 경로는 다음과 같습니다.

`/Game/FestaFiesta/Building/Guide/WBP_FF_BuildModeGuide`

기존 명령 `FF.BuildModeGuideUMG`를 사용했습니다. 이 생성기는 이미 정상 위젯이 있으면 재사용하므로, 디자이너의 기존 수정본을 덮어쓰지 않습니다. 현재 작업에서는 에셋이 없었기 때문에 새로 만들었습니다.

새 에디터 세션에서 위젯과 생성 클래스가 로드되고, FF_BuildModeGuide를 상속하는 것을 확인했습니다. 실제 플레이 화면의 배치·시각 품질은 별도 플레이 확인 항목입니다.

## D. 구역 진단: 잘못된 단정과 누락을 바로잡았습니다

**비유:** 축제장 일부 도로만 확인하고 “구역 전체에 길이 없다”고 말하거나, 확인하지 못한 구역을 전체 구역 수에서 빼던 보고서를 고쳤습니다.

- 게임플레이 구역이 없는 시작 화면에서는 군중용 길과 출구가 없다는 경고를 만들지 않습니다.
- 모든 유효한 구역을 총수에 포함합니다.
- “길 위에 올려놓을 기준점을 못 찾음”과 “기준점은 있지만 출구까지 길이 없음”을 구분합니다.
- 제한된 표본에서 접근점을 찾지 못했을 때, 구역 전체가 단절됐다고 단정하지 않습니다.
- NavMesh가 생성·변경 반영 중이면 최종 도달 판정을 보류합니다. 자동으로 기다렸다가 다시 실행하는 기능은 아닙니다.
- NavMesh가 필요한 실제 게임플레이 맵의 누락 경고와 접근 실패 오류는 계속 남깁니다.

플레이 중 콘솔에서 다음 명령으로 다시 보고서를 볼 수 있습니다.

`ffzs.ZoneAccess.Report`

이 명령은 읽기 전용입니다. 구역 위치, 지형, 저장된 앵커를 바꾸지 않습니다. 앵커 보정은 기존과 같이 초기 시작 시점에만 수행하며, 임의의 지연 재시도를 추가하지 않았습니다. 뒤늦게 앵커를 바꾸면 NPC가 기억한 길찾기 정보를 함께 갱신해야 하기 때문입니다.

기존 **구워 둔 기본 NavMesh + DynamicModifiersOnly** 구조도 유지했습니다. 실제로 길이 끊긴 구역은 레벨 담당자가 경사·충돌·통로·출구를 확인해야 합니다. [Epic의 NavMesh 최적화 안내](https://dev.epicgames.com/documentation/en-us/unreal-engine/optimizing-navigation-mesh-generation-speed-in-unreal-engine?application_version=5.7)

# 3. 이미 현재 파일에서 정상으로 확인한 항목

이 항목은 이번에 새로 고쳤다고 집계하지 않습니다. 첨부 로그에는 이전 작업 상태가 섞여 있습니다.

- **가로등:** 현재 Test1_FestivalLights_20260910 경로의 BP와 생성 클래스가 정상 로드됐습니다. 현재 Test_Main의 참조도 새 경로를 사용합니다. 공유 맵 전체 실행이 검증됐다는 뜻은 아닙니다.
- **주무대 이미지:** 현재 시설 정의가 /Game/Asset/FacilitiesAsset_HKH 아래의 올바른 이미지를 참조하고, 실제 텍스처가 로드됐습니다.
- **꽃 재질:** 현재 M_Flower의 Used with Instanced Static Meshes 값은 이미 True입니다. 이번 작업에서 이 재질을 저장하거나 수정하지 않았습니다.

에셋을 이동할 때는 에디터에서 이동하고 참조를 확인한 뒤 관련 파일을 함께 제출하는 것이 좋습니다. Redirector 정리는 참조하는 다른 패키지도 저장할 수 있으므로 담당 범위를 확인해 수행합니다. [Epic Redirectors 문서](https://dev.epicgames.com/documentation/en-us/unreal-engine/asset-redirectors-in-unreal-engine?application_version=5.7)

# 4. 팀원 담당: 원인과 해결 방법

아래는 **수정 완료 목록이 아닙니다.** 담당 표시는 Git 이력과 팀 문서를 바탕으로 한 확인 요청 대상입니다. 별도의 메시지나 알림은 전송하지 않았습니다.

## 백종태 — 지구본·국가 지도 뒤로가기 오류

**보이는 로그:** `Cannot create action binding ... no action provided`

**원인:** 두 화면은 뒤로가기 기능을 켰지만 공통 Back 액션을 지정한 프로젝트 설정이 확인되지 않습니다. 입력을 받을 창구는 있지만, 어떤 키를 뒤로가기라고 부를지 비어 있는 상태입니다. CommonUI용 Viewport 설정은 이미 되어 있습니다.

**해결 순서:**

1. CommonInputActionDataBase 구조의 데이터테이블에 Back·Accept 행을 만듭니다.
2. 키보드 Escape·Enter와 게임패드 취소·확인 키를 연결합니다.
3. CommonUIInputData 기반 블루프린트의 Default Back Action·Default Click Action에 해당 행을 지정합니다.
4. 프로젝트 설정 → Game → Common Input Settings → Input Data에 등록합니다.
5. 설정과 에셋을 함께 제출합니다.

**완료 기준:** 지구본과 국가 지도를 반복해서 오가도 오류가 없고, 키보드·게임패드 모두 이전 화면으로 돌아갑니다. 화면별 임시 Escape 코드를 늘리기보다 공통 설정을 고치는 것이 적합합니다.

## 강민서·공유 맵 작업자 — 상호작용 노드 오류와 안내창 중복

**보이는 로그:** `TryStartInteraction`, `Bad cast node`, `was already added to the screen`

TryStartInteraction 함수는 현재 C++에 존재합니다. “함수가 삭제됐다”는 결론으로 바로 넘어가면 안 됩니다. Test_Main에 저장된 노드와 현재 빌드의 클래스 정보가 맞는지 먼저 확인합니다.

1. 에디터를 종료하고 현재 코드의 Editor 빌드로 다시 엽니다.
2. 레벨 블루프린트의 오류 노드에서 FF_TriggerInteractionActor 대상과 함수를 확인합니다.
3. 해당 노드만 갱신하거나 다시 연결하고 컴파일합니다.
4. 실제 이름 변경이 확인될 때만 Core Redirects를 사용합니다. 공유 맵 전체 교체는 피합니다.

안내창은 현재 모든 Pawn의 범위 진입에서 표시 요청이 발생할 수 있습니다. 같은 캐릭터의 여러 충돌 컴포넌트가 들어오거나 NPC가 지나가도 반복 호출될 수 있습니다. 실제 Add to Viewport가 있는 BP까지 확인해, 위젯을 한 번 생성하고 Is In Viewport 검사로 중복 추가를 막습니다. 로컬 플레이어만 대상으로 하고 첫 진입에 표시·마지막 이탈에 숨기도록 상태를 관리합니다.

**완료 기준:** 빠른 재진입과 여러 충돌 컴포넌트에서도 안내창은 하나만 유지되고, NPC가 지나갈 때 플레이어 안내창이 뜨지 않습니다. 레벨 Blueprint 컴파일도 성공해야 합니다. [Epic Core Redirects 문서](https://dev.epicgames.com/documentation/en-us/unreal-engine/core-redirects-in-unreal-engine?application_version=5.7)

## 이정원 — 캐릭터 스켈레톤 누락과 아동 여부 초기값

**보이는 로그:** `Invalid USkeleton`, `Skeleton == nullptr`, `bIsChild is not initialized properly`

스켈레톤은 캐릭터의 뼈대 설명서입니다. 애니메이션이 없는 설명서를 가리키면 동작을 적용할 수 없습니다. Breathing_Idle은 현재 읽기 검사에서도 스켈레톤이 비어 있었습니다. Walk_With_BriefcaseTake_001에서도 같은 문제가 추가로 확인됐습니다.

같은 폴더의 50_Man 스켈레톤을 정상적으로 사용하는 메시와 애니메이션이 있습니다. 다만 이름이 비슷하다는 이유만으로 강제 연결하지 말고, 원래 스켈레톤 복구 또는 본 이름·계층 호환 확인 후 재연결/재임포트를 진행합니다.

bIsChild는 “이 캐릭터가 아이인가요?”라는 체크칸에 기본값이 없는 상태입니다. 담당 헤더에서 `bool bIsChild = false;`처럼 기본값을 명시하고 기존 데이터테이블의 아동 행은 True인지 확인합니다.

**완료 기준:** 해당 애니메이션·AnimBP를 열어도 스켈레톤 오류가 없고 실제 동작이 자연스럽습니다. 구조체 초기화 검사도 통과해야 합니다.

## 김시리 — 경제 Queue 경고와 목적지 후보 0개

**경제 경고:** 대기열 시스템은 Game/PIE에만 생성되는데 경제 브리지는 에디터 월드에도 생성될 수 있습니다. “영업 중인 행사장”에만 있는 계산대를 “편집 중인 설계도 화면”에서 찾는 것과 비슷합니다.

브리지는 이미 InitializeDependency를 호출하고 있으므로 단순히 초기화 의존성을 또 추가하거나 타이머로 재시도하는 것이 핵심이 아닙니다. 두 서브시스템의 월드 범위를 맞추고, 실제 Game/PIE에서 Queue가 없을 때는 실패를 계속 보고해야 합니다. 이번 읽기 전용 에디터 검사에서도 해당 경고가 재현됐습니다. 실제 플레이 매출 누락 여부는 별도로 확인해야 합니다.

**후보 0개:** 첨부된 사례는 유일한 시설이 메인 공연 시간 외라서 제외됐습니다. 길찾기 문제라고 단정할 수 없습니다. 다른 운영 시설을 배치하거나, 의도된 휴식·대기·나중에 재선택 규칙을 확인합니다. 반복 경고는 구역과 시간표 변경을 기준으로 집계하되, 잘못된 데이터 자체를 숨기지 않습니다.

**완료 기준:** 실제 부스 이용 완료 후 매출이 정확히 한 번 반영되고, 공연 시간 전후에도 손님이 의도한 행동을 합니다.

## 백종태 — HUD 아이콘, 정상 진단 메시지, 개장 차단

현재 Navigation 폴더에서 아래 다섯 아이콘이 확인되지 않습니다.

- T_FF_LH_Icon_Build_White_256
- T_FF_LH_Icon_Program_White_256
- T_FF_LH_Icon_Promotion_White_256
- T_FF_LH_Icon_Finale_White_256
- T_FF_LH_Icon_Check_White_256

공통 폴더는 `/Game/FestaFiesta/UI/LiveHUD/Themes/LightTeal/Icons/Navigation`입니다. 원본 에셋을 복구하거나 Prep/Live HUD의 TabIconFolder·TabIconNameFormat을 실제 위치에 맞춥니다. 패키지에서도 포함되는지 확인합니다.

“지구본 활성화”, “지구 누름”, “바 실측”은 개발용 진단 출력입니다. 정상 상태는 Verbose 등으로 정리할 수 있습니다. 다만 0×0 크기는 실제 화면 표시 후에도 지속되는지 확인해야 합니다.

“Setup 단계를 넘길 수 없어 개장을 막았다”는 진행 규칙입니다. DT_DaySequence의 해당 Setup 행과 bSkippable을 확인하되, 경고 제거만을 목적으로 무조건 켜지 않습니다. 단계 정책은 기획·시간 담당과 맞춥니다.

# 5. 엔진·공유 설정·레벨 담당과 확인할 항목

<table header-row="true" fit-page-width="true">
<tr><td>로그</td><td>뜻과 적절한 대응</td></tr>
<tr><td>FAssetDataDiscovery / bIsIdle=true / OutNumPathsToSearch=1</td><td>에셋 검색기가 완료 상태인데 남은 경로 수가 0이 아닌 내부 상태를 보고한 것입니다. 이 한 줄로 에셋 손상이나 메모리 누수를 단정할 수 없습니다. 이번 최종 세션에서는 재현되지 않았습니다. 같은 5.7.4 환경에서 반복 여부·실제 검색 누락을 기록한 뒤 엔진 담당자가 조사합니다.</td></tr>
<tr><td>No GameplayCueNotifyPaths</td><td>효과 알림 에셋을 찾을 폴더를 지정하지 않아 /Game 전체를 탐색합니다. GAS 담당자가 실제 GameplayCue 전용 폴더를 확인한 후 DefaultGame.ini에 지정합니다. 공유 설정이므로 이번에는 변경하지 않았습니다.</td></tr>
<tr><td>Unable to find RecastNavMesh / 실제 출구 미도달</td><td>시작 화면에 길찾기가 불필요한 경우와 실제 플레이 맵에서 NavMesh가 빠진 경우를 구분해야 합니다. 실제 맵에서는 구운 기본 NavMesh, Bounds, 캐릭터의 크기·단차 허용치, 충돌, 앵커, 출구를 함께 확인합니다. 공유 맵 지형은 이번에 수정하지 않았습니다.</td></tr>
<tr><td>Nanite instanced mesh + WPO + ray tracing</td><td>풀·꽃을 흔드는 변형과 인스턴스의 레이 트레이싱 조합에 대한 엔진 제한입니다. 담당자가 해당 식생의 RT 변형 평가를 끄거나 별도 표현을 선택하고 그림자·반사를 비교해야 합니다. 프로젝트 전체의 Nanite/RT를 일괄 해제하지 않았습니다.</td></tr>
<tr><td>Slate 아이콘·VisionOS 아이콘 누락 / ToolMenus Compile 섹션</td><td>게임 에셋 경로와 다른 에디터 설치·플러그인 리소스 문제입니다. 필요한 에디터 기능이 실제로 깨지는지 확인하고, 팀이 고정한 동일 엔진 설치·플러그인 버전 및 설치 파일 검사를 우선합니다.</td></tr>
<tr><td>google.com/generate_204 HTTP 3초 초과</td><td>외부 연결 확인 요청이 제시간에 응답하지 않은 상태입니다. 이 로그만으로 게임 기능 실패를 뜻하지는 않습니다. 요청을 만든 기능·플러그인과 네트워크 환경을 확인합니다.</td></tr>
<tr><td>LogTemp [Crowd] FacilityWallCount = 0</td><td>0이라는 측정값만으로 오류인지 확정할 수 없습니다. 실제 벽이 있어야 하는 장면인지 확인한 뒤, 정상 상태 출력이면 담당자가 개발용 로그 수준으로 정리합니다.</td></tr>
</table>

성능 개선은 실제 비용이 있는 곳부터 적용합니다. 미사용 로드 제거, 실제 사용 범위로 검색 제한, 필요 없는 맵 검사 생략이 우선입니다. 무거운 에셋을 자주 동기 로드하는 곳은 담당자가 측정 후 Soft Reference와 비동기 로드를 검토합니다. 없는 에셋을 비동기로 읽는다고 누락 문제가 해결되지는 않습니다. [Epic 비동기 에셋 로드 문서](https://dev.epicgames.com/documentation/unreal-engine/asynchronous-asset-loading-in-unreal-engine?application_version=5.7)

# 6. 검증 결과와 남은 확인

<table header-row="true" fit-page-width="true">
<tr><td>검사</td><td>결과</td></tr>
<tr><td>UE 5.7.4 FestaFiestaEditor / Win64 Development 빌드</td><td>성공, 빌드 종료 코드 0</td></tr>
<tr><td>관련 자동화 테스트</td><td>9개 성공, 경고 포함 성공 0개, 실패 0개, 미실행 0개</td></tr>
<tr><td>대상 시설 정의 자체 검증</td><td>옛 Taco와 정식 Taco 모두 유효</td></tr>
<tr><td>새 에디터 세션에서 에셋 읽기 검사</td><td>옛 항목 비표시·정식 항목 표시·영구 ID 보존·안내 위젯 클래스·서핑 기본 객체 확인</td></tr>
<tr><td>최종 세션의 Mi_Sea_Inst 경로 로그</td><td>0건</td></tr>
<tr><td>팀원 영역과 기존 맵·꽃 재질 보존</td><td>대상 팀원 파일 Git 변경 없음, 맵·재질은 작업 전과 SHA-256 동일</td></tr>
<tr><td>프로젝트 전체 테스트·최종 패키지·전체 플레이·화면 QA·FPS</td><td>이번 범위에서 완료했다고 판정하지 않음</td></tr>
</table>

자동 검사 9개는 카탈로그 정렬·비용·비동기 요청 세대 처리·퇴역 항목 보존·썸네일 계약과 수명, 서핑 3D 장면, 구역 없는 맵과 실제 NavMesh 누락 상황을 검사했습니다. 검증은 NullRHI 환경이므로 화면의 최종 렌더링 품질을 보증하지 않습니다.

**다음 공동 확인:**

- [ ] 담당자가 Test_Main 레벨 Blueprint 오류를 해결한다.
- [ ] 공유 맵에서 실제 출구 도달 실패 구역의 길·충돌·앵커를 확인한다.
- [ ] UI 담당자가 Back 입력, 안내창 중복, 다섯 아이콘을 확인한다.
- [ ] Crowd·경제 담당자가 스켈레톤·초기값·월드 범위를 수정한다.
- [ ] 에디터를 새로 열고 준비 → 개장 → 라이브 → 종료 흐름과 패키지 실행을 확인한다.

기존 Tools/Build/Run-AutomationTests.ps1에는 Editor 빌드·자동화·DataValidation 경로가 있습니다. 팀원 수정이 통합되면 프로젝트 전체 검증에 이 경로를 활용할 수 있습니다. [Epic Data Validation 문서](https://dev.epicgames.com/documentation/en-us/unreal-engine/data-validation-in-unreal-engine?application_version=5.7)

# 7. 개발자 참고: 변경 파일과 재현 도구

<details>
<summary>변경 파일 목록</summary>
	**서핑**
	- Source/FestaFiesta/Private/UI/Minigame/FF_SurfingSceneActor.cpp
	- Source/FestaFiesta/Public/UI/Minigame/FF_SurfingSceneActor.h
	**구역 진단**
	- Plugins/FF_ZoneSystem/Source/FF_ZoneSystem/Private/FF_FFZSZoneAccessSubsystem.cpp
	- Plugins/FF_ZoneSystem/Source/FF_ZoneSystem/Public/FF_FFZSZoneAccessSubsystem.h
	- Plugins/FF_ZoneSystem/Source/FF_ZoneSystemTests/Private/FF_FFZSZoneAccessDiagnosticsTests.cpp
	**카탈로그**
	- Plugins/FF_ZoneSystem/Source/FF_ZoneSystem/Public/FF_FFZSFacilityDefinition.h
	- Source/FestaFiesta/Private/Building/FF_BuildCatalogSubsystem.cpp
	- Source/FestaFiesta/Public/Building/FF_BuildCatalogSubsystem.h
	- Source/FestaFiesta/Private/Building/FF_BuildCatalogRetirementTests.cpp
	**에셋**
	- Content/FestaFiesta/Zone/Data/FacilityDefinitions/Booth/PDA_FFZS_Booth_FoodTruck_Taco.uasset
	- Content/FestaFiesta/Building/Guide/WBP_FF_BuildModeGuide.uasset
	**도구**
	- Tools/Diagnostics/repair_owned_warning_assets.py
	- Tools/Diagnostics/verify_owned_warning_assets.py
</details>

<details>
<summary>검증 기록 위치와 재실행 방법</summary>
	프로젝트 루트는 C:/GitHub/FestaTeam/8th-Team13-Final-Project 입니다.
	- Artifacts/WarningRepair/build-console.log — Editor 빌드 결과
	- Artifacts/WarningRepair/TestReport/index.json — 테스트 9개 결과
	- Artifacts/WarningRepair/owned-asset-repair.json — 실제 저장한 대상
	- Artifacts/WarningRepair/owned-asset-verification.json — 새 세션 검증
	- Artifacts/WarningRepair/verify-console.log — 최종 엔진 로그
	- Artifacts/WarningRepair/Before — 작업 전 대상 에셋 사본
	에디터 빌드 이후 UnrealEditor-Cmd.exe에서 프로젝트 파일과 함께 -run=pythonscript 및 -script 인자로 위 도구를 실행합니다. repair 도구는 옛 Taco 표시 설정과 없는 안내창만 처리하며, verify 도구는 읽기 전용입니다. Save All이나 프로젝트 전체 Redirector 정리를 호출하지 않습니다.
	이번 자동화 필터는 FF_ZoneSystem.Integration.ZoneAccess + FestaFiesta.Building.Catalog + FestaFiesta.Minigame.Surfing.UI.ThreeDimensionalPresentationContract 입니다.
	처음 제한된 실행 환경에서는 Zen 캐시에 접근하지 못했습니다. 이후 엔진에 제공되는 InstalledNoZenLocalFallback 실행 옵션을 사용했습니다. 프로젝트의 공유 캐시 설정은 변경하지 않았습니다.
</details>

