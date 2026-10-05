# 타이틀 인트로 — 시작 화면 · 우주 폭죽 영상 · 우리 지구

게임을 켜고 프론트엔드에 처음 들어오면 한 번만 본다 (v2, 2026-10-02 사용자 지시).

1. **시작 화면** (UE 위젯, 약 3.5초): **검은 바탕**에 큰 꽃 로고(검은 바탕용 밝은 버전) · `스튜디오 들썩` ·
   `내일배움캠프 Unreal 트랙 8기 13조` · 맨 아래 실제 축제 주최 측과 관련 없음 안내. 게임 이름 글자 없음.
2. **인트로 영상** (`Content/Movies/FF_Intro_SpaceFireworks.mp4`, 8초 30fps, 영상엔 소리 없음): 별빛 우주의 실사 지구 →
   부산부터 세계 도시 표면 곳곳에서 축제 폭죽이 터지고 터진 자리부터 우리 스타일라이즈드 지구로 물든다 →
   남은 곳이 한꺼번에 바뀌고 둘레로 마지막 폭죽 → 변신 빛이 퍼지며 우주가 밝은 하늘로 → 흰 빛 정점.
   원본은 Blender `Tools/ui/blender/intro_previs/intro_space_fireworks.py`.
3. **우리 지구** (UE 실시간): 흰 덮개가 걷히며 가운데 지구만 보인다(글자 없음). 1.3초 뒤 **스스로** 다음으로.
4. **메뉴**: 타이틀이 올라오고 카메라가 가운데 → 타이틀 구도로 1.4초 블렌드(지구가 오른쪽으로 비켜 감).

**소리**: `S_FF_Intro_SpaceFireworks`(10.2초)를 영상이 열리는 순간 따로 튼다 — 화음이 지구 공개까지 이어진다.
numpy 로 직접 합성(`synth_intro_audio.py`)해 외부 음원·라이선스가 없다. 음량은 설정 창 **배경음** 슬라이더를 따른다.

**배경음(타이틀 곡)은 메뉴가 나올 때부터**: 장원재 님 `UFF_AudioEventLink` 가 레벨 BeginPlay 에 축제 단계(기본 Setup)를 읽어
「준비 - 개장 전」 곡을 튼다 — 프론트엔드도 마찬가지라 예전엔 시작 화면부터 노래가 나왔다. 인트로가 떠 있는 동안 공개 API
(`StopAmbience`)로 멈춰 두고 메뉴가 나오는 순간(`Finish`) 같은 곡을 `SetAmbience` 로 다시 튼다(2초 페이드인이라 첫 틱 정지는 들리지 않는다).
장원재 님 코드는 고치지 않았다. 나중에 「타이틀」 배경음 칸이 생기면 그쪽이 맞다.

**넘기기**: 시작 화면은 1초 뒤부터, 영상·지구 공개는 바로 아무 키/클릭. 영상을 넘기면 소리는 0.6초에 걸쳐 줄어든다.

상태: `assumption` 2026-10-02. 초수·문구·구도·소리는 전부 조정 대상이다.

## 코드와 에셋

| 무엇 | 어디 |
| --- | --- |
| 화면 (단계 진행·영상·소리·입력) | `Source/FestaFiesta/{Public,Private}/UI/Title/FF_TitleIntroScreen.*` |
| 흐름 연결 (첫 진입 한 번·끝나면 타이틀·히스토리에서 제거) | `UFF_UIFlowSubsystem` — `ShouldPlayTitleIntro` · `HandleTitleIntroFinished` |
| 가운데 구도·살짝 다가옴·타이틀로 블렌드 | `AFF_AtlasCameraRig` — `IntroCamera` 칸(비우면 계산 구도) · `PlayIntroReveal` |
| 구도·시간 값 | `Project Settings > Game > FestaFiesta Atlas > Title Intro` (`UFF_AtlasSettings`) |
| 시작 화면 시간·지구 보여 주는 시간·영상 경로·소리·음량 | `WBP_FF_TitleIntro` Class Defaults → FestaFiesta / Intro |
| WBP 골격 생성기 | `Source/FestaFiestaEditor/Private/UI/Title/FF_TitleIntroWbpBuilder.cpp` — `FF.TitleIntroUMG`(없을 때만) · `FF.ApplyTitleIntroDesign`(🔴 트리 교체) |
| 화면 배선 | `Config/DefaultGame.ini` `FrontendScreens` 의 `TitleIntro` |
| 영상 | `Content/Movies/FF_Intro_SpaceFireworks.mp4` — `Content/Movies` 는 패키징 때 자동 포함(NonUFS) |
| 로고·소리 에셋 | `/Game/FestaFiesta/UI/Title/Intro/T_FF_Title_Logo_OnDark` · `S_FF_Intro_SpaceFireworks` (UI 폴더라 항상 쿡) |
| 원본 | 로고 `make_logo_on_dark.py` → `SourceArt/UI/Title/Intro_v2/`, 소리 `synth_intro_audio.py` → `SourceArt/UI/Intro/SpaceFireworks_v1/Audio/` |

