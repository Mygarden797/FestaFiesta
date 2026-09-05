# -*- coding: utf-8 -*-
"""타이틀용 캐주얼 릴리프 지구 텍스처와 고밀도 UV 구체를 만든다.

최종 해안선은 기존 CasualBright 알베도에서만 계산한다. ImageGen 결과는
미세한 재질 변화의 참고로만 사용하므로 대륙이나 섬의 위치를 바꾸지 않는다.

필요 패키지: Pillow, NumPy (Codex 번들 Python에 포함)
"""

from __future__ import annotations

import hashlib
import json
import math
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFilter


WIDTH = 2048
HEIGHT = 1024
SPHERE_SEGMENTS = 256
SPHERE_RINGS = 128
SPHERE_RADIUS_CM = 50.0

PROJECT_ROOT = Path(__file__).resolve().parents[2]
EARTH_DIR = PROJECT_ROOT / "SourceArt/UI/Title/CasualReliefParts_v1/Earth"
MESH_DIR = PROJECT_ROOT / "SourceArt/UI/Title/CasualReliefParts_v1/Mesh"
GEN_DIR = EARTH_DIR / "GenerationSource"
PREVIEW_PATH = PROJECT_ROOT / "docs/ui/mockups/earth-casual-relief-v1-preview.png"

SOURCE_ALBEDO = (
    PROJECT_ROOT
    / "SourceArt/UI/Title/CasualBrightParts_v1/Earth/"
    "T_FF_Earth_CasualBright_Albedo_2048x1024.png"
)
IMAGEGEN_ALBEDO = GEN_DIR / "ImageGen_CasualRelief_Albedo_Raw.png"
SOURCE_NORMAL = (
    PROJECT_ROOT
    / "SourceArt/ThirdParty/SolarSystemScope/PlanetTextures/Earth/"
    "2k_earth_normal_map.tif"
)

OUT_ALBEDO = EARTH_DIR / "T_FF_Earth_CasualRelief_Albedo_2048x1024.png"
OUT_HEIGHT = EARTH_DIR / "T_FF_Earth_CasualRelief_Height16_2048x1024.png"
OUT_NORMAL = EARTH_DIR / "T_FF_Earth_CasualRelief_Normal_DX_2048x1024.png"
OUT_MASKS = EARTH_DIR / "T_FF_Earth_CasualRelief_Masks_2048x1024.png"
OUT_LAND_DEBUG = GEN_DIR / "T_FF_Earth_CasualRelief_LandMask_Debug.png"
OUT_MOUNTAIN_DEBUG = GEN_DIR / "T_FF_Earth_CasualRelief_MountainGuide_Debug.png"
OUT_SPHERE = MESH_DIR / "SM_FF_Earth_UVSphere_R50cm_256x128.obj"
OUT_MANIFEST = EARTH_DIR.parent / "asset-manifest.json"


def load_rgb(path: Path, size: tuple[int, int] = (WIDTH, HEIGHT)) -> np.ndarray:
    image = Image.open(path).convert("RGB")
    if image.size != size:
        image = image.resize(size, Image.Resampling.LANCZOS)
    return np.asarray(image, dtype=np.uint8)


def blur_scalar(values: np.ndarray, radius: float) -> np.ndarray:
    """X는 Wrap, Y는 Reflect로 가우시안 블러한다."""
    if radius <= 0:
        return values.astype(np.float32, copy=True)
    pad = max(4, int(math.ceil(radius * 3.0)))
    source = np.clip(values * 255.0, 0, 255).astype(np.uint8)
    source = np.pad(source, ((0, 0), (pad, pad)), mode="wrap")
    source = np.pad(source, ((pad, pad), (0, 0)), mode="reflect")
    image = Image.fromarray(source, mode="L").filter(ImageFilter.GaussianBlur(radius))
    result = np.asarray(image, dtype=np.float32) / 255.0
    return result[pad:-pad, pad:-pad]


def blur_rgb(values: np.ndarray, radius: float) -> np.ndarray:
    channels = [blur_scalar(values[..., index] / 255.0, radius) for index in range(3)]
    return np.stack(channels, axis=-1)


