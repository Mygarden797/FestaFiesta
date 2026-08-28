# FestaFiesta 외부 에셋·크레딧 관리

이 문서는 게임에 포함하거나 포함을 검토 중인 외부 폰트·텍스처·음원·아이콘 등의 출처와 라이선스를 관리하는 원장이다.
실제 배포본의 `크레딧 > 외부 에셋` 화면과 동봉 라이선스 파일은 이 문서를 기준으로 작성한다.

> 이 문서는 라이선스 원문을 대체하지 않는다. 출시 전에는 실제로 사용한 파일의 배포본에 동봉된 라이선스도 다시 확인한다.

## 상태 정의

- `confirmed`: 출처와 라이선스를 확인했고 실제 사용이 확정됨
- `assumption`: 사용할 가능성이 높지만 최종 임포트·적용 전임
- `to_verify`: 파일 버전, 저작권자, 추가 조건 또는 최종 사용 여부를 확인해야 함

## 등록 규칙

외부 에셋은 Unreal에 임포트하기 전에 아래 정보를 기록한다.

1. 에셋 이름과 실제 사용 파일명
2. 제작자·권리자
3. 원본 페이지와 직접 다운로드 주소
4. 라이선스명과 라이선스 원문 주소
5. 다운로드 날짜와 원본 버전 또는 파일 해시
6. 크롭·색보정·포맷 변환·리터칭 등 수정 여부
7. 게임 내 크레딧 문구
8. 배포본에 라이선스 원문을 동봉해야 하는지 여부

작업용 원본과 라이선스 사본은 로컬 `SourceArt/ThirdParty/<제공자>/<에셋>/` 아래에 보관한다.
`SourceArt/`는 Git에서 제외되므로, 실제 배포에 필요한 라이선스 원문은 출시 전에 별도의 추적·패키징 경로에도 복사해야 한다.

## 현재 등록 목록

| 에셋 | 용도 | 상태 | 라이선스 | 게임 내 표기 |
| --- | --- | --- | --- | --- |
| Solar System Scope Planet Textures | 지구·구름·노멀·스페큘러 텍스처 | `confirmed` | CC BY 4.0 | 필요 |
| Pretendard | 한국어·공통 UI 폰트 | `confirmed` | SIL OFL 1.1 | 권장, 라이선스 동봉 필요 |
| Michroma | 영문 브랜드 워드마크 폰트 | `confirmed` | SIL OFL 1.1 | 권장, 라이선스 동봉 필요 |
| Dongle | 영문 브랜드 워드마크 비교·Unreal UI 후보 | `confirmed` | SIL OFL 1.1 | 권장, 라이선스 동봉 필요 |
| Copernicus DEM GLO-30 | 대한민국 국가 지도 고도·산악 음영 | `confirmed` | Copernicus WorldDEM-30 User Licence | 필요 |
| geoBoundaries KOR ADM1 | 대한민국 행정구역 경계·6권역 재구성 | `confirmed` | CC BY 4.0 | 필요 |

---

## Solar System Scope Planet Textures

- 상태: `confirmed` — 공식 배포처에서 Earth 2K·8K 텍스처 5종을 확보함. Unreal에 실제 임포트할 최종 해상도는 구현·성능 검증 후 선택
- 제작·배포: Solar System Scope / INOVE
- 원본: https://www.solarsystemscope.com/textures/
- 라이선스: Creative Commons Attribution 4.0 International (`CC BY 4.0`)
- 라이선스 원문: https://creativecommons.org/licenses/by/4.0/
- 확인일: 2026-08-05
- 원본 설명: 실제 천체 지도와 NASA의 고도·이미지 데이터를 바탕으로 조정한 행성 텍스처 팩
- 다운로드 날짜: 2026-08-05
- 원본 보관 경로: `SourceArt/ThirdParty/SolarSystemScope/PlanetTextures/Earth/`
- 확보한 원본:
  - Earth Day Map
  - Earth Night Map
  - Earth Clouds
  - Earth Normal Map
  - Earth Specular Map
