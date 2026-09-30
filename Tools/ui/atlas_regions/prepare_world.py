"""Natural Earth 1:10m 국가 경계로 한국 이외 7개국 선택 도형·마스크를 준비한다.

Python 3.10+, NumPy, Pillow만 사용한다. 고정된 공식 ZIP의 Polygon SHP/DBF만
읽으며 범용 GIS 변환기가 아니다. 원본 좌표·섬·구멍을 보존하고 AI 이미지나
지도 재생성을 사용하지 않는다. 한국 파일 및 Unreal 에셋은 수정하지 않는다.

실행: python Tools/ui/atlas_regions/prepare_world.py [--inspect]
"""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import platform
import struct
import zipfile
from pathlib import Path

import numpy as np
import PIL
from PIL import Image, ImageChops, ImageDraw, ImageFilter

from prepare_korea import area, canonical_ring, contains, in_ring, write_json


ROOT = Path(__file__).resolve().parents[3]
OUTPUT = ROOT / "SourceArt/UI/Atlas/CountryRegions/World"
ZIP_NAME = "ne_10m_admin_0_countries.zip"
SOURCE_SHA256 = "ce1ac7036499a0edd641fbc093cd209a98f96a49d2eca8480aaacad35138a7f6"
SOURCE_URL = "https://naciscdn.org/naturalearth/10m/cultural/ne_10m_admin_0_countries.zip"
PAGE_URL = "https://www.naturalearthdata.com/downloads/10m-cultural-vectors/10m-admin-0-countries/"
LICENSE_URL = "https://www.naturalearthdata.com/about/terms-of-use/"
COUNTRIES = {"BR": "BRA", "DE": "DEU", "JP": "JPN", "US": "USA", "ES": "ESP", "TH": "THA", "AU": "AUS"}
SIZES = {"BR": 2048, "DE": 1024, "JP": 2048, "US": 2048, "ES": 1024, "TH": 1024, "AU": 2048}
POINTS = {
    "BR": [("Brasilia", -47.8825, -15.7942, True), ("Manaus", -60.0217, -3.1190, True),
           ("RioInland", -43.24, -22.86, True), ("Argentina", -58.3816, -34.6037, False),
           ("Atlantic", -32, -12, False)],
    "DE": [("Berlin", 13.4050, 52.5200, True), ("Munich", 11.5820, 48.1351, True),
           ("Rugen", 13.40, 54.43, True), ("France", 7.74, 48.58, False),
           ("NorthSea", 7.5, 55, False)],
    "JP": [("Tokyo", 139.6917, 35.6895, True), ("Hokkaido", 142.35, 43.77, True),
           ("Kyushu", 130.7079, 32.8031, True), ("Okinawa", 127.80, 26.32, True),
           ("Tsushima", 129.33, 34.40, True), ("Korea", 129.0756, 35.1796, False),
           ("Pacific", 148, 36, False)],
    "US": [("Washington", -77.0369, 38.9072, True), ("Denver", -104.9903, 39.7392, True),
           ("Alaska", -149.49, 64.0, True), ("Hawaii", -155.56, 19.62, True),
           ("Adak", -176.65, 51.85, True), ("Attu", 173.0, 52.90, True),
           ("SemisopochnoiPositive179", 179.62, 51.95, True),
           ("AleutianIslandNegative179", -179.11, 51.26, True),
           ("AleutianOpenWater", 179.0, 54.0, False), ("Canada", -123.12, 49.28, False),
           ("Mexico", -102, 23, False), ("Atlantic", -72, 31, False)],
    "ES": [("Madrid", -3.7038, 40.4168, True), ("Mallorca", 2.91, 39.63, True),
           ("Tenerife", -16.57, 28.29, True), ("Portugal", -9.14, 38.72, False),
           ("Mediterranean", 1, 37, False)],
    "TH": [("Bangkok", 100.5018, 13.7563, True), ("ChiangMai", 98.9853, 18.7883, True),
           ("Phuket", 98.34, 7.92, True), ("Laos", 102.14, 19.89, False),
           ("GulfOfThailand", 101.0, 10, False)],
    "AU": [("Canberra", 149.13, -35.28, True), ("Perth", 115.86, -31.95, True),
           ("Tasmania", 146.6, -42.0, True), ("Macquarie", 158.88, -54.62, True),
           ("NewZealand", 174.78, -41.29, False), ("IndianOcean", 108, -24, False)],
}


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def read_source(source: Path) -> tuple[list, dict]:
    """고정 Polygon SHP와 DBF를 대응 레코드 순서 그대로 읽는다."""
    if digest(source) != SOURCE_SHA256:
        raise ValueError("Natural Earth 5.1.1 공식 ZIP의 고정 해시와 다릅니다.")
    prefix = "ne_10m_admin_0_countries"
    with zipfile.ZipFile(source) as archive:
        shp = archive.read(prefix + ".shp")
        dbf = archive.read(prefix + ".dbf")
        prj = archive.read(prefix + ".prj").decode("ascii")
        version = archive.read(prefix + ".VERSION.txt").decode("utf-8").strip()
        if version != "5.1.1" or "GCS_WGS_1984" not in prj:
            raise ValueError("원본 버전 또는 WGS84 좌표계가 다릅니다.")
    if struct.unpack_from(">i", shp, 0)[0] != 9994 or struct.unpack_from("<i", shp, 32)[0] != 5:
        raise ValueError("Polygon Shapefile이 아닙니다.")
    record_count = struct.unpack_from("<I", dbf, 4)[0]
    header_size, record_size = struct.unpack_from("<HH", dbf, 8)
    fields = []
    offset = 32
    while dbf[offset] != 13:
        entry = dbf[offset:offset + 32]
        fields.append((entry[:11].split(b"\0")[0].decode("ascii"), entry[16]))
        offset += 32
    shapes = []
    offset = 100
    for index in range(record_count):
        record = dbf[header_size + index * record_size:header_size + (index + 1) * record_size]
        if record[0] == 42:
            raise ValueError("삭제된 DBF 레코드를 지원하지 않습니다.")
        attributes = {}
        cursor = 1
        for name, size in fields:
            attributes[name] = record[cursor:cursor + size].decode("utf-8").strip(" \0")
            cursor += size
        number, words = struct.unpack_from(">ii", shp, offset)
        payload = shp[offset + 8:offset + 8 + words * 2]
        offset += 8 + words * 2
        if number != index + 1 or struct.unpack_from("<i", payload)[0] != 5:
            raise ValueError("SHP/DBF 레코드 순서 또는 Polygon 타입 불일치")
        parts_count, points_count = struct.unpack_from("<ii", payload, 36)
        parts = list(struct.unpack_from("<" + "i" * parts_count, payload, 44)) + [points_count]
        coords_offset = 44 + parts_count * 4
        coords = [struct.unpack_from("<dd", payload, coords_offset + j * 16) for j in range(points_count)]
        rings = [coords[parts[j]:parts[j + 1]] for j in range(parts_count)]
        if any(len(r) < 4 or r[0] != r[-1] or not all(math.isfinite(x) for p in r for x in p) for r in rings):
            raise ValueError("잘못된 경위도 또는 열린 링입니다.")
        shapes.append({"attributes": attributes, "rings": rings})
    if offset != len(shp):
        raise ValueError("파싱되지 않은 SHP 레코드가 있습니다.")
    return shapes, {"version": version, "projection": prj, "record_count": record_count}


