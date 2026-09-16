# 밝은 지구 Blender 제작·UE 시험

상태: `confirmed` 2026-09-16 Blender 5.2.1 LTS → UE 5.7.4.
v002의 밝고 부드러운 육지색·해안 형태를 P05 전 세계에 확장했다. 24 OBJ·Blender 저장, 실제 형상 검증, 표시용 16메시 UE 시험 임포트까지 완료했다.
공용 Frontend 연결·기존 8개국 프랍 높이·외곽 패널 숨김을 저장했다. P10 밝기·햇살 개선과 P11 작은 화면 레이아웃 수정 후 실제 UI 포함 4개 화면비를 직접 확인했고, 별도 프로세스 PIE API/버튼 델리게이트 14개를 다시 통과했다. 물리 입력·목표 해상도 성능·패키징 검증은 남아 있다.
사용자 외형 판단과 재개 체크리스트는 로컬 `docs/ui/title-bright-globe-plan.md`를 따른다.

## 산출물

- 원본/출력: `SourceArt/UI/Atlas/BrightGlobe_v1/` (로컬 파일, Git 보관 여부 별도 확인).
- 현재 Blender: `Blender/FF_BrightGlobe_P05_world_v001.blend`, 씬 `FF_BrightGlobe_P05_World_v001`. P03 v001/v002와 P02 왕복 파일도 보존한다.
- 현재 시험 레벨: `/Game/FestaFiesta/UI/Atlas/Earth/BrightGlobe_v1/L_BrightGlobe_World`. 지역 `L_BrightGlobe_Pilot_v002`, 배경 `L_BrightGlobe_TitleStudy`도 별도 보존한다.
- 실제 게임 적용 레벨: `/Game/FestaFiesta/Maps/Frontend`.
- 현재 조절 에셋: 위 폴더의 `P05/MPC_BrightGlobe_Controls`.
- 현재 육지·해안 재질 인스턴스: `P05/MI_BrightGlobe_Land`, `P05/MI_BrightGlobe_Coast`. 지역 기준은 `P03v2/MI_BrightGlobe_Land_Soft_v002`, `P03v2/MI_BrightGlobe_Coast_Soft_v002`.
- 세계 OBJ: `SourceArt/UI/Atlas/BrightGlobe_v1/P05/Exports/`. 준비 캐시·구간 완료·내보내기 해시는 `Saved/BrightGlobe/P05v001/`.
- 보고서·원본 보존 검사: `Saved/BrightGlobe/`; 제작 전 백업: `Saved/BrightGlobeBefore/`.

## 제작 순서

1. `prepare_roundtrip.py`: 기준 파일 복사·해시, 지역 입력 추출. 이미 준비했으면 덮어쓰지 않는다.
2. `blender_roundtrip.py`: 켜진 Blender에 MCP로 모듈을 불러 `import_scene()`, `export_scene()` 순으로 호출한다.
3. `validate_roundtrip.py`, `check_height_contract.py`: 외부 Python에서 데이터 확인. 후자는 Pillow가 필요하다.
4. `import_roundtrip.py`: UE 별도 에셋으로 원본/출력을 비교한다. 완료 후 그대로 재실행하면 중단한다.
5. `blender_pilot.py`: MCP에서 `build()`, `export()` 순으로 실행한다. 기존 P03 씬이 있으면 새로 생성하지 않는다.
6. `import_pilot.py`: P03 재질·메시를 임포트하고 Frontend 사본을 만든다. 시험 레벨이 이미 있으면 중단한다.
7. `fix_ocean_uv.py`: 초기 바다 재질만 수정한 재개용 명령. 현재 `import_pilot.py`에도 수정이 포함되어 있다.
8. `capture_review.py`: 과거 다중 캡처 기록용. 종료 후 0xC0000005가 발생했으므로 그대로 재사용하지 않는다.
9. `finalize_pilot.py`: 저장된 에셋 재열기, 시험 레벨 초기 방향(위도 42 / 경도 12)·기본 조절값 확인.

P03 v002 지역 기준:

1. `blender_refine.py`의 `build()` / `export()`: 구간 공유정점 보존, 원본 면적 15% 이상을 남기는 국소 해안 폭 조절.
2. `blender_preview_materials.py`의 `apply()`: 지리 고정 원본 색상으로 Blender 재질 표시, 텍스처를 blend 안에 포함한다.
3. `import_refined_pilot.py`: 새 P03v2 폴더와 v002 시험 레벨. 원래 Frontend와 v001 레벨은 유지한다.
4. `validate_refined_geometry.py`: 내보낸 OBJ 재읽기, 면별 UV·접합·높이 변경 확인.
5. `validate_saved_blend.py`: 별도 Blender 프로세스로 저장 파일과 포함 텍스처 확인.
6. `apply_refined_fix.py` / `reimport_refined_coast_fix.py`: v002 극소 해안 폭을 보존한 일회성 복구·6메시 재임포트. 이미 완료했으며 이전 해시·씬 완료 플래그를 검사하므로 무조건 재실행하지 않는다. 수정 전 자료는 `Saved/BrightGlobeBefore/v002-before-shorefix/`에 보존했다.
7. `apply_soft_palette.py`: 사용자 피드백의 밝고 부드러운 육지색을 두 재질 인스턴스에 저장하고 6개 Top/Coast에 적용한다.
8. `capture_single_pilot.py`: 한 장의 screenshot task 완료와 PNG 헤더 확인 후 callback 해제·keep-alive 해제로 정상 종료. 프로세스 exit와 Python 오류 수도 반드시 확인한다. 기존 출력이 있으면 중단하므로 같은 결과를 무조건 덮어쓰지 않는다.
9. `capture_soft_pilot.py`: 7번 팔레트 저장 후 8번 최종 단일 화면을 수행한다. 최신 성공 기록은 `p04_v002_soft_final_retry.process.json`이다.

P04 배경 시험·P05 전 세계:

1. `setup_title_study.py`: 지역 시험 레벨을 복제해 화면 기준 크림→하늘 배경을 만든다. `tune_title_background.py`는 새 `MI_BrightGlobe_Backdrop`의 `BackgroundBrightness`를 4.5로 저장한다. 조명·노출 변경 없이 현재 타이틀 카메라로 확인했다.
2. `blender_world.prepare()`: 일반 Python에서 원본 8구간을 함께 읽어 공유 경계와 지형 설정을 계산한다. `P05v001/`에 입력 해시·설정 fingerprint·구간 캐시를 저장한다. 입력/설정이 같은 완료 캐시를 재사용하며, 다른 버전을 무조건 덮지 않는다.
3. 켜진 Blender에서 `blender_world.build_tile(0..7)`와 `export_tile(0..7)`를 구간별로 호출하고 `save()`한다. 실제 호출은 각 정수 인자를 넣어 순차 수행한다. 24메시와 포함 텍스처, 표시용 미리보기 바다를 저장한다.
4. `validate_world_geometry.py`: 일반 Python으로 전체 실제 OBJ·UV·방향 있는 면·bilinear 높이·구간/극점 연결을 확인한다. 중단 시 `--tile N`으로 구간 검사 후 `--finalize`로 모으거나 전체 검사를 실행한다. `--cache-only` 결과만으로 내보낸 파일 검증을 대체하지 않는다.
5. `validate_world_blend.py`: 별도 Blender 프로세스로 저장 세계판을 재열어 확인한다. Bottom은 숨김 보존 메시라 표시 재질을 요구하지 않고 Top/Coast의 재질과 포함 텍스처를 확인한다.
6. `import_world.py`: 검증 passed와 OBJ 해시를 확인한 뒤 16개 Top/Coast를 `P05/`에 임포트하고 `L_BrightGlobe_World`를 만든다. 구간별 임포트 기록을 저장하며 완료한 세계판을 무조건 재임포트하지 않는다. 바다 수색은 지역 제한을 제거한 세계용 재질이다.
7. `capture_scene.py`: `Saved/BrightGlobe/capture_request.json`에서 레벨·고유 출력 이름·카메라·해상도와 선택적인 높이 조절값을 읽어 한 장씩 촬영한다. `camera=title`은 현재 타이틀 카메라, `camera=geo`는 `latlon`·`distance`·`fov`를 사용한다. 이미 있는 결과를 덮지 않으며 JSON 저장뿐 아니라 별도 `.process.json`의 exit 0 / Python 오류 0도 확인한다.
8. `check_water_uv.py`는 실제 UE 바다 구체의 UV를 원본 지도와 대조한다. `fix_world_water_uv.py`는 세계용 바다 재질에 `(Frac(U+0.5), 1−V)`를 적용한다. 초기 `world_review/01_*`·`02_*`는 V 보정 전 진단 화면이므로 최종 검토에는 `_fixed` 결과를 사용한다. 기존 P03 에셋은 이 수정 대상이 아니다.
9. `prepare_prop_offsets.py`는 기존 국가 위경도를 유지한 표면 높이 보정 자료를 준비한다. `integrate_world_frontend.py`는 기존 3개 에셋 백업·필드 비교 후 Frontend 메시/재질/배경·카탈로그 프랍 높이·타이틀 외곽 패널 2개 숨김을 저장한다. `p06_frontend_integration.json` passed / `p06_frontend_retry2.process.json` exit 0·오류 0으로 완료했다. 완료한 통합을 무조건 반복하지 않는다.
10. `verify_world_frontend.py`: 별도 프로세스에서 저장 Frontend의 PIE API/버튼 델리게이트를 확인한다. `p08_frontend_verify.json` 14개 passed / `p08_frontend_verify_retry.process.json` exit 0·오류 0. 검사는 에셋을 저장하지 않는다. 물리 마우스·키보드·패드와 시각 품질 검사가 아니다.

