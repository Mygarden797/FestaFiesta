# FESTA FIESTA 미니게임 UI/UX 이미지 세트

현재 구현된 `TargetShooting`과 `Surfing`의 실제 UMG 계약을 기준으로 만든
프로덕션용 이미지 세트입니다. 기존 `AAA` 배경은 롤백용으로 유지하고,
신규 이미지는 `UIUX_v2` 경로에 비파괴 추가했습니다.

## 이미지 목록

| 이미지 | 원본 규격 | 런타임 용도 |
| --- | ---: | --- |
| `T_MG_TargetShooting_Backdrop_UIUX_v2.png` | 1672×941 RGB | 타깃 슈팅 전체 화면 배경 |
| `T_MG_TargetShooting_WaterBlaster_UIUX_v2.png` | 1280×1280 RGBA | 타깃 슈팅 하단 1인칭 물총 |
| `T_MG_Surfing_Backdrop_UIUX_v2.png` | 1672×941 RGB | 서핑 초기 화면과 2D 폴백 배경 |
| `T_MG_Surfing_Rock_UIUX_v2.png` | 1254×1254 RGBA | 바위 장애물 폴백 스프라이트 |
| `T_MG_Surfing_Log_UIUX_v2.png` | 1254×1254 RGBA | 통나무 장애물 폴백 스프라이트 |
| `T_MG_Surfing_Buoy_UIUX_v2.png` | 1254×1254 RGBA | 부표 장애물 폴백 스프라이트 |
| `T_MG_Surfing_Coin_UIUX_v2.png` | 1254×1254 RGBA | 코인 픽업 폴백 스프라이트 |
| `T_MG_Surfing_Heart_UIUX_v2.png` | 1254×1254 RGBA | 라이프 픽업 폴백 스프라이트 |
| `T_MG_Surfing_BoostPickup_UIUX_v2.png` | 1254×1254 RGBA | 부스트 충전 픽업 폴백 스프라이트 |

원본 경로: [`SourceAssets/UI/Minigame/UIUX_v2`](../../../SourceAssets/UI/Minigame/UIUX_v2)

스프라이트는 고해상도 투명 원본을 보존하며 Unreal 임포트 시
`MaxTextureSize=512`로 제한합니다. 현재 UMG 표시 크기는 52×52입니다.

## UX 제약

- 타이머, 점수, 표적, 조준점, 피드백, 결과 수치는 모두 UMG/Slate 동적 레이어입니다.
- 배경에 문자열, 표적, 플레이어, 장애물, 픽업을 굽지 않습니다.
- 타깃 슈팅은 중앙 과녁, 상단 기록 HUD, 하단 1인칭 물총을 우선하는 Aim Trainer형 화면 구성을 사용합니다.
- 물총은 표시 전용 레이어이며, 발사 시 반동·총구 섬광과 별도의 물방울 탄환 궤적을 보여 줍니다.
- 타깃 슈팅 링은 판정 타원과 일치해야 하므로 기존 C++ 벡터 페인트를 유지합니다.
- 서핑 정상 플레이는 3D SceneCapture를 우선하며, 배경과 6개 스프라이트는 시작 상태와 2D 폴백을 완성합니다.
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

## 생성 방식

모든 신규 비트맵은 Codex 내장 ImageGen으로 생성했습니다. 기존 미니게임 배경은
설정·색상·카메라 참고 이미지로만 사용했고, 원본은 수정하지 않았습니다. 전체 프롬프트와
투명 배경 보정 이력은 [PROMPTS.md](PROMPTS.md)에 기록했습니다.
