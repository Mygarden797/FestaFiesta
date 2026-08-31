# FESTA FIESTA 미니게임 UI/UX 이미지 세트

현재 구현된 `TargetShooting`과 `Surfing`의 실제 UMG 계약을 기준으로 만든
프로덕션용 이미지 세트입니다. 기존 `AAA` 배경은 롤백용으로 유지하고,
신규 이미지는 `UIUX_v2` 경로에 비파괴 추가했습니다.

서핑의 현재 기본 플레이 화면은 2026-08-29에 생성한 최종 백드롭과 서퍼를
사용하는 2.5D 구성입니다. 원본은 `SourceAssets/UI/Minigame/Final`에 있습니다.
`UIUX_v2` 배경은 롤백용으로 그대로 보존합니다.

## 이미지 목록

| 이미지 | 원본 규격 | 런타임 용도 |
| --- | ---: | --- |
| `T_MG_TargetShooting_Backdrop_ImageGen_v3.png` | 1672×941 RGB | 기존 3D 사격장 분위기의 롤백용 배경 |
| `T_MG_TargetShooting_Backdrop_Single_ImageGen_v4.png` | 1672×941 RGB | 전경 구조물 없이 사용하는 기본 단일 슈팅 배경 |
| `T_MG_TargetShooting_Target_Simple_ImageGen_v1.png` | 1254×1254 RGBA | WBP `Image_Target`의 미니멀 원형 과녁 |
| `T_MG_TargetShooting_Backdrop_UIUX_v2.png` | 1672×941 RGB | 타깃 슈팅 전체 화면 배경 |
| `T_MG_TargetShooting_WaterBlaster_UIUX_v2.png` | 1280×1280 RGBA | 타깃 슈팅 하단 1인칭 물총 |
| `T_MG_Surfing_Backdrop_UIUX_v2.png` | 1672×941 RGB | 서핑 초기 화면과 2D 폴백 배경 |
| `T_MG_Surfing_Rock_UIUX_v2.png` | 1254×1254 RGBA | 바위 장애물 스프라이트 |
| `T_MG_Surfing_Log_UIUX_v2.png` | 1254×1254 RGBA | 통나무 장애물 스프라이트 |
| `T_MG_Surfing_Buoy_UIUX_v2.png` | 1254×1254 RGBA | 부표 장애물 스프라이트 |
| `T_MG_Surfing_Coin_UIUX_v2.png` | 1254×1254 RGBA | 코인 픽업 스프라이트 |
| `T_MG_Surfing_Heart_UIUX_v2.png` | 1254×1254 RGBA | 라이프 픽업 스프라이트 |
| `T_MG_Surfing_BoostPickup_UIUX_v2.png` | 1254×1254 RGBA | 부스트 충전 픽업 스프라이트 |
| `T_MG_Surfing_Backdrop_Final.png` | 1672×941 RGB | 서핑 최종 플레이 배경 |
| `T_MG_Surfing_Surfer_Final.png` | 1024×1536 RGBA | 서핑 최종 플레이어·보드·물보라 |

원본 경로: [`SourceAssets/UI/Minigame/UIUX_v2`](../../../SourceAssets/UI/Minigame/UIUX_v2)

최신 타깃 슈팅 단일 배경 원본은
[`SourceAssets/UI/Minigame/TargetShooting`](../../../SourceAssets/UI/Minigame/TargetShooting)에
버전 파일명으로 별도 보관합니다. `UIUX_v2`는 즉시 롤백 가능한 이전 버전입니다.

스프라이트는 고해상도 투명 원본을 보존합니다. 기본 UMG 표시 크기는
장애물·픽업 96×96, 서퍼 150×225이며 `VisualStyle`에서 수정할 수 있습니다.

## UX 제약