def force_wrap_seam(values: np.ndarray, width: int = 24) -> np.ndarray:
    """경도 180도 첫/마지막 열을 정확히 일치시킨다.

    입력 블러와 구면 노이즈가 이미 Wrap으로 계산되므로 넓은 띠를 만들 필요가 없다.
    넓게 섞으면 극지 해안이 세로로 번지는 부작용이 생긴다.
    """
    result = values.astype(np.float32, copy=True)
    seam = (result[:, 0] + result[:, -1]) * 0.5
    result[:, 0] = seam
    result[:, -1] = seam
    return result


def classify_surface(albedo: np.ndarray) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
    rgb = albedo.astype(np.int16)
    red, green, blue = rgb[..., 0], rgb[..., 1], rgb[..., 2]

    # 현재 알베도의 청록 바다만 분리한다. 나머지는 대륙 또는 극지 얼음이다.
    water = (
        (blue - red > 45)
        & (green - red > 35)
        & (blue > 135)
        & (green > 125)
        & (red < 120)
    )
    spread = np.maximum.reduce([red, green, blue]) - np.minimum.reduce([red, green, blue])
    ice = (~water) & (np.minimum.reduce([red, green, blue]) > 180) & (spread < 65)
    arid = (~water) & (~ice) & (red > 155) & (green > 145) & (blue < 155)
    return water, ice, arid


def spherical_noise(seed: int = 1308) -> np.ndarray:
    """좌우 이음새와 극점 왜곡이 없는 저주파 구면 노이즈."""
    yy, xx = np.mgrid[0:HEIGHT, 0:WIDTH]
    longitude = (xx / (WIDTH - 1)) * (math.pi * 2.0) - math.pi
    latitude = math.pi * 0.5 - (yy / (HEIGHT - 1)) * math.pi
    cos_lat = np.cos(latitude)
    sx = (cos_lat * np.cos(longitude)).astype(np.float32)
    sy = (cos_lat * np.sin(longitude)).astype(np.float32)
    sz = np.sin(latitude).astype(np.float32)

    rng = np.random.default_rng(seed)
    result = np.zeros((HEIGHT, WIDTH), dtype=np.float32)
    weight_sum = 0.0
    for octave, frequency in enumerate((2.2, 3.4, 5.1, 7.7, 11.5, 17.0)):
        direction = rng.normal(size=3)
        direction /= np.linalg.norm(direction)
        phase = float(rng.uniform(-math.pi, math.pi))
        weight = 1.0 / (1.0 + octave * 0.58)
        argument = (sx * direction[0] + sy * direction[1] + sz * direction[2])
        result += np.sin(argument * frequency * math.pi + phase).astype(np.float32) * weight
        weight_sum += weight
    result /= weight_sum
    low, high = np.percentile(result, (1.0, 99.0))
    result = np.clip((result - low) / max(1e-6, high - low), 0.0, 1.0)
    result[:, -1] = result[:, 0]
    return result.astype(np.float32)


