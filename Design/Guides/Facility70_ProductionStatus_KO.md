# FestaFiesta 시설 70종 — 제작 현황

생성 시각: 2026-09-20T08:28:25+09:00

이 문서는 현재 디스크의 제작 산출물과 Unreal 임포트·검증 보고서를 읽어 만든 상태 기록이다. 원본 제작 브리핑을 대체하지 않는다. 현재 아트 단계는 **1차 아트 패스**이며, 최종 외형 승인이나 전체 플레이 완료를 뜻하지 않는다.

대상은 브리핑의 70종이다. `.blend` 70개, FBX 70개, 사선 미리보기 70개를 확인했다.

（10/14 기준: 이 문서 생성 뒤 같은 날 커밋 `fe1c3ea3`(9/20)에서 #40 `Performance_MainStage_WaveConcertStage`의 시설 정의가 삭제되어, 게임에 등록된 신규 시설 정의는 **69종**이다. 메인 무대는 기존 `Performance_MainStage_SunsetMainStage` 하나다. #40의 Static Mesh·카탈로그 텍스처는 `Content/Asset/FacilitiesAsset_HKH/FacilityExpansion70/Performance/WaveConcertStage`에 남아 있다. 시설 정의 폴더는 9/22 `Content/FestaFiesta/Zone/Data/FacilityDefinitions`에서 `Content/FestaFiesta/Building/FF_Data_Building`으로 옮겨졌고, 현재 전체 시설 정의는 기존 45종 + 신규 69종 = 114개다.）

| 확인 항목 | 현재 수 | 근거 범위 |
|---|---:|---|
| 시설 Definition `.uasset` | 70/70 | 예상 시설별 경로의 실제 파일 존재 |
| Static Mesh `.uasset` | 70/70 | 예상 시설별 경로의 실제 파일 존재 |
| 카탈로그 Texture `.uasset` | 70/70 | 예상 시설별 경로의 실제 파일 존재 |
| 조명 Rig `.uasset` | 6 | 실제 파일 존재; 모든 시설이 리그를 요구하지는 않음 |
| 최신 대상별 임포트 PASS | 70/70 | 저장 성공한 임포트 보고서 |
| 네이티브 `IsDefinitionValid` PASS | 70/70 | 검증 보고서의 시설별 `native_validation=PASS` |

파일 수는 Unreal 클래스 판정이나 로드 성공을 대신하지 않는다. 네이티브 PASS는 저장된 ID·Spec·메시·UV·재질·썸네일·이용 데이터·가격 키·조명 설정 등의 읽기 검증 범위다. 라이브 NavMesh·실제 NPC 접근·플레이어 충돌·시설 배치/회전/이동/철거·저장/불러오기를 포함한 전체 게임 플레이는 이 보고서로 통과를 주장하지 않는다.

## 원본과 확인 화면

- Blender 편집 원본: [C:\Users\gksro\OneDrive\Desktop\Blender](<C:/Users/gksro/OneDrive/Desktop/Blender>). 파일명은 `번호_시설ID.blend`이다.
- [70종 검색 가능한 로컬 갤러리](<C:/Users/gksro/OneDrive/Desktop/Git/TeamProject/8th-Team13-Final-Project/Design/Guides/Facility70_Gallery.html>)
- 제작 소스: [SourceArt/FacilityExpansion70](<C:/Users/gksro/OneDrive/Desktop/Git/TeamProject/8th-Team13-Final-Project/SourceArt/FacilityExpansion70>) — `.gitignore` 대상이므로 별도 원본 보관이 필요하다.（10/14 기준: 9/22 커밋 `69a4bd73`부터 FBX 70개·미리보기 PNG·`production_record.json`·`build_manifest.json`이 저장소의 `Design/SourceArt/FacilityExpansion70`에 올라가 있다. 시설별 `.blend`는 포함되지 않았다. `expansion70_paths.py`의 기본 `.blend` 폴더는 `Design/SourceArt/FacilityExpansion70/Blends`이고, 일부 스크립트는 아직 루트 `SourceArt/FacilityExpansion70`을 읽는다.）
- 재현 스크립트·카탈로그: [Tools/facilities](<C:/Users/gksro/OneDrive/Desktop/Git/TeamProject/8th-Team13-Final-Project/Tools/facilities>) — 저장소 관리 대상인 코드와 기록이다.

미리보기는 Blender 렌더이며 Unreal 렌더와 구분한다. 원본의 부품별 VIS/COL/GUIDES와 저장된 제작 기록을 함께 확인한다. 아래 이미지에는 번호별 시설 10개씩을 배치했다.

![page_01 시설 미리보기](C:/Users/gksro/OneDrive/Desktop/Git/TeamProject/8th-Team13-Final-Project/Design/Guides/Facility70_Preview/page_01.png)

![page_02 시설 미리보기](C:/Users/gksro/OneDrive/Desktop/Git/TeamProject/8th-Team13-Final-Project/Design/Guides/Facility70_Preview/page_02.png)

![page_03 시설 미리보기](C:/Users/gksro/OneDrive/Desktop/Git/TeamProject/8th-Team13-Final-Project/Design/Guides/Facility70_Preview/page_03.png)

![page_04 시설 미리보기](C:/Users/gksro/OneDrive/Desktop/Git/TeamProject/8th-Team13-Final-Project/Design/Guides/Facility70_Preview/page_04.png)

![page_05 시설 미리보기](C:/Users/gksro/OneDrive/Desktop/Git/TeamProject/8th-Team13-Final-Project/Design/Guides/Facility70_Preview/page_05.png)

![page_06 시설 미리보기](C:/Users/gksro/OneDrive/Desktop/Git/TeamProject/8th-Team13-Final-Project/Design/Guides/Facility70_Preview/page_06.png)

![page_07 시설 미리보기](C:/Users/gksro/OneDrive/Desktop/Git/TeamProject/8th-Team13-Final-Project/Design/Guides/Facility70_Preview/page_07.png)

Blender 일반 편집 파일 저장·재개방 검사: **70/70 통과**. [검증 기록](<C:/Users/gksro/OneDrive/Desktop/Git/TeamProject/8th-Team13-Final-Project/Saved/FacilityExpansion70/blend_finalization_report.json>)

## 치수와 데이터 정책

카탈로그 치수는 초기 계획 상한이다. 일반 시설은 종횡비를 유지해 그 범위에 맞추고 Road/Tile은 400×400×8cm로 맞춘다. 최종 PDA의 XY는 실제 메시 크기를 1cm 셀로 올림하고 높이는 실제 메시 높이를 사용한다. 계획 값·제작된 크기·최종 PDA 값은 아래에 별도로 표시한다. Data Asset 생성 이후 런타임 권위는 해당 `Spec`이다.

