from pathlib import Path
import json,unreal
root=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);level.load_level('/Game/FestaFiesta/Maps/Frontend')
report=[]
for actor in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
 if isinstance(actor,unreal.CameraActor) and actor.get_actor_label() in ('CAM_AtlasTitle','CAM_AtlasGlobe'):
  c=actor.get_component_by_class(unreal.CameraComponent)
  before={k:str(c.get_editor_property(k)) for k in ['constrain_aspect_ratio','aspect_ratio','override_aspect_ratio_axis_constraint','aspect_ratio_axis_constraint']}
  c.set_editor_property('constrain_aspect_ratio',False)
  c.set_editor_property('override_aspect_ratio_axis_constraint',True)
  c.set_editor_property('aspect_ratio_axis_constraint',unreal.AspectRatioAxisConstraint.ASPECT_RATIO_MAINTAIN_YFOV)
  report.append({'camera':actor.get_actor_label(),'before':before,'constrain_aspect_ratio':c.get_editor_property('constrain_aspect_ratio')})
assert len(report)==2
assert level.save_current_level()
(root/'Saved/Atlas_aspect_fix.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')