- 수정 예정: Unreal용 압축·리사이즈·색보정·머티리얼 변환 가능성 있음

### 🔴 실제로 쓰는 파일은 파생물이다 (2026-08-23 추가)

Unreal에 임포트해 실제로 쓰는 것은 **스타일라이즈 처리한 파생 폴더**다.
CC BY 4.0은 **수정 사실 표시가 의무**이므로 여기 기록한다 (U3-a 구현 중 확인).

⚠️ 2026-08-24 현재 `Content/FestaFiesta/UI/Atlas/Earth/` 에 **원본 2K 5장도 함께
임포트되어 있다**(`2k_earth_*`). 머티리얼 작업 중에 들어간 것이고 파생물과 중복이다.
둘 다 같은 팩·같은 라이선스라 표기 의무는 아래 문구로 함께 충족되지만,
**원본을 그대로 쓰는 파일에는 "수정됨" 문구가 붙지 않아야** 한다.
중복은 정리 대상이다 — 머티리얼이 아직 `2k_earth_daymap`·`2k_earth_nightmap` 을
참조하므로 그래프의 미사용 노드를 지운 뒤에 에셋을 지운다.

- 파생물 보관 경로: `SourceArt/ThirdParty/EarthTextures_Stylized01/`
- 파생 원본: 같은 팩의 **2K** 판(`2k_earth_daymap.jpg` · `2k_earth_nightmap.jpg` · `2k_earth_clouds.jpg`)

| 파생 파일 | 원본 | 수정 내용 |
| --- | --- | --- |
| `earth_day_soft_stylized.png` | `2k_earth_daymap.jpg` | 스타일라이즈 — 대비·채도를 조정해 캐주얼한 색조로. JPEG → PNG 무손실 재저장 |
| `earth_night_soft_stylized.png` | `2k_earth_nightmap.jpg` | 같은 스타일라이즈 처리 |
| `earth_clouds_soft_stylized.png` | `2k_earth_clouds.jpg` | 같은 스타일라이즈 처리 |
| `earth_normal_original.tif` | `2k_earth_normal_map.tif` | **수정 없음** — 이름만 바꿔 함께 보관 |
| `earth_specular_original.tif` | `2k_earth_specular_map.tif` | **수정 없음** — 이름만 바꿔 함께 보관 |

- 파생 3종은 전부 2048×1024, 8비트 **RGB**다.
  ⚠️ **알파 채널이 없다** — 구름 파생물도 RGB이므로 머티리얼에서 알파를 기대하지 말고
  RGB 밝기(또는 R 채널)를 마스크로 쓴다.
- 파일명의 `_original` 접미사가 **수정하지 않은 파일**을 뜻한다. 이 규칙을 유지한다 —
  나중에 무엇을 고쳤는지 파일명만으로 판별할 수 있어야 크레딧 문구가 정확해진다.
- 상태: 파생 3종은 `수정 완료`. 노멀·스페큘러는 `원본 그대로`.
- 게임 내 표기는 **필수**다 (옵션의 `크레딧` 분류). U3-c에서 채운다.

### 지켜야 할 조건

- 적절한 출처를 표시한다.
- 라이선스 링크를 제공한다.
- 수정했다면 수정 사실을 표시한다.
- 원 제작자가 FestaFiesta를 보증하거나 후원하는 것처럼 표현하지 않는다.
- 실제 다운로드 파일이나 압축 파일에 별도 README·라이선스가 있으면 그 조건도 확인한다.

### 권장 크레딧 문구

한국어:

> 행성 텍스처: Solar System Scope(INOVE), CC BY 4.0. FestaFiesta용으로 수정됨.

영어:

> Planet textures by Solar System Scope (INOVE), licensed under CC BY 4.0. Modified for FestaFiesta.

수정하지 않은 원본 파일을 그대로 사용할 경우 마지막 수정 문구는 제거한다.
출처와 라이선스 이름은 크레딧 화면에서 각각 원본 페이지와 라이선스 원문으로 연결한다.