def compact_domain(rings: list) -> tuple[list, dict]:
    """가장 큰 비어 있는 경도 구간에 이음선을 놓아 미국도 연속 좌표로 만든다."""
    values = sorted({p[0] % 360 for ring in rings for p in ring})
    gaps = [(values[(i + 1) % len(values)] + (360 if i == len(values) - 1 else 0) - x, i)
            for i, x in enumerate(values)]
    _, gap_index = max(gaps)
    start = values[(gap_index + 1) % len(values)]
    end = values[gap_index] + (360 if values[gap_index] < start else 0)
    # 한국 등 기존 지도와 같은 -180..180을 우선한다. 미국만 날짜변경선을 넘어간다.
    center = (start + end) / 2
    shift = -360 * math.floor((center + 180) / 360)
    result = []
    for ring in rings:
        converted = []
        for lon, lat in ring:
            x = lon % 360
            if x < start - 1e-9:
                x += 360
            converted.append((x + shift, lat))
        if any(abs(a[0] - b[0]) > 180 for a, b in zip(converted, converted[1:])):
            raise ValueError("경도 래핑 이후에도 180도를 넘는 선분이 있습니다.")
        result.append(converted)
    return result, {"longitude_domain": [start + shift, end + shift],
                    "longitude_wrap": "lon + 360*round((bounds_center-lon)/360)",
                    "longitude_wrapped": any(abs(a[0] - b[0]) > 1 for original, converted in zip(rings, result)
                                             for a, b in zip(original, converted))}


