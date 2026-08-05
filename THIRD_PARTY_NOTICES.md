# FestaFiesta 외부 에셋·크레딧 관리

이 문서는 게임에 포함하거나 포함을 검토 중인 외부 폰트·텍스처·음원·아이콘 등의 출처와 라이선스를 관리하는 원장이다.
실제 배포본의 `크레딧 > 외부 에셋` 화면과 동봉 라이선스 파일은 이 문서를 기준으로 작성한다.

> 이 문서는 라이선스 원문을 대체하지 않는다. 출시 전에는 실제로 사용한 파일의 배포본에 동봉된 라이선스도 다시 확인한다.

## 상태 정의

- `confirmed`: 출처와 라이선스를 확인했고 실제 사용이 확정됨
- `assumption`: 사용할 가능성이 높지만 최종 임포트·적용 전임
- `to_verify`: 파일 버전, 저작권자, 추가 조건 또는 최종 사용 여부를 확인해야 함

## 등록 규칙

외부 에셋은 Unreal에 임포트하기 전에 아래 정보를 기록한다.

1. 에셋 이름과 실제 사용 파일명
2. 제작자·권리자
3. 원본 페이지와 직접 다운로드 주소
4. 라이선스명과 라이선스 원문 주소
5. 다운로드 날짜와 원본 버전 또는 파일 해시
6. 크롭·색보정·포맷 변환·리터칭 등 수정 여부
7. 게임 내 크레딧 문구
8. 배포본에 라이선스 원문을 동봉해야 하는지 여부

작업용 원본과 라이선스 사본은 로컬 `SourceArt/ThirdParty/<제공자>/<에셋>/` 아래에 보관한다.
`SourceArt/`는 Git에서 제외되므로, 실제 배포에 필요한 라이선스 원문은 출시 전에 별도의 추적·패키징 경로에도 복사해야 한다.

## 현재 등록 목록

| 에셋 | 용도 | 상태 | 라이선스 | 게임 내 표기 |
| --- | --- | --- | --- | --- |
| Solar System Scope Planet Textures | 지구·구름·노멀·스페큘러 텍스처 후보 | `to_verify` | CC BY 4.0 | 필요 |
| Pretendard | 한국어·공통 UI 폰트 후보 | `assumption` | SIL OFL 1.1 | 권장, 라이선스 동봉 필요 |
| Michroma | 영문 브랜드 워드마크 폰트 후보 | `assumption` | SIL OFL 1.1 | 권장, 라이선스 동봉 필요 |

---

## Solar System Scope Planet Textures

- 상태: `to_verify` — 라이선스는 확인했으나 실제 사용할 해상도와 파일은 아직 선택하지 않음
- 제작·배포: Solar System Scope / INOVE
- 원본: https://www.solarsystemscope.com/textures/
- 라이선스: Creative Commons Attribution 4.0 International (`CC BY 4.0`)
- 라이선스 원문: https://creativecommons.org/licenses/by/4.0/
- 확인일: 2026-08-05
- 원본 설명: 실제 천체 지도와 NASA의 고도·이미지 데이터를 바탕으로 조정한 행성 텍스처 팩
- 사용 예정 후보:
  - Earth Day Map
  - Earth Night Map
  - Earth Clouds
  - Earth Normal Map
  - Earth Specular Map
- 수정 예정: Unreal용 압축·리사이즈·색보정·머티리얼 변환 가능성 있음

### 지켜야 할 조건

- 적절한 출처를 표시한다.
- 라이선스 링크를 제공한다.
- 수정했다면 수정 사실을 표시한다.
- 원 제작자가 FestaFiesta를 보증하거나 후원하는 것처럼 표현하지 않는다.
- 실제 다운로드 파일이나 압축 파일에 별도 README·라이선스가 있으면 그 조건도 확인한다.

### 권장 크레딧 문구

한국어:

> 행성 텍스처: Solar System Scope(INOVE), CC BY 4.0. FestaFiesta용으로 수정됨.

영어:

> Planet textures by Solar System Scope (INOVE), licensed under CC BY 4.0. Modified for FestaFiesta.

수정하지 않은 원본 파일을 그대로 사용할 경우 마지막 수정 문구는 제거한다.
출처와 라이선스 이름은 크레딧 화면에서 각각 원본 페이지와 라이선스 원문으로 연결한다.