### 확보 파일과 SHA-256

| 파일 | 해상도 | SHA-256 |
| --- | --- | --- |
| `2k_earth_daymap.jpg` | 2048×1024 | `767EE1DC6EB3802699BFCCF6F264880F8ACD0B80DE3191CD24984FE279B07B7C` |
| `8k_earth_daymap.jpg` | 8192×4096 | `88AB060B6E7D241CFC590C69F528FAB2B3247B738D40124CB590999A6FE44ABC` |
| `2k_earth_nightmap.jpg` | 2048×1024 | `C16FD1BC096AB91A5C5265C6FF9847C43F489F2E2EE790CCDBCBD03251CF3A5A` |
| `8k_earth_nightmap.jpg` | 8192×4096 | `9894E83A585A22C1C425E7CA4F987A9BA625BF08ECEE45D3C9DCACAE3C2AD5F7` |
| `2k_earth_clouds.jpg` | 2048×1024 | `FFFD7F68D41B37274822150E54A6EF605AF1D3EC35624D9F628C3B896BFA42ED` |
| `8k_earth_clouds.jpg` | 8192×4096 | `C792ECA228989D36EBB45D3EA6FF1198BE5E21A25D70D2FBCB2124FFD14BA7F5` |
| `2k_earth_normal_map.tif` | 2048×1024 | `F518CE2646CA935DBC17E316041DE4FEA7A5DA0EC441E4EB22E711EABD843BA2` |
| `8k_earth_normal_map.tif` | 8192×4096 | `79D6DBBDE340C653F435F5A012C026A9BED94BDCFD9714E77885F4CBA046635A` |
| `2k_earth_specular_map.tif` | 2048×1024 | `6B90ECFCE248591A1ECC9A3E49ACCA1A7059B6828877E718302ED9A6B4471BD7` |
| `8k_earth_specular_map.tif` | 8192×4096 | `194E67EF7F14DD50B402A1A6F8C7ED55810E16515B95F7736091951EFE8DC3FB` |

### 임포트 시 채울 항목

- 선택 해상도: 2K 우선, 최종 Day Map 8K 여부 `to_verify`
- Unreal 에셋 경로: `/Game/FestaFiesta/UI/Title/Globe/Textures/` 예정
- 실제 수정 내역: `to_verify`

---

## Pretendard

- 상태: `confirmed` — 공식 v1.3.9 배포본에서 Regular·Medium·SemiBold 원본과 라이선스를 확보하고 Unreal에 임포트함
- 제작자·권리자: Kil Hyung-jin 및 Pretendard가 포함하는 원본 폰트 프로젝트의 권리자들
- 원본: https://github.com/orioncactus/pretendard
- 라이선스: SIL Open Font License 1.1
- 라이선스 원문: https://github.com/orioncactus/pretendard/blob/main/LICENSE
- 확인일: 2026-08-05
- 다운로드 날짜·버전: 2026-08-05 / v1.3.9
- 원본 보관 경로: `SourceArt/ThirdParty/Pretendard/1.3.9/`
- 사용 파일:
  - `Pretendard-Regular.otf` — SHA-256 `3FFBACDE6AB8411F1D2DB54BB9B1F0B3EE2A738932033722CF0388C06AED1C93`
  - `Pretendard-Medium.otf` — SHA-256 `D39E50E4BB52B4993B6A4EEB821A171254745BD824446AF01E1F616B89FFACE0`
  - `Pretendard-SemiBold.otf` — SHA-256 `C89BC43027DC7CDE5726E96223376F8EEC09302B2FC1F8147FD5B57CFC376118`
- 원본 압축 파일: `Pretendard-1.3.9.zip` — SHA-256 `04BE351A74D6BF7D60C480A3087E51D185485D35A52023142AF1DF19EB8C428A`
- 보관 라이선스: `LICENSE.txt` — SHA-256 `B04538C9ABEC39A3DB75108CF0AF0FD9C77032FE8AA2CF38345B4D250E98E38E`
- 예정 용도: 한국어 메뉴·본문·수치·옵션·접근성 UI

