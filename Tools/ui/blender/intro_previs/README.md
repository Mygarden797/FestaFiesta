# 인트로 프리비즈

> **2026-10-02 현재 쓰는 것은 `intro_space_fireworks.py`(v2)다.** 별빛 우주의 실사 지구에 세계 도시 곳곳에서 축제 폭죽이 터지고,
> 터진 자리부터 우리 스타일라이즈드 지구(`FF_BrightGlobe_P05_world_v001.blend` 를 가져와 씀)로 물든 뒤 밝은 하늘·흰 빛으로 끝난다(8초 30fps).
> `blender -b --factory-startup --python intro_space_fireworks.py -- build check render` → `SourceArt/UI/Intro/SpaceFireworks_v1/`.
> UE 쪽·소리·다시 만드는 순서는 `Tools/ui/title_intro/README.md`.
> `intro_globe_spin.py`(캐릭터가 지구를 쓱 돌리는 v1, 2026-10-01)와 아래 「집에서 출발」은 이전 방향의 기록이다.

## 이전 방향 — 「집에서 출발」

상태: `confirmed` 2026-09-29 Blender 5.2.1 LTS.
- **v005(현재)** — 여행가의 집: 1층 거실 **7×11m** + 1층 침실 **7×7m**(거실 북쪽 열린 문). 밤 무드 EEVEE.
  동선: 현관문 → 거실을 가로질러 → 열린 침실 문 → **책상 위 지구본** 돌리기 → 침대에서 짐 싸기 → 나감(S04는 소리로만).
  v004 대비: 계단 입구를 막던 소파·스탠드 → 거실을 남쪽으로 2m 넓혀 라운지를 남쪽 창가로. 침실을 크게 넓히고
  바닥 지구본을 북쪽 벽 책상 위 책상형(지름 40cm)으로 바꿔 S03에서 캐릭터(약 3m)보다 훨씬 뒤(약 5.8m)에 둔다.
- v004 — 침실 5.5×5m. v003 — 침실 3.8×4m. v002 — 지구본·짐 싸기가 거실에 있던 안. v001 — 4×5m 회색 침실.
  모두 `.blend`와 렌더만 기록으로 둔다.

참고 무드는 Fab 「DetectiveOffice」의 **느낌만**이다(에셋·이미지 미사용, 리스팅이 NoAI).
치수·배치·조명·카메라는 전부 프리비즈용 `assumption`이다. 사용자 판단과 재개 체크리스트는 로컬 `docs/ui/intro-astra-blender-higgsfield-plan.md` 8~10절.

## 파일

| 파일 | 역할 |
| --- | --- |
| `previs_kit.py` | 공용 도우미(박스·원기둥·구·판·곡선·카메라·조명·머티리얼) |
| `intro_house.py` | 현재 집(`VERSION`) — `build()` → `pilot()` → `save()`, 샷 자세 `pose('S01'|'S01b'|'S02'|'S03')` |
| `render_house.py` | 렌더 — `layout`(Workbench 배치도 3장) · `shots`(EEVEE 밤 무드) · `pilot`(EEVEE 5초) |
| `intro_previs_set.py`, `render_previs.py` | v001 기록 |

버전을 올릴 때는 `intro_house.py`의 `VERSION`을 바꾸고 `OLD_SCENES`·`OLD_TOPS`에 이전 이름을 더한다. 새 `.blend`와 `Previs/<VERSION>/`로 저장되고 이전 파일은 그대로 남는다.

## 산출물 (로컬, `/SourceArt/`는 gitignore)

- `SourceArt/UI/Intro/HouseDeparture_v1/Blender/FF_Intro_HouseDeparture_v005.blend`, 씬 `FF_Intro_HouseDeparture_v005`
- `Previs/v005/Layout/` 위(동선·카메라)·거실 조감·침실 조감, `Previs/v005/Shots/` 거실·침실 기준 이미지(캐릭터 없음)와 S01·S01b·S02·S03, `Previs/v005/Pilot/` 5초 MP4와 시작·0.5초·끝 PNG

## 실행

1. 켜진 Blender에 MCP로 모듈을 불러 `build()` → `pilot()` → `save()`. 이미 있으면 새로 만들지 않는다(`build(force=True)`).
   이전 버전 씬이 세션에 열려 있으면 새 파일에 섞지 않도록 세션에서만 지운다. 디스크의 이전 파일은 그대로다.
2. 렌더는 별도 프로세스로 돌린다. 배치도·샷 약 20초, 파일럿 MP4 약 4분.

   ```bash
   "/c/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b SourceArt/UI/Intro/HouseDeparture_v1/Blender/FF_Intro_HouseDeparture_v005.blend --python Tools/ui/blender/intro_previs/render_house.py -- layout shots pilot
   ```

## v005 배치 요약

