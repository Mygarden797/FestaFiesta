# Unreal DataTable CSV

이 폴더의 CSV는 UE5 `Import as DataTable` 로 바로 임포트할 수 있도록 헤더를 구조체 프로퍼티명과 일치시킨 파일이다.
원본 계산은 `data/derived/` 에 있고, 이 폴더는 그 결과를 UE 임포트용으로 재배열한 것뿐이다. 여기서 직접 값을 고치지 않는다.

구조체 이름은 프로젝트 태그 `FF`(FestaFiesta) + Unreal 구조체 필수 접두 `F` 를 합쳐 `FFF_` 로 시작한다.
헤더 파일명은 관례상 필수 접두 글자 하나(F)만 뺀 `FF_` 로 시작한다.

| CSV | 대응 구조체 | 헤더 |
|---|---|---|
| npc_archetypes_v0.csv | FFF_ArchetypeRow | Source/FestaFiesta/DataTables/FF_ArchetypeRow.h |
| companion_type_modifier_v0.csv | FFF_CompanionModifierRow | Source/FestaFiesta/DataTables/FF_CompanionModifierRow.h |
| utility_coefficients_v0.csv | FFF_UtilityCoefficientRow | Source/FestaFiesta/DataTables/FF_UtilityCoefficientRow.h |

재생성: `node Tools/data/build_unreal_datatables.mjs`

아키타입 5종·주축 구조는 2026-08-04 확정됐다. 다만 표시 명칭(`ArchetypeLabelDraft`, `CompanionLabelDraft`)은 UI 담당자 논의 전까지 가칭이고, 활동→카테고리 매핑과 카테고리 delta 수치도 여전히 `assumption`/`draft_assumption`이다.
