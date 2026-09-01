# ImageGen provenance

Built-in `image_gen`을 사용했다. 사용자 첨부 이미지는 **스타일·구도 참고 이미지**이며 편집
대상이 아니다. 첫 결과의 3D/네온 표현을 줄인 뒤, 실제 파일 검사에서 알파가 없음을 확인해
배경 추출 단계를 한 번 더 수행했다.

## 최종 베이스 생성 프롬프트

```text
Use case: precise-object-edit
Asset type: Unreal Engine game settings UI keyboard visual layer
Input images: Image 1: original settings-screen style reference; Image 2: current generated keyboard to simplify.
Primary request: Simplify Image 2 so it matches the restrained, clean visual language of Image 1: flatter keycaps, thinner outlines, softer dark-gray values, almost no 3D depth, and only a very subtle cool-teal selection accent.
Subject: one centered full-size ANSI keyboard in a perfectly straight top-down orthographic view with complete function row, alphanumeric block, navigation cluster, arrow keys, and numpad.
Style/medium: simple polished flat game UI asset, minimalist dark translucent settings interface, crisp 2D/very shallow 3D.
Composition/framing: wide landscape; exact full keyboard visible; generous transparent padding; straight and symmetrical.
Color palette: neutral charcoal and medium gray, thin muted borders, tiny restrained teal accents only between/select key areas; no bright neon.
Text: none.
Constraints: genuinely transparent alpha; preserve one complete accurate standard keyboard layout and clear separate key shapes; no printed letters, numbers, icons, legends, labels, branding, logos, mouse, cable, background panel, checkerboard baked into the pixels, watermark, bloom, dramatic lighting, thick chassis, or decorative objects. Keep it much simpler and cleaner than Image 2 so interactive UMG key buttons and labels can sit on top.
```

## 최종 알파 추출 프롬프트

```text
Use case: background-extraction
Asset type: Unreal Engine UMG keyboard artwork
Input images: Image 1: edit target, the current simple dark keyboard render.
Primary request: Remove only the white and light-gray checkerboard background and replace it with genuine transparent alpha.
Constraints: preserve the keyboard exactly: identical shape, key layout, proportions, colors, thin outlines, subtle teal accents, lighting, framing, resolution, and padding; do not redesign, add, remove, move, resize, relabel, or recolor any key; no text, no background color, no checkerboard pixels, no shadow outside the keyboard, no watermark. Output a clean isolated keyboard cutout on actual transparency.
```

최종 PNG는 32-bit ARGB이며 좌상단 알파가 0인 것을 파일 수준에서 확인했다.