def build_height_and_masks(
    source_albedo: np.ndarray,
) -> tuple[np.ndarray, np.ndarray, np.ndarray, np.ndarray, np.ndarray]:
    water, ice, _ = classify_surface(source_albedo)
    land = ~water
    land_f = land.astype(np.float32)
    ice_f = ice.astype(np.float32)

    # Solar System Scope의 노멀은 절대 고도가 아니라 기울기다. 기울기 크기를 넓게
    # 번지게 해서 실제 산맥 위치를 따르는 '산악 가이드'로만 사용한다.
    source_normal = load_rgb(SOURCE_NORMAL).astype(np.float32)
    tangent_xy = (source_normal[..., :2] - 128.0) / 127.0
    slope = np.sqrt(np.sum(tangent_xy * tangent_xy, axis=-1))
    slope = np.clip((slope - 0.008) / 0.24, 0.0, 1.0) * land_f
    mountain = blur_scalar(slope, 3.0) * 0.58 + blur_scalar(slope, 11.0) * 0.42
    valid = mountain[land]
    low, high = np.percentile(valid, (72.0, 99.6)) if valid.size else (0.0, 1.0)
    mountain = np.clip((mountain - low) / max(1e-6, high - low), 0.0, 1.0)
    mountain = np.power(mountain, 0.72) * land_f

    noise = spherical_noise()
    # 256x128 구체의 정점 간격(텍스처 약 8px)에 맞춘 넓은 둥근 해안 베벨.
    coast_blur = blur_scalar(land_f, 4.5)
    soft_land = np.clip((coast_blur - 0.08) / 0.84, 0.0, 1.0)
    soft_land = soft_land * soft_land * (3.0 - 2.0 * soft_land)
    coast_interior = np.clip((coast_blur - 0.42) / 0.56, 0.0, 1.0)
    coast_interior *= land_f

    # 대륙은 한 단 위에 놓고, 해안은 둥글게 깎으며, 산맥은 추가로 솟게 한다.
    height = soft_land * (
        0.47
        + 0.120 * coast_interior
        + 0.075 * noise
        + 0.275 * mountain
    )
    ice_height = soft_land * (
        0.47
        + 0.105 * coast_interior
        + 0.045 * noise
        + 0.115 * mountain
    )
    height = np.where(ice, ice_height, height)
    height = np.clip(height, 0.0, 1.0)
    height = force_wrap_seam(height, 28)
    height[:, -1] = height[:, 0]

    # A 채널은 곱셈용 AO다. 1이 중립이며 해안 안쪽과 큰 산맥만 살짝 눌린다.
    coast_ao = (1.0 - coast_interior) * soft_land
    ao = np.clip(1.0 - 0.22 * coast_ao - 0.075 * mountain, 0.68, 1.0)

    # 바다는 매끈하고, 땅은 무광, 극지는 그 중간이다.
    roughness = np.full((HEIGHT, WIDTH), 0.25, dtype=np.float32)
    roughness[land] = 0.74
    roughness[ice] = 0.58
    roughness = np.clip(roughness + mountain * 0.045, 0.0, 1.0)

    land_mask = land_f
    for item in (land_mask, roughness, ao, mountain):
        item[:, -1] = item[:, 0]
    return height, land_mask, roughness, ao, mountain


def build_albedo(
    source: np.ndarray,
    height: np.ndarray,
    mountain: np.ndarray,
) -> np.ndarray:
    water, ice, arid = classify_surface(source)
    land = ~water
    smoothed = blur_rgb(source, 0.65)
    original = source.astype(np.float32) / 255.0
    result = smoothed * 0.78 + original * 0.22

    # 팔레트는 기존 지역 구분을 지우지 않으면서 목업의 밝은 장난감 재질로 모은다.
    targets = {
        "ocean": np.array([24, 184, 207], dtype=np.float32) / 255.0,
        "ice": np.array([246, 245, 239], dtype=np.float32) / 255.0,
        "arid": np.array([237, 207, 132], dtype=np.float32) / 255.0,
        "land": np.array([150, 202, 79], dtype=np.float32) / 255.0,
    }
    vegetation = land & (~ice) & (~arid)
    for mask, target, amount in (
        (water, targets["ocean"], 0.30),
        (ice, targets["ice"], 0.42),
        (arid, targets["arid"], 0.25),
        (vegetation, targets["land"], 0.16),
    ):
        result[mask] = result[mask] * (1.0 - amount) + target * amount

    # ImageGen은 대륙 형태가 아니라 아주 작은 표면 변주만 제공한다.
    generated = load_rgb(IMAGEGEN_ALBEDO).astype(np.float32) / 255.0
    generated_gray = (
        generated[..., 0] * 0.2126
        + generated[..., 1] * 0.7152
        + generated[..., 2] * 0.0722
    )
    generated_detail = generated_gray - blur_scalar(generated_gray, 7.0)
    generated_detail = np.clip(generated_detail, -0.08, 0.08)
    edge_distance = np.minimum(np.arange(WIDTH), np.arange(WIDTH)[::-1]).astype(np.float32)
    generated_detail *= np.clip(edge_distance / 36.0, 0.0, 1.0)[None, :]
    noise = spherical_noise()
    variation = 1.0 + (noise - 0.5) * 0.035 + generated_detail * 0.14
    variation = np.where(water, 1.0 + (variation - 1.0) * 0.22, variation)
    result *= variation[..., None]

    # 고지대가 아주 조금 밝아지는 것은 재질 색 변화이며 방향성 조명은 아니다.
    altitude_tint = np.clip((height - 0.62) / 0.38, 0.0, 1.0) * land
    warm_high = np.array([0.94, 1.0, 0.82], dtype=np.float32)
    amount = (0.030 * altitude_tint + 0.012 * mountain)[..., None]
    result = result * (1.0 - amount) + warm_high * amount
    result = force_wrap_seam(np.clip(result, 0.0, 1.0), 28)
    return np.clip(np.rint(result * 255.0), 0, 255).astype(np.uint8)