UE Python 실행 예:

```powershell
& Tools/ui/blender/bright_globe/run_editor.ps1 -Script Tools/ui/blender/bright_globe/capture_single_pilot.py -Label p04_single
```

이 실행기는 이미 켜진 Unreal Editor가 있으면 중단한다. 같은 패키지를 두 에디터에서 저장하지 않는다.
Blender는 계속 켜둘 수 있다. 에셋 임포트 등 화면이 필요 없는 명령만 `-NullRHI`를 붙인다.
`capture_baseline.py`의 PIE 캡처는 작은 에디터 뷰포트를 포함하므로 최종 외형/성능 근거로 사용하지 않는다.

UE 5.7의 `MaterialEditingLibrary.set_material_instance_vector_parameter_value`는 설치 엔진 소스상 값을 설정해도 false를 반환한다(`MaterialEditingLibrary.cpp` 1253–1262행). 반환값만 assert하지 말고 설정 후 실제 파라미터를 읽어 비교한다. 첫 `p04_v002_soft_final`은 이 assert로 Python 오류가 발생했으며 최신 retry는 값 읽기 검증 후 오류 0으로 통과했다.

## 유지해야 하는 좌표·변위 계약

- 반지름 5000 uu. P03 지역은 경도 -20~55 / 위도 20~65의 열린 시험 경계이고, P05는 경도 -180~180 / 위도 -90~90의 원본 8구간 전체다.
- Blender OBJ 입출력은 forward Y / up Z / scale 1. UE의 기존 OBJ 임포트 변환까지 포함해 비교했다.
- Top UV는 원본 지리 좌표. Coast UV는 색상 좌표가 아니라 `(원본 높이, 변위 가중치)`다. 일반 UV 언랩 금지.
- 새 형상 기준: 육지 두께 40 / 산 높이 계수 650 / 해안 단면 깊이 12 uu. v002는 둥근 단면 6단, 폭 목표 10uu이며 작은 지형은 좁힌다.
- 원본 안쪽 폭이 0.01uu 미만이면 원래 방향을 보존한다(P03 486정점 / P05 3,790정점). 극소 폭의 수치 오차를 정규화하여 수 uu 폭으로 늘리지 않는다. P05 공유 원본 위치 3,306개와 원본 투영 면적 최소 15%를 유지한다.
- 고도 곡선: `g(h) = max(0, (h - 0.008) / 0.992)^1.15`.
- UE WPO: 반지름 방향 × `[(LandThickness - 40) + g(h) * (MountainHeight - 650)]` × 가중치.
- `MountainHeight`는 계수다. 모든 산의 높이가 650 uu라는 뜻이 아니다.
- 실제 UE 바다 구체는 +X가 U=0이고 지도/육지는 -X가 U=0이며, 바다 V방향도 지도와 반대다. **세계용 바다 마스크 좌표는 `(Frac(U+0.5), 1−V)`**다. `p05_water_uv.json`의 북위 약 35도 샘플은 구체 V≈0.6953 / 지도 V≈0.3047이다. 육지 UV나 액터 전체를 돌려 맞추지 않는다. P03 지역 에셋의 U만 보정한 과거 재질은 별도 보존한다.
- Bottom은 보존/내보내기하지만 UE에는 Top+Coast만 사용한다(P03 6개 / P05 16개). Blender의 미리보기 바다는 UE에 내보낼 육지 메시 수에 포함하지 않는다.

