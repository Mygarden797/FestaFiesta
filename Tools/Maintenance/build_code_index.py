# build_code_index.py
# 역할: 최초 생성자를 확인한 작업 목록에서 파일의 역할·주요 진입점 문서를 만든다.
"""게임 코드나 에셋을 실행하지 않고 유지보수용 Markdown 목록만 갱신한다."""
from __future__ import annotations

import argparse
import ast
import json
from collections import defaultdict
from pathlib import Path
import re

PROJECT = Path(__file__).resolve().parents[2]
CODE_EXTENSIONS = {'.h', '.cpp', '.cs', '.py', '.js', '.ps1', '.psm1', '.ini'}


def source_paths():
    """최초 생성자를 확인한 명시적 목록만 읽는다. 공동 수정 이력은 소유권으로 쓰지 않는다."""
    manifest=json.loads((PROJECT/'Collaboration/Refactor_Ownership.json').read_text(encoding='utf-8'))
    paths={row['path'] for row in manifest['files'] if row['scope'] in {'owned','new_support'}}
    return sorted(p for p in paths if p and (PROJECT/p).is_file() and Path(p).suffix in CODE_EXTENSIONS
                  and '/Intermediate/' not in p and '/Binaries/' not in p)


def role_of(text, filename):
    """파일 앞의 한국어 역할 설명을 우선하고, 기존 요약 주석을 보조로 사용한다."""
    comments=[]
    for line in text.splitlines()[:18]:
        line=line.strip()
        if line.startswith(('//','#',';')) and not line.startswith(('#include','#pragma','#!')):
            value=line.lstrip('/#; ').strip()
            if value.startswith('역할:'): return value.removeprefix('역할:').strip()
            if filename not in value and 'Copyright' not in value and 'coding:' not in value:
                comments.append(value)
    return comments[0] if comments else '파일의 선언과 실행 안내를 확인한다.'


def entrypoints(text, extension):
    """탐색용 함수 이름을 추출한다. 함수 의미와 사용 조건은 원본 주석이 기준이다."""
    if extension=='.py':
        tree=ast.parse(text)
        names=[node.name for node in tree.body if isinstance(node,(ast.FunctionDef,ast.AsyncFunctionDef,ast.ClassDef))]
    elif extension in {'.ps1','.psm1'}:
        names=re.findall(r'^function\s+([\w-]+)',text,re.M)
    else:
        names=re.findall(r'\b(?:[A-Z]\w*)::(\w+)\s*\(',text)
    boilerplate={'StaticClass','Get','GetDefault','Initialize','Deinitialize','StaticStruct','ToString'}
    return list(dict.fromkeys(name for name in names if name not in boilerplate))[:5]


def group_of(path):
    """선언/구현 파일을 같은 기능 폴더 아래 모은다."""
    parts=path.split('/')
    if path.startswith(('Source/FestaFiesta/Public/','Source/FestaFiesta/Private/')):
        return '게임 / '+parts[3]
    if path.startswith('Plugins/'): return '플러그인 / '+parts[1]
    if path.startswith('Tools/'): return '개발 도구 / '+parts[1]
    return parts[0]


def main():
    """현재 존재하는 파일만 목록화하여 삭제된 기능이 새 작업 안내에 다시 나타나지 않게 한다."""
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,default=PROJECT/'Collaboration/Refactor_Code_Index_KO.md')
    args=parser.parse_args()
    groups=defaultdict(list)
    for path in source_paths(): groups[group_of(path)].append(path)
    lines=['# 담당 코드 파일별 찾아보기','',
           '각 행은 현재 존재하는 파일의 역할과 탐색할 함수 이름이다. 함수의 입력·실패 조건은 파일 안 설명을 따른다.',
           'Git의 이름 변경 추적과 최초 추가 커밋에서 생성자가 HanKwangHoon/Asera인 파일만 포함한다. 이번에 분리한 보조 파일은 소유 코드의 유지보수를 위한 신규 파일로 따로 기록했다.',
           '재생성: `python Tools/Maintenance/build_code_index.py`. 미니게임 완전 삭제는 보류되어 전용 코드를 원래 상태로 유지하며 이 작업 목록에서 제외한다.','']
    count=0
    for group,paths in sorted(groups.items()):
        lines += ['## '+group, '', '| 파일 | 역할 | 주요 진입점 |','| --- | --- | --- |']
        for path in paths:
            p=Path(path); text=(PROJECT/p).read_text(encoding='utf-8-sig')
            role=role_of(text,p.name).replace('|',' / ')
            entries=', '.join('`'+name+'`' for name in entrypoints(text,p.suffix))
            lines.append(f'| `{path}` | {role} | {entries or "선언·설정·실행 절차"} |')
            count+=1
        lines.append('')
    lines += [f'현재 파일: {count}개. 자동 추출된 함수 이름은 탐색 보조 정보이며 전체 호출 관계를 보장하지 않는다.','']
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text('\n'.join(lines),encoding='utf-8')
    print(f'{count} files documented: {args.output}')


if __name__=='__main__':
    main()
