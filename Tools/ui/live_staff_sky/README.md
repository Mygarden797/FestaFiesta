# 라이브 P7 — 스태프 화면

전체 인원/정원 · 사건 대응 중 · 대응 중이 아닌 인원 카드와 인원 구성, 대응 현황을 표시한다.
미배정 인원은 별도로 표시한다. 남은 인원을 실제 출동 가능 인원으로 단정하지 않는다.
라이브 중 고용·배정 명령은 이 현황 화면에서 추가하지 않는다. 스태프 숙련도·승급은 후속 [P7 공통 숙련도 탭](../staff_training/README.md)으로 연결한다.

## 에디터에서 수정

- 위치·전체 크기: `WBP_FF_LiveHudScreen → Class Defaults → Panel Layouts → Staff`.
  최초1040×660, Center/Offset0. Width/Height로 크기, Offset X/Y로 위치를 조절한다.
- 패널: `/Game/FestaFiesta/UI/LiveHUD/Panels/WBP_FF_LivePanelStaff`.
  `Sky_SizeBox_Header` 높이80, 제목26/설명15, 기존 닫기 버튼 유지.
- 요약 카드: `Staff_Total_Size`, `Staff_Busy_Size`, `Staff_Idle_Size` 폭320·최소높이128.
  카드 간격은 `Staff_Metrics`, 배경·테두리는 각 `Staff_*_Border`, 숫자 글꼴은 기존 `Text_*Staff`.
- 인원 구성/대응 영역: `Staff_Columns` 양쪽 Fill과 패딩. `Staff_Scroll_Details`에서만 스크롤한다.
  상단 요약·제목·하단 안내는 고정한다.
- 정원 막대: `Staff_SizeBox_CapacityBar` 높이20.
  `Progress_StaffCapacity`의 Fill Image는 고용 구간 연초록, Background Image는 빈 정원색.
  위에 겹친 `Progress_BaseCapacity`의 Fill Image는 기본 인원 진초록, Background Image는 투명으로 유지한다.
  실제 비율은 전체÷정원과 기본÷정원을 사용한다. 예: 7/24면 총 막대29.17%, 기본6이면 기본25%다.
  두 막대의 색·브러시·크기는 Designer, 비율과 미확인 표시만 코드에서 갱신한다.
  두 막대의 Overlay 슬롯은 가로·세로 **Fill**이어야 칸을 채운다. 기본값(왼쪽 위)이면 약 20px로 줄어든다.
  2026-10-06 이전에 만든 WBP는 `FF.FixLiveStaffCapacityBar`(`fix_capacity_bar.py`)로 두 슬롯만 고쳤다.
  막대 길이는 `Staff_SizeBox_CapacityBar`가 들어 있는 행 폭, 두께는 그 SizeBox 높이로 조절한다.
- 기본/고용/미배정: `Text_BaseStaff`, `Text_HiredStaff`, `Text_UnassignedStaff`.
  `Staff_UnassignedBorder`에서 미배정 영역 색·패딩을 수정한다.
- 대응 아이콘: `Staff_SizeBox_ResponseIcon`60×60, `Image_ResponseStatus` Tint.
  Class Defaults의 `Response Idle Icon / Active Icon / Unknown Icon`에서 기존 상태별 텍스처를 바꾼다.
- 안내: `Staff_FooterBorder` 배경/패딩, `Staff_FooterColumns` 좌우 크기.
  숫자 형식·대응/미확인/고용 안내 문구는 패널 Class Defaults의 Staff 설정에서 수정한다.

Compile/Save 후 새 PIE에서 확인한다. 실행 때 글꼴·카드 크기·색을 고정값으로 덮어쓰지 않는다.
기존 장식은 접힌 `Staff_LegacyDecorations`와 `Sky_ScrollBox_Body`에 보관한다.

## 수치와 기능 경계

- 기존 ViewModel과 인원/정원/배정 변경 이벤트로 읽는다. 닫기·소멸 시 구독을 해제한다.
- 대응 인원은 이동·대응·복귀 중 인원을 포함한다. 0명만 보고 사건 자체가 없다고 단정하지 않는다.
- 시설별 사건명 목록은 현재 표시 데이터가 없어 임의로 만들지 않는다.
- 모르는 값은 `—`, 구성/정원 불일치는 안내로 표시한다. 정원0으로 나누지 않는다.
- 기본+고용 구성이 불일치하면 두 색으로 가짜 구성을 만들지 않고 안내한다.
- 고용은 셋업/개장 준비에서, 시설별 배정은 기존 안내 시설 UI에서 수행한다.

## 적용

최신 C++ 에디터 빌드 후 에디터를 종료하고 실행한다.

```powershell
& Tools/ui/live_staff_sky/run.ps1
```

`FF.ApplyLiveStaffSkyDesign` 최초1회만 구성. 재실행은 저장된 Designer 편집과 다른 탭 Layout을 보존한다.
백업/보고서는 `Saved/LiveStaffSky/`. 빌드·WBP 컴파일·필수 연결만 확인하고 화면은 사용자가 확인한다.

| 사용자 확인 | 기대 표시 | 이상할 때 확인 |
| --- | --- | --- |
| 라이브 → 스태프 | 중앙 3카드·인원 구성·대응 현황 | Staff Layout·카드 폭 |
| 7/24, 기본6·고용1 | 전체29.17%, 기본25% 길이와 일치 | 두 막대 오버레이·원본 구성 |
| 미배정/대응 상태 | 미배정 별도, 남은 수를 출동 가능으로 표시하지 않음 | 원본 배정 상태·문구 |
| 인원/정원 변경 | 숫자·막대 갱신, 재열기 중복 없음 | 기존 ViewModel 이벤트 |
| 미집계/불일치 | — 또는 안내, 이전 숫자나 정상 상태로 위장하지 않음 | 원본 센티널·CapacityNote |
| 작은 창/큰 글자 | 제목·하단 안내 고정, 가운데만 스크롤 | 카드 크기·중앙 내용/폰트 |
