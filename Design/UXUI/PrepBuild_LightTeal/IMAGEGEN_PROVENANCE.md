# 이미지 생성 기록과 사용 원칙

## 생성 방식

- 도구: Codex 내장 ImageGen
- 모드: 1차 신규 생성 후, 1차 결과를 참조한 편집 생성
- 목적: UMG 제작 전에 톤앤매너와 두 핵심 상태를 합의하기 위한 콘셉트 시안
- 제품 내 사용 범위: 디자인 레퍼런스 및 선택적 빈 상태 일러스트

## 결과 파일

1. `Generated/FF_PrepBuild_CatalogOpen_LightTeal_v1.png`
   - 하단 도킹형 시설 카탈로그가 열린 상태
2. `Generated/FF_PrepBuild_Placing_LightTeal_v1.png`
   - 카드 선택 후 월드 배치 프리뷰에 집중하는 축약 상태

## 최종 프롬프트 세트

### 1차 — Catalog Open

```text
Create a polished 16:9 product UI concept for an Unreal Engine festival-building game during its preparation phase. Show a warm, clean isometric festival ground in the upper area and a bottom-docked facility catalog integrated into the existing HUD. Use a refined LightTeal design system: paper #F7F8F6, sunken surface #EEF2EF, dark ink #24332E, muted ink #6F7B75, teal #527C6B, deep teal #315747, line #CCD5CF, small amber accents only for cost warnings. The panel should feel calm, friendly, professional, and production-ready—not sci-fi, not neon, not a dark modal. Include a compact title area, category chips, search field, current gold, a virtualized-looking row of clean facility cards, clear affordability states, and restrained bottom control hints. Preserve generous whitespace, 6–8px rounded geometry, subtle shadows, and a Korean game UI sensibility. Keep the world visible with no full-screen dim overlay. Text is only a layout reference and will be rebuilt in UMG.
```

### 2차 — Placing State 편집

```text
Edit the provided LightTeal festival facility-catalog concept into the active placement state while preserving the same isometric world, palette, typography mood, proportions, and visual identity. Collapse the large catalog into a slim bottom control bar so the world becomes the focus. Add a clearly readable food-truck placement footprint/ghost in the world with a valid soft green outline, subtle grid alignment, and natural scale. In the compact bar, show the selected facility summary and restrained controls for confirm, rotate, and cancel; keep the Build tab visibly active. Do not add a dark scrim, neon styling, oversized overlays, extra windows, or decorative clutter. This is a production UI concept; all dynamic text and icons will be rebuilt as UMG widgets.
```

## 실제 제작 원칙

- 생성 이미지 속 텍스트, 숫자, 아이콘을 최종 UI 에셋으로 사용하지 않는다.
- 가격, 골드, 시설명, 상태 문구, 버튼 힌트는 UMG Text/Icon 위젯으로 구성한다.
- 실제 시설 썸네일은 기존 카탈로그의 Soft reference 및 비동기 preload 경로를 사용한다.
- 배경 일러스트를 사용할 경우 신규 고유 파일명과 크기 예산을 적용하고 기존 이미지를 덮어쓰지 않는다.
- 최종 색상과 간격은 이미지의 픽셀을 샘플링하지 않고 프로젝트 팔레트 토큰을 사용한다.
- 이미지 생성 결과는 방향 합의 자료이며, 게임 로직과 접근성 정보를 담는 소스 오브 트루스가 아니다.
