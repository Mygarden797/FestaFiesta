import unreal

asset = unreal.load_asset('/Game/FestaFiesta/UI/Facility/WBP_FF_FacilityInfoPanel')
if not asset:
    raise RuntimeError('facility info widget asset did not load')

generated_class = asset.generated_class()
default_object = unreal.get_default_object(generated_class)
unreal.log_warning(f'FF_WIDGET_INSPECT asset attrs: {[name for name in dir(asset) if "tree" in name.lower() or "class" in name.lower()]}')
unreal.log_warning(f'FF_WIDGET_INSPECT class attrs: {[name for name in dir(generated_class) if "tree" in name.lower() or "widget" in name.lower()]}')
unreal.log_warning(f'FF_WIDGET_INSPECT cdo attrs: {[name for name in dir(default_object) if "tree" in name.lower() or "widget" in name.lower()]}')