def build_normal_dx(height: np.ndarray, land_mask: np.ndarray) -> np.ndarray:
    surface = blur_scalar(height, 1.15)
    dx = (np.roll(surface, -1, axis=1) - np.roll(surface, 1, axis=1)) * 0.5
    dy = np.empty_like(surface)
    dy[1:-1] = (surface[2:] - surface[:-2]) * 0.5
    dy[0] = surface[1] - surface[0]
    dy[-1] = surface[-1] - surface[-2]

    latitude = np.linspace(math.pi * 0.5, -math.pi * 0.5, HEIGHT, dtype=np.float32)
    longitude_metric = 1.0 / np.maximum(np.cos(latitude), 0.30)
    slope_x = np.clip(dx * longitude_metric[:, None] * 8.0, -0.95, 0.95)
    slope_y = np.clip(dy * 8.0, -0.95, 0.95)

    # UE는 DirectX 탄젠트 노멀을 기본으로 쓴다. V가 아래로 증가하므로 G는 -dH/dV.
    nx = -slope_x
    ny = -slope_y
    nz = np.ones_like(nx)
    length = np.sqrt(nx * nx + ny * ny + nz * nz)
    normal = np.stack((nx / length, ny / length, nz / length), axis=-1)
    normal = force_wrap_seam(normal, 16)
    length = np.linalg.norm(normal, axis=-1, keepdims=True)
    normal /= np.maximum(length, 1e-6)
    encoded = np.clip(np.rint((normal * 0.5 + 0.5) * 255.0), 0, 255).astype(np.uint8)
    encoded[:, -1] = encoded[:, 0]
    return encoded


def save_outputs(
    albedo: np.ndarray,
    height: np.ndarray,
    normal: np.ndarray,
    land_mask: np.ndarray,
    roughness: np.ndarray,
    ao: np.ndarray,
    mountain: np.ndarray,
) -> None:
    EARTH_DIR.mkdir(parents=True, exist_ok=True)
    GEN_DIR.mkdir(parents=True, exist_ok=True)
    Image.fromarray(albedo, mode="RGB").save(OUT_ALBEDO, optimize=True)
    Image.fromarray(np.rint(height * 65535.0).astype(np.uint16)).save(
        OUT_HEIGHT, optimize=True
    )
    Image.fromarray(normal, mode="RGB").save(OUT_NORMAL, optimize=True)
    masks = np.stack((height, land_mask, roughness, ao), axis=-1)
    Image.fromarray(np.rint(masks * 255.0).astype(np.uint8), mode="RGBA").save(
        OUT_MASKS, optimize=True
    )
    Image.fromarray(np.rint(land_mask * 255.0).astype(np.uint8), mode="L").save(
        OUT_LAND_DEBUG, optimize=True
    )
    Image.fromarray(np.rint(mountain * 255.0).astype(np.uint8), mode="L").save(
        OUT_MOUNTAIN_DEBUG, optimize=True
    )


def bilinear_sample(image: np.ndarray, u: np.ndarray, v: np.ndarray) -> np.ndarray:
    height, width = image.shape[:2]
    x = (u % 1.0) * (width - 1)
    y = np.clip(v, 0.0, 1.0) * (height - 1)
    x0 = np.floor(x).astype(np.int32)
    y0 = np.floor(y).astype(np.int32)
    x1 = (x0 + 1) % width
    y1 = np.minimum(y0 + 1, height - 1)
    tx = (x - x0)[..., None]
    ty = (y - y0)[..., None]
    if image.ndim == 2:
        image = image[..., None]
    a = image[y0, x0] * (1.0 - tx) + image[y0, x1] * tx
    b = image[y1, x0] * (1.0 - tx) + image[y1, x1] * tx
    return a * (1.0 - ty) + b * ty