## v002 확인 결과와 기본 색상

- `p03_v002_validation.json` passed: 실제 OBJ·UV·PNG bilinear 샘플로 40/650·75/650·40/1000 확인. 해안 반전·퇴화면·짧은 변 0, 최대 접합 차이 0.012842694uu(허용 0.05). 기본 설정 접합 차이 0.
- `p03_v002_coast_reimport.json` passed: 수정된 Top/Coast 6메시 UE 재임포트·삼각형 수 읽기 완료. `p03_v002_blend_reopen.json` passed: 저장한 9메시·UV·6재질·포함 텍스처 재열기 확인.
- 사용자 최종 피드백은 “육지색을 조금 더 밝고 부드럽게”. linear RGB: Grass (0.32, 0.64, 0.12), Forest (0.10, 0.42, 0.11), Sand (0.96, 0.75, 0.37), Coast (0.73, 0.52, 0.24). 바다색·지형 높이 기본값은 유지했다.
- 실제 UE 최종 이미지: `Saved/BrightGlobe/review_v002/region_default_40_650_final.png` (1600×900). `p04_v002_soft_final_retry.process.json` exit 0 / Python 오류 0, `review_v002/capture.json` passed. 이미지 저장과 프로세스 정상 종료를 따로 확인했다.
- 새 단일 캡처가 정상 종료한 사실과 v001 종료 충돌 원인 규명은 별개다. v001의 0xC0000005 근본 원인은 미확정이며 옛 다중 캡처를 그대로 다시 돌리지 않는다.

## P05 세계판 확인 결과

- `p05_validation.json` passed: 24 실제 OBJ, 40/650·75/650·40/1000의 변위 계약, 공유 원본 위치 3,306개·극점 인스턴스 4,112개 검사. 최대 Top/Coast 접합 차이 0.022391399uu(허용 0.05), 공유 위치 차이 0, 조절 후 최대 공유 차이 약 2.27e-13uu.
- 산 계수 1000에서 법선이 기준과 90도 이상 달라진 해안 어깨 30면은 방사 방향의 부호 있는 면적과 조절 전 구간의 4차 면적식 최소값으로 연속 회전과 붕괴를 구분했다. 단순한 법선 dot 음수만으로 면 뒤집힘이라고 판정하지 않는다. 확인한 설정에서 퇴화면·짧은 변·검증되지 않은 반전은 없다.
- `p05_blend_reopen.json` passed: 24메시·UV·포함 텍스처·미리보기 바다 확인. 숨김 Bottom의 표시 재질이 없다는 사실은 오류가 아니다. 이 가정을 고친 `p05_blend_reopen_retry.log`에서 재열기 통과를 확인했다.
- `p05_ue_import.json` passed: 16 표시 메시의 해시·삼각형 수·UV 개수·슬롯 확인. `p05_world_import.process.json` exit 0 / Python 오류 0.
- 전체 소스 4,725,988삼각형 / 표시용 육지 메시의 원본 삼각형 합계 2,963,888. 옛 합계 2,091,252보다 약 41.7% 많다. GPU의 실제 그린 삼각형 수 측정값은 아니다. 성능 향상을 주장하지 않으며 같은 조건의 프레임 측정이 필요하다. P03 지역은 전체 446,004 / 표시용 원본 합계 292,387이었다.
- P04 배경 시험 `title_study/title_cream_sky_bright.png`는 현재 타이틀 카메라/FOV 90·1600×900, 밝기 4.5이다. `p04_title_bright_capture.process.json` exit 0 / Python 오류 0. 세계판 화면은 `Saved/BrightGlobe/world_review/`에 순차 저장한다.
- 초기 세계판 01/02 캡처는 프로세스가 정상 종료했어도 바다 V방향으로 수색이 어긋난 진단용 화면이다. `p05_water_uv_fix.json` passed / `p05_water_fix.process.json` exit 0·오류 0. 최종 `03_world_asia_fixed.png`·`04_frontend_title_scene.png`·`05_world_south_pole.png`는 정상 종료와 화면 확인을 마쳤다. 모두 UI 없는 장면이다.

## Frontend 열기·조절·복원

