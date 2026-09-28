"""첫 실제 화면에서 확인한 지도 데이터 채널/브러시 유형을 마감하고 재확인한다."""
from pathlib import Path
import runpy
import unreal

ROOT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
height = unreal.load_asset('/Game/FestaFiesta/UI/Atlas/Korea/CleanBright/T_KoreaCleanBrightHeight')
height.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_GRAYSCALE)
assert unreal.EditorAssetLibrary.save_loaded_asset(height, only_if_is_dirty=False)
country = unreal.load_asset('/Game/FestaFiesta/UI/Atlas/WBP_FF_AtlasCountry_Natural')
tree = unreal.find_object(country, 'WidgetTree')
image = unreal.find_object(tree, 'Image_Backdrop', unreal.Image)
brush = image.get_editor_property('brush')
assert brush.get_editor_property('resource_object').get_name() == 'MI_KoreaCleanBrightBackdrop'
brush.set_editor_property('draw_as', unreal.SlateBrushDrawType.IMAGE)
brush.set_editor_property('tint_color', unreal.SlateColor(specified_color=unreal.LinearColor(1., 1., 1., 1.)))
image.set_brush(brush)
unreal.BlueprintEditorLibrary.compile_blueprint(country)
assert unreal.EditorAssetLibrary.save_loaded_asset(country, only_if_is_dirty=False)
pin_path = '/Game/FestaFiesta/UI/Atlas/WBP_FF_AtlasPin_Natural'
pin = unreal.load_asset(pin_path)
pin_tree = unreal.find_object(pin, 'WidgetTree')
pin_root = unreal.find_object(pin_tree, 'SizeBox_Root', unreal.SizeBox)
pin_root.set_width_override(128.)
pin_root.set_height_override(116.)
unreal.find_object(pin_tree, 'Text_Label', unreal.TextBlock).set_editor_property('wrap_text_at', 95.)
unreal.find_object(pin_tree, 'Border_PinLabel', unreal.Border).set_padding(unreal.Margin(4., 5., 4., 6.))
unreal.BlueprintEditorLibrary.compile_blueprint(pin)
pin_defaults = unreal.get_default_object(unreal.EditorAssetLibrary.load_blueprint_class(pin_path))
pin_defaults.set_editor_property('map_anchor', unreal.Vector2D(.5, 18./116.))
assert unreal.EditorAssetLibrary.save_loaded_asset(pin, only_if_is_dirty=False)
runpy.run_path(str(ROOT/'Tools/ui/bright_country_hud/capture.py'),
               init_globals={'FF_CAPTURE_MAP_ONLY': True})