def render_preview(
    albedo: np.ndarray,
    height_map: np.ndarray,
    normal_map: np.ndarray,
    masks: np.ndarray,
) -> None:
    pw, ph = 1600, 900
    xx, yy = np.meshgrid(np.arange(pw), np.arange(ph))
    cx, cy, base_radius = 1010.0, 455.0, 405.0
    screen_x = (xx - cx) / base_radius
    screen_y = (yy - cy) / base_radius
    radial_sq = screen_x * screen_x + screen_y * screen_y

    center_lon = math.radians(25.0)
    center_lat = math.radians(28.0)
    forward = np.array(
        [math.cos(center_lat) * math.cos(center_lon), math.cos(center_lat) * math.sin(center_lon), math.sin(center_lat)],
        dtype=np.float32,
    )
    right = np.array([-math.sin(center_lon), math.cos(center_lon), 0.0], dtype=np.float32)
    up = np.array(
        [-math.sin(center_lat) * math.cos(center_lon), -math.sin(center_lat) * math.sin(center_lon), math.cos(center_lat)],
        dtype=np.float32,
    )

    radius = np.full((ph, pw), 1.028, dtype=np.float32)
    sampled_height = np.zeros_like(radius)
    world_dir = np.zeros((ph, pw, 3), dtype=np.float32)
    valid = radial_sq <= radius * radius
    for _ in range(4):
        z = np.sqrt(np.maximum(radius * radius - radial_sq, 0.0))
        world_pos = (
            z[..., None] * forward
            + screen_x[..., None] * right
            - screen_y[..., None] * up
        )
        norm = np.linalg.norm(world_pos, axis=-1, keepdims=True)
        world_dir = world_pos / np.maximum(norm, 1e-6)
        lon = np.arctan2(world_dir[..., 1], world_dir[..., 0])
        lat = np.arcsin(np.clip(world_dir[..., 2], -1.0, 1.0))
        u = (lon + math.pi) / (math.pi * 2.0)
        v = (math.pi * 0.5 - lat) / math.pi
        sampled_height = bilinear_sample(height_map, u, v)[..., 0]
        radius = 1.0 + sampled_height * 0.028
        valid = radial_sq <= radius * radius

    lon = np.arctan2(world_dir[..., 1], world_dir[..., 0])
    lat = np.arcsin(np.clip(world_dir[..., 2], -1.0, 1.0))
    u = (lon + math.pi) / (math.pi * 2.0)
    v = (math.pi * 0.5 - lat) / math.pi
    color = bilinear_sample(albedo.astype(np.float32) / 255.0, u, v)
    tangent_normal = bilinear_sample(normal_map.astype(np.float32) / 255.0, u, v) * 2.0 - 1.0
    tangent_normal /= np.maximum(np.linalg.norm(tangent_normal, axis=-1, keepdims=True), 1e-6)
    sampled_masks = bilinear_sample(masks, u, v)

    east = np.stack((-np.sin(lon), np.cos(lon), np.zeros_like(lon)), axis=-1)
    south = np.stack(
        (np.sin(lat) * np.cos(lon), np.sin(lat) * np.sin(lon), -np.cos(lat)), axis=-1
    )
    surface_normal = (
        tangent_normal[..., 0:1] * east
        + tangent_normal[..., 1:2] * south
        + tangent_normal[..., 2:3] * world_dir
    )
    surface_normal /= np.maximum(np.linalg.norm(surface_normal, axis=-1, keepdims=True), 1e-6)

    light = forward * 0.70 - right * 0.48 + up * 0.64
    light /= np.linalg.norm(light)
    diffuse = np.clip(np.sum(surface_normal * light, axis=-1), 0.0, 1.0)
    shade = 0.64 + diffuse * 0.42
    view = forward
    half_vector = light + view
    half_vector /= np.linalg.norm(half_vector)
    ndoth = np.clip(np.sum(surface_normal * half_vector, axis=-1), 0.0, 1.0)
    roughness = sampled_masks[..., 2]
    specular = np.power(ndoth, 50.0) * (1.0 - roughness) * 0.28
    ao = sampled_masks[..., 3]
    lit = np.clip(color * (shade * ao)[..., None] + specular[..., None], 0.0, 1.0)

    left = np.array([253, 248, 231], dtype=np.float32) / 255.0
    right_bg = np.array([213, 245, 242], dtype=np.float32) / 255.0
    gradient = np.linspace(0.0, 1.0, pw, dtype=np.float32)[None, :, None]
    background = np.broadcast_to(left * (1.0 - gradient) + right_bg * gradient, (ph, pw, 3)).copy()
    glow = np.exp(-np.square(np.maximum(np.sqrt(radial_sq) - 1.015, 0.0)) / 0.0035) * 0.16
    background += glow[..., None] * np.array([0.65, 1.0, 0.87], dtype=np.float32)
    output = np.where(valid[..., None], lit, np.clip(background, 0.0, 1.0))

    image = Image.fromarray(np.rint(output * 255.0).astype(np.uint8), mode="RGB")
    draw = ImageDraw.Draw(image)
    # 작은 색상칩으로 재질 분리를 보여 주되 게임 UI나 설명 문구는 넣지 않는다.
    chips = ((72, 695, 42, 184, 207), (128, 695, 150, 202, 79), (184, 695, 237, 207, 132))
    for x, y, red, green, blue in chips:
        draw.ellipse((x, y, x + 36, y + 36), fill=(red, green, blue), outline=(255, 255, 255), width=3)
    PREVIEW_PATH.parent.mkdir(parents=True, exist_ok=True)
    image.save(PREVIEW_PATH, optimize=True)


