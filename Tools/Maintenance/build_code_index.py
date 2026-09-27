# 역할: Git 최초 작성 이력과 현재 파일의 설명으로 팀용 코드 찾아보기를 만든다.
"""소유 목록 갱신: python Tools/Maintenance/build_code_index.py --refresh-ownership"""
from __future__ import annotations

import argparse
import ast
from collections import defaultdict
from concurrent.futures import ThreadPoolExecutor
import json
from pathlib import Path
import re
import subprocess

PROJECT = Path(__file__).resolve().parents[2]
MANIFEST = PROJECT / "Collaboration/Refactor_Ownership.json"
AUTHORS = {"Asera", "HanKwangHoon"}
CODE_EXTENSIONS = {".h", ".cpp", ".cs", ".py", ".js", ".ps1", ".psm1", ".ush", ".ini"}


def git(*arguments: str) -> str:
    """셸을 거치지 않고 실행해 공백이 있는 경로와 인자를 그대로 보존한다."""
    return subprocess.run(
        ["git", "-c", "core.quotepath=false", *arguments],
        cwd=PROJECT, check=True, capture_output=True, encoding="utf-8",
    ).stdout


def is_source(path: str) -> bool:
    """엔진이 자동 생성한 파일과 바이너리는 코드 목록에서 제외한다."""
    return (Path(path).suffix in CODE_EXTENSIONS
            and not {"Intermediate", "Binaries"}.intersection(Path(path).parts))


def first_author(path: str) -> tuple[str, str]:
    """이름 변경 이전까지 추적한다. 마지막 수정자를 최초 작성자로 오인하지 않는다."""
    record = git("log", "--follow", "--no-merges", "--diff-filter=A", "-1",
                 "--format=%H%x00%an", "--", path).strip()
    if not record:
        return "", ""
    return tuple(record.split("\0", 1))


def refresh_ownership(manifest: dict) -> dict:
    """이동 대응표로 아직 커밋하지 않은 파일 이동도 원래 작성자와 연결한다."""
    moves = manifest.get("moves", {})
    supports = set(manifest.get("support", []))
    tracked = set(filter(None, git("ls-files", "-z").split("\0")))
    paths = sorted(path for path in tracked if is_source(path))

    def inspect(path: str) -> dict | None:
        destination = moves.get(path, path)
        if not (PROJECT / destination).is_file():
            return None
        # 이동이 커밋된 뒤에는 새 경로의 --follow 이력만 사용한다.
        if path in moves and destination in tracked:
            return None
        commit, author = first_author(path)
        if author not in AUTHORS and path not in moves and destination not in moves.values():
            return None
        return {
            "path": destination,
            "scope": "owned" if author in AUTHORS else "integration",
            "author": author,
            "creation_commit": commit,
        }

    # 읽기 전용 Git 조회만 병렬화한다. 결과 파일은 조회가 모두 끝나면 한 번 쓴다.
    with ThreadPoolExecutor(max_workers=8) as executor:
        files = [row for row in executor.map(inspect, paths) if row is not None]
    recorded = {row["path"] for row in files}
    for path in sorted(supports - recorded):
        if (PROJECT / path).is_file() and is_source(path):
            files.append({"path": path, "scope": "new_support", "author": "", "creation_commit": ""})
    return {
        "schema_version": 1,
        "base_commit": git("rev-parse", "HEAD").strip(),
        "authors": sorted(AUTHORS),
        "moves": moves,
        "support": sorted(supports),
        "files": sorted(files, key=lambda row: row["path"]),
    }


def source_paths(manifest: dict) -> list[str]:
    """공동 모듈 이동에 포함된 타 작성자 파일은 담당 코드 목록과 구분한다."""
    return sorted({row["path"] for row in manifest["files"]
                   if row["scope"] in {"owned", "new_support"}
                   and is_source(row["path"]) and (PROJECT / row["path"]).is_file()})


