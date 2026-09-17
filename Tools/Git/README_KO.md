# Windows + Git LFS pull 사고 방지 (.uasset unlink 실패)

## 증상

`git pull` 이 이렇게 끝난다.

```
Updating 48d77992..af42f098
error: unable to unlink old 'Content/FestaFiesta/Data/Economy/DT_FacilityCosts.uasset': Invalid argument
Completed with errors, see above.
```

## 원인

Windows 에서 어떤 프로세스가 파일에 **메모리 매핑 섹션**을 열어두면 `DeleteFileW` 가
`ERROR_USER_MAPPED_FILE(1224)` 로 실패한다. CRT 가 1224 를 매핑 테이블에서 못 찾아
`EINVAL` 로 떨어뜨리고, git 은 그걸 `Invalid argument` 로 출력한다.

git 의 unlink 재시도 로직은 *공유 위반* 과 *접근 거부* 만 재시도 대상으로 본다.
이 오류는 거기에 없어서 **재시도 없이 즉시 실패**한다.

파일을 매핑하는 범인은 사후에 특정할 수 없지만 현실적으로 이 셋 중 하나다.

- 언리얼 에디터 (`.uasset` 을 메모리 매핑해서 읽는다) — 1순위
- Git GUI(Sourcetree, Fork 등)의 diff/미리보기 창에 그 `.uasset` 이 선택돼 있음
- 실시간 백신 / Windows 검색 인덱서가 방금 smudge 된 LFS 파일을 스캔 중

## 왜 그냥 다시 pull 하면 안 되는가

이게 진짜 위험한 부분이다. git 은 **워킹 트리를 이미 다 덮어쓴 뒤에** fast-forward 를
취소한다. 그래서 저장소가 이렇게 갈라진다.

| | 상태 |
|---|---|
| 워킹 트리 | 새 커밋 (파일은 다 갱신됨) |
| HEAD / 인덱스 | 옛 커밋 (전혀 안 움직임) |
| unlink 실패한 파일 | 옛 버전 그대로 |

이 상태에서 `git status` 를 보면 새로 들어온 파일 수천 개가 untracked 로, 변경/삭제분이
로컬 수정으로 보인다. **내가 뭘 잔뜩 고쳐놓은 것처럼 보이지만 전부 pull 결과물이다.**

모르고 이 상태에서 브랜치를 옮겨 커밋하면 **되돌려진 에셋이 커밋된다.**
`DT_FacilityCosts.uasset` 처럼 여러 사람이 만지는 DataTable 이 조용히 옛 버전으로
롤백된 채 push 될 수 있다.

## 예방

### 1. pull 은 이 스크립트로

```bash
powershell -ExecutionPolicy Bypass -File Tools\Git\Invoke-SafePull.ps1
```

- 언리얼 에디터가 떠 있으면 pull 을 **막는다**
- 그래도 오류가 나면 반쯤 적용된 상태를 **자동 복구**한다
- 워킹 트리는 이미 목표 상태이므로 `reset --hard` 로 수천 개 LFS 파일을 다시 쓰지 않고,
  HEAD/인덱스만 전진시킨 뒤 실패한 파일만 되돌린다

원하면 alias 로 등록해 둔다.

```bash
git config alias.safe-pull "!powershell -ExecutionPolicy Bypass -File Tools/Git/Invoke-SafePull.ps1"
```

### 2. 팀 규칙 세 줄

- **pull / 브랜치 전환 전에 언리얼 에디터를 완전히 종료한다.** 이게 8할이다.
- Git GUI 에서 `.uasset` / `.umap` 을 **선택한 상태로 pull 하지 않는다.** 커밋 목록에서
  다른 항목(또는 아무것도)을 선택하고 pull 한다. 탐색기 미리보기 창도 닫는다.
- pull 이 `Completed with errors` 로 끝나면 **다시 pull 하지 말고** 아래 판별 절차를 먼저 본다.

### 3. 백신 예외 (각자 1회, 관리자 권한)

대량 LFS smudge 직후의 일시적 락이 사라진다.

```powershell
Add-MpPreference -ExclusionPath 'C:\GitHub\FestaTeam\8th-Team13-Final-Project'
```

경로는 각자 클론 위치로 바꿀 것.

## 이미 당했는지 판별하는 법

```bash
git status -sb
```

`## Dev...origin/Dev [behind N]` 인데 동시에 로컬 수정/untracked 가 잔뜩 보이면 이 상태다.
확인 사살은 이렇게 한다. 어떤 파일 하나를 골라

```bash
git hash-object -- <파일경로>
git rev-parse origin/Dev:<파일경로>
```

두 해시가 **같으면** 그 파일은 내 수정이 아니라 **pull 이 쓴 것**이다.

## 수동 복구

워킹 트리가 이미 목표 상태이므로 HEAD 와 인덱스만 옮기면 된다.

```bash
git reset origin/Dev
```

`--hard` 가 아니라 기본(mixed) 이다. 워킹 트리를 건드리지 않으므로 수천 개 LFS 파일을
다시 내려받지 않는다. 그다음 남은 파일만 되돌린다.

```bash
git restore --worktree -- :/
```

`git restore` 가 또 unlink 로 실패하면 파일을 잡고 있는 프로그램이 아직 살아 있는 것이다.
언리얼 에디터 / Git GUI / 탐색기 미리보기를 닫고 다시 실행한다.

마지막으로 검증한다.

```bash
git status --porcelain
git rev-parse HEAD origin/Dev
```

`status` 가 비고 두 해시가 같으면 정상이다.

## 이미 되돌려진 에셋을 커밋해 버렸다면

그 커밋이 push 되기 전이라면 해당 파일만 올바른 버전으로 되살려 커밋한다.

```bash
git checkout <정상이던_커밋> -- <파일경로>
git commit -m "[fix] pull 실패로 롤백된 에셋 복원"
```

이미 push 됐다면 팀에 알리고 같은 방식으로 정정 커밋을 올린다. `.uasset` 은 병합이
불가능하므로(`merge=lfs`, `-text`) 조용히 넘어가면 다른 사람 작업이 사라진다.