### 배포 메모

- 폰트를 게임에 번들·임베드하여 배포할 수 있다.
- 배포본에는 원 저작권 고지와 OFL 1.1 라이선스를 함께 포함해야 한다.
- 폰트를 수정할 경우 Reserved Font Name 조건을 별도로 검토한다.

권장 크레딧 문구:

> Pretendard — Copyright (c) 2021 Kil Hyung-jin and contributors. Licensed under the SIL Open Font License 1.1.

### Unreal 임포트

- FontFace:
  - `/Game/FestaFiesta/UI/Fonts/Pretendard-Regular`
  - `/Game/FestaFiesta/UI/Fonts/Pretendard-Medium`
  - `/Game/FestaFiesta/UI/Fonts/Pretendard-SemiBold`
- Composite Font: `/Game/FestaFiesta/UI/Fonts/F_UI_Pretendard`
- 기본 Typeface 순서는 `Medium`, `SemiBold`, `Regular`이며 기존 기본값인 `Medium`을 유지함
- 라이선스 동봉 경로: `to_verify`

---

## Michroma

- 상태: `confirmed` — Google Fonts 공식 배포본의 Regular 원본과 라이선스를 확보함. Unreal 임포트 전 단계
- 제작자·권리자: The Michroma Project Authors
- 원본: https://github.com/google/fonts/tree/main/ofl/michroma
- 라이선스: SIL Open Font License 1.1
- 라이선스 원문: https://github.com/google/fonts/blob/main/ofl/michroma/OFL.txt
- 저작권 고지: Copyright 2011 The Michroma Project Authors
- 확인일: 2026-08-05
- 다운로드 날짜: 2026-08-05
- 원본 보관 경로: `SourceArt/ThirdParty/GoogleFonts/Michroma/`
- 사용 파일: `Michroma-Regular.ttf` — SHA-256 `AEB90DA44B9F39FD7DF446507F4109A39E0B8FFCAD76AD428A0F737E9860E3EC`
- 원본 압축 파일: `Michroma.zip` — SHA-256 `8F3E2B316E0BEC343E1D7EEF0574AEE46C74953722AB7A42A5AF0E1610D77049`
- 보관 라이선스: `OFL.txt` — SHA-256 `06B63D1A13183CB7062CD44107DFAFF4757D8FB5BEBA6551A4CE46CB1AEAF1CC`
- 예정 용도: `FESTA FIESTA` 영문 브랜드 워드마크

### 배포 메모

- 폰트를 게임에 번들·임베드하여 배포할 수 있다.
- 배포본에는 원 저작권 고지와 OFL 1.1 라이선스를 함께 포함해야 한다.
- 폰트를 수정할 경우 Reserved Font Name 조건을 별도로 검토한다.

권장 크레딧 문구:

> Michroma — Copyright 2011 The Michroma Project Authors. Licensed under the SIL Open Font License 1.1.

### 임포트 시 채울 항목

- Unreal Font Asset 경로: `/Game/FestaFiesta/UI/Fonts/` 예정
- 라이선스 동봉 경로: `to_verify`

---

## Dongle

- 상태: `confirmed` — Google Fonts 공식 저장소의 Light·Regular·Bold 원본과 라이선스를 확보하고 Unreal에 임포트함
- 제작자·권리자: The Dongle Project Authors
- 원본: https://github.com/google/fonts/tree/main/ofl/dongle
- 라이선스: SIL Open Font License 1.1
- 라이선스 원문: https://github.com/google/fonts/blob/main/ofl/dongle/OFL.txt
- 저작권 고지: Copyright 2021 The Dongle Project Authors
- 확인일: 2026-08-07
- 다운로드 날짜: 2026-08-07
- 원본 보관 경로: `SourceArt/ThirdParty/GoogleFonts/Dongle/`
- 사용 파일:
  - `Dongle-Light.ttf` — SHA-256 `BB04E03DBE44FB0C619434F31565E62B5BAC67229D5ED72EEAC69C2E74E9A45C`
  - `Dongle-Regular.ttf` — SHA-256 `2966E7A9D312A868E2B5477BF9A3E59B8CE16BEB76E7E52305AB5897D1CE9741`
  - `Dongle-Bold.ttf` — SHA-256 `944C498C0D1A1832AB36F173B1B3AA5AE77B2A914E00C4D79E05338FAE36472D`
