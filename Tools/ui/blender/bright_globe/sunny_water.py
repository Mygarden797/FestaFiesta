"""P10 맑은 바다 재질 생성. import만으로 에셋이나 레벨을 변경하지 않는다."""

import unreal


DESTINATION = '/Game/FestaFiesta/UI/Atlas/Earth/BrightGlobe_v1/P10'
MASK_PATH = '/Game/FestaFiesta/UI/Atlas/Earth/BrightGlobe_v1/P03v2/T_Bright_LandClass'
VECTOR_DEFAULTS = {
    'OceanColor': (.003, .155, .90),
    'ShallowColor': (.025, .64, .90),
    'PolarIceColor': (.91, .95, 1.),
}
SCALAR_DEFAULTS = {
    'CoastWidthTexels': 2.6,
    'OceanLatitudeVariation': .04,
    'OceanRoughness': .34,
    'OceanSpecular': .28,
    'OceanEmissive': .028,
}


def build_water(dest):
    """P10 재질과 MI를 저장하고 MI 객체를 반환한다. 호출자가 레벨에 연결한다.

    기존 P05 재질·마스크·메시를 수정하지 않는다. 재호출은 이 모듈 소유의
    P10 재질 두 개만 재생성하므로 호출 전에 상위 실행기가 백업해야 한다.
    """
    dest = dest.rstrip('/')
    if dest != DESTINATION:
        raise ValueError('맑은 바다 재질은 P10 전용 경로에서만 생성합니다: ' + dest)

    assets = unreal.EditorAssetLibrary
    mel = unreal.MaterialEditingLibrary
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    mask = unreal.load_asset(MASK_PATH)
    if not isinstance(mask, unreal.Texture2D):
        raise RuntimeError('2K 지리 분류 마스크를 찾을 수 없습니다: ' + MASK_PATH)
    # 공유 마스크 설정은 읽기 검증만 한다. U만 wrap하고 극점 V는 clamp한다.
    expected = {
        'srgb': False,
        'address_x': unreal.TextureAddress.TA_WRAP,
        'address_y': unreal.TextureAddress.TA_CLAMP,
        'compression_settings': unreal.TextureCompressionSettings.TC_GRAYSCALE,
        'mip_gen_settings': unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS,
    }
    for key, value in expected.items():
        if mask.get_editor_property(key) != value:
            raise RuntimeError('공유 지리 마스크 설정이 원본 계약과 다릅니다: ' + key)

    def asset(name, cls, factory):
        path = dest + '/' + name
        result = unreal.load_asset(path) if assets.does_asset_exist(path) else tools.create_asset(name, dest, cls, factory)
        if not isinstance(result, cls):
            raise RuntimeError('에셋 생성 또는 형식 확인 실패: ' + path)
        return result

    material = asset('M_BrightGlobe_SunnyWater', unreal.Material, unreal.MaterialFactoryNew())
    mel.delete_all_material_expressions(material)

    def node(cls, **properties):
        result = mel.create_material_expression(material, cls, 0, 0)
        if result is None:
            raise RuntimeError('머티리얼 노드 생성 실패: ' + str(cls))
        for key, value in properties.items():
            result.set_editor_property(key, value)
        return result

    def wire(source, target, pin):
        if not mel.connect_material_expressions(source, '', target, pin):
            raise RuntimeError('머티리얼 핀 연결 실패: ' + pin)

    def constant(value):
        return node(unreal.MaterialExpressionConstant, r=value)

    def binary(cls, left, right):
        result = node(cls)
        wire(left, result, 'A')
        wire(right, result, 'B')
        return result

    def add(left, right):
        return binary(unreal.MaterialExpressionAdd, left, right)

    def mul(left, right):
        return binary(unreal.MaterialExpressionMultiply, left, right)

    def sub(left, right):
        return binary(unreal.MaterialExpressionSubtract, left, right)

    def saturate(source):
        result = node(unreal.MaterialExpressionSaturate)
        wire(source, result, '')
        return result

    def channel(source, index):
        result = node(unreal.MaterialExpressionComponentMask, r=index == 0, g=index == 1, b=False, a=False)
        wire(source, result, '')
        return result

    def mix(left, right, alpha):
        result = node(unreal.MaterialExpressionLinearInterpolate)
        wire(left, result, 'A')
        wire(right, result, 'B')
        wire(alpha, result, 'Alpha')
        return result

    vectors = {
        name: node(unreal.MaterialExpressionVectorParameter,
                   parameter_name=name, default_value=unreal.LinearColor(*rgb, 1.))
        for name, rgb in VECTOR_DEFAULTS.items()
    }
    scalars = {
        name: node(unreal.MaterialExpressionScalarParameter,
                   parameter_name=name, default_value=value)
        for name, value in SCALAR_DEFAULTS.items()
    }
    one = constant(1.)
    byte_scale = constant(255.)
    uv = node(unreal.MaterialExpressionTextureCoordinate)
    longitude = node(unreal.MaterialExpressionFrac)
    wire(add(channel(uv, 0), constant(.5)), longitude, '')
    latitude = sub(one, channel(uv, 1))
    geography_uv = binary(unreal.MaterialExpressionAppendVector, longitude, latitude)

    # P05와 같은 9회 표본. 반경만 2→2.6 texel로 늘려 시안 해안을 조금 넓힌다.
    # 분류 원값 0/1/2를 먼저 복원하므로 낮은 정밀도의 밉 평균을 사용하지 않는다.
    taps = []
    ice = None
    for dx, dy in [(0, 0), (-1, 0), (1, 0), (0, -1), (0, 1),
                   (-1, -1), (1, -1), (-1, 1), (1, 1)]:
        coords = geography_uv
        if dx or dy:
            direction = node(unreal.MaterialExpressionConstant2Vector, r=dx / 2048., g=dy / 1024.)
            coords = add(geography_uv, mul(direction, scalars['CoastWidthTexels']))
        sample = node(
            unreal.MaterialExpressionTextureSample,
            texture=mask,
            sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_GRAYSCALE,
            mip_value_mode=unreal.TextureMipValueMode.TMVM_MIP_LEVEL,
            const_mip_value=0,
        )
        wire(coords, sample, 'UVs')
        classification = mul(channel(sample, 0), byte_scale)
        taps.append(saturate(classification))
        if dx == 0 and dy == 0:
            # 얼음은 해안 확장값이 아니라 원본 중심 표본의 class 2로 결정한다.
            ice = saturate(sub(classification, one))
    coast = taps[0]
    for tap in taps[1:]:
        coast = add(coast, tap)
    coast = mul(coast, constant(1. / len(taps)))

    # 회전·경도 이음선에 연속적인 위도 변화만 사용한다. 바다 명도 차이는 최대 4%.
    # 노멀/시간/노이즈를 쓰지 않아 거친 반짝임이나 흐르는 무늬를 만들지 않는다.
    equatorial_weight = mul(mul(latitude, sub(one, latitude)), constant(4.))
    ocean_scale = sub(one, mul(sub(one, equatorial_weight), scalars['OceanLatitudeVariation']))
    ocean = mul(vectors['OceanColor'], ocean_scale)
    color = mix(ocean, vectors['ShallowColor'], coast)
    surface = mix(color, vectors['PolarIceColor'], ice)
    roughness = mix(scalars['OceanRoughness'], constant(.78), ice)
    specular = mix(scalars['OceanSpecular'], constant(.16), ice)
    outputs = [
        (surface, unreal.MaterialProperty.MP_BASE_COLOR),
        (roughness, unreal.MaterialProperty.MP_ROUGHNESS),
        (specular, unreal.MaterialProperty.MP_SPECULAR),
        (mul(surface, scalars['OceanEmissive']), unreal.MaterialProperty.MP_EMISSIVE_COLOR),
    ]
    for expression, prop in outputs:
        if not mel.connect_material_property(expression, '', prop):
            raise RuntimeError('머티리얼 출력 연결 실패: ' + str(prop))
    mel.layout_material_expressions(material)
    mel.recompile_material(material)
    if not assets.save_loaded_asset(material, False):
        raise RuntimeError('바다 부모 재질 저장 실패')

    instance = asset('MI_BrightGlobe_SunnyWater', unreal.MaterialInstanceConstant,
                     unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(instance, material)
    for name, rgb in VECTOR_DEFAULTS.items():
        # UE 5.7의 벡터 setter는 적용 후에도 false를 반환한다. 실제 값을 확인한다.
        mel.set_material_instance_vector_parameter_value(instance, name, unreal.LinearColor(*rgb, 1.))
        actual = mel.get_material_instance_vector_parameter_value(instance, name)
        if max(abs(a - b) for a, b in zip((actual.r, actual.g, actual.b), rgb)) > 1.e-6:
            raise RuntimeError('바다 벡터 파라미터 검증 실패: ' + name)
    for name, value in SCALAR_DEFAULTS.items():
        mel.set_material_instance_scalar_parameter_value(instance, name, value)
        if abs(mel.get_material_instance_scalar_parameter_value(instance, name) - value) > 1.e-6:
            raise RuntimeError('바다 스칼라 파라미터 검증 실패: ' + name)
    mel.update_material_instance(instance)
    if not assets.save_loaded_asset(instance, False):
        raise RuntimeError('바다 인스턴스 저장 실패')
    return instance
