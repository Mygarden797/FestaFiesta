# 시설 카탈로그

실제 PrepHUD 건설 패널과 시설 카드 WBP에 적용되는 편집 가능한 디자인이다.

- 두 줄 시설 카드에 썸네일, 이름, 가격을 표시한다.
- 기존 대분류·소분류·검색·즐겨찾기에 필수·배치 가능·프로그램·조명 조건을 함께 적용한다.
- 추천순, 이름순, 가격 오름/내림차순으로 정렬한다. 가격 미확정 시설은 가격 정렬의 마지막에 표시한다.
- 배치 가능 조건은 현재 금화와 설치 수 제한을 확인한다. 실제 지면·충돌 검사는 배치 미리보기에서 수행한다.
- 목록 드래그는 클릭과 구분하며, 놓기 직전 이동과 관성 감속을 처리한다. 호버는 썸네일에만 적용하여 카드 클릭 영역을 유지한다.

이미 적용된 에셋은 바로 사용한다. 디자인을 다시 적용할 때는 에디터/PIE를 종료하고 프로젝트 루트에서 실행한다.

```powershell
& ./Tools/Build/Build-Editor.ps1 -EngineRoot 'C:\Unreal\UE_5.7'
& 'C:\Unreal\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' "$PWD\FestaFiesta.uproject" '/Engine/Maps/Entry' '-run=pythonscript' "-script=$PWD\Tools\ui\build_intuitive_catalog\apply.py" '-unattended' '-nop4' '-nullrhi' '-nosound' '-ddc=FestaFiestaBuildDDC'
```

`apply.py`는 두 WBP의 원본을 `Saved/CatalogDesign/<시각>/`에 복사하고 `FF.ApplyBuildIntuitiveCatalog`를 호출한다. 다른 화면 에셋과 이벤트 그래프는 변경하지 않는다. 실행 중인 에디터에서도 PIE 종료 후 같은 콘솔 명령을 사용할 수 있다.

검증 필터는 `FestaFiesta.UI.PrepBuild`이다. `-RenderOffscreen`으로 실행하면 세 해상도의 실제 Slate 카드·필터 배치와 PNG 결과를 확인할 수 있다. `-nullrhi`는 PNG 렌더 검증을 생략한다. 출력은 `Saved/Automation/BuildableGrid/BuildCatalog_*.png`에 저장된다.