- 보관 라이선스: `OFL.txt` — SHA-256 `F9E59049E824264BFFD626D0901CFCAAC9D5BA756988B904D3E3E0E233493607`
- 용도: HTML 프로토타입과 Unreal에서 `FESTA FIESTA` 영문 브랜드 워드마크 비교. 최종 채택 여부는 별도 결정
- 수정 내역: 원본 파일 수정 없음

권장 크레딧 문구:

> Dongle — Copyright 2021 The Dongle Project Authors. Licensed under the SIL Open Font License 1.1.

### Unreal 임포트

- FontFace:
  - `/Game/FestaFiesta/UI/Fonts/Dongle-Light`
  - `/Game/FestaFiesta/UI/Fonts/Dongle-Regular`
  - `/Game/FestaFiesta/UI/Fonts/Dongle-Bold`
- Composite Font: `/Game/FestaFiesta/UI/Fonts/F_UI_Dongle`
- 기본 Typeface 순서는 `Regular`, `Light`, `Bold`이며 기본값은 `Regular`
- 최종 타이틀 워드마크 폰트 채택 여부: `to_verify`
- 라이선스 동봉 경로: `to_verify`

---

## Copernicus DEM GLO-30

- 상태: `confirmed` — 대한민국 국가 지도용 실제 고도·산악 음영 기준으로 사용
- 데이터셋: `COP-DEM_GLO-30-DGED` / 제품 유형 `SAR_DGE_30_A4AD`
- 제작·권리: © DLR e.V. 2010-2014, © Airbus Defence and Space GmbH 2014-2018; European Union 및 ESA의 COPERNICUS로 제공
- 원본 페이지: https://documentation.dataspace.copernicus.eu/Data/Others/CCM.html
- 라이선스: Copernicus WorldDEM-30 User Licence
- 라이선스 원문: https://documentation.dataspace.copernicus.eu/APIs/SentinelHub/Data/DEM/resources/license/License-COPDEM-30.pdf
- 다운로드 날짜: 2026-08-24
- 확보 범위: 대한민국 본토·제주·울릉도·독도와 서해 도서를 포함하는 1°×1° GLO-30 DGED 타일
- 원본 형식: WGS84/EPSG:4326, 1 arc-second, 3601×3601 Float GeoTIFF
- 작업 원본: 사용자 로컬 `Downloads/DEM1_SAR_DGE_30_*.zip`; 필요한 라이선스 사본과 처리 스크립트는 `SourceArt/ThirdParty/CopernicusDEM/Korea/`에 보관
- 로컬 라이선스 사본: `SourceArt/ThirdParty/CopernicusDEM/Korea/licenses/Copernicus-WorldDEM-30-eula_F.pdf` — SHA-256 `32049914C37F14E7D53B48D13D74A49E77C030236E2ACC7A0DF426F9344FEBA2`
- 수정 내역: 타일 병합, 대한민국 ADM1 경계 마스킹, 리샘플링, 고도 색상화, hillshade 생성 및 게임 지도용 합성
- 사용 산출물: `docs/ui/mockups/festival-atlas-adm1/terrain-reference/`의 지형 기준 이미지와
  `SourceArt/UI/Atlas/KoreaNaturalParts_v1/Map/T_FF_FA_Map_KoreaNaturalADM1_2400x2160.png`,
  `SourceArt/UI/Atlas/KoreaNaturalParts_v1/Overlays/T_FF_FA_Overlay_ADM1_08_Busan_Selected_2400x2160.png`
- 배포본 라이선스 동봉 경로: `to_verify`

변형·가공 데이터 권장 크레딧 문구:

> produced using Copernicus WorldDEM-30 © DLR e.V. 2010-2014 and © Airbus Defence and Space GmbH 2014-2018 provided under COPERNICUS by the European Union and ESA; all rights reserved.

책임 제한 고지:

> The organisations in charge of the Copernicus programme by law or by delegation do not incur any liability for any use of the Copernicus WorldDEM-30.

---

## geoBoundaries KOR ADM1

- 상태: `confirmed` — 대한민국 17개 ADM1을 6개 큰 권역으로 재구성하는 경계 기준으로 사용
- 제품: `geoBoundaries-KOR-ADM1`, boundary ID `KOR-ADM1-68945753`, 경계 기준 연도 2021, 빌드 2023-12-12
- 제작·배포: geoBoundaries / William & Mary geoLab
- 원본 페이지: https://www.geoboundaries.org
- 학술 출처: Runfola et al. (2020), https://doi.org/10.1371/journal.pone.0231866
- 라이선스: Creative Commons Attribution 4.0 International (`CC BY 4.0`)
- 라이선스 원문: https://creativecommons.org/licenses/by/4.0/
- 다운로드 날짜: 2026-08-21
- 원본 보관 경로: `C:/Users/jt/Downloads/geoBoundaries-KOR-ADM1-all/`
- 로컬 인용·메타데이터 사본:
  - `SourceArt/ThirdParty/CopernicusDEM/Korea/licenses/geoBoundaries-CITATION-AND-USE.txt` — SHA-256 `F6EA7572BEA6036C4CDCACF8C0CA7BF09098D4E600D19546D7432533E9A290D5`
  - `SourceArt/ThirdParty/CopernicusDEM/Korea/licenses/geoBoundaries-KOR-ADM1-metaData.json` — SHA-256 `0CA1426407D6ACC6B9BBC258F3DEFB65F17A37B424ABC93D629A16AF189549E4`
- 수정 내역: 17개 ADM1 도형을 6개 선택 권역으로 통합·단순화하고, UI용 경계선·마스크로 변환. 독도는 원본 ADM1에 없어 별도 좌표 기준으로 보완
- 사용 산출물: `docs/ui/mockups/festival-atlas-adm1/`의 지도 실루엣·권역 경계·파생 UI 목업과
  `SourceArt/UI/Atlas/KoreaNaturalParts_v1/Map/T_FF_FA_Map_KoreaNaturalADM1_2400x2160.png`,
  `SourceArt/UI/Atlas/KoreaNaturalParts_v1/Overlays/T_FF_FA_Overlay_ADM1_08_Busan_Selected_2400x2160.png`
- 배포본 라이선스 동봉 경로: `to_verify`

권장 크레딧 문구:

> Administrative boundaries by geoBoundaries, licensed under CC BY 4.0. Modified for FestaFiesta.

---

## 새 외부 에셋 등록 템플릿

```md
## 에셋 이름

- 상태: `to_verify`
- 실제 사용 파일:
- 제작자·권리자:
- 원본 페이지:
- 직접 다운로드:
- 라이선스:
- 라이선스 원문:
- 다운로드 날짜·버전:
- SHA-256:
- Unreal 에셋 경로:
- 수정 내역:
- 배포본 라이선스 동봉 경로:

권장 크레딧 문구:

> 작성 예정
```

## 출시 전 체크리스트

- [ ] `to_verify` 상태의 실제 사용 에셋을 모두 `confirmed`로 변경
- [ ] 사용하지 않은 후보 에셋은 목록에서 제거하거나 미사용으로 표시
- [ ] 원본 URL·라이선스 URL 재확인
- [ ] 모든 수정 에셋에 수정 사실 표시
- [ ] OFL·CC 등 필요한 라이선스 원문을 배포본에 포함
- [ ] 옵션의 `크레딧 > 외부 에셋` 화면과 이 문서의 문구 일치 확인
- [ ] 한국어·영어 크레딧 모두 링크 또는 QR/URL 접근 가능 여부 확인