설치 비용과 관련 외부 키는 기존 동일 소분류 템플릿/경제 테이블 정책을 사용한다. 신규 70종 전용 가격·수익 균형 조정이 완료되었다는 뜻은 아니다.（10/14 기준: 1인 이용 매출은 9/20 추가된 시설별 표 `DT_FacilityRevenues`(`data/unreal/facility_revenues_v0.csv`, 114행)에 신규 69종 모두 행이 있으며 10/9 경제 2차 밸런싱 값이 들어 있다. 설치 비용 표 `facility_costs_v0.csv`에는 신규 시설 ID 행이 없다.） 유료 공연 프로그램 등록, 영상, VFX, 게임 규칙·점수, 음향, 가동 부품·캐릭터 애니메이션은 별도 구현 범위다. 치료·보안·화재·대피·판매 효과도 형상 존재만으로 구현되었다고 보지 않는다.

### 전면 방향과 좌표 변환

적용 정책은 일반 시설 #1–64의 FBX 임포트 Yaw 180° 보정을 정적 메시 자체에 굽고, PDA `VisualRotation`은 `(0,0,0)`으로 유지하는 것이다. 일반 시설의 Blender 원본 `(x,y,z)` m는 Unreal 메시 `(-100x,+100y,+100z)` cm에 대응하며 고객 전면 +Y를 유지한다. 조명 위치도 같은 변환으로 맞춘다.

Road/Tile #65–70은 임포트 Yaw 0°를 사용해 진행 +X를 유지한다. 이 경우 Blender `(x,y,z)` m는 Unreal `(+100x,-100y,+100z)` cm에 대응한다. FBX 좌표계 변환과 제작 전면 규칙은 별개이며, 원점·크기 검사만으로 전후 방향이나 글자 좌우 반전까지 확인했다고 보지 않는다.

현재 에셋과 일치하는 방향 정책 읽기 검증: **70/70종**. 전체 시설의 임포트 Yaw·메타데이터·PDA 회전 검사를 통과했다.

## 시설별 기록

`tris`는 출력 본체 삼각형 수, `parts`는 편집 가능한 시각 부품 수, `hulls`는 제작 충돌체 수다. 빈 값은 확인 자료가 아직 없다는 뜻이다.