## 다시 만드는 순서

1. 영상: `blender -b --factory-startup --python Tools/ui/blender/intro_previs/intro_space_fireworks.py -- build check render`
   (이미 같은 버전 .blend 가 있으면 실패한다 — `VERSION` 을 올리거나 이전 파일을 치운다) → MP4 를 `Content/Movies/FF_Intro_SpaceFireworks.mp4` 로 복사.
   같은 실행이 폭죽 시각 표 `events.json` 을 쓴다.
2. 소리: `python Tools/ui/title_intro/synth_intro_audio.py` — `events.json` 의 시각에 맞춰 만든다. 폭죽 시각을 바꿨으면 반드시 다시.
3. UE (에디터를 닫고 새 C++ 빌드 후): `& Tools/ui/title_intro/run.ps1 -Mode Apply [-Rebuild]` — 로고·소리 가져오기 + WBP 생성·검사.
   `-Rebuild` 는 🔴 WBP 트리를 코드 골격으로 바꾼다(디자이너 수정 사라짐).
4. 확인: `& Tools/ui/title_intro/run.ps1 -Mode Capture` → `Saved/TitleIntro/capture_*/`.

## 에디터에서 고치기

- 로고 크기·문구·위치: `WBP_FF_TitleIntro` Designer 의 `Panel_Splash`. 위젯 이름(`Image_Backdrop` · `Image_Video` · `Panel_Splash` · `Image_FlashCover`)은 C++ 가 찾으므로 바꾸지 않는다.
- 지구 크기(화면 높이 비율)·위아래·다가오는 정도·메뉴로 가는 시간: Project Settings → FestaFiesta Atlas → Title Intro. ⚠️ 팀 공용 `DefaultGame.ini` 에 저장된다.
- 영상 마지막 지구 크기는 Blender `SCREEN_FRACTION` 끝 값(0.56)과 UE `IntroGlobeScreenHeightFraction`(0.56)을 같게 둔다.

## 콘솔

- `FF.Intro.Enable 0` — 인트로 없이 바로 타이틀(PIE 반복 작업용). 기본 1.
- `FF.Intro.Skip` · `FF.Intro.Status` — 단계 넘기기·지금 단계(개발용).
- `FF.Flow.Status` 의 `IntroPlayed=`, `FF.Atlas.Camera` 의 `Intro=`.

## 함정

- 🔴 **프론트엔드 첫 화면은 액터 BeginPlay 보다 먼저 올라간다.** `UEngine::LoadMap` 이 `SpawnPlayActor`(PostLogin) 뒤에
  `World->BeginPlay()` 를 부른다. 리그 `BeginPlay` 가 타이틀 구도로 덮으므로 인트로는 지구 공개 때 뷰 타깃을 다시 잡는다.
- Blender FBX 가져오기는 장면 fps 를 바꾼다(v1 에서 24fps 로 태그돼 1.25배 느렸다). 스크립트는 가져온 뒤 30fps 로 되돌린다.
- 영상이 없거나 열리지 않으면(4초) 기다리지 않고 지구 단계로 간다. 로그 `인트로 영상이 없어 건너뛴다`.
- 영상 첫 프레임 전에는 MediaTexture 의 `ClearColor`(검은색)가 보인다. 영상 처음은 검은 우주, 끝은 흰 빛이어야 이음매가 없다.
- 스택 크로스페이드 도중 인트로 위젯을 빼면 화면이 끊긴다. 흐름 서브시스템이 전환이 끝난 뒤 뺀다.
- 소리는 화면이 내려가도 꼬리가 울리게 끄지 않는다(영상을 넘겼을 때만 줄인다).
- `.mp4` 는 아직 Git LFS 대상이 아니다. 커밋 전에 `.gitattributes` 에 `*.mp4 filter=lfs diff=lfs merge=lfs -text` 를 더한다.
- 영상의 실사 지구 텍스처는 Solar System Scope(CC BY 4.0, 등록됨) — 크레딧 화면 표기 대상이다.
- 실사 지구 대기: 껍질 `RIM_SCALE` 1.06 + Facing 램프(지구 가장자리 바로 바깥 최대 → 반지름 약 6% 에 걸쳐 0) × 햇빛 쪽(`SUN_FROM`)만 밝게.
  Facing 거듭제곱만 쓰면 껍질 바깥 끝이 가장 밝아 딱딱한 띠가 되고(2026-10-05 사용자 지적 「두껍고 어색」), 얇게만 하면 어두운 쪽까지 외곽선처럼 보인다.
