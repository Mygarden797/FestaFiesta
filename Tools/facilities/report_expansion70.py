# report_expansion70.py
# 역할: 시설 모델·Data Asset·썸네일 작업에서 관련 도구가 공유하는 설정과 처리 함수를 제공한다.

"""Build an honest Korean delivery report and local gallery for Facility70.

Run with the bundled rich Python (Pillow required), not Unreal or Blender Python.
Partial builds are supported. This is a read-only inventory of production inputs;
it writes only its Markdown, HTML and contact-sheet outputs under Design/Guides.
"""
from __future__ import annotations

import argparse
from collections import Counter
from datetime import datetime
import html
import json
import math
from pathlib import Path
import sys
sys.path.insert(0, str(Path(__file__).resolve().parent))
from expansion70_paths import blend_directory
import re

from PIL import Image, ImageDraw, ImageFont, ImageOps


def read_json(path: Path, fallback):
    if not path.exists():
        return fallback
    return json.loads(path.read_text(encoding="utf-8-sig"))


def local_path(root: Path, value):
    if not value:
        return None
    path = Path(value)
    return path if path.is_absolute() else root / path


def package_path(root: Path, package: str):
    return root / "Content" / (package.removeprefix("/Game/") + ".uasset")


def expected_paths(entry):
    identity, category, slug = entry["id"], entry["category"], entry["slug"]
    base = f"/Game/FestaFiesta/Art/FacilityExpansion70/{category}/{slug}"
    return {
        "definition": f"/Game/FestaFiesta/Building/FF_Data_Building/{category}/PDA_FFZS_{identity}",
        "mesh": f"{base}/SM_FF_{identity}",
        "thumbnail": f"{base}/T_UI_F70_{identity}",
        "rig": f"{base}/BP_F70_{identity}_LightingRig",
    }


def report_evidence(root, kind, catalog):
    """Newer selected failures override older pilot success for the same entry."""
    evidence, sources = {}, []
    by_number = {e["number"]: e["id"] for e in catalog}
    for path in sorted((root / "Saved/FacilityExpansion70").glob(kind + "_*.json"),
                       key=lambda p: (p.stat().st_mtime_ns, p.name)):
        report = read_json(path, {})
        suffix = path.stem[len(kind) + 1:]
        numbers = [int(v) for v in re.findall(r"\d+", suffix)]
        identities = [by_number[n] for n in numbers if n in by_number] if suffix != "all" else list(by_number.values())
        rows = {e["id"]: e for e in report.get("entries", [])}
        status = report.get("status", "UNKNOWN")
        info = {"path": path, "status": status, "selected_count": report.get("selected_count"),
                "entries": len(rows), "failures": report.get("failures", []),
                "engine_version": report.get("engine_version"),
                "modified": datetime.fromtimestamp(path.stat().st_mtime).astimezone().isoformat(timespec="seconds")}
        sources.append(info)
        for identity in set(identities) | set(rows):
            row = rows.get(identity)
            # Failed import transactions can roll back entries, so require run PASS.
            passed = status == "PASS" and row is not None
            if kind == "verify" and row and row.get("native_validation") == "PASS":
                # This row is independently validated even when another row failed.
                passed = True
            evidence[identity] = {"passed": passed, "row": row, "source": path,
                                  "run_status": status, "modified": info["modified"]}
    return evidence, sources


def markdown_link(label, path):
    return f"[{label}](<{Path(path).resolve().as_posix()}>)"


def uri(path):
    return Path(path).resolve().as_uri() if path else ""


def evidence_is_stale(report_path, input_paths):
    # A small grace interval avoids same-commandlet filesystem timestamp noise.
    cutoff = Path(report_path).stat().st_mtime + 2.0
    return any(path and path.exists() and path.stat().st_mtime > cutoff for path in input_paths)


def dimensions(values):
    return " × ".join(f"{float(v):.1f}".removesuffix(".0") for v in values) if values else "—"