| 번호 | 시설 / 분류 | 계획 상한 cm | Blender 실제 cm | PDA cm | parts / tris / hulls | 상태 / 원본 |
|---:|---|---|---|---|---:|---|
| 1 | 카페 트럭<br>Booth/FoodStall | 600 × 320 × 380 | 535.3 × 288.6 × 380 | 536 × 289 × 380 | 85 / 22200 / 9 | Unreal 네이티브 검증 PASS<br>[blend](<C:/Users/gksro/OneDrive/Desktop/Blender/01_Booth_FoodStall_CafeTruck.blend>) |
| 2 | 과일 에이드 바<br>Booth/FoodStall | 600 × 320 × 390 | 535.5 × 288.7 × 390 | 536 × 289 × 390 | 78 / 21428 / 9 | Unreal 네이티브 검증 PASS<br>[blend](<C:/Users/gksro/OneDrive/Desktop/Blender/02_Booth_FoodStall_FruitAdeBar.blend>) |
| 3 | 부산 어묵 포장마차<br>Booth/FoodStall | 420 × 270 × 330 | 420 × 257.4 × 295.6 | 420 × 258 × 295.6 | 59 / 13292 / 7 | Unreal 네이티브 검증 PASS<br>[blend](<C:/Users/gksro/OneDrive/Desktop/Blender/03_Booth_FoodStall_BusanFishCakeCart.blend>) |
| 4 | 씨앗호떡 부스<br>Booth/FoodStall | 350 × 260 × 330 | 350 × 220.9 × 301.1 | 350 × 221 × 301.1 | 54 / 10648 / 7 | Unreal 네이티브 검증 PASS<br>[blend](<C:/Users/gksro/OneDrive/Desktop/Blender/04_Booth_FoodStall_SeedHotteokStall.blend>) |
| 5 | 숯불 닭꼬치 부스<br>Booth/FoodStall | 430 × 280 × 340 | 430 × 263.6 × 302.6 | 430 × 264 × 302.6 | 93 / 12656 / 7 | Unreal 네이티브 검증 PASS<br>[blend](<C:/Users/gksro/OneDrive/Desktop/Blender/05_Booth_FoodStall_CharcoalChickenSkewerStall.blend>) |
| 6 | 비건 샌드위치 트럭<br>Booth/FoodStall | 600 × 320 × 410 | 591.6 × 318.9 × 410 | 592 × 319 × 410 | 63 / 13588 / 9 | Unreal 네이티브 검증 PASS<br>[blend](<C:/Users/gksro/OneDrive/Desktop/Blender/06_Booth_FoodStall_VeganSandwichTruck.blend>) |
| 7 | 구름 솜사탕 부스<br>Booth/FoodStall | 320 × 260 × 350 | 320 × 196.1 × 281 | 320 × 197 × 281 | 44 / 12108 / 7 | Unreal 네이티브 검증 PASS<br>[blend](<C:/Users/gksro/OneDrive/Desktop/Blender/07_Booth_FoodStall_CloudCottonCandyStall.blend>) |
| 8 | 로컬 기념품 편집숍<br>Booth/MarketGoods | 620 × 320 × 380 | 583.9 × 314.7 × 380 | 584 × 315 × 380 | 72 / 17085 / 9 | Unreal 네이티브 검증 PASS<br>[blend](<C:/Users/gksro/OneDrive/Desktop/Blender/08_Booth_MarketGoods_LocalSouvenirShop.blend>) |
| 9 | 페스티벌 의류숍<br>Booth/MarketGoods | 450 × 350 × 390 | 450 × 321 × 342.4 | 450 × 322 × 342.4 | 49 / 12910 / 7 | Unreal 네이티브 검증 PASS<br>[blend](<C:/Users/gksro/OneDrive/Desktop/Blender/09_Booth_MarketGoods_FestivalClothingShop.blend>) |
| 10 | 야광 응원용품점<br>Booth/MarketGoods | 620 × 320 × 410 | 593.6 × 320 × 407.3 | 594 × 320 × 407.3 | 68 / 19152 / 9 | Unreal 네이티브 검증 PASS<br>[blend](<C:/Users/gksro/OneDrive/Desktop/Blender/10_Booth_MarketGoods_GlowCheeringGoodsShop.blend>) |
| 11 | 조개 팔찌 공방<br>Booth/CraftExperience | 550 × 450 × 400 | 497.3 × 450 × 363.6 | 498 × 450 × 363.6 | 114 / 16780 / 8 | Unreal 네이티브 검증 PASS<br>[blend](<C:/Users/gksro/OneDrive/Desktop/Blender/11_Booth_CraftExperience_ShellBraceletWorkshop.blend>) |
| 12 | 파도 레진 키링 공방<br>Booth/CraftExperience | 550 × 420 × 380 | 485.2 × 420 × 363.2 | 486 × 420 × 363.2 | 86 / 11864 / 8 | Unreal 네이티브 검증 PASS<br>[blend](<C:/Users/gksro/OneDrive/Desktop/Blender/12_Booth_CraftExperience_WaveResinKeyringWorkshop.blend>) |
| 13 | 나만의 부채 공방<br>Booth/CraftExperience | 550 × 420 × 410 | 490 × 420 × 373.5 | 491 × 420 × 373.5 | 110 / 12912 / 8 | Unreal 네이티브 검증 PASS<br>[blend](<C:/Users/gksro/OneDrive/Desktop/Blender/13_Booth_CraftExperience_PersonalFanWorkshop.blend>) |
| 14 | 풍선 다트 부스<br>Booth/ActivityGame | 450 × 400 × 380 | 448.8 × 383.7 × 380 | 449 × 384 × 380 | 68 / 20296 / 7 | Unreal 네이티브 검증 PASS<br>[blend](<C:/Users/gksro/OneDrive/Desktop/Blender/14_Booth_ActivityGame_BalloonDartStall.blend>) |
| 15 | 미니 양궁장<br>Booth/ActivityGame | 750 × 1000 × 380 | 721.8 × 940.1 × 380 | 722 × 941 × 380 | 89 / 10952 / 11 | Unreal 네이티브 검증 PASS<br>[blend](<C:/Users/gksro/OneDrive/Desktop/Blender/15_Booth_ActivityGame_MiniArcheryRange.blend>) |
| 16 | 농구 슛 챌린지<br>Booth/ActivityGame | 500 × 650 × 450 | 465 × 590 × 450 | 465 × 590 × 450 | 67 / 19196 / 6 | Unreal 네이티브 검증 PASS<br>[blend](<C:/Users/gksro/OneDrive/Desktop/Blender/16_Booth_ActivityGame_BasketballShotChallenge.blend>) |
| 17 | 리듬 게임 부스<br>Booth/ActivityGame | 550 × 450 × 380 | 541.1 × 450 × 373.5 | 542 × 450 × 373.5 | 47 / 14420 / 8 | Unreal 네이티브 검증 PASS<br>[blend](<C:/Users/gksro/OneDrive/Desktop/Blender/17_Booth_ActivityGame_RhythmGameStall.blend>) |
| 18 | 보드게임 텐트<br>Booth/ActivityGame | 600 × 550 × 460 | 585.1 × 550 × 436.8 | 586 × 550 × 436.8 | 135 / 21612 / 8 | Unreal 네이티브 검증 PASS<br>[blend](<C:/Users/gksro/OneDrive/Desktop/Blender/18_Booth_ActivityGame_BoardGameTent.blend>) |
| 19 | 서핑 밸런스 체험장<br>Booth/ActivityGame | 550 × 500 × 240 | 549 × 488.5 × 240 | 550 × 489 × 240 | 22 / 13840 / 7 | Unreal 네이티브 검증 PASS<br>[blend](<C:/Users/gksro/OneDrive/Desktop/Blender/19_Booth_ActivityGame_SurfBalanceExperience.blend>) |
| 20 | 선셋 모크테일 바<br>Booth/Nightlife | 550 × 420 × 380 | 523.6 × 420 × 366.8 | 524 × 420 × 366.8 | 93 / 17624 / 19 | Unreal 네이티브 검증 PASS<br>[blend](<C:/Users/gksro/OneDrive/Desktop/Blender/20_Booth_Nightlife_SunsetMocktailBar.blend>) |
| 21 | 수제맥주 탭룸<br>Booth/Nightlife | 550 × 430 × 380 | 539.4 × 430 × 374.5 | 540 × 430 × 374.5 | 65 / 13856 / 9 | Unreal 네이티브 검증 PASS<br>[blend](<C:/Users/gksro/OneDrive/Desktop/Blender/21_Booth_Nightlife_CraftBeerTaproom.blend>) |
| 22 | 사일런트 디스코 클럽<br>Booth/Nightlife | 900 × 750 × 500 | 900 × 746.5 × 463.8 | 900 × 747 × 463.8 | 102 / 32024 / 13 | Unreal 네이티브 검증 PASS<br>[blend](<C:/Users/gksro/OneDrive/Desktop/Blender/22_Booth_Nightlife_SilentDiscoClub.blend>) |
| 23 | 해변 목조 화장실<br>Toilet/Standard | 500 × 460 × 470 | 485.8 × 415.6 × 470 | 486 × 416 × 470 | 78 / 12202 / 9 | Unreal 네이티브 검증 PASS<br>[blend](<C:/Users/gksro/OneDrive/Desktop/Blender/23_Toilet_Standard_BeachWoodRestroom.blend>) |
| 24 | 넓은 회전 공간 화장실<br>Toilet/Accessible | 400 × 400 × 310 | 389.4 × 389.4 × 310 | 390 × 390 × 310 | 25 / 4192 / 6 | Unreal 네이티브 검증 PASS<br>[blend](<C:/Users/gksro/OneDrive/Desktop/Blender/24_Toilet_Accessible_WideAccessRestroom.blend>) |
| 25 | 유아 동반 화장실<br>Toilet/Family | 500 × 400 × 340 | 481.6 × 368.9 × 340 | 482 × 369 × 340 | 41 / 12868 / 6 | Unreal 네이티브 검증 PASS<br>[blend](<C:/Users/gksro/OneDrive/Desktop/Blender/25_Toilet_Family_InfantFamilyRestroom.blend>) |
| 26 | 이동식 화장실 캐빈<br>Toilet/Temporary | 160 × 170 × 270 | 147.2 × 162.3 × 270 | 148 × 163 × 270 | 26 / 7408 / 5 | Unreal 네이티브 검증 PASS<br>[blend](<C:/Users/gksro/OneDrive/Desktop/Blender/26_Toilet_Temporary_PortableRestroomCabin.blend>) |
| 27 | 등대형 종합안내소<br>Info/GeneralInfo | 500 × 420 × 520 | 407.1 × 420 × 471.9 | 408 × 420 × 471.9 | 46 / 9036 / 6 | Unreal 네이티브 검증 PASS<br>[blend](<C:/Users/gksro/OneDrive/Desktop/Blender/27_Info_GeneralInfo_LighthouseInformation.blend>) |
| 28 | 양면 지도 키오스크<br>Info/GeneralInfo | 450 × 220 × 340 | 450 × 203.2 × 337.1 | 450 × 204 × 337.1 | 74 / 14912 / 6 | Unreal 네이티브 검증 PASS<br>[blend](<C:/Users/gksro/OneDrive/Desktop/Blender/28_Info_GeneralInfo_DoubleSidedMapKiosk.blend>) |
| 29 | 컨테이너 통합 매표소<br>Info/Ticketing | 700 × 350 × 420 | 658.7 × 350 × 407.9 | 659 × 350 × 407.9 | 58 / 10760 / 9 | Unreal 네이티브 검증 PASS<br>[blend](<C:/Users/gksro/OneDrive/Desktop/Blender/29_Info_Ticketing_ContainerTicketOffice.blend>) |
| 30 | 미아·분실물 통합센터<br>Info/LostFound | 600 × 400 × 390 | 531.1 × 343 × 390 | 532 × 343 × 390 | 82 / 23936 / 12 | Unreal 네이티브 검증 PASS<br>[blend](<C:/Users/gksro/OneDrive/Desktop/Blender/30_Info_LostFound_LostChildAndFoundCenter.blend>) |
| 31 | 다국어 안내 라운지<br>Info/Translation | 600 × 450 × 440 | 570.5 × 450 × 416.6 | 571 × 450 × 416.6 | 66 / 25700 / 16 | Unreal 네이티브 검증 PASS<br>[blend](<C:/Users/gksro/OneDrive/Desktop/Blender/31_Info_Translation_MultilingualInformationLounge.blend>) |
| 32 | 해먹 라운지<br>Rest/Seating | 450 × 300 × 330 | 432 × 265 × 330 | 432 × 265 × 330 | 34 / 6192 / 6 | Unreal 네이티브 검증 PASS<br>[blend](<C:/Users/gksro/OneDrive/Desktop/Blender/32_Rest_Seating_HammockLounge.blend>) |
| 33 | 해안 전망 벤치<br>Rest/Seating | 300 × 130 × 125 | 300 × 63.8 × 115.4 | 300 × 64 × 115.4 | 12 / 1296 / 2 | Unreal 네이티브 검증 PASS<br>[blend](<C:/Users/gksro/OneDrive/Desktop/Blender/33_Rest_Seating_CoastalViewBench.blend>) |
| 34 | 돛천 그늘 쉼터<br>Rest/Shade | 600 × 550 × 480 | 411.5 × 378.1 × 480 | 412 × 379 × 480 | 18 / 1216 / 7 | Unreal 네이티브 검증 PASS<br>[blend](<C:/Users/gksro/OneDrive/Desktop/Blender/34_Rest_Shade_SailShadeShelter.blend>) |
| 35 | 덩굴 퍼걸러<br>Rest/Shade | 550 × 450 × 360 | 522.8 × 424.7 × 360 | 523 × 425 × 360 | 156 / 48654 / 13 | Unreal 네이티브 검증 PASS<br>[blend](<C:/Users/gksro/OneDrive/Desktop/Blender/35_Rest_Shade_VinePergola.blend>) |
| 36 | 가족 돗자리 피크닉존<br>Rest/Picnic | 500 × 450 × 340 | 480 × 419.5 × 340 | 480 × 420 × 340 | 71 / 8720 / 5 | Unreal 네이티브 검증 PASS<br>[blend](<C:/Users/gksro/OneDrive/Desktop/Blender/36_Rest_Picnic_FamilyMatPicnicZone.blend>) |
| 37 | 단체 피크닉 테이블<br>Rest/Picnic | 420 × 280 × 105 | 420 × 277.1 × 90.1 | 420 × 278 × 90.1 | 15 / 1364 / 6 | Unreal 네이티브 검증 PASS<br>[blend](<C:/Users/gksro/OneDrive/Desktop/Blender/37_Rest_Picnic_GroupPicnicTable.blend>) |
| 38 | 음수 라운지<br>Rest/Cooling | 550 × 400 × 330 | 528 × 372.9 × 330 | 528 × 373 × 330 | 44 / 12812 / 11 | Unreal 네이티브 검증 PASS<br>[blend](<C:/Users/gksro/OneDrive/Desktop/Blender/38_Rest_Cooling_DrinkingWaterLounge.blend>) |
| 39 | 해변 족욕 쉼터<br>Rest/Cooling | 550 × 500 × 420 | 539 × 490 × 420 | 539 × 490 × 420 | 35 / 2398 / 13 | Unreal 네이티브 검증 PASS<br>[blend](<C:/Users/gksro/OneDrive/Desktop/Blender/39_Rest_Cooling_BeachFootbathShelter.blend>) |
| 40 | 파도형 콘서트 무대<br>Performance/MainStage | 1400 × 800 × 720 | 1400 × 800 × 702 | 1400 × 800 × 702 | 120 / 13250 / 12 | Unreal 네이티브 검증 PASS（10/14 기준: 시설 정의 삭제됨）<br>[blend](<C:/Users/gksro/OneDrive/Desktop/Blender/40_Performance_MainStage_WaveConcertStage.blend>) |
| 41 | 가족 인형극장<br>Performance/SubStage | 650 × 600 × 450 | 633.2 × 576.2 × 450 | 634 × 577 × 450 | 67 / 14240 / 16 | Unreal 네이티브 검증 PASS<br>[blend](<C:/Users/gksro/OneDrive/Desktop/Blender/41_Performance_SubStage_FamilyPuppetTheater.blend>) |
| 42 | 야외 영화 상영장<br>Performance/SubStage | 1100 × 1100 × 650 | 1100 × 1017 × 622.3 | 1100 × 1018 × 622.3 | 58 / 7384 / 10 | Unreal 네이티브 검증 PASS<br>[blend](<C:/Users/gksro/OneDrive/Desktop/Blender/42_Performance_SubStage_OutdoorCinema.blend>) |
| 43 | 로컬 밴드 쇼케이스<br>Performance/SubStage | 900 × 650 × 500 | 846.8 × 596.6 × 500 | 847 × 597 × 500 | 106 / 24304 / 12 | Unreal 네이티브 검증 PASS<br>[blend](<C:/Users/gksro/OneDrive/Desktop/Blender/43_Performance_SubStage_LocalBandShowcase.blend>) |
| 44 | 어쿠스틱 버스킹 데크<br>Performance/Busking | 450 × 400 × 350 | 432.8 × 400 × 343.1 | 433 × 400 × 343.1 | 31 / 2788 / 3 | Unreal 네이티브 검증 PASS<br>[blend](<C:/Users/gksro/OneDrive/Desktop/Blender/44_Performance_Busking_AcousticBuskingDeck.blend>) |
| 45 | 거리 마술 스테이지<br>Performance/Busking | 600 × 500 × 410 | 566.8 × 478.4 × 410 | 567 × 479 × 410 | 41 / 5808 / 8 | Unreal 네이티브 검증 PASS<br>[blend](<C:/Users/gksro/OneDrive/Desktop/Blender/45_Performance_Busking_StreetMagicStage.blend>) |
| 46 | 관객 댄스 챌린지 무대<br>Performance/ExperienceStage | 950 × 800 × 520 | 893.9 × 716.7 × 520 | 894 × 717 × 520 | 111 / 28364 / 13 | Unreal 네이티브 검증 PASS<br>[blend](<C:/Users/gksro/OneDrive/Desktop/Blender/46_Performance_ExperienceStage_AudienceDanceChallengeStage.blend>) |
| 47 | 소형 응급처치 카트<br>Safety/FirstAid | 300 × 230 × 300 | 296.9 × 218.1 × 300 | 297 × 219 × 300 | 23 / 5296 / 1 | Unreal 네이티브 검증 PASS<br>[blend](<C:/Users/gksro/OneDrive/Desktop/Blender/47_Safety_FirstAid_CompactFirstAidCart.blend>) |
| 48 | 순찰요원 초소<br>Safety/Security | 340 × 300 × 350 | 298.7 × 300 × 323.8 | 299 × 300 × 323.8 | 22 / 8748 / 7 | Unreal 네이티브 검증 PASS<br>[blend](<C:/Users/gksro/OneDrive/Desktop/Blender/48_Safety_Security_PatrolOfficerPost.blend>) |
| 49 | 소방 장비 스테이션<br>Safety/FireSafety | 400 × 180 × 300 | 371.4 × 119.4 × 300 | 372 × 120 × 300 | 18 / 10352 / 5 | Unreal 네이티브 검증 PASS<br>[blend](<C:/Users/gksro/OneDrive/Desktop/Blender/49_Safety_FireSafety_FireEquipmentStation.blend>) |
| 50 | 야광 표지 대피 집결지<br>Safety/Evacuation | 700 × 600 × 380 | 700 × 600 × 370.5 | 700 × 600 × 370.5 | 15 / 7600 / 3 | Unreal 네이티브 검증 PASS<br>[blend](<C:/Users/gksro/OneDrive/Desktop/Blender/50_Safety_Evacuation_LuminousEvacuationAssembly.blend>) |
| 51 | 대형 모래성<br>PhotoSpot/Landmark | 750 × 600 × 650 | 750 × 563.3 × 552.6 | 750 × 564 × 552.6 | 52 / 5517 / 10 | Unreal 네이티브 검증 PASS<br>[blend](<C:/Users/gksro/OneDrive/Desktop/Blender/51_PhotoSpot_Landmark_GrandSandcastle.blend>) |
| 52 | 고래 꼬리 조형물<br>PhotoSpot/Landmark | 750 × 600 × 630 | 690 × 342 × 630 | 690 × 342 × 630 | 7 / 522 / 1 | Unreal 네이티브 검증 PASS<br>[blend](<C:/Users/gksro/OneDrive/Desktop/Blender/52_PhotoSpot_Landmark_WhaleTailSculpture.blend>) |
| 53 | FESTAFIESTA 레터링<br>PhotoSpot/Landmark | 900 × 300 × 430 | 899.8 × 244.8 × 430 | 900 × 245 × 430 | 30 / 12260 / 7 | Unreal 네이티브 검증 PASS<br>[blend](<C:/Users/gksro/OneDrive/Desktop/Blender/53_PhotoSpot_Landmark_FestaFiestaLettering.blend>) |
| 54 | 해적선 촬영 세트<br>PhotoSpot/Theme | 850 × 450 × 620 | 850 × 391.8 × 613.6 | 850 × 392 × 613.6 | 39 / 5920 / 9 | Unreal 네이티브 검증 PASS<br>[blend](<C:/Users/gksro/OneDrive/Desktop/Blender/54_PhotoSpot_Theme_PirateShipPhotoSet.blend>) |
| 55 | 서핑보드 컬러월<br>PhotoSpot/Theme | 650 × 220 × 350 | 603 × 68.4 × 350 | 603 × 69 × 350 | 31 / 10236 / 7 | Unreal 네이티브 검증 PASS<br>[blend](<C:/Users/gksro/OneDrive/Desktop/Blender/55_PhotoSpot_Theme_SurfboardColorWall.blend>) |
| 56 | 일몰 액자 전망대<br>PhotoSpot/Scenic | 650 × 500 × 440 | 593.2 × 500 × 408.7 | 594 × 500 × 408.7 | 28 / 2976 / 11 | Unreal 네이티브 검증 PASS<br>[blend](<C:/Users/gksro/OneDrive/Desktop/Blender/56_PhotoSpot_Scenic_SunsetFrameOverlook.blend>) |
| 57 | 네컷 사진 부스<br>PhotoSpot/Interactive | 350 × 300 × 380 | 310.3 × 272.3 × 380 | 311 × 273 × 380 | 34 / 5236 / 5 | Unreal 네이티브 검증 PASS<br>[blend](<C:/Users/gksro/OneDrive/Desktop/Blender/57_PhotoSpot_Interactive_FourCutPhotoBooth.blend>) |
| 58 | 그림자 놀이 벽<br>PhotoSpot/Interactive | 550 × 350 × 380 | 511.5 × 200.8 × 380 | 512 × 201 × 380 | 24 / 2460 / 5 | Unreal 네이티브 검증 PASS<br>[blend](<C:/Users/gksro/OneDrive/Desktop/Blender/58_PhotoSpot_Interactive_ShadowPlayWall.blend>) |
| 59 | 해파리 조명 군락<br>Lighting/Decorative | 450 × 350 × 460 | 398.6 × 340.1 × 460 | 399 × 341 × 460 | 35 / 7298 / 2 | Unreal 네이티브 검증 PASS<br>[blend](<C:/Users/gksro/OneDrive/Desktop/Blender/59_Lighting_Decorative_JellyfishLightCluster.blend>) |
| 60 | 파도 곡선 조형등<br>Lighting/Decorative | 600 × 300 × 440 | 590.5 × 203 × 440 | 591 × 203 × 440 | 13 / 2196 / 4 | Unreal 네이티브 검증 PASS<br>[blend](<C:/Users/gksro/OneDrive/Desktop/Blender/60_Lighting_Decorative_WaveCurveLight.blend>) |
| 61 | 소원등 행렬<br>Lighting/Decorative | 750 × 180 × 430 | 750 × 141.5 × 395.5 | 750 × 142 × 395.5 | 84 / 13098 / 4 | Unreal 네이티브 검증 PASS<br>[blend](<C:/Users/gksro/OneDrive/Desktop/Blender/61_Lighting_Decorative_WishLanternRow.blend>) |
| 62 | 조개갓 펜던트<br>Lighting/Pendant | 500 × 500 × 450 | 443.9 × 423.5 × 450 | 444 × 424 × 450 | 87 / 18258 / 6 | Unreal 네이티브 검증 PASS<br>[blend](<C:/Users/gksro/OneDrive/Desktop/Blender/62_Lighting_Pendant_ShellShadePendant.blend>) |
| 63 | 등대형 가로등<br>Lighting/Streetlamp | 250 × 180 × 480 | 236.5 × 95.9 × 480 | 237 × 96 × 480 | 40 / 3514 / 9 | Unreal 네이티브 검증 PASS<br>[blend](<C:/Users/gksro/OneDrive/Desktop/Blender/63_Lighting_Streetlamp_LighthouseStreetlamp.blend>) |
| 64 | 마린 볼라드 조명<br>Lighting/Streetlamp | 80 × 80 × 140 | 64.6 × 64.6 × 140 | 65 × 65 × 140 | 12 / 2086 / 2 | Unreal 네이티브 검증 PASS<br>[blend](<C:/Users/gksro/OneDrive/Desktop/Blender/64_Lighting_Streetlamp_MarineBollardLight.blend>) |
| 65 | 청백 스트라이프 보행로<br>Road/Surface | 400 × 400 × 8 | 400 × 400 × 8 | 400 × 400 × 8 | 11 / 32 / 1 | Unreal 네이티브 검증 PASS<br>[blend](<C:/Users/gksro/OneDrive/Desktop/Blender/65_Road_Surface_BlueWhiteStripedWalkway.blend>) |
| 66 | 조개 문양 포장길<br>Road/Surface | 400 × 400 × 8 | 400 × 400 × 8 | 400 × 400 × 8 | 34 / 57 / 1 | Unreal 네이티브 검증 PASS<br>[blend](<C:/Users/gksro/OneDrive/Desktop/Blender/66_Road_Surface_ShellPatternWalkway.blend>) |
| 67 | 발광 경계 야간길<br>Road/Surface | 400 × 400 × 8 | 400 × 400 × 8 | 400 × 400 × 8 | 31 / 72 / 1 | Unreal 네이티브 검증 PASS<br>[blend](<C:/Users/gksro/OneDrive/Desktop/Blender/67_Road_Surface_LuminousEdgeNightPath.blend>) |
| 68 | 바다 모자이크 광장 타일<br>Tile/Surface | 400 × 400 × 8 | 400 × 400 × 8 | 400 × 400 × 8 | 5 / 524 / 1 | Unreal 네이티브 검증 PASS<br>[blend](<C:/Users/gksro/OneDrive/Desktop/Blender/68_Tile_Surface_OceanMosaicPlazaTile.blend>) |
| 69 | 선박 데크 타일<br>Tile/Surface | 400 × 400 × 8 | 400 × 400 × 8 | 400 × 400 × 8 | 17 / 44 / 1 | Unreal 네이티브 검증 PASS<br>[blend](<C:/Users/gksro/OneDrive/Desktop/Blender/69_Tile_Surface_ShipDeckTile.blend>) |
| 70 | 잔디블록 피크닉 타일<br>Tile/Surface | 400 × 400 × 8 | 400 × 400 × 8 | 400 × 400 × 8 | 21 / 52 / 1 | Unreal 네이티브 검증 PASS<br>[blend](<C:/Users/gksro/OneDrive/Desktop/Blender/70_Tile_Surface_GrassBlockPicnicTile.blend>) |

