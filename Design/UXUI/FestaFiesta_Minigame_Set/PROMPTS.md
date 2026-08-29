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

## Target Shooting 3D 폴백 배경 ImageGen v3

생성일: 2026-08-29

생성 도구: Codex 내장 ImageGen

프로젝트 원본: `SourceAssets/UI/Minigame/TargetShooting/T_MG_TargetShooting_Backdrop_ImageGen_v3.png`

```text
Use case: stylized-concept
Asset type: production fallback backdrop plate for a 3D aim-trainer minigame in FestaFiesta
Primary request: create a polished nighttime waterfront festival shooting-gallery environment that can sit behind a real-time 3D target and HUD
Scene/backdrop: open-air steel pavilion on a Korean coastal pier, wet reflective floor, distant illuminated bridge and soft festival skyline, architectural rails and light strips creating real depth; no people and no physical targets
Style/medium: premium stylized-realistic 3D game environment render, clean optimized Unreal Engine art direction, friendly festival atmosphere rather than military
Composition/framing: exact 16:9 landscape; centered one-point perspective; broad empty central target field; lower-center space for a first-person toy water blaster; dark uncluttered top band for HUD; no foreground object blocking the play field
Lighting/mood: deep navy night, restrained cyan and mint edge lighting, small warm amber practical lights, crisp readable silhouettes, moderate contrast
Color palette: deep navy, ocean blue, cyan, mint, limited amber and coral accents
Constraints: background environment only; no UI, text, numbers, crosshair, target, weapon, projectile, hands, people, logo, trademark or watermark; no checkerboard; no placeholder look; production-ready game art
```

## Target Shooting 미니멀 원형 과녁 ImageGen v1

생성일: 2026-08-29

생성 도구: Codex 내장 ImageGen

프로젝트 원본: `SourceAssets/UI/Minigame/TargetShooting/T_MG_TargetShooting_Target_Simple_ImageGen_v1.png`

최종 아트 정제 프롬프트:

```text
Use case: image-editing
Primary request: redesign the referenced circular shooting target so it looks deliberately art-directed by a senior game UI designer, with zero generative-AI visual residue.
Design language: minimalist professional arcade UI asset; exact geometric construction; clean flat vector-like shapes rendered as a production bitmap; mathematically perfect concentric circles; disciplined spacing and optical balance.
Keep: one perfectly circular target; deep navy outer rim, coral-red main ring, pale cyan inner ring, warm yellow bullseye; transparent background; square canvas; centered front view.
Change: remove all inflated toy-like volume, excessive gloss, blurry gradients, random highlights, soft shadowing, uneven bevels, embossed appearance, and painterly texture. Use almost-flat color fields with only a tiny consistent 1–2% edge value shift for separation. Make every circular edge razor-clean and evenly weighted. Use a thin, uniform deep-navy separator line between color zones. Keep the outer silhouette perfectly smooth.
Composition: large target with exactly equal transparent padding on all four sides; no cropping; strict radial symmetry; no perspective.
Production constraints: genuine alpha transparency with fully transparent corners; crisp antialiased edges; readable from 64px to 512px; no text, numbers, logo, watermark, bolts, screws, tabs, stand, lights, decorations, scratches, seams, floor, environment, cast shadow, checkerboard, or extra objects. The result should feel intentional, restrained, timeless, and immediately shippable in a polished Unreal Engine UMG interface.
```

첫 정제 결과의 체크무늬가 실제 픽셀이어서 아래 배경 추출 프롬프트로 보정했다.

```text
Use case: background-extraction
Primary request: remove the entire white-and-gray checkerboard background from the referenced image and replace it with genuine alpha transparency.
Preserve exactly: the single circular target, its dimensions, center position, mathematical concentric geometry, crisp deep-navy outlines, flat deep-navy/coral/pale-cyan/warm-yellow palette, subtle restrained value shifts, and clean antialiased outer edge.
Constraints: change only the background; all pixels outside the outer circular silhouette must be fully transparent including every corner; do not add a checkerboard, white matte, colored halo, cast shadow, glow, texture, text, logo, watermark, extra object, or new detail; retain the square 1254 by 1254 canvas and equal padding; production-ready RGBA PNG for Unreal Engine UMG.
```

- 최종 검증: 1254×1254, 32-bit RGBA, 모서리 알파 0
- Unreal: `/Game/FestaFiesta/UI/Minigame/TargetShooting/T_MG_TargetShooting_Target_Simple_ImageGen_v1`

## Target Shooting 단일 배경 ImageGen v4

생성일: 2026-08-29

생성 도구: Codex 내장 ImageGen

프로젝트 원본: `SourceAssets/UI/Minigame/TargetShooting/T_MG_TargetShooting_Backdrop_Single_ImageGen_v4.png`