def native_run_evidence(root):
    """Summarize actual full-run automation/GPU evidence without inventing PASS."""
    lines = ["", "## Unreal 자동 테스트와 실제 렌더", ""]
    tests_path = root / "Saved/FacilityExpansion70/native-full70-tests_Report/index.json"
    capture_path = root / "Saved/FacilityExpansion70/renders/capture_report.json"
    catalog = read_json(root / "Tools/facilities/expansion70_catalog.json", {}).get("facilities", [])
    owned_assets = list((root / "Content/FestaFiesta/Art/FacilityExpansion70").rglob("*.uasset"))
    owned_assets += [package_path(root, expected_paths(e)["definition"]) for e in catalog]
    for path, label in ((tests_path, "전체 대상 네이티브 자동 테스트"),
                        (capture_path, "Unreal GPU 캡처")):
        if not path.exists():
            lines += [f"- {label}: **기록 대기**. 해당 보고서가 아직 생성되지 않았다."]
            continue
        try:
            record = read_json(path, {})
        except (json.JSONDecodeError, UnicodeError):
            lines += [f"- {label}: **기록 작성 중**. {markdown_link('보고서', path)}"]
            continue
        stale = evidence_is_stale(path, owned_assets)
        if path == tests_path:
            names = ("succeeded", "succeededWithWarnings", "failed", "notRun", "inProcess")
            counts = {key: int(record.get(key, 0)) for key in names}
            total = sum(counts.values())
            success = counts["succeeded"] + counts["succeededWithWarnings"]
            state = ("FAIL" if counts["failed"] else "RUNNING" if counts["inProcess"] else
                     "미완료" if counts["notRun"] or not total or not all(key in record for key in names) else "PASS")
            if state == "PASS" and stale:
                state = "이전 PASS / 에셋 변경 후 재검사 대기"
            lines += [f"- {label}: **{state}** — 성공 {success}/{total}, 실패 {counts['failed']}, "
                      f"미실행 {counts['notRun']}, 진행 중 {counts['inProcess']}. {markdown_link('index.json', path)}"]
            if record.get("tests"):
                lines += ["", "| 자동 테스트 | 결과 |", "|---|---|"]
                for test in record["tests"]:
                    name = test.get("fullTestPath") or test.get("testDisplayName") or "이름 없음"
                    lines.append(f"| `{name}` | {test.get('state', 'UNKNOWN')} |")
                lines.append("")
        else:
            captures = record.get("captures", [])
            actual = [(row, local_path(root, row.get("file"))) for row in captures]
            actual = [(row, image) for row, image in actual if image and image.exists()]
            expected = record.get("expected_captures", "—")
            reported_state = record.get("status", "UNKNOWN")
            state = reported_state
            if reported_state == "PASS" and (len(actual) != len(captures) or expected != len(actual)):
                state = "FAIL (보고서와 실제 PNG 수 불일치)"
            elif reported_state == "PASS" and stale:
                state = "이전 PASS / 에셋 변경 후 재캡처 대기"
            lines += [f"- {label}: **{state}** — 실제 PNG {len(actual)}/{expected}, "
                      f"대상 시설 {record.get('expected_facilities', '—')}종. {markdown_link('캡처 기록', path)}"]
            if actual:
                lines.append("- 실제 Unreal 화면: " + ", ".join(
                    markdown_link(row.get("name") or image.stem, image) for row, image in actual))
            if record.get("error"):
                lines.append("- 캡처 오류: " + " ".join(str(record["error"]).strip().splitlines()[-2:])[:500])
    lines += ["", "자동 테스트 PASS와 GPU 캡처는 각각 해당 검사와 렌더 결과의 근거다. 전체 NPC 이용·배치·이동·철거·저장 복원 플레이 완료를 대신하지 않는다.", ""]
    return lines


def font(size, bold=False):
    paths = ([Path("C:/Windows/Fonts/malgunbd.ttf")] if bold else []) + [
        Path("C:/Windows/Fonts/malgun.ttf"), Path("C:/Windows/Fonts/arial.ttf"),
        Path("/usr/share/fonts/truetype/noto/NotoSansCJK-Regular.ttc"),
        Path("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf")]
    for path in paths:
        if path.exists():
            return ImageFont.truetype(str(path), size)
    return ImageFont.load_default(size=size)


def fit_text(draw, text, xy, width, size=24, fill="#153e42", bold=False):
    while size > 12 and draw.textbbox((0, 0), text, font=font(size, bold))[2] > width:
        size -= 1
    draw.text(xy, text, font=font(size, bold), fill=fill)