def generate_uv_sphere() -> None:
    MESH_DIR.mkdir(parents=True, exist_ok=True)
    lines: list[str] = [
        "# FestaFiesta casual relief UV sphere",
        f"# radius_cm={SPHERE_RADIUS_CM} segments={SPHERE_SEGMENTS} rings={SPHERE_RINGS}",
        "# north_pole_v=0 south_pole_v=1 seam=+X",
        "o SM_FF_Earth_UVSphere_R50cm_256x128",
    ]

    vertices: list[tuple[float, float, float]] = []
    uvs: list[tuple[float, float]] = []
    normals: list[tuple[float, float, float]] = []
    faces: list[tuple[int, int, int]] = []

    def add_vertex(
        position: tuple[float, float, float],
        uv: tuple[float, float],
        normal: tuple[float, float, float],
    ) -> int:
        vertices.append(position)
        uvs.append(uv)
        normals.append(normal)
        return len(vertices)

    # Each cap triangle gets its own pole UV to reduce equirectangular pinching.
    top: list[int] = []
    for segment in range(SPHERE_SEGMENTS):
        u_mid = (segment + 0.5) / SPHERE_SEGMENTS
        top.append(
            add_vertex(
                (0.0, 0.0, SPHERE_RADIUS_CM),
                (u_mid, 0.0),
                (0.0, 0.0, 1.0),
            )
        )

    rows: list[list[int]] = []
    for ring in range(1, SPHERE_RINGS):
        v = ring / SPHERE_RINGS
        phi = math.pi * v
        sin_phi = math.sin(phi)
        cos_phi = math.cos(phi)
        row: list[int] = []
        for segment in range(SPHERE_SEGMENTS + 1):
            u = segment / SPHERE_SEGMENTS
            theta = math.pi * 2.0 * u
            nx = sin_phi * math.cos(theta)
            ny = sin_phi * math.sin(theta)
            nz = cos_phi
            row.append(
                add_vertex(
                    (
                        nx * SPHERE_RADIUS_CM,
                        ny * SPHERE_RADIUS_CM,
                        nz * SPHERE_RADIUS_CM,
                    ),
                    (u, v),
                    (nx, ny, nz),
                )
            )
        rows.append(row)

    bottom: list[int] = []
    for segment in range(SPHERE_SEGMENTS):
        u_mid = (segment + 0.5) / SPHERE_SEGMENTS
        bottom.append(
            add_vertex(
                (0.0, 0.0, -SPHERE_RADIUS_CM),
                (u_mid, 1.0),
                (0.0, 0.0, -1.0),
            )
        )

    first_row = rows[0]
    for segment in range(SPHERE_SEGMENTS):
        faces.append((top[segment], first_row[segment], first_row[segment + 1]))

    for row_index in range(len(rows) - 1):
        upper = rows[row_index]
        lower = rows[row_index + 1]
        for segment in range(SPHERE_SEGMENTS):
            upper_left = upper[segment]
            upper_right = upper[segment + 1]
            lower_left = lower[segment]
            lower_right = lower[segment + 1]
            faces.append((upper_left, lower_left, upper_right))
            faces.append((upper_right, lower_left, lower_right))

    last_row = rows[-1]
    for segment in range(SPHERE_SEGMENTS):
        faces.append((last_row[segment], bottom[segment], last_row[segment + 1]))

    for x, y, z in vertices:
        lines.append(f"v {x:.9f} {y:.9f} {z:.9f}")
    for u, v in uvs:
        lines.append(f"vt {u:.9f} {v:.9f}")
    for nx, ny, nz in normals:
        lines.append(f"vn {nx:.9f} {ny:.9f} {nz:.9f}")

    lines.extend(("usemtl M_FF_Earth_CasualRelief", "s 1"))
    for a, b, c in faces:
        lines.append(f"f {a}/{a}/{a} {b}/{b}/{b} {c}/{c}/{c}")
    OUT_SPHERE.write_text("\n".join(lines) + "\n", encoding="utf-8", newline="\n")


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def validate_outputs() -> dict[str, object]:
    files = (OUT_ALBEDO, OUT_HEIGHT, OUT_NORMAL, OUT_MASKS, OUT_SPHERE, PREVIEW_PATH)
    for path in files:
        if not path.exists() or path.stat().st_size == 0:
            raise RuntimeError(f"생성 실패 또는 빈 파일: {path}")

    texture_report: dict[str, object] = {}
    for path in (OUT_ALBEDO, OUT_HEIGHT, OUT_NORMAL, OUT_MASKS):
        image = Image.open(path)
        data = np.asarray(image)
        if image.size != (WIDTH, HEIGHT):
            raise RuntimeError(f"크기 오류: {path.name} = {image.size}")
        seam = np.abs(data[:, 0].astype(np.int64) - data[:, -1].astype(np.int64))
        seam_max = int(seam.max())
        if seam_max != 0:
            raise RuntimeError(f"좌우 이음새 오류: {path.name} max={seam_max}")
        texture_report[path.name] = {
            "mode": image.mode,
            "size": image.size,
            "seam_max": seam_max,
            "sha256": sha256(path),
        }

    obj_lines = OUT_SPHERE.read_text(encoding="utf-8").splitlines()
    obj_report = {
        "vertices": sum(line.startswith("v ") for line in obj_lines),
        "uvs": sum(line.startswith("vt ") for line in obj_lines),
        "normals": sum(line.startswith("vn ") for line in obj_lines),
        "triangles": sum(line.startswith("f ") for line in obj_lines),
        "sha256": sha256(OUT_SPHERE),
    }
    expected_vertices = (
        (SPHERE_RINGS - 1) * (SPHERE_SEGMENTS + 1)
        + 2 * SPHERE_SEGMENTS
    )
    expected_triangles = SPHERE_SEGMENTS * 2 * (SPHERE_RINGS - 1)
    if obj_report["vertices"] != expected_vertices or obj_report["triangles"] != expected_triangles:
        raise RuntimeError(f"OBJ 통계 오류: {obj_report}")

    return {
        "textures": texture_report,
        "mesh": obj_report,
        "preview": {"path": str(PREVIEW_PATH), "sha256": sha256(PREVIEW_PATH)},
    }