- 타이머, 점수, 표적, 조준점, 피드백, 결과 수치는 모두 UMG/Slate 동적 레이어입니다.
- 배경에 문자열, 표적, 플레이어, 장애물, 픽업을 굽지 않습니다.
- 타깃 슈팅은 중앙 과녁, 상단 기록 HUD, 하단 1인칭 물총을 우선하는 Aim Trainer형 화면 구성을 사용합니다.
- 단일 배경은 고정되며, 실제 마우스 화면 좌표와 동일한 위치에 커스텀 조준점을 표시하고 총기를 조준합니다.
- 기본 슈팅 화면은 단일 v4 배경만 사용하며, 천장·기둥·난간·바닥 가이드 같은 전경 환경 오브젝트를 표시하지 않습니다.
- 물총은 회전 가능한 표시 레이어이며, 발사 시 에셋별 총구 좌표에서 반동·총구 섬광과 물방울 탄환 궤적을 시작합니다.
- 타깃 슈팅 프로덕션 외형은 WBP에 저장된 `Image_Backdrop`, `Image_Target`, `Image_WaterBlaster` Brush가 담당합니다. C++ 벡터 과녁은 WBP가 없는 비상 폴백에서만 표시합니다.
- 런타임은 디자이너 WBP의 Brush, Tint, Font, Text Color와 Button Style을 덮어쓰지 않고 값·위치·표시 상태만 갱신합니다.
- 서핑 정상 플레이는 최종 생성 아트를 사용하는 2.5D 경로입니다. 원근 크기, 깊이 순서,
  서퍼 보간·뱅킹·보빙, 배경 드리프트는 코드 표시 레이어에서 처리합니다.
- 바다 배경 위의 인위적인 수평 가이드, 전체 화면 회색 scrim, arena glass/vignette는
  사용하지 않습니다. 배경 한 장과 실제 플레이 오브젝트만 합성합니다.
- 프리미티브 기반 3D SceneCapture는 `VisualStyle.bEnableSceneCapture3D`를 켠 경우에만
  사용되는 선택 경로이며, 프로덕션 3D 에셋이 없는 기본 상태에서는 꺼져 있습니다.
- 현재 흐름에 없는 인트로, 규칙 설명, 카운트다운, 일시정지, 재도전 화면은 생성하지 않았습니다.

## Unreal 임포트

에디터를 닫은 상태에서 실행합니다.

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' `
  'FestaFiesta.uproject' `
  '-ExecutePythonScript=Tools/ui/import_minigame_uiux_v2_assets.py' `
  -nop4 -nosplash -nullrhi -DDC-ForceMemoryCache
```

임포트 위치는 `/Game/FestaFiesta/UI/Minigame/UIUX_v2`이며 `UI`, `sRGB`,
`NoMipmaps`, `NeverStream` 설정을 강제합니다. 이 폴더는 기존 AlwaysCook 범위에 포함됩니다.

ImageGen v4 단일 타깃 슈팅 배경은 다음 명령으로 별도 임포트합니다.

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' `
  'FestaFiesta.uproject' `
  '-ExecutePythonScript=Tools/ui/import_target_shooting_backdrop_imagegen_v4.py' `
  -nop4 -nosplash -nullrhi -DDC-ForceMemoryCache
```

임포트 위치는 `/Game/FestaFiesta/UI/Minigame/TargetShooting`입니다.

미니멀 과녁만 다시 임포트할 때는 다음 명령을 사용합니다.

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' `
  'FestaFiesta.uproject' `
  '-ExecutePythonScript=Tools/ui/import_target_shooting_target_imagegen_v1.py' `
  -nop4 -nosplash -nullrhi -DDC-ForceMemoryCache
```

개별 이미지 교체 지점은 `WBP_FF_TargetShooting > Widget Designer`의 각 Image Brush입니다.
`Image_Target`은 이름과 `Overlay_TargetVisual` 부모를 유지합니다. 화면 전체를 교체할 때는
같은 위젯 이름 계약을 가진 WBP 복제본을 만든 뒤 `DA_Minigame_TargetShooting > Screen Class`에
새 클래스를 지정합니다.

최종 서핑 백드롭과 서퍼를 임포트할 때는 다음 명령을 사용합니다.

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' `
  'FestaFiesta.uproject' `
  '-ExecutePythonScript=Tools/ui/import_surfing_final_assets.py' `
  -nop4 -nosplash -nullrhi -DDC-ForceMemoryCache
```

임포트 위치는 `/Game/FestaFiesta/UI/Minigame/Final`입니다.

## 생성 방식

모든 신규 비트맵은 Codex 내장 ImageGen으로 생성했습니다. 기존 미니게임 배경은
설정·색상·카메라 참고 이미지로만 사용했고, 원본은 수정하지 않았습니다. 전체 프롬프트와
투명 배경 보정 이력은 [PROMPTS.md](PROMPTS.md)에 기록했습니다.
