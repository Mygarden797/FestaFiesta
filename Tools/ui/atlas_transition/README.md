# Atlas 화면 암전 검증

UE 5.7.4 Development Editor 빌드 후 에디터가 종료된 상태에서 실행한다.

```powershell
& 'Tools/ui/blender/bright_globe/run_editor.ps1' -Script 'Tools/ui/atlas_transition/verify.py' -Label atlas_transition_verify
```

`verify.py`는 실제 Frontend PIE를 시작하여 한국 선택 → 국가 지도 → 지구본 왕복을 확인한다.
기본 속도 검사는 프레임별 알파/화면/카메라를 기록하며 캡처 요청으로 타이밍을 지연시키지 않는다.
별도 캡처 왕복에서만 메모리 설정의 시간을 늘린 뒤 원복한다. 프로젝트 설정과 에셋은 저장하지 않는다.

- 새 화면은 완전 불투명 알파에서 한 번만 교체되어야 한다.
- 반복 GoToScreen/GoBack은 전환 중 추가 이력을 만들지 않는다.
- 원래 화면은 가려지기 전 카메라 위치를 유지한다.
- 전환 끝에 불투명도 0, 비활성 상태, 일반 뒤로 이동과 합성 Slate 클릭이 복구된다.
- 월드 시간은 진행하고 일시정지하지 않는다.
- 실제 물리 입력/게임패드 체감 검사는 포함하지 않는다.

결과는 `Saved/AtlasTransition/latest.json`이 가리키는 실행 폴더의 report.json과 PNG에 남긴다.
색상/속도 조절은 Project Settings → Game → FestaFiesta UI Flow → Atlas Transition이다.