def polygons_from_rings(rings: list) -> list:
    """SHP의 시계방향 외곽과 반시계방향 구멍을 원본 관계 그대로 보존한다."""
    outers = [r for r in rings if area(r) < 0]
    holes = [r for r in rings if area(r) > 0]
    if len(outers) + len(holes) != len(rings):
        raise ValueError("면적 0인 링")
    polygons = [{"outer": canonical_ring(r, True), "holes": []} for r in outers]
    bounds = [ring_bounds(p["outer"]) for p in polygons]
    for ring in holes:
        candidates = [i for i, p in enumerate(polygons) if bounds_contains(bounds[i], ring[0]) and in_ring(ring[0], p["outer"])]
        if not candidates:
            raise ValueError("외곽에 속하지 않는 구멍")
        owner = min(candidates, key=lambda i: abs(area(polygons[i]["outer"])))
        polygons[owner]["holes"].append(canonical_ring(ring, False))
    return sorted(polygons, key=lambda p: (-abs(area(p["outer"])), p["outer"][0]))


def ring_bounds(ring: list) -> list:
    return [min(p[0] for p in ring), min(p[1] for p in ring), max(p[0] for p in ring), max(p[1] for p in ring)]


def bounds_contains(bounds: list, point: tuple) -> bool:
    return bounds[0] <= point[0] <= bounds[2] and bounds[1] <= point[1] <= bounds[3]


def raster(polygons: list, bounds: list, size: int) -> tuple[Image.Image, dict]:
    """작은 섬도 원본 좌표로 그린다. 인위적으로 확장하거나 픽셀 위치를 바꾸지 않는다."""
    scale = 2
    pixels = size * scale
    west, south, east, north = bounds
    def xy(ring):
        return [((p[0] - west) / (east - west) * pixels - .5,
                 (north - p[1]) / (north - south) * pixels - .5) for p in ring]
    fill = Image.new("L", (pixels, pixels))
    border = Image.new("L", fill.size)
    draw_fill, draw_border = ImageDraw.Draw(fill), ImageDraw.Draw(border)
    width = 2
    for poly in polygons:
        if poly["holes"]:
            # 이번 원본 7개국에는 구멍이 없다. 입력이 바뀌면 조용히 덮어 그리지 않는다.
            raise ValueError("고정 7개국 원본과 다른 hole이 발견되었습니다.")
        draw_fill.polygon(xy(poly["outer"]), fill=255)
        draw_border.line(xy(poly["outer"]), fill=255, width=width, joint="curve")
    halo = ImageChops.subtract(fill.filter(ImageFilter.GaussianBlur(2.5 * scale)), fill)
    channels = [channel.resize((size, size), Image.Resampling.LANCZOS) for channel in (fill, border, halo)]
    rgba = Image.merge("RGBA", (*channels, Image.new("L", (size, size), 255)))
    return rgba, {"supersampling": scale, "boundary_total_width_output_pixels": width / scale,
                  "halo_gaussian_sigma_output_pixels": 2.5,
                  "boundary_total_width_lon_degrees": (east - west) / size,
                  "boundary_total_width_lat_degrees": (north - south) / size}


def coordinate_in_domain(lon: float, lat: float, bounds: list) -> tuple:
    return lon + 360 * round(((bounds[0] + bounds[2]) / 2 - lon) / 360), lat


def prepared_contains(point: tuple, prepared: list) -> bool:
    return any(bounds_contains(bound, point) and in_ring(point, poly["outer"])
               and not any(in_ring(point, h) for h in poly["holes"]) for poly, bound in prepared)