## 검증 증거

다음은 읽은 보고서 전체 목록이다. 과거 실패는 이후 성공으로 해결될 수 있으나 삭제해 숨기지 않는다. 최신 대상별 판정은 위 표에 반영한다.

| 보고서 | 수정 시각 | 실행 상태 | 포함 시설 수 |
|---|---|---|---:|
| [import_1_59_65.json](<C:/Users/gksro/OneDrive/Desktop/Git/TeamProject/8th-Team13-Final-Project/Saved/FacilityExpansion70/import_1_59_65.json>) | 2026-09-20T07:34:23+09:00 | FAIL | 1 |
| [import_1_65.json](<C:/Users/gksro/OneDrive/Desktop/Git/TeamProject/8th-Team13-Final-Project/Saved/FacilityExpansion70/import_1_65.json>) | 2026-09-20T07:36:02+09:00 | PASS | 2 |
| [import_59.json](<C:/Users/gksro/OneDrive/Desktop/Git/TeamProject/8th-Team13-Final-Project/Saved/FacilityExpansion70/import_59.json>) | 2026-09-20T07:36:51+09:00 | PASS | 1 |
| [import_.json](<C:/Users/gksro/OneDrive/Desktop/Git/TeamProject/8th-Team13-Final-Project/Saved/FacilityExpansion70/import_.json>) | 2026-09-20T07:52:37+09:00 | FAIL | 0 |
| [import_1.json](<C:/Users/gksro/OneDrive/Desktop/Git/TeamProject/8th-Team13-Final-Project/Saved/FacilityExpansion70/import_1.json>) | 2026-09-20T08:18:07+09:00 | PASS | 1 |
| [import_all.json](<C:/Users/gksro/OneDrive/Desktop/Git/TeamProject/8th-Team13-Final-Project/Saved/FacilityExpansion70/import_all.json>) | 2026-09-20T08:22:19+09:00 | PASS | 70 |
| [verify_1_65.json](<C:/Users/gksro/OneDrive/Desktop/Git/TeamProject/8th-Team13-Final-Project/Saved/FacilityExpansion70/verify_1_65.json>) | 2026-09-20T07:36:12+09:00 | PASS | 2 |
| [verify_59.json](<C:/Users/gksro/OneDrive/Desktop/Git/TeamProject/8th-Team13-Final-Project/Saved/FacilityExpansion70/verify_59.json>) | 2026-09-20T07:37:13+09:00 | PASS | 1 |
| [verify_1.json](<C:/Users/gksro/OneDrive/Desktop/Git/TeamProject/8th-Team13-Final-Project/Saved/FacilityExpansion70/verify_1.json>) | 2026-09-20T08:18:20+09:00 | PASS | 1 |
| [verify_all.json](<C:/Users/gksro/OneDrive/Desktop/Git/TeamProject/8th-Team13-Final-Project/Saved/FacilityExpansion70/verify_all.json>) | 2026-09-20T08:20:18+09:00 | PASS | 70 |