```text
Use case: precise-object-edit
Asset type: single production gameplay backdrop for the FestaFiesta target-shooting UMG screen
Input image: edit target; preserve only its restrained deep-navy waterfront-night palette and distant bridge atmosphere
Primary request: simplify the entire scene into one continuous, clean, unobstructed background plate. Remove every awkward near-camera or foreground structure.
Scene/backdrop: a calm dark-blue night sky above distant softly lit waterfront and bridge, with a simple broad dark shooting-field gradient occupying most of the frame; the background must read as one unified layer rather than a physical pavilion or room.
Style/medium: polished senior game UI environment art, restrained stylized 3D backdrop, clean large shapes, low visual noise, no generative-AI clutter
Composition/framing: exact 16:9 landscape; uninterrupted empty central and lower-center play space; distant horizon kept low and subtle; darker edges for HUD readability; no object overlaps the gameplay field
Lighting/mood: even cool navy and muted cyan, extremely limited warm lights only on the distant horizon, no dramatic foreground reflections or spotlights
Change/remove: remove the entire roof and canopy, all ceiling beams and trusses, all columns, railings, fences, pennant flags, hanging bulbs, floor lamps, posts, foreground wet floor, close reflections, frames, platforms, barriers and all near-camera geometry
Constraints: one seamless flat backdrop plate only; no foreground objects of any kind; no architectural frame around the image; no floor plane converging toward camera; no target, weapon, projectile, hand, person, HUD, reticle, text, number, logo, watermark, checkerboard or split layers; clean production-ready background with ample negative space for moving circular targets.
```

- 규격: 1672×941 RGB
- Unreal: `/Game/FestaFiesta/UI/Minigame/TargetShooting/T_MG_TargetShooting_Backdrop_Single_ImageGen_v4`
- v3 배경은 롤백용으로 유지

## Surfing 최종 기본 백드롭 (2026-08-29)

```text
Use case: stylized-concept
Asset type: production 16:9 gameplay backdrop plate for the FestaFiesta ocean surfing minigame
Primary request: create a polished, energetic third-person surfing course background inspired by the readable flow and cheerful arcade energy of modern casual surf games, while remaining visually original
Scene/backdrop: open ocean off Busan at late afternoon, camera hovering low behind an implied surfer path, long rolling turquoise swells and layered foam lanes flowing toward a bright horizon, faint Korean coastline silhouettes far away
Style/medium: premium stylized 3D game render, clean shapes, realistic water material with slightly simplified arcade readability
Composition/framing: wide landscape; centered forward racing corridor must remain open and uncluttered; darker water on far left and right edges for HUD contrast; strong depth and speed lines in the foam; no foreground subject
Lighting/mood: warm golden sunlight, fresh sea air, optimistic festival energy, high clarity
Color palette: deep navy edges, vivid turquoise center, white foam, warm coral-gold highlights
Constraints: background only; seamless visual hierarchy for overlaid gameplay; no surfer, no surfboard, no rocks, no buoys, no pickups, no boats, no buildings in foreground
Avoid: all text, logos, UI, watermarks, split screens, excessive bloom, photographic noise, duplicated horizons, dangerous storm mood
```

- 생성 모드: Codex 내장 ImageGen
- 원본: `SourceAssets/UI/Minigame/Final/T_MG_Surfing_Backdrop_Final.png`
- Unreal: `/Game/FestaFiesta/UI/Minigame/Final/T_MG_Surfing_Backdrop_Final`
- 기존 `UIUX_v2` 백드롭은 롤백용으로 유지

## Surfing 최종 플레이어 서퍼 (2026-08-29)

```text
Use case: stylized-concept
Asset type: final in-game player sprite for a fast ocean surfing minigame
Primary request: a premium stylized 3D game render of one athletic surfer riding a sleek shortboard at speed, with a compact white-and-turquoise water wake integrated directly beneath and behind the board
Scene/backdrop: genuinely transparent background; no ocean, no sky, no ground plane
Subject: full surfer and complete surfboard visible, rider in a dynamic low carving stance with both arms naturally balancing, modern coral-orange and deep-navy wetsuit, warm natural skin tone, ivory board with a small teal accent, readable silhouette at 120 pixels
Style/medium: polished AAA-friendly stylized 3D game asset, physically believable materials, clean cinematic lighting, matching glossy high-detail collectible and obstacle renders
Composition/framing: centered isolated cutout, three-quarter rear/top-down gameplay camera, board nose points straight upward, entire wake contained in canvas, generous transparent padding, vertical silhouette
Lighting/mood: bright warm sunset key light from upper right, cool turquoise rim light from water
Materials/textures: wet neoprene, lightly waxed fiberglass board, crisp translucent foam and droplets
Constraints: exactly one surfer and one board; actual alpha transparency; clean antialiased edges; no opaque or checkerboard background; no text; no logo; no watermark
Avoid: mannequin, low-poly geometry, T-pose, front-facing portrait, cropped limbs, cropped board, extra people, extra boards, flat icon style, black background, white background, cast shadow detached from the wake
```

- 생성 모드: Codex 내장 ImageGen
- 원본: `SourceAssets/UI/Minigame/Final/T_MG_Surfing_Surfer_Final.png`
- Unreal: `/Game/FestaFiesta/UI/Minigame/Final/T_MG_Surfing_Surfer_Final`
- 크기/채널: 1024×1536 RGBA, 실제 알파 채널

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