def validate(country_id: str, source_rings: list, polygons: list, bounds: list, rgba: Image.Image) -> dict:
    prepared = [(p, ring_bounds(p["outer"])) for p in polygons]
    original = polygons_from_rings(source_rings)
    prepared_source = [(p, ring_bounds(p["outer"])) for p in original]
    point_checks = []
    for name, lon, lat, expected in POINTS[country_id]:
        point = coordinate_in_domain(lon, lat, bounds)
        actual = prepared_contains(point, prepared)
        source = prepared_contains((lon, lat), prepared_source)
        u, v = (point[0] - bounds[0]) / (bounds[2] - bounds[0]), (bounds[3] - lat) / (bounds[3] - bounds[1])
        red = rgba.getpixel((int(u * rgba.width), int(v * rgba.height)))[0] if 0 <= u < 1 and 0 <= v < 1 else 0
        if actual != expected or source != expected or (red >= 128) != expected:
            raise ValueError(f"{country_id}/{name}: 예상 {expected}, 원본 {source}, 결과 {actual}, mask {red}")
        point_checks.append({"name": name, "coordinate": [lon, lat], "domain_coordinate": point,
                             "expected_inside": expected, "source_inside": source, "result_inside": actual, "mask_r": red})
    # 원본 전체 정점이 경도 ±360 이동과 방향/시작점 정규화 외에는 바뀌지 않았는지 확인한다.
    original_points = sorted((round(p[0] % 360, 9), p[1]) for r in source_rings for p in r[:-1])
    result_points = sorted((round(p[0] % 360, 9), p[1]) for poly in polygons
                           for r in [poly["outer"], *poly["holes"]] for p in r[:-1])
    if original_points != result_points:
        raise ValueError(f"{country_id}: 원본 정점이 누락되거나 변형되었습니다.")
    original_area = -sum(area(r) for r in source_rings)
    output_area = sum(area(p["outer"]) - sum(abs(area(h)) for h in p["holes"]) for p in polygons)
    if not math.isclose(original_area, output_area, rel_tol=1e-9, abs_tol=1e-7):
        raise ValueError(f"{country_id}: 래핑 후 면적이 다릅니다.")
    # 양방향 경도 표현은 똑같은 점으로 판정되어야 한다.
    wrap_checks = []
    for name, lon, lat, expected in POINTS[country_id]:
        results = [prepared_contains(coordinate_in_domain(lon + shift, lat, bounds), prepared) for shift in (-360, 0, 360)]
        if results != [expected] * 3:
            raise ValueError(f"{country_id}/{name}: 동경/서경 래핑 불일치")
        wrap_checks.append({"name": name, "equivalent_longitudes": [lon - 360, lon, lon + 360], "results": results})
    rgb = np.asarray(rgba)[:, :, :3]
    if np.any(rgb[0]) or np.any(rgb[-1]) or np.any(rgb[:, 0]) or np.any(rgb[:, -1]):
        raise ValueError(f"{country_id}: 텍스처 가장자리에 마스크가 닿았습니다.")
    return {"status": "passed", "country_id": country_id, "point_checks": point_checks,
            "wrapped_longitude_checks": wrap_checks, "source_vertices_preserved": True,
            "source_area_square_degrees": original_area, "result_area_square_degrees": output_area,
            "texture_edge_rgb_zero": True, "alpha_constant_255": rgba.getchannel("A").getextrema() == (255, 255),
            "simplification_degrees": 0, "ring_orientation": "outer_ccw_holes_cw",
            "max_segment_longitude_degrees": max(abs(a[0] - b[0]) for p in polygons for r in [p["outer"], *p["holes"]] for a, b in zip(r, r[1:])),
            "limitations": ["1:10m 원본에 없는 섬과 경계는 보충하지 않음", "극소 도서는 래스터보다 원본 폴리곤 판정이 정밀함"]}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, default=OUTPUT / "source" / ZIP_NAME)
    parser.add_argument("--output", type=Path, default=OUTPUT)
    parser.add_argument("--inspect", action="store_true")
    args = parser.parse_args()
    shapes, info = read_source(args.source)
    selected = {}
    for country_id, adm in COUNTRIES.items():
        matches = [s for s in shapes if s["attributes"]["ADM0_A3"] == adm]
        if len(matches) != 1:
            raise ValueError(f"국가 레코드가 정확히 하나가 아닙니다: {country_id}")
        shape = matches[0]
        rings, wrap = compact_domain(shape["rings"])
        polygons = polygons_from_rings(rings)
        selected[country_id] = {"polygons": polygons, "wrap": wrap, "source": shape}
        print(json.dumps({"country_id": country_id, "name": shape["attributes"]["ADMIN"],
                          "polygons": len(polygons), "holes": sum(len(p["holes"]) for p in polygons),
                          "vertices": sum(len(r) for r in rings), "bounds": ring_bounds([p for r in rings for p in r]),
                          "size": SIZES[country_id], **wrap}, ensure_ascii=False), flush=True)
    if args.inspect:
        return
    metadata = {"source": "Natural Earth", "source_version": info["version"], "source_scale": "1:10m",
                "source_file": ZIP_NAME, "source_sha256": SOURCE_SHA256, "source_url": SOURCE_URL,
                "source_page": PAGE_URL, "license": "Public Domain", "license_url": LICENSE_URL,
                "download_date": "2026-09-21", "simplification_degrees": 0,
                "boundary_policy": "원본 Admin0 countries의 de facto 경계를 수정 없이 사용"}
    args.output.mkdir(parents=True, exist_ok=True)
    entries = []
    validations = []
    for country_id, selected_country in selected.items():
        polygons = selected_country["polygons"]
        points = [point for poly in polygons for point in poly["outer"]]
        extent = ring_bounds(points)
        # 최소 .2도 또는 긴 변의 1% 여백으로 선·후광이 clamp 가장자리에 닿지 않게 한다.
        margin = max(.2, max(extent[2] - extent[0], extent[3] - extent[1]) * .01)
        bounds = [math.floor((extent[i] - margin) * 10) / 10 for i in (0, 1)]
        bounds += [math.ceil((extent[i] + margin) * 10) / 10 for i in (2, 3)]
        size = SIZES[country_id]
        rgba, raster_info = raster(polygons, bounds, size)
        validation = validate(country_id, selected_country["source"]["rings"], polygons, bounds, rgba)
        country_output = args.output / country_id
        country_output.mkdir(exist_ok=True)
        texture_name = f"T_FF_AtlasRegion_{country_id}_{size}.png"
        region_name = f"{country_id}.region.json"
        region = {"schema_version": 1, "country_id": country_id, "coordinate_system": "WGS84/CRS84",
                  "bounds": bounds, "geometry_bounds": extent, "polygons": polygons,
                  "texture": {"file": texture_name, "size": [size, size], "north_v": 0,
                              "channels": {"R": "interior", "G": "boundary", "B": "soft_outer_halo", "A": "255"},
                              "uv": "lon+=360*round((bounds_center-lon)/360); u=(lon-west)/(east-west); v=(north-lat)/(north-south)",
                              "srgb": False, **raster_info}, "metadata": {**metadata, **selected_country["wrap"],
                              "source_adm0_a3": COUNTRIES[country_id], "source_name": selected_country["source"]["attributes"]["ADMIN"]}}
        write_json(country_output / region_name, region)
        rgba.save(country_output / texture_name, optimize=False, compress_level=9)
        write_json(country_output / "validation.json", validation)
        entries.append({"country_id": country_id, "region_file": f"{country_id}/{region_name}",
                        "texture_file": f"{country_id}/{texture_name}", "validation_file": f"{country_id}/validation.json",
                        "bounds": bounds, "size": [size, size], "polygon_count": len(polygons),
                        "hole_count": sum(len(p["holes"]) for p in polygons), "vertex_count": sum(len(r) for p in polygons for r in [p["outer"], *p["holes"]]),
                        "generated_files_sha256": {name: digest(country_output / name) for name in [region_name, texture_name, "validation.json"]}})
        validations.append({"country_id": country_id, "status": validation["status"], "point_count": len(validation["point_checks"])})
        print(json.dumps({"country_id": country_id, "status": "passed", "bounds": bounds, "points": len(validation["point_checks"])}, ensure_ascii=False), flush=True)
    write_json(args.output / "validation.json", {"status": "passed", "countries": validations,
                                                "point_count": sum(v["point_count"] for v in validations), "korea_untouched": True})
    write_json(args.output / "manifest.json", {"schema_version": 1, "status": "confirmed", "metadata": metadata,
             "countries": entries, "source_files_sha256": {"source/" + ZIP_NAME: SOURCE_SHA256},
             "generator": "Tools/ui/atlas_regions/prepare_world.py", "generator_sha256": digest(Path(__file__)),
             "runtime_versions": {"python": platform.python_version(), "numpy": np.__version__, "Pillow": PIL.__version__},
             "reproduce": "python Tools/ui/atlas_regions/prepare_world.py"})


if __name__ == "__main__":
    main()
