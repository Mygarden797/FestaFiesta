# 미니게임 UIUX v2 ImageGen 프롬프트

## 공통 제작 원칙

- 사용 모드: Codex 내장 ImageGen
- 배경: 정확한 16:9, 텍스트·UI·플레이 오브젝트 없음
- 서핑 오브젝트: 정사각형, 단일 오브젝트, 실제 알파 채널, 52×52에서도 읽히는 실루엣
- 팔레트: 딥 네이비, 오션 블루, 시안·민트, 제한적인 랜턴 앰버·코랄
- 금지: 로고, 상표, 워터마크, 배경에 구워진 동적 UI

## Target Shooting 배경

```text
Use case: stylized-concept
Asset type: production game UI backdrop plate for the FestaFiesta target-shooting minigame
Input images: existing target-shooting backdrop as a style and setting reference only
Primary request: a polished night target-range environment on a Korean waterfront festival pier, cinematic but practical behind a HUD
Scene/backdrop: open-air steel pavilion facing dark water, distant illuminated bridge and subtle festival skyline, wet deck reflections, no people and no physical shooting targets
Style/medium: premium stylized-realistic 3D game environment key art, clean high-end Unreal Engine look
Composition/framing: exact 16:9; symmetrical one-point perspective; large uncluttered central play field; darker edges and calm center; no foreground objects crossing the center
Lighting/mood: deep navy night, restrained cyan/teal architectural light, small warm amber festival practicals
Constraints: background only; no UI, reticle, target, people, weapons, text, logos, trademarks or watermark; preserve top-HUD and centered-modal safe areas
```

## Target Shooting 1인칭 물총

```text
Use case: stylized-concept
Asset type: transparent foreground weapon sprite for an Unreal Engine festival target-shooting minigame UI
Primary request: create a polished, unmistakably toy-like water blaster viewed from the player's first-person perspective, centered at the bottom and aiming straight toward the upper center, suitable for overlaying on a night waterfront shooting-range background
Subject: one chunky arcade water blaster with a short wide cyan barrel, visible translucent water tank, compact dark navy body, mint glow details, and small warm coral-orange accents; no hands
Style/medium: premium stylized-realistic 3D game UI render, clean high-end Unreal Engine asset, bold silhouette readable at about 360 by 240 pixels
Composition/framing: square canvas; weapon base near the bottom edge, barrel extending toward upper center with strong depth perspective; balanced and nearly symmetrical; generous transparent padding around the silhouette
Lighting/mood: cool cyan rim light with subtle warm festival highlights, glossy wet material accents
Constraints: genuinely transparent background with fully transparent corners; exactly one water blaster; no environment, no floor, no target, no projectile, no hands, no people, no text, no logo, no brand, no watermark; must look like a friendly festival toy water gun, never a realistic firearm
```

## Surfing 배경

```text
Use case: stylized-concept
Asset type: production game UI backdrop plate for the FestaFiesta surfing minigame 2D fallback and loading state
Input images: existing surfing backdrop as a color and water-style reference only
Primary request: a premium ocean race-lane backdrop viewed from a high trailing game camera
Scene/backdrop: Korean coastal festival water course in bright late-afternoon daylight; clear turquoise center channel, deep navy sides, subtle wake rails and foam
Style/medium: polished stylized-realistic 3D game environment render, high-end Unreal Engine look
Composition/framing: exact 16:9; forward motion toward top center; broad uncluttered playable channel; calmer lower center; darker side falloff
Constraints: background only; no UI, surfer, board, boat, rock, log, buoy, pickup, text, logo, trademark or watermark; preserve top, bottom and centered-modal safe areas
```

## Surfing 장애물과 픽업

각 프롬프트는 다음 공통 꼬리를 사용했습니다.

```text
Use case: stylized-concept
Asset type: transparent game UI obstacle or collectible icon for FestaFiesta surfing
Style/medium: premium stylized-realistic 3D game icon, bold simplified details, readable at 52x52 pixels
Composition/framing: exactly one object centered in a square canvas, generous transparent padding, strong silhouette
Constraints: genuinely transparent background with preserved alpha; no environment, border, badge, text, logo, trademark, watermark or external drop shadow
```

오브젝트별 주 요청:

- Rock: `dark slate volcanic rock cluster with wet teal highlights and a small rim of sea foam`
- Log: `short floating weathered driftwood log with restrained sea foam along the lower edge`
- Buoy: `compact red-orange and white marine course buoy with a dark lower section and cyan reflective band`
- Coin: `thick golden coin with an original embossed sunburst-and-wave motif and faint warm inner glow`
- Heart: `glossy coral-red translucent resin heart with a simple white highlight and subtle mint edge glow`
- Boost Pickup: `rounded hydro-energy capsule with a bold mint-cyan lightning bolt and amber cap accents`

## Boost 투명 배경 보정

첫 결과의 체크무늬가 실제 투명도가 아니라 이미지에 포함되어 재편집했습니다.

```text
Use case: background-extraction
Primary request: remove the entire white-and-gray checkerboard background and replace it with genuine transparency
Constraints: change only the background; preserve the capsule shape, proportions, colors, lightning symbol and highlights; actual alpha channel with fully transparent corners; no added elements
```
