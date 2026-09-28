"""geoBoundaries 한국 ADM1 원본에서 재현 가능한 국가 선택 도형·마스크를 만든다.

원본: geoBoundaries KOR-ADM1-68945753 (2021, 2023-12-12 빌드).
외부 GIS 라이브러리 없이 정확히 일치하는 공유 선분만 제거한다. 다른 데이터의
교차점이나 어긋난 경계를 보정하는 범용 polygon union이 아니므로 원본 해시를 고정한다.
원본 좌표를 단순화하거나 생성형 이미지로 대체하지 않는다.

실행: python Tools/ui/atlas_regions/prepare_korea.py --source <원본.geojson>
첫 생성 뒤에는 SourceArt의 원본 사본을 기본 입력으로 사용하므로 --source를 생략한다.
Python 3.10+, NumPy, Pillow가 필요하다. Unreal이나 기존 에셋을 열거나 수정하지 않는다.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import platform
import shutil
from collections import Counter, defaultdict
from pathlib import Path

import numpy as np
import PIL
from PIL import Image, ImageChops, ImageDraw, ImageFilter


SOURCE_SHA256 = "6683cd1ad991676d96493fd0aae068215426497ccf82c8b0eb5683cad341cddc"
SIDECARS = {
    "geoBoundaries-KOR-ADM1-metaData.json": "0ca1426407d6acc6b9bbc258f3defb65f17a37b424abc93d629a16af189549e4",
    "CITATION-AND-USE-geoBoundaries.txt": "f6ea7572bea6036c4cdcacf8c0ca7bf09098d4e600d19546d7432533e9a290d5",
}
SOURCE_NAME = "geoBoundaries-KOR-ADM1.geojson"
TEXTURE_NAME = "T_FF_AtlasRegion_KR_1024.png"
REPO_ROOT = Path(__file__).resolve().parents[3]
DEFAULT_OUTPUT = REPO_ROOT / "SourceArt/UI/Atlas/CountryRegions/KR"


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def write_json(path: Path, value: object) -> None:
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2, allow_nan=False) + "\n", encoding="utf-8")


def area(ring: list) -> float:
    return sum(a[0] * b[1] - b[0] * a[1] for a, b in zip(ring, ring[1:])) * 0.5


def canonical_ring(ring: list, positive: bool) -> list:
    points = [tuple(p) for p in ring[:-1]]
    if (area(ring) > 0) != positive:
        points.reverse()
    start = min(range(len(points)), key=lambda index: points[index])
    points = points[start:] + points[:start]
    return points + [points[0]]


def on_segment(point: tuple, a: tuple, b: tuple) -> bool:
    dx, dy = b[0] - a[0], b[1] - a[1]
    cross = (point[0] - a[0]) * dy - (point[1] - a[1]) * dx
    return (abs(cross) <= 1e-11 * max(1.0, abs(dx), abs(dy))
            and min(a[0], b[0]) - 1e-11 <= point[0] <= max(a[0], b[0]) + 1e-11
            and min(a[1], b[1]) - 1e-11 <= point[1] <= max(a[1], b[1]) + 1e-11)


def in_ring(point: tuple, ring: list) -> bool:
    """경계점을 포함하는 even-odd 판정. 경위도 순서를 유지한다."""
    x, y = point
    inside = False
    for a, b in zip(ring, ring[1:]):
        if on_segment(point, a, b):
            return True
        if (a[1] > y) != (b[1] > y) and x < (b[0] - a[0]) * (y - a[1]) / (b[1] - a[1]) + a[0]:
            inside = not inside
    return inside


def contains(point: tuple, polygons: list) -> bool:
    # 외곽 경계는 포함하고 구멍의 내부·경계는 제외한다.
    return any(in_ring(point, poly["outer"]) and not any(in_ring(point, h) for h in poly["holes"])
               for poly in polygons)


def source_polygons(features: list) -> list:
    polygons = []
    for feature in features:
        geometry = feature["geometry"]
        if geometry["type"] not in ("Polygon", "MultiPolygon"):
            raise ValueError("Polygon/MultiPolygon 이외의 원본은 지원하지 않습니다.")
        coordinates = ([geometry["coordinates"]] if geometry["type"] == "Polygon" else geometry["coordinates"])
        for rings in coordinates:
            for ring in rings:
                if len(ring) < 4 or ring[0] != ring[-1] or not all(len(p) == 2 and all(map(math.isfinite, p)) for p in ring):
                    raise ValueError("원본에 열린 링 또는 잘못된 경위도가 있습니다.")
            polygons.append({"outer": canonical_ring(rings[0], True),
                             "holes": [canonical_ring(r, False) for r in rings[1:]]})
    return polygons


def dissolve(polygons: list) -> tuple[list, dict]:
    """방향을 정규화한 후 공유 선분을 상쇄하고 닫힌 외곽/구멍 링을 재구성한다."""
    edges = defaultdict(list)
    for poly in polygons:
        for ring in [poly["outer"], *poly["holes"]]:
            for a, b in zip(ring, ring[1:]):
                if a == b:
                    raise ValueError("길이가 0인 선분은 지원하지 않습니다.")
                key = tuple(sorted((a, b)))
                edges[key].append(1 if (a, b) == key else -1)
    for signs in edges.values():
        if len(signs) > 2 or (len(signs) == 2 and sum(signs) != 0):
            raise ValueError("겹치거나 방향이 잘못된 공유 경계입니다. 원본을 확인하세요.")
    boundary = {edge for edge, signs in edges.items() if len(signs) == 1}
    adjacency = defaultdict(list)
    for a, b in boundary:
        adjacency[a].append(b)
        adjacency[b].append(a)
    if any(len(neighbors) != 2 for neighbors in adjacency.values()):
        raise ValueError("공유 경계 제거 후 열린 선 또는 갈림길이 생겼습니다.")
    remaining = set(boundary)
    rings = []
    while remaining:
        a, b = min(remaining)
        ring = [a, b]
        remaining.remove((a, b))
        while ring[-1] != ring[0]:
            previous, current = ring[-2:]
            next_point = next(point for point in adjacency[current] if point != previous)
            edge = tuple(sorted((current, next_point)))
            if edge not in remaining:
                raise ValueError("경계 링을 한 번씩 방문할 수 없습니다.")
            remaining.remove(edge)
            ring.append(next_point)
        if len(ring) < 4 or area(ring) == 0:
            raise ValueError("면적이 없는 경계 링입니다.")
        rings.append(ring)
    # 큰 링부터 배치해 중첩 깊이의 짝수는 외곽, 홀수는 구멍으로 해석한다.
    rings.sort(key=lambda ring: (-abs(area(ring)), min(ring)))
    parents, depths, result, owner = {}, {}, [], {}
    for index, ring in enumerate(rings):
        containers = [j for j in range(index) if in_ring(ring[0], rings[j])]
        parents[index] = min(containers, key=lambda j: abs(area(rings[j]))) if containers else None
        depths[index] = depths[parents[index]] + 1 if containers else 0
        if depths[index] % 2 == 0:
            owner[index] = len(result)
            result.append({"outer": canonical_ring(ring, True), "holes": []})
        else:
            result[owner[parents[index]]]["holes"].append(canonical_ring(ring, False))
    return result, {"source_unique_edges": len(edges),
                    "removed_shared_edges": sum(len(signs) == 2 for signs in edges.values()),
                    "boundary_edges": len(boundary), "boundary_rings": len(rings)}


def raster(polygons: list, bounds: list, size: int, supersampling: int, boundary_width: float, halo_width: float):
    west, south, east, north = bounds
    pixels = size * supersampling
    pixel_degrees = math.sqrt((east - west) * (north - south)) / pixels
    line_width = max(1, round(boundary_width / pixel_degrees))

    def xy(ring):
        # 텍셀 중심 i+.5가 해당 경위도다. ImageDraw의 중심 좌표에 맞춰 .5를 뺀다.
        return [((p[0] - west) / (east - west) * pixels - .5,
                 (north - p[1]) / (north - south) * pixels - .5) for p in ring]

    fill = Image.new("L", (pixels, pixels))
    border = Image.new("L", (pixels, pixels))
    border_draw = ImageDraw.Draw(border)
    for poly in polygons:
        part = Image.new("L", fill.size)
        draw = ImageDraw.Draw(part)
        draw.polygon(xy(poly["outer"]), fill=255)
        for hole in poly["holes"]:
            draw.polygon(xy(hole), fill=0)
        fill = ImageChops.lighter(fill, part)
        for ring in [poly["outer"], *poly["holes"]]:
            border_draw.line(xy(ring), fill=255, width=line_width, joint="curve")
    blur = fill.filter(ImageFilter.GaussianBlur(halo_width / pixel_degrees))
    halo = ImageChops.subtract(blur, fill)
    channels = [channel.resize((size, size), Image.Resampling.LANCZOS) for channel in (fill, border, halo)]
    rgba = Image.merge("RGBA", (*channels, Image.new("L", (size, size), 255)))
    return rgba, {"boundary_total_width_degrees_requested": boundary_width,
                  "boundary_supersampled_pixels": line_width,
                  "boundary_total_width_output_pixels": line_width / supersampling,
                  "boundary_total_width_lon_degrees": line_width / pixels * (east - west),
                  "boundary_total_width_lat_degrees": line_width / pixels * (north - south),
                  "halo_gaussian_sigma_degrees": halo_width,
                  "supersampling": supersampling}


def validate(original: list, polygons: list, bounds: list, rgba: Image.Image) -> dict:
    points = [
        ("Seoul", 126.9780, 37.5665, True), ("Busan", 129.0756, 35.1796, True),
        ("Jeju", 126.5312, 33.4996, True), ("Ulleung", 130.87, 37.50, True),
        ("Pyongyang", 125.7625, 39.0392, False), ("Tsushima", 129.3305, 34.2027, False),
        ("WestSea", 125.7, 35.5, False), ("EastSea", 130.2, 36.5, False),
        ("JejuNorthSea", 126.6, 33.85, False),
    ]
    point_checks = []
    for name, lon, lat, expected in points:
        source = contains((lon, lat), original)
        actual = contains((lon, lat), polygons)
        if source != expected or actual != expected:
            raise ValueError(f"지점 검증 실패: {name}, source={source}, result={actual}")
        west, south, east, north = bounds
        u, v = (lon - west) / (east - west), (north - lat) / (north - south)
        red = rgba.getpixel((int(u * rgba.width), int(v * rgba.height)))[0] if 0 <= u < 1 and 0 <= v < 1 else 0
        if (red >= 128) != expected:
            raise ValueError(f"래스터 지점 검증 실패: {name}, R={red}")
        point_checks.append({"name": name, "coordinate": [lon, lat], "expected_inside": expected,
                             "source_inside": source, "result_inside": actual, "mask_r": red})
    # 공유 경계 부근 및 해안 바깥을 포함하는 결정적 격자 비교.
    mismatches, grid_count = 0, 0
    for lat in np.linspace(bounds[1], bounds[3], 65):
        for lon in np.linspace(bounds[0], bounds[2], 65):
            grid_count += 1
            mismatches += contains((lon, lat), original) != contains((lon, lat), polygons)
    if mismatches:
        raise ValueError(f"원본 union과 결과의 격자 비교 불일치: {mismatches}")
    source_area = sum(abs(area(p["outer"])) - sum(abs(area(h)) for h in p["holes"]) for p in original)
    result_area = sum(abs(area(p["outer"])) - sum(abs(area(h)) for h in p["holes"]) for p in polygons)
    if not math.isclose(source_area, result_area, rel_tol=1e-10, abs_tol=1e-10):
        raise ValueError("행정구역 합계와 국가 도형 면적이 다릅니다.")
    # 합성 도넛+떨어진 섬으로 구멍과 다중 폴리곤의 직렬화·래스터 의미를 검증한다.
    fixtures = [{"outer": canonical_ring([(0, 0), (4, 0), (4, 4), (0, 4), (0, 0)], True),
                 "holes": [canonical_ring([(1, 1), (1, 3), (3, 3), (3, 1), (1, 1)], False)]},
                {"outer": canonical_ring([(5, 0), (6, 0), (6, 1), (5, 1), (5, 0)], True), "holes": []}]
    rebuilt, _ = dissolve(fixtures)
    if len(rebuilt) != 2 or sum(len(p["holes"]) for p in rebuilt) != 1:
        raise ValueError("합성 구멍/다중 폴리곤 재구성 실패")
    fixture_image, _ = raster(rebuilt, [-1, -1, 7, 5], 128, 2, .04, .02)
    for point, expected in [((.5, .5), True), ((2, 2), False), ((5.5, .5), True), ((4.5, .5), False)]:
        x, y = int((point[0] + 1) / 8 * 128), int((5 - point[1]) / 6 * 128)
        if contains(point, rebuilt) != expected or (fixture_image.getpixel((x, y))[0] >= 128) != expected:
            raise ValueError("합성 구멍/다중 폴리곤 포함 판정 실패")
    rgb = np.asarray(rgba)[:, :, :3]
    if np.any(rgb[0]) or np.any(rgb[-1]) or np.any(rgb[:, 0]) or np.any(rgb[:, -1]):
        raise ValueError("영역 마스크가 이미지 가장자리에 닿았습니다. margin을 늘리세요.")
    if rgba.getchannel("A").getextrema() != (255, 255):
        raise ValueError("알파는 전체 255여야 합니다.")
    return {"status": "passed", "point_checks": point_checks, "source_union_grid_samples": grid_count,
            "source_union_grid_mismatches": mismatches, "source_area_square_degrees": source_area,
            "result_area_square_degrees": result_area, "synthetic_hole_and_multipolygon": "passed",
            "texture_edge_rgb_zero": True, "alpha_constant_255": True,
            "ring_orientation": "outer_ccw_holes_cw", "simplification_degrees": 0.0,
            "geography_note": "원본의 제주·울릉도·도서를 보존하며 원본에 없는 독도는 보충하지 않음"}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, default=DEFAULT_OUTPUT / "source" / SOURCE_NAME)
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    parser.add_argument("--margin-degrees", type=float, default=.15)
    parser.add_argument("--boundary-width-degrees", type=float, default=.02)
    parser.add_argument("--halo-width-degrees", type=float, default=.035)
    parser.add_argument("--supersampling", type=int, default=4)
    args = parser.parse_args()
    if not (0 < args.margin_degrees <= 2 and 0 < args.boundary_width_degrees <= .2
            and 0 < args.halo_width_degrees <= .2 and 1 <= args.supersampling <= 8):
        parser.error("margin/선폭/후광 폭/초과표본화 값이 허용 범위를 벗어났습니다.")
    if sha256(args.source) != SOURCE_SHA256:
        raise ValueError("고정 geoBoundaries 원본 SHA-256과 다릅니다. 다른 버전을 조용히 변환하지 않습니다.")
    for name, digest in SIDECARS.items():
        if sha256(args.source.parent / name) != digest:
            raise ValueError(f"원본 메타데이터/인용문 SHA-256 불일치: {name}")
    source = json.loads(args.source.read_text(encoding="utf-8"))
    if len(source["features"]) != 17 or source["crs"]["properties"]["name"] != "urn:ogc:def:crs:OGC:1.3:CRS84":
        raise ValueError("17개 ADM1 / CRS84 원본이 아닙니다.")
    original = source_polygons(source["features"])
    polygons, topology = dissolve(original)
    all_points = [point for poly in polygons for point in poly["outer"]]
    extent = [min(p[0] for p in all_points), min(p[1] for p in all_points),
              max(p[0] for p in all_points), max(p[1] for p in all_points)]
    bounds = [math.floor((extent[i] - args.margin_degrees) * 10) / 10 for i in (0, 1)]
    bounds += [math.ceil((extent[i] + args.margin_degrees) * 10) / 10 for i in (2, 3)]
    rgba, raster_info = raster(polygons, bounds, 1024, args.supersampling,
                               args.boundary_width_degrees, args.halo_width_degrees)
    report = validate(original, polygons, bounds, rgba)
    metadata = {"boundary_id": "KOR-ADM1-68945753", "boundary_year": "2021", "build_date": "2023-12-12",
                "source": "geoBoundaries / William & Mary geoLab", "source_sha256": SOURCE_SHA256,
                "license": "CC BY 4.0", "license_url": "https://creativecommons.org/licenses/by/4.0/",
                "source_url": "https://www.geoboundaries.org", "underlying_source": "Natural Earth",
                "underlying_source_license": "Public Domain", "simplification_degrees": 0.0}
    region = {"schema_version": 1, "country_id": "KR", "coordinate_system": "WGS84/CRS84",
              "bounds": bounds, "geometry_bounds": extent, "polygons": polygons,
              "texture": {"file": TEXTURE_NAME, "size": [1024, 1024], "north_v": 0,
                          "channels": {"R": "interior", "G": "boundary", "B": "soft_outer_halo", "A": "255"},
                          "uv": "u=(lon-west)/(east-west); v=(north-lat)/(north-south)",
                          "srgb": False, **raster_info}, "metadata": metadata}
    args.output.mkdir(parents=True, exist_ok=True)
    source_dir = args.output / "source"
    source_dir.mkdir(exist_ok=True)
    for name in [SOURCE_NAME, *SIDECARS]:
        src = args.source if name == SOURCE_NAME else args.source.parent / name
        dst = source_dir / name
        if src.resolve() != dst.resolve():
            shutil.copyfile(src, dst)
    write_json(args.output / "KR.region.json", region)
    rgba.save(args.output / TEXTURE_NAME, optimize=False, compress_level=9)
    write_json(args.output / "validation.json", report)
    generated = ["KR.region.json", TEXTURE_NAME, "validation.json"]
    manifest = {"status": "confirmed", "country_id": "KR", "bounds": bounds, "metadata": metadata,
                "topology": topology, "polygon_count": len(polygons),
                "hole_count": sum(len(p["holes"]) for p in polygons),
                "vertex_count": sum(len(r) for p in polygons for r in [p["outer"], *p["holes"]]),
                "source_feature_count": 17,
                "source_files_sha256": {"source/" + name: sha256(source_dir / name) for name in [SOURCE_NAME, *SIDECARS]},
                "generated_files_sha256": {name: sha256(args.output / name) for name in generated},
                "generator": "Tools/ui/atlas_regions/prepare_korea.py", "generator_sha256": sha256(Path(__file__)),
                "runtime_versions": {"python": platform.python_version(), "numpy": np.__version__, "Pillow": PIL.__version__},
                "reproduce": "python Tools/ui/atlas_regions/prepare_korea.py",
                "parameters": {"margin_degrees": args.margin_degrees, **raster_info}}
    write_json(args.output / "manifest.json", manifest)
    print(json.dumps({"status": report["status"], "output": str(args.output), "bounds": bounds,
                      "polygons": len(polygons), "holes": manifest["hole_count"], "vertices": manifest["vertex_count"],
                      "grid_samples": report["source_union_grid_samples"], "texture": raster_info}, ensure_ascii=False))


if __name__ == "__main__":
    main()