- Unreal에서 `/Game/FestaFiesta/Maps/Frontend`를 열고 실행한다. 별도 시험이 필요하면 `L_BrightGlobe_World`를 사용한다.
- 두께/산: `/Game/FestaFiesta/UI/Atlas/Earth/BrightGlobe_v1/P05/MPC_BrightGlobe_Controls`의 `LandThickness=40`, `MountainHeight=650`. 높이를 크게 바꾸면 기본값 기준으로 맞춘 프랍 바닥도 다시 확인한다.
- 현재 P10 색/배경 조절은 아래 햇살 마감 절을 따른다. 이전 P05 육지/해안색과 상위 `MI_BrightGlobe_Backdrop`(밝기 4.5)는 복원용으로 보존했다.
- 기존 `Frontend.umap`, `DA_FestivalAtlas.uasset`, `WBP_TitleMain_MyCasual.uasset`만 변경했다. 두 외곽 Border만 Collapsed로 하고 다른 위젯·카메라·카탈로그 필드를 보존했다. 프랍 위경도는 유지했으며 8개국 표면 오프셋은 `p05_prop_offsets.json`에서 대조한다.
- 실제 백업: `Saved/BrightGlobeBefore/P06-20260916-183442-520428/Content/`. 복원은 에디터를 닫고 현재 파일에 후속 변경이 없는지 확인한 뒤 해당 파일만 원래 상대 경로에 복사한다. `p06_frontend_integration.json`에 원본/최종 해시가 있다. 전체 reset/clean을 복원 수단으로 쓰지 않는다.
- `p08_original_hash_audit.json`: 기준 98개 중 95개 동일, 위 3개만 의도한 변경. 옛 `original_hash_check.json`의 98개 동일 결과는 통합 이전 기록이다.
- PIE 14개 결과에는 타이틀→플레이 허브→Atlas→한국 지도→뒤로→독일 준비 중/진입 방어→드래그 API/포커스→숨김/다시 표시→타이틀이 포함된다. 물리 입력은 미검증이며 검사 중 에셋 해시는 유지됐다. 첫 `WidgetBlueprintLibrary` 별칭 오류 기록보다 `p08_frontend_verify_retry` 결과가 최신이다.

## 현재 한계

- 형상 검사에는 전체 극지·이음선이 포함되지만, 모든 방향의 GPU 외형·물리 입력을 통한 국가 선택·조형물 바닥의 근접 외형은 별도 검증이 필요하다. PIE API 국가 선택/화면 이동은 위 14개 검사에서 통과했다.
- Frontend 저장 연결과 패널 숨김, P11 실제 타이틀 UI 가독성·4개 화면비 검토는 완료했다. P09는 실제 PIE 559×190에서 구형/신형 GPU 중앙값 4.84575→5.63930ms(+16.376%)를 측정했다. 이는 P10 이전 재질의 소형 뷰포트 비교이며 목표 해상도/현재 P10 성능 보장이 아니다. 물리 입력·패키징·콘솔 실기는 미완료다.
- 원값 0/1/2 마스크를 그대로 밉 생성하면 반올림으로 블록이 생기므로 원본 샘플을 정규화한 뒤 평균한다. P03 수색은 지역 제한, P05 수색은 전 세계 범위다.
- 색상은 옛 `T_Diorama_Color`가 아니라 `T_FF_Earth_CasualClay_Aligned_2048x1024.png`를 참조한다. 새 AI 지도를 그리거나 UV를 이동하지 않았다.

## 출처와 제작 도구

- NOAA ETOPO1 및 Solar System Scope 파생 자료: `THIRD_PARTY_NOTICES.md` 참조.
- 참고한 외부 스킬: RobLe3/cc-blender-skill, v1.3.0, commit `11016c9`의 blender-modeling / blender-uv-texturing / blender-export.
- 스킬의 일반 축·UV 예제보다 이 프로젝트의 실제 왕복 검증과 위 계약을 우선했다.

## P10 맑은 파랑·햇살 마감 (2026-09-16)

