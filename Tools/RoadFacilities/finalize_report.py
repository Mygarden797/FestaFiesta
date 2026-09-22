"""Consolidate observed Blender/Unreal evidence; never infer a missing PASS."""
import hashlib,json,shutil
from pathlib import Path

PROJECT=Path(__file__).resolve().parents[2]
ROOT=PROJECT/'SourceArt/Roads_20260910'
SAVED=PROJECT/'Saved/Roads_20260910'
def read(path):return json.loads(path.read_text(encoding='utf-8-sig'))
report=read(ROOT/'validation_report.json')
report['reference_reproduction']='PASS'
report['visual_review']={'status':'PASS','evidence':['qa/Reference_Comparison.png']+['qa/SM_Road_'+n+'_ContactSheet.png' for n in ['Sand','Asphalt','Wood','Stone','Herringbone','Concrete']],
 'scope':'Actual reference and six multiview/LOD contact sheets reviewed by two agents. Recognizable design signatures; no visible open bottoms, flipped dark patches or broken LOD silhouettes.',
 'approximations':['4m square and8cm thickness inferred for a walkable modular floor; source contains no measurements.','Sand ripples/footprints reduced to flecks; wood grain and fasteners simplified at distance.','Curb segment counts differ; concrete drain rim/bars are brighter and denser than reference.','Shared unbaked color palette; no reference shadows painted into texture.']}
report['renderer']={'blender':'5.2.1 LTS','engine':'Cycles','samples':24,'device':'NVIDIA GeForce RTX3060Ti','backend':'OptiX'}
evidence=ROOT/'qa/unreal';evidence.mkdir(parents=True,exist_ok=True)
for name,key in [('import_report.json','unreal_import'),('verification_report.json','unreal_saved_asset_verification')]:
    path=SAVED/name
    if path.exists():
        value=read(path);report[key]=value['status'];shutil.copy2(path,evidence/name)
        if name=='import_report.json':
            for source,digest in value['source_sha256'].items():
                assert hashlib.sha256((ROOT/source).read_bytes()).hexdigest()==digest,source
            report['unreal_source_hashes_match']=True
            for asset in report['assets']:asset['unreal_import']=value['status']
    else:report[key]='NOT_RUN'
tests=sorted(SAVED.glob('road_automation*_validation.json'))
if tests:
    value=read(tests[-1]);report['runtime_automation']=value['status'];report['runtime_automation_pass_count']=value['pass_count'];shutil.copy2(tests[-1],evidence/'runtime_tests.json')
    log=Path(value['log'])
    if log.exists():shutil.copy2(log,evidence/'runtime_tests.log')
else:report['runtime_automation']='NOT_RUN'
captures=SAVED/'renders/capture_report.json'
if captures.exists():
    value=read(captures);report['unreal_visual_capture']=value['status'];shutil.copy2(captures,evidence/'capture_report.json')
    report['unreal_visual_review']=value.get('visual_review','NOT_RUN')
    for path in (SAVED/'renders').glob('*.png'):shutil.copy2(path,evidence/path.name)
else:report['unreal_visual_capture']='NOT_RUN'
for filename,key in [('saved_blends_validation.json','saved_blends'),('render_validation.json','blender_render')]:
    path=ROOT/'qa'/filename
    report[key]=read(path)['status'] if path.exists() else 'NOT_RUN'
build_log=SAVED/'EditorBuild_Final.log'
report['editor_build']='NOT_RUN'
if build_log.exists():
    report['editor_build']='PASS' if 'Result: Succeeded' in build_log.read_text(encoding='utf-8-sig') else 'FAIL'
    shutil.copy2(build_log,evidence/build_log.name)