보고된 오류(과거 실행 포함):

- `import_1_59_65.json`:     raise RuntimeError(message) RuntimeError: Ground-center pivot mismatch: Lighting_Decorative_JellyfishLightCluster
- `import_.json`:     raise RuntimeError(message) RuntimeError: Missing template: Booth_Nightlife_EDMBar

## Unreal 자동 테스트와 실제 렌더

- 전체 대상 네이티브 자동 테스트: **PASS** — 성공 4/4, 실패 0, 미실행 0, 진행 중 0. [index.json](<C:/Users/gksro/OneDrive/Desktop/Git/TeamProject/8th-Team13-Final-Project/Saved/FacilityExpansion70/native-full70-tests_Report/index.json>)

| 자동 테스트 | 결과 |
|---|---|
| `FestaFiesta.Content.FacilityDefinitions.AllDefinitionsAreProjectable` | Success |
| `FestaFiesta.Content.FacilityDefinitions.ReservationUnionProtectsBodies` | Success |
| `FestaFiesta.Data.FacilityDefinitions.ActiveCatalogIdentityAndPrices` | Success |
| `FestaFiesta.Data.FacilityDefinitions.DisplayNamePreservesIdentityAndPrice` | Success |

- Unreal GPU 캡처: **PASS** — 실제 PNG 12/12, 대상 시설 70종. [캡처 기록](<C:/Users/gksro/OneDrive/Desktop/Git/TeamProject/8th-Team13-Final-Project/Saved/FacilityExpansion70/renders/capture_report.json>)
- 실제 Unreal 화면: [UE_Booth](<C:/Users/gksro/OneDrive/Desktop/Git/TeamProject/8th-Team13-Final-Project/Saved/FacilityExpansion70/renders/UE_Booth.png>), [UE_Toilet](<C:/Users/gksro/OneDrive/Desktop/Git/TeamProject/8th-Team13-Final-Project/Saved/FacilityExpansion70/renders/UE_Toilet.png>), [UE_Info](<C:/Users/gksro/OneDrive/Desktop/Git/TeamProject/8th-Team13-Final-Project/Saved/FacilityExpansion70/renders/UE_Info.png>), [UE_Rest](<C:/Users/gksro/OneDrive/Desktop/Git/TeamProject/8th-Team13-Final-Project/Saved/FacilityExpansion70/renders/UE_Rest.png>), [UE_Performance](<C:/Users/gksro/OneDrive/Desktop/Git/TeamProject/8th-Team13-Final-Project/Saved/FacilityExpansion70/renders/UE_Performance.png>), [UE_Safety](<C:/Users/gksro/OneDrive/Desktop/Git/TeamProject/8th-Team13-Final-Project/Saved/FacilityExpansion70/renders/UE_Safety.png>), [UE_PhotoSpot](<C:/Users/gksro/OneDrive/Desktop/Git/TeamProject/8th-Team13-Final-Project/Saved/FacilityExpansion70/renders/UE_PhotoSpot.png>), [UE_Lighting](<C:/Users/gksro/OneDrive/Desktop/Git/TeamProject/8th-Team13-Final-Project/Saved/FacilityExpansion70/renders/UE_Lighting.png>), [UE_Road](<C:/Users/gksro/OneDrive/Desktop/Git/TeamProject/8th-Team13-Final-Project/Saved/FacilityExpansion70/renders/UE_Road.png>), [UE_Tile](<C:/Users/gksro/OneDrive/Desktop/Git/TeamProject/8th-Team13-Final-Project/Saved/FacilityExpansion70/renders/UE_Tile.png>), [UE_Lighting_OFF](<C:/Users/gksro/OneDrive/Desktop/Git/TeamProject/8th-Team13-Final-Project/Saved/FacilityExpansion70/renders/UE_Lighting_OFF.png>), [UE_Lighting_BLUE](<C:/Users/gksro/OneDrive/Desktop/Git/TeamProject/8th-Team13-Final-Project/Saved/FacilityExpansion70/renders/UE_Lighting_BLUE.png>)

