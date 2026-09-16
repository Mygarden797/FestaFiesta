"""P10 햇살 타이틀의 재질·조명. 지형/카메라/국가 데이터는 변경하지 않는다."""
import unreal

BASE = '/Game/FestaFiesta/UI/Atlas/Earth/BrightGlobe_v1'
DEST = BASE + '/P10'
mel = unreal.MaterialEditingLibrary


def save(obj):
    assert unreal.EditorAssetLibrary.save_loaded_asset(obj, False), obj.get_path_name()


def new_asset(name, cls, factory):
    assert not unreal.EditorAssetLibrary.does_asset_exist(DEST + '/' + name), name
    obj = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, DEST, cls, factory())
    assert obj
    return obj


def build_background():
    mat = new_asset('M_BrightGlobe_SunnyBackdrop', unreal.Material, unreal.MaterialFactoryNew)
    mat.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property('two_sided', True)

    def node(cls, **props):
        obj = mel.create_material_expression(mat, cls, 0, 0)
        for key, value in props.items():
            obj.set_editor_property(key, value)
        return obj

    custom = node(unreal.MaterialExpressionCustom,
                  output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT3,
                  description='화면 오른쪽 위의 맑은 하늘과 은은한 햇살')
    inputs = []
    names = ['UV', 'ViewSize', 'Cream', 'Sky', 'SunPosition', 'Brightness',
             'GradientStart', 'GradientEnd', 'SunGlow', 'SunRadius', 'RayStrength']
    for name in names:
        entry = unreal.CustomInput()
        entry.set_editor_property('input_name', name)
        inputs.append(entry)
    custom.set_editor_property('inputs', inputs)
    custom.set_editor_property('code', '''
float t = smoothstep(GradientStart, max(GradientStart + 0.01, GradientEnd), UV.x);
t *= 1.0 - 0.10 * smoothstep(0.35, 1.0, UV.y);
float3 color = lerp(Cream, Sky, t);
float2 p = (UV - SunPosition.xy) * float2(ViewSize.x / max(ViewSize.y, 1.0), 1.0);
float r2 = dot(p, p);
float halo = exp(-r2 / max(SunRadius * SunRadius, 0.001));
float core = exp(-r2 / 0.004);
float a = atan2(p.y, p.x);
float rays = pow(saturate(0.5 + 0.5 * cos(a * 12.0)), 24.0) * exp(-r2 / 0.22);
color += float3(1.0, 0.98, 0.94) * (SunGlow * (halo + core) + RayStrength * rays);
return max(color, 0.0) * Brightness;
''')
    uv = node(unreal.MaterialExpressionScreenPosition)
    assert mel.connect_material_expressions(uv, 'ViewportUV', custom, 'UV')
    size = node(unreal.MaterialExpressionViewSize)
    assert mel.connect_material_expressions(size, '', custom, 'ViewSize')
    vectors = {'Cream': (1., .97, .88), 'Sky': (.025, .21, .88), 'SunPosition': (.94, -.055, 0.)}
    for name, value in vectors.items():
        expr = node(unreal.MaterialExpressionVectorParameter, parameter_name=name,
                    default_value=unreal.LinearColor(*value, 1.))
        assert mel.connect_material_expressions(expr, 'RGB', custom, name)
    scalars = {'Brightness': 4.2, 'GradientStart': .22, 'GradientEnd': .66,
               'SunGlow': .60, 'SunRadius': .30, 'RayStrength': .07}
    for name, value in scalars.items():
        expr = node(unreal.MaterialExpressionScalarParameter, parameter_name=name, default_value=value)
        assert mel.connect_material_expressions(expr, '', custom, name)
    assert mel.connect_material_property(custom, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.layout_material_expressions(mat)
    mel.recompile_material(mat)
    save(mat)
    mi = new_asset('MI_BrightGlobe_SunnyBackdrop', unreal.MaterialInstanceConstant,
                   unreal.MaterialInstanceConstantFactoryNew)
    mel.set_material_instance_parent(mi, mat)
    save(mi)
    return mi


def build_land():
    result = []
    palettes = {
        'Land': {'GrassColor': (.33, .66, .12), 'ForestColor': (.10, .42, .11),
                 'SandColor': (.98, .79, .41), 'SnowColor': (.94, .98, 1.)},
        'Coast': {'CoastColor': (.85, .69, .40)},
    }
    for label, palette in palettes.items():
        mi = unreal.EditorAssetLibrary.duplicate_asset(BASE + '/P05/MI_BrightGlobe_' + label,
                                                       DEST + '/MI_BrightGlobe_Sunny' + label)
        assert mi
        for name, rgb in palette.items():
            mel.set_material_instance_vector_parameter_value(mi, name, unreal.LinearColor(*rgb, 1.))
            read = mel.get_material_instance_vector_parameter_value(mi, name)
            assert max(abs(x - y) for x, y in zip((read.r, read.g, read.b), rgb)) < 1.e-5
        save(mi)
        result.append(mi)
    return result


def snapshot():
    actors = list(unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors())
    globe = next(a for a in actors if isinstance(a, unreal.FF_AtlasGlobeActor))
    cameras = {a.get_actor_label(): {'transform': a.get_actor_transform().export_text(),
                'fov': a.get_component_by_class(unreal.CameraComponent).get_editor_property('field_of_view'),
                'aspect_constrained': a.get_component_by_class(unreal.CameraComponent).get_editor_property('constrain_aspect_ratio')}
               for a in actors if isinstance(a, unreal.CameraActor)}
    location = globe.get_editor_property('start_lat_long')
    return {'cameras': cameras, 'globe_transform': globe.get_actor_transform().export_text(),
            'root_rotation': globe.get_editor_property('globe_root').get_editor_property('relative_rotation').export_text(),
            'radius': globe.get_editor_property('globe_radius'),
            'mesh_paths': [m.get_path_name() for m in globe.get_editor_property('land_layer_meshes')],
            'start_lat_long': {key: float(location.get_editor_property(key)) for key in ('latitude', 'longitude')},
            'start_facing_first_country': globe.get_editor_property('start_facing_first_country'),
            'yaw_offset': globe.get_editor_property('globe_texture_yaw_offset')}


def configure():
    actors = list(unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors())
    def find(label):
        matches = [a for a in actors if a.get_actor_label() == label]
        assert len(matches) == 1, label
        return matches[0]
    globe = next(a for a in actors if isinstance(a, unreal.FF_AtlasGlobeActor))
    water = unreal.load_asset(DEST + '/MI_BrightGlobe_SunnyWater')
    land = unreal.load_asset(DEST + '/MI_BrightGlobe_SunnyLand')
    coast = unreal.load_asset(DEST + '/MI_BrightGlobe_SunnyCoast')
    backdrop = unreal.load_asset(DEST + '/MI_BrightGlobe_SunnyBackdrop')
    assert all((water, land, coast, backdrop))
    globe.set_editor_property('globe_material', water)
    globe.get_editor_property('globe_mesh').set_material(0, water)
    layers = list(globe.get_editor_property('land_layer_components'))
    assert len(layers) == 16
    for i, comp in enumerate(layers):
        comp.set_material(0, land if i % 2 == 0 else coast)
    find('FF_Clay_Backdrop').static_mesh_component.set_material(0, backdrop)
    lighting = []
    for label, intensity, color, rotation in [
        ('DirectionalLight', 4.0, (1., .99, .96), (-38., -28., 0.)),
        ('FF_Clay_FillLight', .48, (.87, .95, 1.), (-12., 48., 0.))]:
        actor = find(label)
        actor.set_actor_rotation(unreal.Rotator(*rotation), False)
        light = actor.get_component_by_class(unreal.DirectionalLightComponent)
        light.set_intensity(intensity)
        light.set_light_color(unreal.LinearColor(*color, 1.))
        light.set_editor_property('light_source_angle', 5.)
        lighting.append({'label': label, 'intensity': intensity, 'rgb': color, 'rotation': rotation})
    sky = find('FF_Clay_SkyFill').get_component_by_class(unreal.SkyLightComponent)
    sky.set_intensity(.14)
    # 화면 좌표 그라데이션은 환경맵이 아니다. 저장된 환경을 다시 캡처하지 않는다.
    pp = next(a for a in actors if isinstance(a, unreal.PostProcessVolume))
    settings = pp.get_editor_property('settings')
    for key, value in {'override_bloom_intensity': True, 'bloom_intensity': .22,
                       'override_bloom_threshold': True, 'bloom_threshold': 1.5,
                       'override_lens_flare_intensity': True, 'lens_flare_intensity': .025,
                       'override_lens_flare_threshold': True, 'lens_flare_threshold': 4.,
                       'override_vignette_intensity': True, 'vignette_intensity': 0.,
                       'override_ambient_occlusion_intensity': True, 'ambient_occlusion_intensity': .30}.items():
        settings.set_editor_property(key, value)
    pp.set_editor_property('settings', settings)
    return {'lights': lighting, 'sky_intensity': .14, 'bloom': .22, 'bloom_threshold': 1.5,
            'lens_flare_intensity': .025, 'vignette': 0., 'ao': .30,
            'quality_independent_sun_glow': True, 'exposure_unchanged': True,
            'water': water.get_path_name(), 'land': land.get_path_name(),
            'coast': coast.get_path_name(), 'backdrop': backdrop.get_path_name()}