def main() -> None:
    missing = [str(path) for path in (SOURCE_ALBEDO, IMAGEGEN_ALBEDO, SOURCE_NORMAL) if not path.exists()]
    if missing:
        raise FileNotFoundError("필수 원본이 없습니다:\n" + "\n".join(missing))

    source_albedo = load_rgb(SOURCE_ALBEDO)
    height, land_mask, roughness, ao, mountain = build_height_and_masks(source_albedo)
    albedo = build_albedo(source_albedo, height, mountain)
    normal = build_normal_dx(height, land_mask)
    masks = np.stack((height, land_mask, roughness, ao), axis=-1)

    save_outputs(albedo, height, normal, land_mask, roughness, ao, mountain)
    render_preview(albedo, height, normal, masks)
    generate_uv_sphere()
    report = validate_outputs()
    manifest = {
        "schema_version": 1,
        "asset_set": "FestaFiesta Title Casual Relief v1",
        "projection": "2:1 equirectangular",
        "mask_channels": {
            "R": "height (ocean 0, land plateau plus mountains)",
            "G": "land mask",
            "B": "roughness",
            "A": "ambient occlusion (1 is neutral)",
        },
        "source_policy": "Original assets are never overwritten.",
        **report,
    }
    OUT_MANIFEST.write_text(
        json.dumps(manifest, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
        newline="\n",
    )
    print(json.dumps(manifest, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
