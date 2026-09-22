# -*- coding: utf-8 -*-
# capture_expansion70.py
# 역할: 시설 모델·Data Asset·썸네일 작업에서 실행 화면이나 모델을 촬영해 시각 검증 자료를 만든다.

"""GPU galleries of imported native Facility70 assets, using a transient Entry map.

Run an unattended UnrealEditor-Cmd /Engine/Maps/Entry -RenderOffscreen with
-ExecutePythonScript=<this file>, a real RHI, and optional FF70_NUMBERS filter.
Only PNGs/reports under Saved are written; no project package or map is saved.
"""
import json
import math
from pathlib import Path
import sys
import time
import traceback

import unreal

sys.path.insert(0, str(Path(__file__).resolve().parent))
import import_expansion70 as pipeline

OUT = pipeline.OUT / "renders"


def start():
    OUT.mkdir(parents=True, exist_ok=True)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    world = editor.get_editor_world()
    pipeline.check(world and world.get_path_name().startswith("/Engine/Maps/Entry."), "Use the transient Entry map")
    manifest, entries, palette = pipeline.load_inputs()
    spawned, records, groups = [], [], {}
    state = dict(status="RUNNING", started=time.time(), phase=0, frame=0, captures=[],
                 map_saved=False, package_writes=False, expected_facilities=len(entries))

    def report():
        (OUT / "capture_report.json").write_text(json.dumps(
            {key: value for key, value in state.items() if key not in {"callback", "busy"}},
            ensure_ascii=False, indent=2), encoding="utf-8")

    def spawn(cls, location, rotation=None):
        actor = actors.spawn_actor_from_class(cls, unreal.Vector(*location), rotation or unreal.Rotator())
        pipeline.check(actor, "Could not spawn " + str(cls))
        spawned.append(actor)
        return actor

    by_category = {}
    for entry in entries:
        by_category.setdefault(entry["category"], []).append(entry)
    for category, items in by_category.items():
        maximum_width = max(pipeline.authored_dimensions(item)[0][0] for item in items)
        maximum_depth = max(pipeline.authored_dimensions(item)[0][1] for item in items)
        maximum_height = max(pipeline.authored_dimensions(item)[1] for item in items)
        columns = min(4, len(items))
        rows = math.ceil(len(items) / columns)
        pitch_x, pitch_y = maximum_width + 180, maximum_depth + 180
        for index, entry in enumerate(items):
            paths = pipeline.paths(entry)
            definition = pipeline.owned_existing(paths["definition"], entry["id"])
            pipeline.check(definition, "Import before capture: " + entry["id"])
            spec = definition.get_editor_property("spec")
            mesh = spec.get_editor_property("visual_mesh")
            bounds = mesh.get_bounding_box()
            size = bounds.max - bounds.min
            footprint = spec.get_editor_property("footprint_size_cells")
            scale = unreal.Vector(footprint.x / size.x, footprint.y / size.y,
                                  spec.get_editor_property("placement_obstacle_height_cm") / size.z)
            center = (bounds.min + bounds.max) * .5
            position = [(index % columns - (columns - 1) * .5) * pitch_x - center.x * scale.x,
                        (index // columns - (rows - 1) * .5) * pitch_y - center.y * scale.y,
                        -bounds.min.z * scale.z]
            model = spawn(unreal.StaticMeshActor, position)
            model.set_actor_label(entry["id"])
            component = model.static_mesh_component
            component.set_mobility(unreal.ComponentMobility.MOVABLE)
            component.set_static_mesh(mesh)
            component.set_collision_profile_name("NoCollision")
            model.set_actor_scale3d(scale)
            rig = None
            if category == "Lighting":
                rig = spawn(spec.get_editor_property("lighting_settings").get_editor_property("lighting_rig_class"), position)
                rig.set_actor_scale3d(scale)
            record = dict(id=entry["id"], category=category, model=model, component=component, rig=rig)
            records.append(record)
            groups.setdefault(category, []).append(record)
        span_x = columns * pitch_x
        span_y = rows * pitch_y
        span = max(span_x * .8, span_y, maximum_height * 2, 650)
        by_category[category] = dict(camera=[span * .9, span * 1.05, span * .95],
                                     target=[0, 0, maximum_height * .25],
                                     span=span, ids=[item["id"] for item in items])

    ground = spawn(unreal.StaticMeshActor, [0, 0, -15])
    ground.static_mesh_component.set_static_mesh(unreal.load_asset("/Engine/BasicShapes/Cube"))
    ground.static_mesh_component.set_material(0, unreal.load_asset("/Engine/BasicShapes/BasicShapeMaterial"))
    largest_span = max(value["span"] for value in by_category.values())
    ground.set_actor_scale3d(unreal.Vector(largest_span * .05, largest_span * .05, .25))
    light_components = []
    for label, rotation, intensity in [("Key", unreal.Rotator(pitch=-48, yaw=-120, roll=0), 2.4),
                                        ("Fill", unreal.Rotator(pitch=-35, yaw=45, roll=0), 1.0)]:
        actor = spawn(unreal.DirectionalLight, [0, 0, 2000], rotation)
        actor.set_actor_label("F70Capture_" + label)
        component = actor.get_component_by_class(unreal.DirectionalLightComponent)
        component.set_mobility(unreal.ComponentMobility.MOVABLE)
        component.set_editor_property("cast_shadows", label == "Key")
        component.set_intensity(intensity)
        light_components.append(component)

    capture_actor = spawn(unreal.SceneCapture2D, [0, 0, 0])
    capture = capture_actor.get_component_by_class(unreal.SceneCaptureComponent2D)
    render_target = unreal.RenderingLibrary.create_render_target2d(world, 1800, 1200,
                                                                   unreal.TextureRenderTargetFormat.RTF_RGBA8)
    for key, value in dict(texture_target=render_target,
                           capture_source=unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR,
                           projection_type=unreal.CameraProjectionMode.PERSPECTIVE, fov_angle=48.0,
                           capture_every_frame=False, capture_on_movement=False,
                           always_persist_rendering_state=True,
                           primitive_render_mode=unreal.SceneCapturePrimitiveRenderMode.PRM_USE_SHOW_ONLY_LIST).items():
        capture.set_editor_property(key, value)
    pp = capture.get_editor_property("post_process_settings")
    for key, value in dict(override_auto_exposure_min_brightness=True, override_auto_exposure_max_brightness=True,
                           auto_exposure_min_brightness=1.0, auto_exposure_max_brightness=1.0,
                           override_bloom_intensity=True, bloom_intensity=.12,
                           override_vignette_intensity=True, vignette_intensity=0.0).items():
        pp.set_editor_property(key, value)
    capture.set_editor_property("post_process_settings", pp)
    phases = [dict(category=category, mode="ON", name="UE_" + category, **data)
              for category, data in by_category.items()]
    if "Lighting" in by_category:
        phases += [dict(category="Lighting", mode=mode, name="UE_Lighting_" + mode,
                        **by_category["Lighting"]) for mode in ["OFF", "BLUE"]]
    state["expected_captures"] = len(phases)
    unreal.EditorPythonScripting.set_keep_python_script_alive(True)
    report()

    def finish():
        handle = state.pop("callback", None)
        if handle is not None:
            unreal.unregister_slate_post_tick_callback(handle)
        for actor in reversed(spawned):
            actors.destroy_actor(actor)
        state["elapsed_seconds"] = time.time() - state["started"]
        state["status"] = "PASS" if "error" not in state and len(state["captures"]) == len(phases) else "FAIL"
        report()
        unreal.log("FF70_CAPTURE_" + state["status"])
        unreal.EditorPythonScripting.set_keep_python_script_alive(False)
        unreal.SystemLibrary.quit_editor()

    def apply(phase):
        location = unreal.Vector(*phase["camera"])
        rotation = unreal.MathLibrary.find_look_at_rotation(location, unreal.Vector(*phase["target"]))
        capture_actor.set_actor_location_and_rotation(location, rotation, False, False)
        capture.clear_show_only_components()
        capture.show_only_actor_components(ground)
        lighting = unreal.FFZSFacilityLightingState()
        lighting.set_editor_property("enabled", phase["mode"] != "OFF")
        lighting.set_editor_property("brightness_multiplier", 1.0)
        lighting.set_editor_property("override_color", phase["mode"] == "BLUE")
        lighting.set_editor_property("color", unreal.LinearColor(.03, .16, 1, 1))
        light_readback = {}
        for record in records:
            visible = record["category"] == phase["category"]
            record["component"].set_visibility(visible)
            if visible:
                capture.show_only_actor_components(record["model"])
            if record["rig"]:
                record["rig"].apply_lighting_state(lighting, visible, record["component"])
                lights = record["rig"].get_components_by_class(unreal.LightComponent)
                values = [float(light.get_editor_property("intensity")) for light in lights]
                if visible:
                    pipeline.check(lights and (any(v > 0 for v in values) if phase["mode"] != "OFF" else all(v == 0 for v in values)),
                                   "Runtime lighting state mismatch: " + record["id"])
                    light_readback[record["id"]] = values
        light_components[0].set_intensity(.9 if phase["category"] == "Lighting" else 2.4)
        light_components[1].set_intensity(.45 if phase["category"] == "Lighting" else 1.0)
        phase["light_readback"] = light_readback
        capture.set_editor_property("camera_cut_this_frame", True)
        state["phase_started"] = time.time()

    def tick(dt):
        if state.get("busy"):
            return
        state["busy"] = True
        try:
            if time.time() - state["started"] > 900:
                raise TimeoutError("Facility70 captures exceeded fifteen minutes")
            if state["phase"] >= len(phases):
                finish()
                return
            phase = phases[state["phase"]]
            if state["frame"] == 0:
                apply(phase)
            state["frame"] += 1
            delay = 25.0 if state["phase"] == 0 else 3.0
            if state["frame"] >= 70 and time.time() - state["phase_started"] >= delay and "render_requested" not in state:
                capture.capture_scene()
                state["render_requested"] = time.time()
                state["render_frame"] = state["frame"]
                return
            if "render_requested" in state and state["frame"] > state["render_frame"] + 10 and time.time() - state["render_requested"] > .6:
                unreal.RenderingLibrary.export_render_target(world, render_target, str(OUT), phase["name"] + ".png")
                filename = OUT / (phase["name"] + ".png")
                pipeline.check(filename.is_file() and filename.stat().st_size > 1000, "PNG capture missing")
                state["captures"].append(dict(file=str(filename), **phase))
                unreal.log("FF70_CAPTURE_READY " + phase["name"])
                state["phase"] += 1
                state["frame"] = 0
                state.pop("render_requested", None)
                state.pop("render_frame", None)
                report()
        except Exception:
            state["error"] = traceback.format_exc()
            unreal.log_error(state["error"])
            finish()
        finally:
            state["busy"] = False

    state["callback"] = unreal.register_slate_post_tick_callback(tick)


if __name__ == "__main__":
    try:
        start()
    except Exception:
        OUT.mkdir(parents=True, exist_ok=True)
        error = traceback.format_exc()
        (OUT / "capture_error.txt").write_text(error, encoding="utf-8")
        unreal.log_error(error)
        unreal.EditorPythonScripting.set_keep_python_script_alive(False)
        unreal.SystemLibrary.quit_editor()
