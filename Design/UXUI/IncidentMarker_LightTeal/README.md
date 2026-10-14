# 사건 월드 마커 규격 (LightTeal, 초안 v2)

사건 위치 위에 뜨는 원형 마커. Frostpunk 방식 — 어두운 원판 + 흰 기호 + 링 하나.
생성: `node Tools/ui/incident_marker_drafts/generate.js` (Chrome 헤드리스로 PNG를 굽는다).
Unreal 반영: `Tools/ui/incident_marker/apply_and_verify.py` (텍스처 임포트 + `M_UI_IncidentTimerRing` 생성, 에디터를 닫고 `UnrealEditor-Cmd -ExecutePythonScript`).
세부 값의 정본은 `asset-manifest.json`. 어떤 요청으로 어떻게 만들었는지는 `PROMPTS.md`（10/14 기준: 이 폴더에 `PROMPTS.md` 는 없다）.

## 무엇이 무엇을 말하나

| 요소 | 출동 전 (`ResponseStage == None`) | 출동 후 (`ResponseStage != None`) |
|---|---|---|
| 기호 | `Tone == Notice` → `?` / 그 외 → `!` | 출동 중 (사람 + 플러스) |
| 링 색 | `Urgency` 색 | Teal (밝은 톤) |
| 링 채움 | 남은 시간 비율, **줄어든다**. 시간 제한 없음이면 1 고정 | 작업 진행률, **차오른다**. 이동 단계는 0 |

- 링 하나가 출동 전후로 의미를 바꾼다. 09-28 스태프배치 가이드 ⑤ "테두리가 진행률로 차오름"을 출동 후 상태로 받는다.
- 색이 바뀔 때 기호와 채움 방향도 함께 바뀌므로 색만으로 상태를 가르지 않는다 (AT-011).
- 긴급도는 색에 더해 High = 느린 펄스, Immediate = 빠른 깜빡임 + 링 +1px.

## 긴급도 색

| Urgency | 색 | 팔레트 |
|---|---|---|
| Low | `#F1F4F2` | 신규 `IncidentUrgencyLow` |
| Medium | `#F2C230` | 신규 `IncidentUrgencyMedium` |
| High | `#E39A22` | 기존 `Amber` |
| Immediate | `#D74E43` | 기존 `Danger` |
| 출동 후 | `#6FB59A` | 신규 `IncidentDispatch` |

## 기하 (위젯 정사각형 한 변 대비 UV 반지름)

| 항목 | 값 |
|---|---|
| 원판 | 0.45, Ink, 불투명도 0.88 — 코드 브러시 (`RoundedBox`, `HalfHeightRadius`) |
| 링 | 0.355 ~ 0.425, 중심 (0.5, 0.5), 트랙 흰색 0.14 — `M_UI_IncidentTimerRing` |
| 기호 | 텍스처가 원판과 같은 정사각형. 기호는 그 안에서 0.9배 |
| 권장 표시 크기 | 40px (1080p). 32px도 기호는 읽히나 링 진행이 얇다 |

## 텍스처

`/Game/FestaFiesta/UI/LiveHUD/Themes/LightTeal/Icons/Incident/`

| 파일 | 쓰임 |
|---|---|
| `T_FF_LH_Icon_IncidentQuest_White_256` | `!` |
| `T_FF_LH_Icon_IncidentInfo_White_256` | `?` |
| `T_FF_LH_Icon_IncidentDispatched_White_256` | 출동 중 |

임포트는 LightTeal과 같다: `TC_EditorIcon`, `TEXTUREGROUP_UI`, 밉맵 없음, Never Stream, Clamp, sRGB.

**LightTeal 예외**: HUD 아이콘은 흰 선(4.5/64)·18~24px 규칙이지만, 월드 마커 기호는 40px 안팎 월드 위에서 읽혀야 해서 면 채움을 쓴다.

## 사건 정의 쪽

- 사건별 `MarkerIcon` 필드는 2026-10-06에 Definition·View에서 삭제했다. 기호는 Tone으로 정하고 텍스처는 마커 레이어가 갖는다.
- 현재 4개 정의 모두 `Tone`/`Urgency`가 기본값(문제·보통)이라 전부 노랑 `!`로 보인다. 기획이 값을 채워야 한다.