| 구역 | 내용 |
| --- | --- |
| 현관(거실 서쪽 벽) | 남쪽 경첩 문, 옷걸이(모자·목도리), 우산꽂이, 콘솔·열쇠 그릇, 축제 포스터 |
| 거실 서재 쪽(북쪽) | 코르크 **여행 보드**(세계지도 + 8개국 도시 핀·붉은 실·사진·티켓), 녹색 가죽 책상·은행원 스탠드, 책장(기념품 = 나라별 조형물 복선), 세계 시계 셋, 여행 가방 더미·트렁크 |
| 거실 가운데 | 긴 러그 — 현관에서 침실 문으로 가는 동선이 그 위를 지난다 |
| 거실 라운지(남쪽 창가) | 둥근 러그, 체스터필드 소파(동쪽 벽), 탁자, 안락의자(탁자 서쪽, 소파와 마주 봄), 스탠드. **계단 첫 단 앞 2m 이상 비움** |
| 계단(거실 동쪽 벽) | 18단, 계단 구멍으로 2층 복도가 보임(2층은 암시만) |
| 침실 문 | 거실 북쪽 벽, 여행 보드와 계단 사이 x 1.15~2.05. 서쪽 경첩, 침실 쪽으로 80° 열린 채 고정 |
| 침실 7×7m | 퀸 침대(머리 서쪽 벽, 남색 이불) 위 **배낭**, 양쪽 협탁·스탠드, 발치 벤치, 침대 위 세계지도 액자, 북쪽 블라인드 창(S03 캐릭터 뒤), **북쪽 벽 책상 + 책상 지구본**(지름 40cm, 은행원 스탠드·위 벽등), 책장, 북동 독서 구석(녹색 안락의자·스탠드), 동쪽 옷장·포스터, 문 옆 서랍장 |
| 조명(밤) | 거실: 달빛(남쪽 창), 녹색 펜던트 둘, 책상·거실·서랍장 스탠드, 현관 바깥등, 채움광. 침실: 머리맡 스탠드, 책상 스탠드, 지구본 벽등(스팟), 독서 스탠드, 창 달빛, 채움광. 두 방 옅은 안개 |

## 샷

| 샷 | 카메라 | 대역 |
| --- | --- | --- |
| S01 | 거실 남동쪽 18mm — 현관문(왼쪽)부터 침실 문(오른쪽 안)까지 거실 전체 | 현관문 안쪽(문 -95° 열림) |
| S01b | 같은 카메라 | 거실을 가로질러 침실 문 앞 |
| S02 | 책상 동쪽 30mm — 손과 책상 지구본, 뒤에 창 | 책상 앞에 서서 오른손으로 지구본 앞면을 서→동으로 민다 |
| S03 | 침실 남서쪽 35mm f/4 — 침대 너머로 캐릭터 정면(약 3m), 뒤쪽 책상 지구본(약 5.8m) | 침대 북쪽에서 남쪽(카메라)을 보며 배낭에 짐을 넣음 |
| P0 | 침대 위를 지나 책상 지구본으로 4.5m → 1.0m 직선 접근 | 없음(나간 뒤) |

## 조절 지점

| 무엇 | 어디 |
| --- | --- |
| 방·문·창·계단·보드·침대·책상·지구본·배낭 위치 | `intro_house.py` 상단 상수(`HALL`·`COFFEE`·`BED_DOOR`·`BEDROOM`·`BED`·`DESK`·`GLOBE`·`BACKPACK` 등) |
| 샷별 대역 위치·방향·팔 | `POSES` (yaw 0=-Y, 90=+X, 180=+Y / 팔은 어깨·팔꿈치 X 회전, 음수=앞으로) |
| 샷 카메라 | `S01_CAM`·`S02_CAM`·`S03_CAM`, 조준점·렌즈·조리개는 `_cameras()` |
| 파일럿 경로·속도 | `_pilot_path()`, `GLOBE_SPIN_DEG_PER_SEC` (60°/s 서→동) |
| 색·재질 | `PALETTE` (색, 거칠기, 금속, 발광) |
| 조명 세기 | `_lights()` |

## 함정

- **EEVEE 그림자 풀 기본 512MB로는 "섀도우 버퍼 전체"가 나며 가까운 샷이 거의 검게 나온다.** `shadow_pool_size='2048'`, `shadow_resolution_scale=0.5`, 채움광·창 조명 그림자 끔으로 해결. 렌더 로그에서 `섀도우 버퍼`를 먼저 확인한다.
- **계단 첫 단 앞은 비워 둔다.** 소파·스탠드·의자를 그 앞에 두면 계단 입구를 막은 것처럼 보인다(사용자 지적, v005에서 라운지를 남쪽으로 옮김).
- 두 방 사이 벽이 거실 조감에서 침실을 가린다 — 침실은 `CAM_Layout_Bedroom`(거실 위 높은 곳)에서 따로 본다.
- 방 안개(`FX_Volume`)는 Workbench에서 불투명 상자로 보인다. 배치도 렌더와 뷰포트에서는 숨긴다.
- 스탠드 갓 발광이 세면 AgX에서 하얗게 날아간다(현재 0.7).
- 소품을 옮기면 S01·S03 렌더로 가림을 다시 확인한다(스탠드가 지구본을 가린 적이 있다).
- `bmesh.ops.create_uvsphere(calc_uvs=True)`는 UV 레이어를 먼저 만들어야 UV를 쓴다.
- Blender 5.x 액션은 레이어 방식이라 `action.fcurves`가 없다. `anim_utils.action_get_channelbag_for_slot()`.
- 타임라인 마커에 카메라를 묶지 않는다(다른 카메라 렌더가 바뀐다).
- MCP 뷰포트 스크린샷은 카메라 시점을 반영하지 않을 때가 있다. 구도 판단은 렌더 결과로 한다.