- 사용자 밝기 피드백을 반영해 신규 `P10/` 재질과 `L_BrightGlobe_SunnyStudy`를 만들고 실제 Frontend에 연결했다. P05 메시/원본 슬롯·지도·Blender 파일·MPC 높이·카메라·WBP·국가 카탈로그는 유지했다. 육지는 Frontend 컴포넌트의 재질 override로 연결한다.
- `P10/MI_BrightGlobe_SunnyWater`: `OceanColor=(.003,.155,.90)`, `ShallowColor=(.025,.64,.90)`, `OceanRoughness=.34`, `OceanSpecular=.28`, `CoastWidthTexels=2.6`. 기존 지리 UV와 원본 얼음 범위 유지.
- `P10/MI_BrightGlobe_SunnyLand`·`MI_BrightGlobe_SunnyCoast`: 초원/숲/모래/눈과 해안 색. 부모는 기존 P05 재질이므로 높이 조절 계약도 같다.
- `P10/MI_BrightGlobe_SunnyBackdrop`: `Sky`, `Cream`, `Brightness=4.2`, `GradientStart=.22`, `GradientEnd=.66`. `SunGlow=.60`, `SunRadius=.30`, `RayStrength=.07`, `SunPosition=(.94,-.055,0)`으로 오른쪽 위 햇살을 조절한다. 화면 가로세로 비율을 반영한 재질 효과이며 물리적인 태양 위치는 아니다.
- 조명은 Frontend의 `DirectionalLight=4.0`, `FF_Clay_FillLight=.48`, `FF_Clay_SkyFill=.14`. 고정 노출은 유지하고 Bloom .22, AO .30, Vignette 0으로 조정했다. 배경 그라데이션으로 SkyLight를 재캡처하지 않는다.
- `PostProcessQuality@1`은 엔진 설정상 `r.LensFlareQuality=0`이다. 품질 설정을 강제로 바꾸지 않으며 햇살은 배경 재질로 표시한다. 고품질에서 사용하는 기본 LensFlareIntensity는 .025, Threshold 4다. 강한 렌즈 고스트를 추가한 작업은 아니다.
- 제작 순서: `prepare_sunny_title.py` → `tune_sunny_title.py` → `capture_scene.py`의 `sunny_review/02_sunny_title_final.png` 시각 확인 → `apply_sunny_frontend.py` → `verify_sunny_frontend.py`. 준비 스크립트는 기존 준비 기록/에셋을 덮어쓰지 않는다.
- P10 직전 백업은 `p10_prepare.json`의 `backup_directory`를 따른다. 원래 Frontend만 되돌리면 이전 재질과 조명이 복구된다. 후속 수정 여부/해시를 확인하고 에디터를 종료한 뒤 복원한다. 새 재질을 일괄 삭제할 필요는 없다.
- 적용/검증 증거는 `p10_frontend_apply.json`, `p10_frontend_verify.json` 및 각 `.process.json`을 함께 확인한다. `sunny_review/` 캡처는 UI를 제외한 실제 UE 장면이다.

## P11 실제 타이틀 UI·화면비 마감 (2026-09-16)

