"""실제 UE Top 메시와 높이 텍스처 export로 국가 선택 표면 가중치를 만든다.

python Tools/ui/atlas_regions/prepare_surface.py
입력은 collect_surface.py의 Saved/AtlasRegions/Surface 두 파일이며,
출력은 SourceArt/UI/Atlas/CountryRegions/KR/surface.json이다.
메시 정점·UV·인덱스는 그대로 보존하고 높이 텍스처만 선형 데이터로 읽는다.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import platform
import struct
from pathlib import Path

import numpy as np
import PIL
from PIL import Image


ROOT = Path(__file__).resolve().parents[3]
INPUT = ROOT / "Saved/AtlasRegions/Surface"
OUTPUT = ROOT / "SourceArt/UI/Atlas/CountryRegions/KR/surface.json"


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def path_label(path: Path) -> str:
    try:
        return path.resolve().relative_to(ROOT).as_posix()
    except ValueError:
        return str(path.resolve())


def load_height(path: Path) -> tuple[np.ndarray, dict]:
    encoded = path.read_bytes()
    if encoded[:8] != b"\x89PNG\r\n\x1a\n" or encoded[12:16] != b"IHDR":
        raise ValueError("높이 입력은 PNG여야 합니다.")
    width, height, depth, color_type, compression, filter_method, interlace = struct.unpack(">IIBBBBB", encoded[16:29])
    if color_type != 0 or depth not in (8, 16):
        raise ValueError("높이는 8/16비트 단일 채널 회색조 PNG여야 합니다. 색상 변환을 추측하지 않습니다.")
    with Image.open(path) as image:
        pixels = np.asarray(image, dtype=np.float64)
        mode = image.mode
    if pixels.shape != (height, width):
        raise ValueError("PNG 헤더와 실제 높이 배열의 크기가 다릅니다.")
    normalization = (1 << depth) - 1
    if not np.isfinite(pixels).all() or pixels.min() < 0 or pixels.max() > normalization:
        raise ValueError("높이 텍셀 값이 비트 깊이의 유효 범위를 벗어났습니다.")
    # 관측 최대값이 아니라 실제 저장 비트 깊이의 최대값으로 정규화한다.
    return pixels / normalization, {
        "format": "PNG", "mode": mode, "bit_depth": depth, "color_type": color_type,
        "size": [width, height], "raw_value_range": [float(pixels.min()), float(pixels.max())],
        "normalization_divisor": normalization, "color_space": "linear_data_no_gamma_conversion",
        "compression_method": compression, "filter_method": filter_method, "interlace_method": interlace,
    }


def bilinear(height: np.ndarray, uvs: np.ndarray) -> np.ndarray:
    """UE 텍셀 중심과 동일한 x=u*W-.5, y=v*H-.5. U wrap / V clamp."""
    rows, columns = height.shape
    x = uvs[:, 0] * columns - .5
    y = np.clip(uvs[:, 1] * rows - .5, 0, rows - 1)
    x_floor, y_floor = np.floor(x), np.floor(y)
    tx, ty = x - x_floor, y - y_floor
    x0, y0 = x_floor.astype(np.int64) % columns, y_floor.astype(np.int64)
    x1, y1 = (x0 + 1) % columns, np.minimum(y0 + 1, rows - 1)
    top = height[y0, x0] * (1 - tx) + height[y0, x1] * tx
    bottom = height[y1, x0] * (1 - tx) + height[y1, x1] * tx
    return top * (1 - ty) + bottom * ty


def scalar_sample(height: np.ndarray, uv: list) -> float:
    rows, columns = height.shape
    x, y = uv[0] * columns - .5, uv[1] * rows - .5
    x0, y0 = math.floor(x), math.floor(y)
    result = 0.
    for ix, wx in ((x0, 1 - (x - x0)), (x0 + 1, x - x0)):
        for iy, wy in ((y0, 1 - (y - y0)), (y0 + 1, y - y0)):
            result += float(height[min(max(iy, 0), rows - 1), ix % columns]) * wx * wy
    return result


def validate(raw: dict, height: np.ndarray, samples: np.ndarray, weights: np.ndarray) -> dict:
    vertices = np.asarray(raw["vertices"], dtype=np.float64)
    indices = np.asarray(raw["indices"], dtype=np.int64).reshape(-1, 3)
    a, b, c = vertices[indices[:, 0]], vertices[indices[:, 1]], vertices[indices[:, 2]]
    double_areas = np.linalg.norm(np.cross(b - a, c - a), axis=1)
    if np.any(double_areas <= 1e-9):
        raise ValueError("표면에 면적이 0인 삼각형이 있습니다.")
    reference = np.asarray([scalar_sample(height, uv) for uv in raw["uvs"]])
    max_sample_error = float(np.max(np.abs(reference - samples)))
    if max_sample_error > 1e-12:
        raise ValueError("벡터/스칼라 bilinear 결과가 다릅니다.")
    fixture = np.array([[0., .25], [.5, 1.]])
    fixture_uvs = np.array([[.25, .25], [.75, .75], [0., .25], [1., .25],
                            [.25, -1.], [.75, 2.], [.5, .5]])
    expected = np.array([0., 1., .125, .125, 0., 1., .4375])
    if not np.allclose(bilinear(fixture, fixture_uvs), expected, rtol=0, atol=1e-12):
        raise ValueError("텍셀 중심/U wrap/V clamp 합성 검증 실패")
    if not np.isfinite(weights).all() or weights.min() < 0 or weights.max() > 1:
        raise ValueError("높이 가중치가 유효 범위를 벗어났습니다.")
    radii = np.linalg.norm(vertices, axis=1)
    return {"status": "passed", "vertex_count": len(vertices), "triangle_count": len(indices),
            "vertices_uvs_indices_preserved": True, "finite_arrays": True, "indices_in_range": True,
            "minimum_triangle_double_area": float(double_areas.min()),
            "source_radius_range": [float(radii.min()), float(radii.max())],
            "sample_range": [float(samples.min()), float(samples.max())],
            "weight_range": [float(weights.min()), float(weights.max())],
            "zero_weight_count": int(np.count_nonzero(weights == 0)),
            "scalar_reference_samples": len(reference), "max_scalar_sample_error": max_sample_error,
            "texel_center_wrap_clamp_fixture": "passed",
            "exported_max_vertex_uv_deviation_degrees": raw.get("max_vertex_uv_deviation_degrees")}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--surface", type=Path, default=INPUT / "surface_raw.json")
    parser.add_argument("--height", type=Path, default=INPUT / "height.png")
    parser.add_argument("--output", type=Path, default=OUTPUT)
    parser.add_argument("--country-id", default="KR", help="표면 자료 범위. 전 세계판은 ALL")
    parser.add_argument("--exporter", type=Path, default=ROOT / "Tools/ui/atlas_regions/collect_surface.py")
    args = parser.parse_args()
    raw = json.loads(args.surface.read_text(encoding="utf-8"))
    if raw.get("status") != "passed" or raw.get("source_radius") != 5000.:
        raise ValueError("성공한 반지름 5000 UE export가 필요합니다.")
    vertices, uvs = np.asarray(raw["vertices"], dtype=np.float64), np.asarray(raw["uvs"], dtype=np.float64)
    indices = raw["indices"]
    if vertices.ndim != 2 or vertices.shape[1] != 3 or uvs.shape != (len(vertices), 2) or len(vertices) < 3:
        raise ValueError("정점/UV 배열 형태가 잘못되었습니다.")
    if not np.isfinite(vertices).all() or not np.isfinite(uvs).all():
        raise ValueError("정점/UV에 비유한 값이 있습니다.")
    if not indices or len(indices) % 3 or any(type(i) is not int or i < 0 or i >= len(vertices) for i in indices):
        raise ValueError("삼각형 인덱스가 유효하지 않습니다.")
    height, height_info = load_height(args.height)
    samples = bilinear(height, uvs)
    weights = np.power(np.clip((samples - .008) / .992, 0., 1.), 1.15)
    report = validate(raw, height, samples, weights)
    exporter = args.exporter
    asset_paths = [mesh["path"] for mesh in raw.get("meshes", []) if mesh.get("selected", 0)]
    asset_paths.append(raw["height_texture"])
    asset_hashes = {}
    for asset_path in asset_paths:
        package = asset_path.split(".")[0]
        if not package.startswith("/Game/"):
            raise ValueError("프로젝트 외부 에셋 경로는 해시를 추측하지 않습니다.")
        local = ROOT / "Content" / (package[len("/Game/"):] + ".uasset")
        if local.is_file():
            asset_hashes[path_label(local)] = sha256(local)
    result = {"schema_version": 1, "country_id": args.country_id, "status": "passed",
              "vertices": raw["vertices"], "uvs": raw["uvs"], "indices": indices, "weights": weights.tolist(),
              "source_radius": 5000., "base_land_thickness": 40., "base_mountain_height": 650.,
              "provenance": {"surface_file": path_label(args.surface), "surface_sha256": sha256(args.surface),
                             "height_file": path_label(args.height), "height_sha256": sha256(args.height),
                             "exporter": path_label(exporter), "exporter_sha256": sha256(exporter),
                             "generator": path_label(Path(__file__)), "generator_sha256": sha256(Path(__file__)),
                             "source_assets_sha256": asset_hashes, "source_meshes": raw.get("meshes", []),
                             "height_texture_asset": raw["height_texture"],
                             "geographic_bounds": raw.get("geographic_bounds"),
                             "height_texture": height_info,
                             "sample_contract": "x=u*W-.5; y=v*H-.5; bilinear; U wrap; V clamp; north V=0",
                             "weight_contract": "pow(saturate((sample-.008)/.992),1.15)",
                             "runtime_versions": {"python": platform.python_version(), "numpy": np.__version__, "Pillow": PIL.__version__}},
              "validation": report}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, ensure_ascii=False, indent=2, allow_nan=False) + "\n", encoding="utf-8")
    print(json.dumps({"output": path_label(args.output), "sha256": sha256(args.output),
                      "height": height_info, "validation": report}, ensure_ascii=False))


if __name__ == "__main__":
    main()