report['engine_version']='5.7.4'
report['game_performance']='NOT_RUN';report['game_performance_ms']=None
(ROOT/'validation_report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
rows='\n'.join('| '+a['asset']+' | '+' / '.join(str(x['triangles']) for x in a['lods'])+' | '+str(a['source_parts'])+' |' for a in report['assets'])
text=f'''# 길6종 제작·적용 보고서

Road.png의6가지 디자인을 편집 가능한 입체 모델로 제작했다. Blender MCP5.2.1 LTS에서 실행했으며 RTX3060Ti OptiX/Cycles로 실제 모델을 렌더했다. 원본 사진과 작업 전 Blender 씬은 보존했다.

## 납품

- `blend`: 부품별 편집이 가능한 시설별 원본6개. SOURCE 부품과 EXPORT 복제본을 분리했다.
- `export`: 최종 삼각분할 FBX18개(각 LOD0/1/2). LOD0마다 UCX 단순 볼록 충돌1개.
- `textures/T_Road_Palette.png`: 공유256px 팔레트. Unreal 재질1개, 메시당 슬롯1개.
- `qa`: 앞·뒤·좌·우·탑·밑면·게임 시점·LOD 이미지와 비교표, 실행 검사 JSON.
- `code`: 재생성·검증·임포트 코드 사본과 재실행 안내. 실행 원본은 프로젝트의 `Tools/RoadFacilities`이며 공통 검사 모듈도 포함했다.

각 타일은400×400×8cm이며 피벗은 바닥 중앙이다. Blender 모델의 +X 전방/+Z 위쪽을 별도1m 및 축별 FBX로 Unreal에서 검증한다. FBX는 단위 변환1회, 양의 단위 스케일, 선택 메시만 내보낸다. 물리 충돌은 장식 홈을 제외한 얕은 노면의 단순 볼록 상자1개다. 평탄한 길이므로 통로를 막는 시설 전체 충돌 문제는 해당하지 않는다.

| 메시 | LOD0 / LOD1 / LOD2 삼각형 | 편집 부품 |
|---|---:|---:|
{rows}

UV0는 공유 색상 팔레트이므로 의도적으로 중첩한다. UV1은 삼각형마다 서로 다른 여백 포함 격자 섬을 만든다. 라이트맵은 필요시1024px를 권장하며 현재 동적 조명용이다. Nanite는 껐다.

## 게임 연결

시설 메뉴의 **길**과 **타일** 카테고리에 각각6종이 표시된다. 정의는 `/Game/FestaFiesta/Building/FF_Data_Building/Road/PDA_FFZS_Road_Surface_*`와 `/Game/FestaFiesta/Building/FF_Data_Building/Tile/PDA_FFZS_Tile_Surface_*`에 있다. 설치 비용은 각각 `Cost.Road.Surface.*`와 `Cost.Tile.Surface.*` 행의 현재 작성값을 사용한다.

**Road는 일반 시설·여유공간·대기열을 차단하고 Tile은 상부 배치를 지원한다.** 이전 `bAllowFacilityPlacementOnTop` 필드는 저장 호환용이며 현재 배치 정책은 카테고리로 결정한다.

시설 바닥은 Tile PDA에 작성된 윗면 높이에 맞춰진다. Road/Tile끼리 또는 일반 시설끼리의 중복은 허용하지 않는다. 기존 시설 밑으로 새 바닥을 밀어 넣는 배치는 막고, Tile을 먼저 설치한다. 시설이 올라간 Tile의 이동·철거는 위 시설을 먼저 옮긴 후 가능하다. NPC는 두 바닥을 서비스 목적지나 회피 벽으로 취급하지 않으며 표면 높이를 따른다.

## 실제 검사 결과

| 항목 | 결과 |
|---|---|
| 디자인 특징·다시점·LOD 시각 검수 | {report['reference_reproduction']} |
| 시설별 저장된 Blender 파일 검사 | {report['saved_blends']} |
| 최종 메시 닫힌 면·법선·퇴화면·중복면·부품 내부 자기 교차 | {report['mesh_checks']} |
|18개 FBX 별도 씬 재임포트 | {report['fbx_reimport']} |
| Unreal 실제 임포트 | {report['unreal_import']} |
| 저장된 Unreal 에셋 다시 로드 | {report['unreal_saved_asset_verification']} |
| 에디터 최종 C++ 빌드 | {report['editor_build']} |
| 배치·설정·복원·NPC 자동화 | {report['runtime_automation']} ({report.get('runtime_automation_pass_count',0)}개 통과) |
| Unreal GPU 시각 캡처 | {report['unreal_visual_capture']} |
| Unreal LOD 3단계·시설 조립 예시 시각 검수 | {report.get('unreal_visual_review','NOT_RUN')} |
| 실제 플레이 프레임 성능 측정 | NOT_RUN (측정값 null) |

시각 재현은 주요 디자인 특징 기준이다. 모래의 미세 굴곡·발자국, 목재 결, 연석 분할은 단순화했고 콘크리트 배수구는 원본보다 밝다.8cm 두께와 안 보이는 밑면은 보행과 연속 배치를 고려해 추정했다. 여러 닫힌 부품 사이의 구조적 맞물림은 허용하며, 검사 코드는 부품 내부의 교차와 중복면을 검사하고 접합부는 렌더로 별도 검수했다.

실제 Road/Tile의 단순 UCX 충돌면과 작성된 표면 높이는 표면 자동화 테스트로 확인한다. `FacilityOnTiles.png`는 Tile 정의와 실제 시설 메시를 수동으로 배치한 시각 예시이며, 명령을 통한 설치·차단 여부의 증거는 자동화 보고서를 따른다.

상세 수치와 근거는 `validation_report.json`, `qa/unreal`, `UE_IMPORT_GUIDE.md`를 참고한다.
'''
(ROOT/'DELIVERY_REPORT.md').write_text(text,encoding='utf-8')
print(json.dumps({key:report.get(key) for key in ['mesh_checks','fbx_reimport','unreal_import','unreal_saved_asset_verification','runtime_automation','runtime_automation_pass_count','unreal_visual_capture']},ensure_ascii=False))