def role_of(source: str, filename: str) -> str:
    """파일의 한국어 역할 설명을 우선한다. 전처리 코드 행은 일반 주석과 구분한다."""
    comments = []
    for line in source.splitlines()[:24]:
        line = line.strip()
        if not line.startswith(("//", "#", ";")) or line.startswith(("#include", "#pragma", "#!", "#if", "#endif")):
            continue
        value = line.lstrip("/#; ").strip()
        if value.startswith("역할:"):
            return value.removeprefix("역할:").strip()
        if filename not in value and "Copyright" not in value and "coding:" not in value:
            comments.append(value)
    return comments[0] if comments else "파일의 선언과 실행 안내를 확인한다."


def entrypoints(source: str, extension: str) -> list[str]:
    """주요 선언을 찾아 탐색을 돕는다. 이 목록은 전체 호출 관계를 뜻하지 않는다."""
    if extension == ".py":
        tree = ast.parse(source)
        names = [node.name for node in tree.body
                 if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef, ast.ClassDef))]
    elif extension in {".ps1", ".psm1"}:
        names = re.findall(r"^function\s+([\w-]+)", source, re.M)
    else:
        names = re.findall(r"\b(?:[A-Z]\w*)::(\w+)\s*\(", source)
    boilerplate = {"StaticClass", "Get", "GetDefault", "Initialize", "Deinitialize", "StaticStruct", "ToString"}
    return list(dict.fromkeys(name for name in names if name not in boilerplate))[:5]


def group_of(path: str) -> str:
    """런타임·제작 도구·플러그인을 기능별로 묶어 팀의 수정 위치를 안내한다."""
    parts = path.split("/")
    if path.startswith(("Source/FestaFiesta/Public/", "Source/FestaFiesta/Private/")):
        return "게임 / " + parts[3]
    if path.startswith("Source/FestaFiestaEditor/"):
        return "에디터 제작 도구"
    if path.startswith("Plugins/"):
        return "플러그인 / " + parts[1]
    if path.startswith("Tools/"):
        return "개발 도구 / " + parts[1]
    return parts[0]


def main() -> None:
    """현재 목록을 출력한다. 게임, 에디터와 에셋은 실행하거나 수정하지 않는다."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=PROJECT / "Collaboration/Refactor_Code_Index_KO.md")
    parser.add_argument("--refresh-ownership", action="store_true")
    args = parser.parse_args()
    manifest = json.loads(MANIFEST.read_text(encoding="utf-8")) if MANIFEST.is_file() else {}
    if args.refresh_ownership or not manifest.get("files"):
        manifest = refresh_ownership(manifest)
        MANIFEST.parent.mkdir(parents=True, exist_ok=True)
        MANIFEST.write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")

    groups = defaultdict(list)
    for path in source_paths(manifest):
        groups[group_of(path)].append(path)
    lines = ["# 담당 코드 파일별 찾아보기", "",
             "Git 최초 작성자가 Asera/HanKwangHoon인 현재 파일과 이번 정비의 보조 파일이다.",
             "목록은 작성 이력과 탐색 안내이며 모든 코드의 동작을 검증했다는 의미는 아니다.",
             "공동 에디터 모듈 이동에 포함된 타 작성자 파일은 Refactor_Ownership.json에서 integration으로 구분한다.",
             "재생성: python Tools/Maintenance/build_code_index.py. 이력 갱신은 --refresh-ownership을 추가한다.", ""]
    count = 0
    for group, paths in sorted(groups.items()):
        lines += ["## " + group, "", "| 파일 | 역할 | 주요 진입점 |", "| --- | --- | --- |"]
        for path in paths:
            file = Path(path)
            source = (PROJECT / path).read_text(encoding="utf-8-sig")
            role = role_of(source, file.name).replace("|", " / ")
            entries = ", ".join("`" + name + "`" for name in entrypoints(source, file.suffix))
            lines.append(f'| `{path}` | {role} | {entries or "선언·설정·실행 절차"} |')
            count += 1
        lines.append("")
    lines += [f"현재 담당/보조 파일: {count}개.", ""]
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text("\n".join(lines), encoding="utf-8")
    print(f"{count} files documented: {args.output}")


if __name__ == "__main__":
    main()