def make_sheets(rows, target):
    target.mkdir(parents=True, exist_ok=True)
    pages = []
    # Every numbered card remains visible on partial runs; no invented preview.
    for page in range(math.ceil(len(rows) / 10)):
        sheet = Image.new("RGB", (1800, 3060), "#edf4f2")
        draw = ImageDraw.Draw(sheet)
        draw.text((55, 26), "FESTA FIESTA · 시설 70종", font=font(39, True), fill="#103c42")
        draw.text((56, 83), f"1차 아트 패스 · Blender 사선 미리보기 · {page+1:02d} / 07", font=font(21), fill="#56716e")
        for cell, row in enumerate(rows[page * 10:(page + 1) * 10]):
            left, top = 45 + (cell % 2) * 870, 143 + (cell // 2) * 576
            draw.rounded_rectangle((left, top, left+840, top+548), radius=16, fill="white")
            image_box = (left+22, top+12, left+818, top+410)
            if row["preview"] and row["preview"].exists():
                with Image.open(row["preview"]) as source:
                    image = ImageOps.contain(source.convert("RGBA"), (796, 398), Image.Resampling.LANCZOS)
                    backdrop = Image.new("RGBA", image.size, "#f7faf8")
                    backdrop.alpha_composite(image)
                    sheet.paste(backdrop.convert("RGB"), (image_box[0]+(796-image.width)//2, image_box[1]+(398-image.height)//2))
            else:
                draw.text((left+243, top+187), "미리보기 생성 대기", font=font(31), fill="#819591")
            e = row["entry"]
            fit_text(draw, f'{e["number"]:02d}  {e["name_ko"]}', (left+24, top+428), 790, 28, bold=True)
            fit_text(draw, f'{e["category"]} / {e["subtype"]}', (left+25, top+473), 790, 20, fill="#607874")
            fit_text(draw, f'{row["state_label"]} · 실제 {dimensions(row["authored"])} cm', (left+25, top+508), 790, 18, fill="#607874")
        output = target / f"page_{page+1:02d}.png"
        sheet.save(output, optimize=True)
        pages.append(output)
    return pages


def collect(root, blend_dir):
    catalog = read_json(root / "Tools/facilities/expansion70_catalog.json", {})["facilities"]
    manifest = read_json(root / "SourceArt/FacilityExpansion70/build_manifest.json", {"facilities": []})
    built = {e["id"]: e for e in manifest["facilities"]}
    imported, import_sources = report_evidence(root, "import", catalog)
    verified, verify_sources = report_evidence(root, "verify", catalog)
    shared_materials = list((root / "Content/FestaFiesta/Art/FacilityExpansion70/Materials").glob("*.uasset"))
    rows = []
    for entry in catalog:
        identity = entry["id"]
        build = built.get(identity, {})
        imp, ver = imported.get(identity, {}), verified.get(identity, {})
        paths = expected_paths(entry)
        disk = {k: package_path(root, value).exists() for k, value in paths.items()}
        blend = local_path(root, build.get("blend")) or blend_dir / f'{entry["number"]:02d}_{identity}.blend'
        preview = local_path(root, build.get("preview"))
        fbx = local_path(root, build.get("fbx"))
        inputs = [package_path(root, value) for value in paths.values()] + [fbx, preview] + shared_materials
        if imp.get("passed") and evidence_is_stale(imp["source"], inputs):
            imp = {**imp, "passed": False, "stale": True}
        if ver.get("passed") and evidence_is_stale(ver["source"], inputs):
            ver = {**ver, "passed": False, "stale": True}
        authored = [v * 100 for v in build["bounds_m"]] if build.get("bounds_m") else None
        ver_row, imp_row = ver.get("row") or {}, imp.get("row") or {}
        native = bool(ver.get("passed") and ver_row.get("native_validation") == "PASS")
        imported_ok = bool(imp.get("passed"))
        if native and all(disk[k] for k in ("definition", "mesh", "thumbnail")):
            state, label = "verified", "Unreal 네이티브 검증 PASS"
        elif imported_ok and all(disk[k] for k in ("definition", "mesh", "thumbnail")):
            state, label = "imported", "임포트 PASS / 검증 대기"
        elif build and blend.exists() and fbx and fbx.exists():
            state, label = "built", "Blender 제작 / Unreal 검증 대기"
        else:
            state, label = "pending", "제작 대기"
        if ver.get("stale"):
            label += " · 변경 후 재검증 대기"
        elif ver and not ver.get("passed"):
            label += " · 최신 검증 미통과"
        rows.append(dict(entry=entry, build=build, imp=imp, ver=ver, paths=paths, disk=disk,
                         blend=blend, preview=preview, fbx=fbx, authored=authored, native=native,
                         imported_ok=imported_ok, state=state, state_label=label,
                         definition_dimensions=([*ver_row["footprint_cm"], ver_row["height_cm"]] if ver_row.get("footprint_cm") else
                                                [*imp_row["authored_footprint_cm"], imp_row["authored_height_cm"]] if imp_row.get("authored_footprint_cm") else None)))
    return rows, import_sources + verify_sources


def make_html(rows, output, generated, root):
    cards = []
    for r in rows:
        e, b = r["entry"], r["build"]
        esc = html.escape
        image = (f'<img src="{esc(uri(r["preview"]), quote=True)}" alt="{esc(e["name_ko"])} Blender 사선 미리보기" loading="lazy">'
                 if r["preview"] and r["preview"].exists() else '<div class="placeholder">미리보기 생성 대기</div>')
        links = []
        for label, path in (("Blender 원본", r["blend"]), ("FBX", r["fbx"]), ("사선 PNG", r["preview"])):
            if path and path.exists():
                links.append(f'<a href="{esc(uri(path), quote=True)}">{label}</a>')
        ref = root / e["reference_image"]
        if ref.exists():
            links.append(f'<a href="{esc(uri(ref), quote=True)}">콘셉트 이미지</a>')
        features = ' '.join(e.get("followup_features", []))
        cards.append(f'''<article id="facility-{e['number']:02d}" data-category="{esc(e['category'])}" data-state="{r['state']}" data-search="{esc(e['name_ko']+' '+e['id'],quote=True)}">
{image}<div class="body"><p class="eyebrow">{e['number']:02d} · {esc(e['category'])} / {esc(e['subtype'])}</p>
<h2>{esc(e['name_ko'])}</h2><p class="status">{esc(r['state_label'])}</p>
<p class="description">{esc(e['description_ko'])}</p><dl><dt>계획 상한</dt><dd>{dimensions([*e['footprint_cm'],e['height_cm']])} cm</dd>
<dt>Blender 실제</dt><dd>{dimensions(r['authored'])} cm</dd><dt>PDA 사양</dt><dd>{dimensions(r['definition_dimensions'])} cm</dd>
<dt>형상</dt><dd>{b.get('editable_parts','—')} parts · {b.get('triangles','—')} tris · {b.get('collision_count','—')} hulls</dd></dl>
<nav>{''.join(links)}</nav><details><summary>연결 경로와 후속 구현</summary><code>{esc(r['paths']['definition'])}</code><p>{esc(features)}</p></details></div></article>''')
    categories = sorted({r["entry"]["category"] for r in rows})
    options = ''.join(f'<option value="{html.escape(c)}">{html.escape(c)}</option>' for c in categories)
    document = '''<!doctype html><html lang="ko"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Festa Fiesta 시설 70종 · 1차 아트 패스</title><style>
*{box-sizing:border-box}body{margin:0;background:#eef4f1;color:#173f43;font-family:"Malgun Gothic",system-ui,sans-serif;line-height:1.65}header,main,footer{max-width:1480px;margin:auto;padding:28px}header{padding-top:48px}h1{font-size:clamp(28px,4vw,46px);line-height:1.2;margin:10px 0 20px}h2{font-size:23px;margin:5px 0}.intro{max-width:1050px;color:#4e6866}.toolbar{display:flex;gap:12px;flex-wrap:wrap;padding:20px 0}label{font-size:14px;display:grid;gap:5px}input,select{font:inherit;padding:10px 14px;border:1px solid #b8cdca;border-radius:8px;background:white;min-width:185px}#search{width:min(430px,80vw)}#count{align-self:end;padding-bottom:10px;color:#607874}.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(300px,1fr));gap:24px}article{overflow:hidden;border-radius:16px;background:white;box-shadow:0 4px 20px #164c4010}article[hidden]{display:none}article img,.placeholder{width:100%;height:285px;object-fit:contain;background:#f7faf8}.placeholder{display:grid;place-items:center;color:#80958e}.body{padding:22px}.eyebrow{font-size:12px;letter-spacing:.04em;color:#5d8078;margin:0}.status{font-size:13px;color:#267968}.description{min-height:3.2em;color:#526d67;font-size:14px}dl{display:grid;grid-template-columns:90px 1fr;font-size:12px;gap:6px;margin:18px 0}dt{color:#74918a}dd{margin:0}nav{display:flex;flex-wrap:wrap;gap:8px}a{color:#11746b;text-decoration:none}nav a{padding:6px 10px;border:1px solid #b7d6ca;border-radius:6px;font-size:12px}a:hover{background:#e9f7ee}details{font-size:12px;margin-top:18px;color:#687c77}code{display:block;overflow-wrap:anywhere;margin-top:12px}footer{color:#718780;font-size:12px}.badge{display:inline-block;padding:5px 10px;border-radius:20px;background:#d5e8dd;font-size:12px}
</style><header><span class="badge">LOCAL PRODUCTION GALLERY</span><h1>Festa Fiesta 시설 70종</h1>
<p class="intro">브리핑과 콘셉트 이미지를 바탕으로 제작한 1차 아트 패스입니다. Blender 사선 렌더를 보여 주며, Unreal 실시간 게임 화면이나 최종 플레이 검증을 대신하지 않습니다. 제작 중인 항목은 대기 상태로 표시합니다. 원본 버튼은 이 PC의 로컬 파일을 엽니다.</p>
<div class="toolbar"><label>시설 검색<input id="search" type="search" placeholder="한국어 이름 또는 시설 ID"></label><label>대분류<select id="category"><option value="">전체 분류</option>__OPTIONS__</select></label>
<label>진행 상태<select id="state"><option value="">전체 상태</option><option value="verified">네이티브 검증 PASS</option><option value="imported">임포트 PASS</option><option value="built">Blender 제작</option><option value="pending">제작 대기</option></select></label><span id="count" aria-live="polite"></span></div></header>
<main><div class="grid">__CARDS__</div></main><footer>생성: __GENERATED__ · SourceArt는 Git 제외 경로이며 이 갤러리는 현재 PC의 원본 파일에 연결됩니다. 시설별 가격 키는 기존 소분류 템플릿 정책을 이용합니다. 신규 콘텐츠 균형 조정·유료 프로그램·게임 규칙·VFX·음향·애니메이션은 별도 작업입니다.</footer>
<script>const cards=[...document.querySelectorAll('article')],search=document.querySelector('#search'),category=document.querySelector('#category'),state=document.querySelector('#state');function filter(){let count=0;for(const card of cards){const visible=card.dataset.search.toLowerCase().includes(search.value.trim().toLowerCase())&&(!category.value||card.dataset.category===category.value)&&(!state.value||card.dataset.state===state.value);card.hidden=!visible;if(visible)count++}document.querySelector('#count').textContent=count+' / '+cards.length+' 시설'}for(const el of [search,category,state])el.addEventListener('input',filter);filter();</script></html>'''
    output.write_text(document.replace('__OPTIONS__', options).replace('__CARDS__', '\n'.join(cards)).replace('__GENERATED__', html.escape(generated)), encoding="utf-8")


def make_markdown(rows, sources, pages, output, gallery, root, blend_dir, generated):
    total = len(rows)
    count = lambda predicate: sum(bool(predicate(r)) for r in rows)
    lines = ["# FestaFiesta 시설 70종 — 제작 현황", "", f"생성 시각: {generated}", "",
             "이 문서는 현재 디스크의 제작 산출물과 Unreal 임포트·검증 보고서를 읽어 만든 상태 기록이다. 원본 제작 브리핑을 대체하지 않는다. 현재 아트 단계는 **1차 아트 패스**이며, 최종 외형 승인이나 전체 플레이 완료를 뜻하지 않는다.", "",
             f"대상은 브리핑의 {total}종이다. `.blend` {count(lambda r:r['blend'].exists())}개, FBX {count(lambda r:r['fbx'] and r['fbx'].exists())}개, 사선 미리보기 {count(lambda r:r['preview'] and r['preview'].exists())}개를 확인했다.", "",
             "| 확인 항목 | 현재 수 | 근거 범위 |", "|---|---:|---|",
             f"| 시설 Definition `.uasset` | {count(lambda r:r['disk']['definition'])}/{total} | 예상 시설별 경로의 실제 파일 존재 |",
             f"| Static Mesh `.uasset` | {count(lambda r:r['disk']['mesh'])}/{total} | 예상 시설별 경로의 실제 파일 존재 |",
             f"| 카탈로그 Texture `.uasset` | {count(lambda r:r['disk']['thumbnail'])}/{total} | 예상 시설별 경로의 실제 파일 존재 |",
             f"| 조명 Rig `.uasset` | {count(lambda r:r['disk']['rig'])} | 실제 파일 존재; 모든 시설이 리그를 요구하지는 않음 |",
             f"| 최신 대상별 임포트 PASS | {count(lambda r:r['imported_ok'])}/{total} | 저장 성공한 임포트 보고서 |",
             f"| 네이티브 `IsDefinitionValid` PASS | {count(lambda r:r['native'])}/{total} | 검증 보고서의 시설별 `native_validation=PASS` |", "",
             "파일 수는 Unreal 클래스 판정이나 로드 성공을 대신하지 않는다. 네이티브 PASS는 저장된 ID·Spec·메시·UV·재질·썸네일·이용 데이터·가격 키·조명 설정 등의 읽기 검증 범위다. 라이브 NavMesh·실제 NPC 접근·플레이어 충돌·시설 배치/회전/이동/철거·저장/불러오기를 포함한 전체 게임 플레이는 이 보고서로 통과를 주장하지 않는다.", "",
             "## 원본과 확인 화면", "",
             f"- Blender 편집 원본: {markdown_link(str(blend_dir), blend_dir)}. 파일명은 `번호_시설ID.blend`이다.",
             f"- {markdown_link('70종 검색 가능한 로컬 갤러리', gallery)}", 
             f"- 제작 소스: {markdown_link('SourceArt/FacilityExpansion70', root/'SourceArt/FacilityExpansion70')} — `.gitignore` 대상이므로 별도 원본 보관이 필요하다.",
             f"- 재현 스크립트·카탈로그: {markdown_link('Tools/facilities', root/'Tools/facilities')} — 저장소 관리 대상인 코드와 기록이다.", "",
             "미리보기는 Blender 렌더이며 Unreal 렌더와 구분한다. 원본의 부품별 VIS/COL/GUIDES와 저장된 제작 기록을 함께 확인한다. 아래 이미지에는 번호별 시설 10개씩을 배치했다.", ""]
    for page in pages:
        lines += [f"![{page.stem} 시설 미리보기]({page.resolve().as_posix()})", ""]
    blend_report=root/'Saved/FacilityExpansion70/blend_finalization_report.json'
    if blend_report.exists():
        finalization=json.loads(blend_report.read_text(encoding='utf-8'))
        passed=sum(row.get('status')=='passed' for row in finalization.get('facilities',[]))
        lines += [f"Blender 일반 편집 파일 저장·재개방 검사: **{passed}/70 통과**. " + markdown_link('검증 기록',blend_report), ""]
    lines += ["## 치수와 데이터 정책", "",
              "카탈로그 치수는 초기 계획 상한이다. 일반 시설은 종횡비를 유지해 그 범위에 맞추고 Road/Tile은 400×400×8cm로 맞춘다. 최종 PDA의 XY는 실제 메시 크기를 1cm 셀로 올림하고 높이는 실제 메시 높이를 사용한다. 계획 값·제작된 크기·최종 PDA 값은 아래에 별도로 표시한다. Data Asset 생성 이후 런타임 권위는 해당 `Spec`이다.", "",
              "설치 비용과 관련 외부 키는 기존 동일 소분류 템플릿/경제 테이블 정책을 사용한다. 신규 70종 전용 가격·수익 균형 조정이 완료되었다는 뜻은 아니다. 유료 공연 프로그램 등록, 영상, VFX, 게임 규칙·점수, 음향, 가동 부품·캐릭터 애니메이션은 별도 구현 범위다. 치료·보안·화재·대피·판매 효과도 형상 존재만으로 구현되었다고 보지 않는다.", "",
              ]
    axis_rows = [r for r in rows if r["native"] and
                 (r["ver"].get("row") or {}).get("import_yaw_degrees") ==
                 (0 if r["entry"]["category"] in {"Road", "Tile"} else 180) and
                 (r["ver"].get("row") or {}).get("visual_rotation_degrees") == [0, 0, 0]]
    lines += ["### 전면 방향과 좌표 변환", "",
              "적용 정책은 일반 시설 #1–64의 FBX 임포트 Yaw 180° 보정을 정적 메시 자체에 굽고, PDA `VisualRotation`은 `(0,0,0)`으로 유지하는 것이다. 일반 시설의 Blender 원본 `(x,y,z)` m는 Unreal 메시 `(-100x,+100y,+100z)` cm에 대응하며 고객 전면 +Y를 유지한다. 조명 위치도 같은 변환으로 맞춘다.", "",
              "Road/Tile #65–70은 임포트 Yaw 0°를 사용해 진행 +X를 유지한다. 이 경우 Blender `(x,y,z)` m는 Unreal `(+100x,-100y,+100z)` cm에 대응한다. FBX 좌표계 변환과 제작 전면 규칙은 별개이며, 원점·크기 검사만으로 전후 방향이나 글자 좌우 반전까지 확인했다고 보지 않는다.", "",
              f"현재 에셋과 일치하는 방향 정책 읽기 검증: **{len(axis_rows)}/{total}종**. " +
              ("전체 시설의 임포트 Yaw·메타데이터·PDA 회전 검사를 통과했다." if len(axis_rows) == total else "남은 시설은 방향 보정 후 최신 읽기 검증을 기다린다."), "",
              "## 시설별 기록", "",
              "`tris`는 출력 본체 삼각형 수, `parts`는 편집 가능한 시각 부품 수, `hulls`는 제작 충돌체 수다. 빈 값은 확인 자료가 아직 없다는 뜻이다.", "",
              "| 번호 | 시설 / 분류 | 계획 상한 cm | Blender 실제 cm | PDA cm | parts / tris / hulls | 상태 / 원본 |",
              "|---:|---|---|---|---|---:|---|"]
    for r in rows:
        e, b = r["entry"], r["build"]
        source_link = markdown_link("blend", r["blend"]) if r["blend"].exists() else "원본 대기"
        lines.append(f"| {e['number']} | {e['name_ko']}<br>{e['category']}/{e['subtype']} | {dimensions([*e['footprint_cm'],e['height_cm']])} | {dimensions(r['authored'])} | {dimensions(r['definition_dimensions'])} | {b.get('editable_parts','—')} / {b.get('triangles','—')} / {b.get('collision_count','—')} | {r['state_label']}<br>{source_link} |")
    lines += ["", "## 검증 증거", "", "다음은 읽은 보고서 전체 목록이다. 과거 실패는 이후 성공으로 해결될 수 있으나 삭제해 숨기지 않는다. 최신 대상별 판정은 위 표에 반영한다.", "",
              "| 보고서 | 수정 시각 | 실행 상태 | 포함 시설 수 |", "|---|---|---|---:|"]
    for source in sources:
        lines.append(f"| {markdown_link(source['path'].name, source['path'])} | {source['modified']} | {source['status']} | {source['entries']} |")
    if not sources:
        lines.append("| 아직 없음 | — | 대기 | 0 |")
    failed_sources = [s for s in sources if s["failures"]]
    if failed_sources:
        lines += ["", "보고된 오류(과거 실행 포함):", ""]
        for source in failed_sources:
            for failure in source["failures"]:
                # Preserve useful tail without embedding huge Python traceback blocks.
                brief = " ".join(str(failure).strip().splitlines()[-2:])
                lines.append(f"- `{source['path'].name}`: {brief[:800]}")
    lines += native_run_evidence(root)
    lines += ["", "## 시설별 남은 연결", ""]
    for r in rows:
        e = r["entry"]
        lines.append(f"- **{e['number']:02d} {e['name_ko']}**: {' '.join(e.get('followup_features', []))}")
    lines += ["", "이 문서는 `Tools/facilities/report_expansion70.py`를 재실행해 갱신한다. 보고서 생성은 모델·Data Asset·경제 데이터를 수정하지 않는다.", ""]
    output.write_text("\n".join(lines), encoding="utf-8")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument("--blend-dir", type=Path, default=blend_directory())
    parser.add_argument("--output-dir", type=Path)
    args = parser.parse_args()
    root = args.root.resolve()
    target = (args.output_dir or root / "Design/Guides").resolve()
    target.mkdir(parents=True, exist_ok=True)
    rows, sources = collect(root, args.blend_dir)
    generated = datetime.now().astimezone().isoformat(timespec="seconds")
    pages = make_sheets(rows, target / "Facility70_Preview")
    gallery = target / "Facility70_Gallery.html"
    report = target / "Facility70_ProductionStatus_KO.md"
    make_html(rows, gallery, generated, root)
    make_markdown(rows, sources, pages, report, gallery, root, args.blend_dir, generated)
    print(json.dumps({"facilities": len(rows), "states": dict(Counter(r['state'] for r in rows)),
                      "report": str(report), "gallery": str(gallery), "contact_sheets": len(pages)}, ensure_ascii=False))


if __name__ == "__main__":
    main()