자동 테스트 PASS와 GPU 캡처는 각각 해당 검사와 렌더 결과의 근거다. 전체 NPC 이용·배치·이동·철거·저장 복원 플레이 완료를 대신하지 않는다.


## 시설별 남은 연결

- **01 카페 트럭**: 커피 제공 상호작용과 제조 애니메이션은 별도 연결한다.
- **02 과일 에이드 바**: 음료 제공 상호작용과 디스펜서 액체 연출은 별도 연결한다.
- **03 부산 어묵 포장마차**: 조리 김과 음식 제공 상호작용은 별도 연결한다.
- **04 씨앗호떡 부스**: 철판 조리와 음식 수령 애니메이션은 별도 연결한다.
- **05 숯불 닭꼬치 부스**: 숯 발광 제어와 화염·연기 효과는 별도 연결한다.
- **06 비건 샌드위치 트럭**: 음식 제공 상호작용과 제조 애니메이션은 별도 연결한다.
- **07 구름 솜사탕 부스**: 솜사탕 제조 회전과 음식 수령 동작은 별도 연결한다.
- **08 로컬 기념품 편집숍**: 기념품 구매 UI와 상품 데이터는 별도 연결한다.
- **09 페스티벌 의류숍**: 의류 구매 UI와 캐릭터 의상 적용은 별도 연결한다.
- **10 야광 응원용품점**: 응원용품 구매·착용과 발광 제어는 별도 연결한다.
- **11 조개 팔찌 공방**: 팔찌 제작 UI와 작업 애니메이션은 별도 연결한다.
- **12 파도 레진 키링 공방**: 레진 제작 체험과 결과물 선택 UI는 별도 연결한다.
- **13 나만의 부채 공방**: 부채 채색·무늬 선택과 결과물 저장은 별도 연결한다.
- **14 풍선 다트 부스**: 풍선 터짐·다트 투척·표적 판정·경품 지급은 별도 연결한다.
- **15 미니 양궁장**: 활·화살 가동 부품과 과녁 판정·점수·안전 구역 로직은 별도 연결한다.
- **16 농구 슛 챌린지**: 공 물리·득점 판정·제한 시간·점수판 UI는 별도 연결한다.
- **17 리듬 게임 부스**: 음악·영상·발판 입력·판정·점수 UI는 별도 연결한다.
- **18 보드게임 텐트**: 보드게임 규칙·말 조작·NPC 착석 애니메이션은 별도 연결한다.
- **19 서핑 밸런스 체험장**: 서핑보드의 회전 부품·균형 입력·점수·탑승 애니메이션은 별도 연결한다.
- **20 선셋 모크테일 바**: 음료 주문·제공과 바텐더 애니메이션은 별도 연결한다.
- **21 수제맥주 탭룸**: 음료 주문·탭 작동·제공 애니메이션은 별도 연결한다.
- **22 사일런트 디스코 클럽**: 헤드폰 소품·NPC 착용·춤·음악 채널 전환은 별도 연결한다.
- **23 해변 목조 화장실**: 화장실 출입·문 가동·점유 상태 표시는 별도 연결한다.
- **24 넓은 회전 공간 화장실**: 문 가동·점유 상태와 접근성 이용 애니메이션은 별도 연결한다.
- **25 유아 동반 화장실**: 성인·유아 이용 애니메이션과 문·점유 상태는 별도 연결한다.
- **26 이동식 화장실 캐빈**: 문 가동과 점유 상태 표시는 별도 연결한다.
- **27 등대형 종합안내소**: 지도·안내·상담 UI와 안내 서비스 동작은 별도 연결한다.
- **28 양면 지도 키오스크**: 양면 지도 텍스처·언어 선택·실시간 경로 안내는 별도 연결한다.
- **29 컨테이너 통합 매표소**: 매표 UI·이용권 발급·실제 창구별 서비스 연결은 별도 작업이다.
- **30 미아·분실물 통합센터**: 미아 보호·분실물 접수·조회 업무는 별도 연결한다.
- **31 다국어 안내 라운지**: 언어 전환·번역·상담 UI는 별도 연결한다.
- **32 해먹 라운지**: 해먹 탑승·착석·천 움직임 애니메이션은 별도 연결한다.
- **33 해안 전망 벤치**: NPC 착석 애니메이션과 실제 전망 배치는 별도 확인한다.
- **34 돛천 그늘 쉼터**: 그늘의 게임 효과와 천 움직임은 별도 연결한다.
- **35 덩굴 퍼걸러**: 식물 바람 연출·착석 애니메이션·그늘 효과는 별도 연결한다.
- **36 가족 돗자리 피크닉존**: 돗자리 착석·음식 상호작용과 가족 그룹 이용은 별도 연결한다.
- **37 단체 피크닉 테이블**: 여러 NPC의 착석·식사 애니메이션은 별도 연결한다.
- **38 음수 라운지**: 수도 물줄기·병 리필·음수 애니메이션은 별도 연결한다.
- **39 해변 족욕 쉼터**: 수면 재질·물결·족욕 애니메이션과 냉각 효과는 별도 연결한다.
- **40 파도형 콘서트 무대**: 공연 프로그램·음악·영상 화면·무대 조명·관객 행동은 별도 연결한다.（10/14 기준: 시설 정의가 삭제되어 연결 대상 아님）
- **41 가족 인형극장**: 인형·커튼 가동·공연 프로그램·관객 행동은 별도 연결한다.
- **42 야외 영화 상영장**: 영상 재생·음향·상영 시간표·관객 행동은 별도 연결한다.
- **43 로컬 밴드 쇼케이스**: 악기 연주 애니메이션·음향·공연 프로그램·관객 행동은 별도 연결한다.
- **44 어쿠스틱 버스킹 데크**: 연주 애니메이션·음향·공연 시간표·관객 행동은 별도 연결한다.
- **45 거리 마술 스테이지**: 마술 소품 가동·공연 동작·프로그램·관객 행동은 별도 연결한다.
- **46 관객 댄스 챌린지 무대**: 댄스 입력·애니메이션·점수·화면·음악·관객 행동은 별도 연결한다.
- **47 소형 응급처치 카트**: 응급처치 서비스·회복 효과와 카트 이동은 별도 연결한다.
- **48 순찰요원 초소**: 요원 순찰·보안 대응·감시 효과는 별도 연결한다.
- **49 소방 장비 스테이션**: 소화 장비 상호작용·화재 감지·진화 효과는 별도 연결한다.
- **50 야광 표지 대피 집결지**: 비상 신호·대피 경로·NPC 집결 행동은 별도 연결한다.
- **51 대형 모래성**: 사진 포즈·촬영 UI·이미지 저장은 별도 연결한다.
- **52 고래 꼬리 조형물**: 사진 포즈·촬영 UI·이미지 저장과 물 연출은 별도 연결한다.
- **53 FESTAFIESTA 레터링**: 단체 사진 포즈·촬영 UI·이미지 저장은 별도 연결한다.
- **54 해적선 촬영 세트**: 촬영 UI·저장과 키·소품 상호작용은 별도 연결한다.
- **55 서핑보드 컬러월**: 사진 포즈·촬영 UI·이미지 저장은 별도 연결한다.
- **56 일몰 액자 전망대**: 실제 일몰 방향 배치·촬영 UI·이미지 저장은 별도 연결한다.
- **57 네컷 사진 부스**: 연속 촬영·카메라·화면·출력·이미지 저장·커튼 가동은 별도 연결한다.
- **58 그림자 놀이 벽**: 방향성 광원·실제 캐릭터 그림자·촬영 저장은 별도 연결한다.
- **59 해파리 조명 군락**: 실제 조명 리그·밝기·색 제어·저장 복원과 촉수 움직임은 별도 연결한다.
- **60 파도 곡선 조형등**: 실제 조명 리그·밝기·색 제어·저장 복원은 별도 연결한다.
- **61 소원등 행렬**: 실제 대표 광원·밝기·색 제어·저장 복원은 별도 연결한다.
- **62 조개갓 펜던트**: 지지 구조물 기준의 실제 광원·밝기·색 제어·저장 복원은 별도 연결한다.
- **63 등대형 가로등**: 실제 가로등 리그·밝기·색 제어·저장 복원은 별도 연결한다.
- **64 마린 볼라드 조명**: 실제 조명 리그·밝기·색 제어·저장 복원은 별도 연결한다.
- **65 청백 스트라이프 보행로**: 반복 연결·90도 회전·NPC 통행·상부 시설 차단은 실제 배치로 검증한다.
- **66 조개 문양 포장길**: 반복 문양·연결·NPC 통행·상부 시설 차단은 실제 배치로 검증한다.
- **67 발광 경계 야간길**: 발광 경계의 밝기·색 UI 연결과 반복 배치·NPC 통행은 별도 검증한다.
- **68 바다 모자이크 광장 타일**: 파도 문양 연결·상부 시설 설치·NPC 통행은 실제 배치로 검증한다.
- **69 선박 데크 타일**: 판재 문양 연결·상부 시설 설치·NPC 통행은 실제 배치로 검증한다.
- **70 잔디블록 피크닉 타일**: 상부 시설 설치·잔디 겹침·NPC 통행은 실제 배치로 검증한다.

이 문서는 `Tools/facilities/report_expansion70.py`를 재실행해 갱신한다. 보고서 생성은 모델·Data Asset·경제 데이터를 수정하지 않는다.