### 임포트 시 채울 항목

- 다운로드 파일명: `to_verify`
- 선택 해상도: `to_verify`
- 다운로드 날짜: `to_verify`
- SHA-256: `to_verify`
- Unreal 에셋 경로: `to_verify`
- 실제 수정 내역: `to_verify`

---

## Pretendard

- 상태: `assumption` — UI 기본 폰트 후보이며 아직 프로젝트에 임포트하지 않음
- 제작자·권리자: Kil Hyung-jin 및 Pretendard가 포함하는 원본 폰트 프로젝트의 권리자들
- 원본: https://github.com/orioncactus/pretendard
- 라이선스: SIL Open Font License 1.1
- 라이선스 원문: https://github.com/orioncactus/pretendard/blob/main/LICENSE
- 확인일: 2026-08-05
- 예정 용도: 한국어 메뉴·본문·수치·옵션·접근성 UI

### 배포 메모

- 폰트를 게임에 번들·임베드하여 배포할 수 있다.
- 배포본에는 원 저작권 고지와 OFL 1.1 라이선스를 함께 포함해야 한다.
- 폰트를 수정할 경우 Reserved Font Name 조건을 별도로 검토한다.

권장 크레딧 문구:

> Pretendard — Copyright (c) 2021 Kil Hyung-jin and contributors. Licensed under the SIL Open Font License 1.1.

### 임포트 시 채울 항목

- 사용 파일·굵기: `to_verify`
- 다운로드 날짜·버전: `to_verify`
- SHA-256: `to_verify`
- Unreal Font Asset 경로: `to_verify`
- 라이선스 동봉 경로: `to_verify`

---

## Michroma

- 상태: `assumption` — 영문 브랜드 폰트 후보이며 아직 프로젝트에 임포트하지 않음
- 제작자·권리자: The Michroma Project Authors
- 원본: https://github.com/google/fonts/tree/main/ofl/michroma
- 라이선스: SIL Open Font License 1.1
- 라이선스 원문: https://github.com/google/fonts/blob/main/ofl/michroma/OFL.txt
- 저작권 고지: Copyright 2011 The Michroma Project Authors
- 확인일: 2026-08-05
- 예정 용도: `FESTA FIESTA` 영문 브랜드 워드마크

### 배포 메모

- 폰트를 게임에 번들·임베드하여 배포할 수 있다.
- 배포본에는 원 저작권 고지와 OFL 1.1 라이선스를 함께 포함해야 한다.
- 폰트를 수정할 경우 Reserved Font Name 조건을 별도로 검토한다.

권장 크레딧 문구:

> Michroma — Copyright 2011 The Michroma Project Authors. Licensed under the SIL Open Font License 1.1.

### 임포트 시 채울 항목

- 사용 파일: `Michroma-Regular.ttf` 예정
- 다운로드 날짜·버전: `to_verify`
- SHA-256: `to_verify`
- Unreal Font Asset 경로: `to_verify`
- 라이선스 동봉 경로: `to_verify`

---

## 새 외부 에셋 등록 템플릿

```md
## 에셋 이름

- 상태: `to_verify`
- 실제 사용 파일:
- 제작자·권리자:
- 원본 페이지:
- 직접 다운로드:
- 라이선스:
- 라이선스 원문:
- 다운로드 날짜·버전:
- SHA-256:
- Unreal 에셋 경로:
- 수정 내역:
- 배포본 라이선스 동봉 경로:

권장 크레딧 문구:

> 작성 예정
```

## 출시 전 체크리스트

- [ ] `to_verify` 상태의 실제 사용 에셋을 모두 `confirmed`로 변경
- [ ] 사용하지 않은 후보 에셋은 목록에서 제거하거나 미사용으로 표시
- [ ] 원본 URL·라이선스 URL 재확인
- [ ] 모든 수정 에셋에 수정 사실 표시
- [ ] OFL·CC 등 필요한 라이선스 원문을 배포본에 포함
- [ ] 옵션의 `크레딧 > 외부 에셋` 화면과 이 문서의 문구 일치 확인
- [ ] 한국어·영어 크레딧 모두 링크 또는 QR/URL 접근 가능 여부 확인
