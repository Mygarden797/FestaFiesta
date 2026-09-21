# 도감 보상 아이콘

- 생성일: 2026-09-21
- 방식: 내장 imagegen 이미지 편집 도구
- 입력: 사용자가 첨부한 빨강·흰색 선물 상자와 노란 리본 이미지
- 출력: `Generated/reward_gift_v1.png` (실제 알파 투명도 보존)
- 게임 텍스처: `/Game/FestaFiesta/Building/FF_WBP_Building/FacilityCodex/Icons/T_FF_Codex_RewardGift`
- 현재 표시: 수수료 오른쪽 24×24 선물 아이콘과 `받기` 버튼. 수령 후에는 기존 체크 아이콘과 `완료` 배지로 전환.

최종 프롬프트:

```text
Use case: background-extraction. Asset type: small reward-claim icon for an Unreal game collection card. Input image 1 is the edit target. Extract the single gift box from the supplied image as a crisp high-quality transparent PNG with an actual alpha channel. Keep the original rounded cylindrical white and red striped gift box, red lid, large glossy yellow ribbon bow, colorful toy-like 3D styling, and same three-quarter viewing angle. Remove only the black/magenta background and surrounding glow/shadow; preserve the recognizable gift shape and colors. Center the full gift, no cropping, with a small 5 percent transparent margin on a square canvas. The silhouette must be very legible when displayed at 28 pixels. No background, no checkerboard pixels, no text, no letters, no border, no additional symbols or objects.
```