- P10 장면 캡처에는 UI가 없었다. `capture_title_ui.py`는 실제 Frontend PIE의 로고·워드마크·플레이/옵션/종료 버튼을 함께 캡처하고 표시 상태·라벨 색·geometry 가용 여부를 보고서에 기록한다. 이번 비표시 창에서는 geometry가 0이어서 **경계·겹침은 PNG를 직접 확인**했다. 보고서의 `contained=false`를 잘림 판정으로 해석하지 않는다.
- 4:3에서 확인한 로고·지구 겹침은 `fit_title_layout.py`로 수정했다. 타이틀의 기존 Canvas를 `SafeZone > ScaleBox(ScaleToFit, DownOnly) > SizeBox(1920×1080) > 기존 Canvas`로 감싸고 왼쪽·세로 중앙에 정렬한다. 기존 위젯·버튼 속성과 이벤트 배선, 타이틀 외곽 패널의 Collapsed 상태는 보존한다. 넓은 화면에 맞춰 메뉴를 무한히 확대하지 않는다.
- 적용은 `p11_title_layout.json` status `passed`, `p11_title_layout_apply.process.json` exit 0 / Python 오류 0이다. `/Game/FestaFiesta/UI/Title/GlassOrbit/WBP_TitleMain_MyCasual` 하나만 저장했고 다른 6,869패키지가 변하지 않았음을 확인했다. 기존 Frontend·국가 카탈로그·P10 재질·Blender 파일은 이번 레이아웃 수정 대상이 아니다.
- WBP 원본과 SHA256은 `p11_title_layout.json`의 `backup_file`·`title_sha256_before`에 기록한다. 이번 백업은 `Saved/BrightGlobeBefore/P11-20260916-233549-784880/Content/FestaFiesta/UI/Title/GlassOrbit/WBP_TitleMain_MyCasual.uasset`이다. 완료 후 다시 실행하면 완료 보고서와 실제 구조·해시를 비교하고 저장 없이 반환한다. 미완료 보고서나 후속 수정을 발견하면 자동으로 덮어쓰지 않는다.
- **최종 네 화면 직접 검토 완료:** `Saved/BrightGlobe/title_ui_v005_final/report.json` status `captured`, 실제 viewport 960×540·960×720·1600×900·1680×720가 모두 요청과 일치했다. `visual_review.json` status `visually_reviewed`에 로고/버튼 가독성, UI 문구 잘림 없음·메뉴끼리 겹침 없음·검은 기둥 여백 없음의 이미지 판정을 기록했다. 4:3에서는 지구 일부가 불투명 버튼 뒤로 보이지만 로고 글자와 버튼 문구가 가려지거나 잘리지 않는다. 햇살 수치는 유지했다.
- `p11_title_ui_v005_final.process.json` exit 0 / Python 오류 0, 92.5초, PIE 종료·에셋 무변경이다. 실제 UI 렌더로 저장된 ScaleBox/SizeBox 구조를 별도 프로세스에서 확인했다. 적용 당시 보고서의 `separate_process_verified=false`와 캡처 원본의 `visual_review_complete=false`는 자동으로 갱신하지 않으며, **후속 `visual_review.json`이 시각 검토 결과**다. PNG 크기는 창 장식을 포함한 966×578·966×758·1606×938·1686×758이고 viewport와 구분한다.
- 레이아웃 저장 후 `p11_frontend_verify.json`은 기존 화면 이동·국가 잠금·회전/숨김 API 등 14개 `passed`, 19.985초, 에셋 무변경·PIE 종료다. `p11_frontend_verify.process.json` exit 0 / Python 오류 0. 물리 마우스/키보드/게임패드 입력·성능·패키징 검사로 확대 해석하지 않는다.

### UI 포함 캡처와 해상도 확인

1. GUI 에디터를 닫고 `Saved/BrightGlobe/title_ui_request.json`에 고유 `output` 이름과 선택적인 `cases` 목록을 지정한다. 같은 결과 이미지를 덮지 않는다. 실행 전 PIE 창 설정도 백업한다. 창 위치 설정은 에디터 종료 때 자동 저장될 수 있다.
2. `run_editor.ps1 -Script Tools/ui/blender/bright_globe/capture_title_ui.py -Label <고유 이름>`으로 실행한다. 각 화면에서 로딩과 렌더링을 기다리고 `Shot SHOWUI`로 실제 UI를 포함해 캡처한다. 카메라/지구 방향은 화면 사이에 고정해 비교한다.
3. `report.json`의 `requested`, `viewport`, `viewport_matches_requested`와 PNG `png_size`를 대조한다. **SHOWUI PNG에는 Slate 제목바가 포함될 수 있으므로 PNG 크기를 게임 뷰포트 크기로 쓰지 않는다.** 실제 뷰포트 기준은 `PlayerController.get_viewport_size()`다.
4. 네 이미지 직접 검토, PIE 종료, 에셋 해시 보존, 별도 `.process.json`의 exit 0 / Python 오류 0을 확인한다. `captured`는 캡처 종료 상태이며 자동으로 외형 검토 통과를 뜻하지 않는다.

`RenderOffscreen`의 `FNullApplication`은 `r.SetRes` 값을 가상 작업영역 크기로 사용한다. 이전 PIE 창 크기가 남으면 다음 큰 창이 그 크기에 제한되어 실행 인자와 실제 뷰포트가 달라질 수 있다. 현재 캡처 스크립트는 매 PIE 생성 전에 `r.SetRes 4096x2160w`를 실행하고 다음 단계까지 2초 기다린 뒤 창을 만든다. 창 생성 크기의 −2/−12 보정은 이 환경의 실측값이며, 결과 보고서의 viewport 일치 여부로 다시 검증한다. 일반 사용자 창에 적용하는 게임 해상도 설정이 아니다.

v001은 WidgetTree 대체 탐색 실패, v002/v003/v004는 UI가 있어도 목표 해상도가 다른 진단 결과다. 최종 검토나 성능 근거로 인용하지 않는다. 원본을 찾지 못할 때 임의 WidgetTree를 선택하지 않고 런타임 타이틀 인스턴스의 Outer로 유일성을 확인한다.
