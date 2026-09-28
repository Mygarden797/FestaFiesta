# -*- coding: utf-8 -*-
# update_facility_movement_targets.py
# 역할: 시설 모델·Data Asset·썸네일 작업에서 연결 정보나 설정을 현재 기준에 맞게 갱신한다.

"""Atomically migrate production FacilityDefinition movement targets.

The migration is intentionally data-specific.  Every production Definition,
anchor, queue point, queue rule, footprint, and reservation rectangle must
match the audited pre-migration snapshot before any UObject is changed.  All
package files are then protected by AssetSaveTransaction so a failed save
restores the complete 16-package batch.

This is a one-shot migration: after a successful run, rerunning it safely
fails the pre-migration value checks instead of rewriting the assets again.
"""

import os
import sys

import unreal


SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
if SCRIPT_DIR not in sys.path:
    sys.path.insert(0, SCRIPT_DIR)

from ffzs_asset_save_transaction import AssetSaveTransaction
from ffzs_definition_source import (
    read_facility_definitions,
    require_unattended_commandlet,
)


CROWD_AGENT_RADIUS_CM = 34.0
AUDITED_CLEARANCE_CM = 100.0


def _profile(
    footprint,
    entry_before,
    entry_after,
    service_before,
    service_after,
    exit_location,
    queue_points_before,
    queue_points_after,
    spacing_before,
    spacing_after,
    queue_rects_before,
    queue_rects_after=None,
    max_waiting_agents=12,
):
    return {
        "footprint": footprint,
        "entry_before": entry_before,
        "entry_after": entry_after,
        "service_before": service_before,
        "service_after": service_after,
        "exit": exit_location,
        "queue_points_before": queue_points_before,
        "queue_points_after": queue_points_after,
        "spacing_before": spacing_before,
        "spacing_after": spacing_after,
        "queue_rects_before": queue_rects_before,
        "queue_rects_after": queue_rects_after or queue_rects_before,
        "max_waiting_agents": max_waiting_agents,
    }


PROFILES = {
    "booth": _profile(
        footprint=(1000, 500),
        entry_before=(450.0, 300.0, 0.0),
        entry_after=(650.0, 300.0, 0.0),
        service_before=(450.0, 250.0, 0.0),
        service_after=(525.0, 250.0, 0.0),
        exit_location=(-450.0, 300.0, 0.0),
        queue_points_before=(
            (450.0, 300.0, 0.0),
            (450.0, 550.0, 0.0),
            (450.0, 850.0, 0.0),
        ),
        queue_points_after=(
            (450.0, 300.0, 0.0),
            (450.0, 550.0, 0.0),
            (450.0, 1290.0, 0.0),
        ),
        spacing_before=50.0,
        spacing_after=90.0,
        queue_rects_before=(((450, 500), (1450, 1600)),),
    ),
    "info": _profile(
        footprint=(1500, 1000),
        entry_before=(700.0, 550.0, 0.0),
        entry_after=(900.0, 550.0, 0.0),
        service_before=(700.0, 500.0, 0.0),
        service_after=(775.0, 500.0, 0.0),
        exit_location=(-700.0, 550.0, 0.0),
        queue_points_before=((700.0, 550.0, 0.0), (700.0, 1100.0, 0.0)),
        queue_points_after=((700.0, 550.0, 0.0), (700.0, 1540.0, 0.0)),
        spacing_before=50.0,
        spacing_after=90.0,
        queue_rects_before=(((1410, 1010), (1490, 1640)),),
        queue_rects_after=(((1410, 1010), (1685, 2100)),),
    ),
    "performance": _profile(
        footprint=(3000, 2000),
        entry_before=(1450.0, 1050.0, 0.0),
        entry_after=(1650.0, 1050.0, 0.0),
        service_before=(1450.0, 1000.0, 0.0),
        service_after=(1525.0, 1000.0, 0.0),
        exit_location=(-1450.0, 1050.0, 0.0),
        queue_points_before=((1450.0, 1050.0, 0.0), (1450.0, 2350.0, 0.0)),
        queue_points_after=((1450.0, 1050.0, 0.0), (1450.0, 2350.0, 0.0)),
        spacing_before=115.0,
        spacing_after=115.0,
        queue_rects_before=(((2900, 2000), (3000, 3400)),),
        queue_rects_after=(((2900, 2000), (3185, 3400)),),
    ),
    "photo": _profile(
        footprint=(4000, 2000),
        entry_before=(1950.0, 1050.0, 0.0),
        entry_after=(2150.0, 1050.0, 0.0),
        service_before=(1950.0, 1000.0, 0.0),
        service_after=(2025.0, 1000.0, 0.0),
        exit_location=(-1950.0, 1050.0, 0.0),
        queue_points_before=((1950.0, 1050.0, 0.0), (1950.0, 2350.0, 0.0)),
        queue_points_after=((1950.0, 1050.0, 0.0), (1950.0, 2350.0, 0.0)),
        spacing_before=115.0,
        spacing_after=115.0,
        queue_rects_before=(((3900, 2000), (4000, 3400)),),
        queue_rects_after=(((3900, 2000), (4185, 3400)),),
    ),
    "rest": _profile(
        footprint=(1500, 1000),
        entry_before=(700.0, 550.0, 0.0),
        entry_after=(900.0, 550.0, 0.0),
        service_before=(700.0, 500.0, 0.0),
        service_after=(775.0, 500.0, 0.0),
        exit_location=(-700.0, 550.0, 0.0),
        queue_points_before=((700.0, 550.0, 0.0), (700.0, 1850.0, 0.0)),
        queue_points_after=((700.0, 550.0, 0.0), (700.0, 1850.0, 0.0)),
        spacing_before=115.0,
        spacing_after=115.0,
        queue_rects_before=(((1400, 1000), (1500, 2400)),),
        queue_rects_after=(((1400, 1000), (1685, 2400)),),
    ),
    "safety": _profile(
        footprint=(500, 200),
        entry_before=(200.0, 150.0, 0.0),
        entry_after=(400.0, 150.0, 0.0),
        service_before=(200.0, 100.0, 0.0),
        service_after=(275.0, 100.0, 0.0),
        exit_location=(-200.0, 150.0, 0.0),
        queue_points_before=((200.0, 150.0, 0.0), (200.0, 610.0, 0.0)),
        queue_points_after=((200.0, 150.0, 0.0), (200.0, 610.0, 0.0)),
        spacing_before=115.0,
        spacing_after=115.0,
        queue_rects_before=(((400, 200), (500, 760)),),
        queue_rects_after=(((400, 200), (685, 760)),),
        max_waiting_agents=5,
    ),
    "toilet": _profile(
        footprint=(1000, 700),
        entry_before=(450.0, 400.0, 0.0),
        entry_after=(650.0, 400.0, 0.0),
        service_before=(450.0, 350.0, 0.0),
        service_after=(525.0, 350.0, 0.0),
        exit_location=(-450.0, 400.0, 0.0),
        queue_points_before=((450.0, 400.0, 0.0), (450.0, 1700.0, 0.0)),
        queue_points_after=((450.0, 400.0, 0.0), (450.0, 1700.0, 0.0)),
        spacing_before=115.0,
        spacing_after=115.0,
        queue_rects_before=(((900, 700), (1000, 2100)),),
        queue_rects_after=(((900, 700), (1185, 2100)),),
    ),
}


EXPECTED_DEFINITIONS = {
    "Booth_ActivityGame_RingToss": (
        "/Game/FestaFiesta/Building/FF_Data_Building/Booth/"
        "PDA_FFZS_Booth_ActivityGame_RingToss",
        "booth",
    ),
    "Booth_CraftExperience_SandArt": (
        "/Game/FestaFiesta/Building/FF_Data_Building/Booth/"
        "PDA_FFZS_Booth_CraftExperience_SandArt",
        "booth",
    ),
    "Booth_FoodStall_DadaPochaFoodTruck": (
        "/Game/FestaFiesta/Building/FF_Data_Building/Booth/"
        "PDA_FFZS_Booth_FoodStall_DadaPochaFoodTruck",
        "booth",
    ),
    "Booth_MarketGoods_FestivalGoods": (
        "/Game/FestaFiesta/Building/FF_Data_Building/Booth/"
        "PDA_FFZS_Booth_MarketGoods_FestivalGoods",
        "booth",
    ),
    "Booth_Nightlife_EDMBar": (
        "/Game/FestaFiesta/Building/FF_Data_Building/Booth/"
        "PDA_FFZS_Booth_Nightlife_EDMBar",
        "booth",
    ),
    "Info_GeneralInfo_MainInformation": (
        "/Game/FestaFiesta/Building/FF_Data_Building/Info/"
        "PDA_FFZS_Info_GeneralInfo_MainInformation",
        "info",
    ),
    "Performance_Busking_StreetStage": (
        "/Game/FestaFiesta/Building/FF_Data_Building/Performance/"
        "PDA_FFZS_Performance_Busking_StreetStage",
        "performance",
    ),
    "Performance_ExperienceStage_DanceBattle": (
        "/Game/FestaFiesta/Building/FF_Data_Building/Performance/"
        "PDA_FFZS_Performance_ExperienceStage_DanceBattle",
        "performance",
    ),
    "Performance_MainStage_SunsetMainStage": (
        "/Game/FestaFiesta/Building/FF_Data_Building/Performance/"
        "PDA_FFZS_Performance_MainStage_SunsetMainStage",
        "performance",
    ),
    "Performance_SubStage_LocalArtist": (
        "/Game/FestaFiesta/Building/FF_Data_Building/Performance/"
        "PDA_FFZS_Performance_SubStage_LocalArtist",
        "performance",
    ),
    "PhotoSpot_Landmark_SunsetLandmark": (
        "/Game/FestaFiesta/Building/FF_Data_Building/PhotoSpot/"
        "PDA_FFZS_PhotoSpot_Landmark_SunsetLandmark",
        "photo",
    ),
    "Rest_Seating_MainRestArea": (
        "/Game/FestaFiesta/Building/FF_Data_Building/Rest/"
        "PDA_FFZS_Rest_Seating_MainRestArea",
        "rest",
    ),
    "Safety_FirstAid_MedicalTent": (
        "/Game/FestaFiesta/Building/FF_Data_Building/Safety/"
        "PDA_FFZS_Safety_FirstAid_MedicalTent",
        "safety",
    ),
    "Toilet_Accessible_WestGate": (
        "/Game/FestaFiesta/Building/FF_Data_Building/Toilet/"
        "PDA_FFZS_Toilet_Accessible_WestGate",
        "toilet",
    ),
    "Toilet_Standard_MainRestroom": (
        "/Game/FestaFiesta/Building/FF_Data_Building/Toilet/"
        "PDA_FFZS_Toilet_Standard_MainRestroom",
        "toilet",
    ),
    "Toilet_Temporary_CampingArea": (
        "/Game/FestaFiesta/Building/FF_Data_Building/Toilet/"
        "PDA_FFZS_Toilet_Temporary_CampingArea",
        "toilet",
    ),
}


def _enum_token(value):
    name = getattr(value, "name", None)
    if name:
        return str(name).upper()
    text = str(value).rsplit(".", 1)[-1]
    return text.split(":", 1)[0].strip("<> ").upper()


def _vector_tuple(value):
    return (float(value.x), float(value.y), float(value.z))


def _int_point_tuple(value):
    return (int(value.x), int(value.y))


def _transform_translation(transform):
    try:
        return transform.get_editor_property("translation")
    except Exception:
        return transform.translation


def _set_transform_translation(transform, value):
    translation = unreal.Vector(*value)
    try:
        transform.set_editor_property("translation", translation)
    except Exception:
        transform.translation = translation


def _vectors_match(actual, expected, tolerance=0.001):
    return len(actual) == len(expected) and all(
        all(abs(a - e) <= tolerance for a, e in zip(actual_value, expected_value))
        for actual_value, expected_value in zip(actual, expected)
    )


def _find_anchors(spec):
    anchors = list(spec.get_editor_property("anchors"))
    by_key = {}
    for anchor in anchors:
        key = (
            _enum_token(anchor.get_editor_property("type")),
            int(anchor.get_editor_property("index")),
        )
        if key in by_key:
            raise RuntimeError("Duplicate movement Anchor {0}".format(key))
        by_key[key] = anchor
    required_keys = {("ENTRY", 0), ("SERVICE", 0), ("EXIT", 0)}
    if set(by_key) != required_keys:
        raise RuntimeError(
            "Expected exactly Entry[0], Service[0], Exit[0]; got {0}".format(
                sorted(by_key)
            )
        )
    return anchors, by_key


def _queue_rect_tuples(spec):
    result = []
    for rect in spec.get_editor_property("queue_reserved_rects"):
        result.append(
            (
                _int_point_tuple(rect.get_editor_property("min")),
                _int_point_tuple(rect.get_editor_property("max")),
            )
        )
    return tuple(result)


def _is_local_disc_reserved(profile, location, radius_cm):
    """Check a centered local-space disc against lower-left reservation rects."""
    center_x = float(location[0]) + profile["footprint"][0] * 0.5
    center_y = float(location[1]) + profile["footprint"][1] * 0.5
    minimum_x = center_x - radius_cm
    minimum_y = center_y - radius_cm
    maximum_x = center_x + radius_cm
    maximum_y = center_y + radius_cm
    for minimum, maximum in profile["queue_rects_after"]:
        if (
            minimum[0] <= minimum_x
            and minimum[1] <= minimum_y
            and maximum[0] > maximum_x
            and maximum[1] > maximum_y
        ):
            return True
    return False


def _is_local_disc_inside_footprint_clearance(profile, location, radius_cm):
    half_width = profile["footprint"][0] * 0.5
    half_height = profile["footprint"][1] * 0.5
    return (
        location[0] - radius_cm >= -half_width - AUDITED_CLEARANCE_CM
        and location[0] + radius_cm <= half_width + AUDITED_CLEARANCE_CM
        and location[1] - radius_cm >= -half_height - AUDITED_CLEARANCE_CM
        and location[1] + radius_cm <= half_height + AUDITED_CLEARANCE_CM
    )


def _validate_profile_protection():
    """Reject constants that expose Entry[0] to another facility placement."""
    failures = []
    for definition_id, (_expected_path, profile_name) in sorted(
        EXPECTED_DEFINITIONS.items()
    ):
        profile = PROFILES[profile_name]
        entry = profile["entry_after"]
        inside_clearance = _is_local_disc_inside_footprint_clearance(
            profile, entry, CROWD_AGENT_RADIUS_CM
        )
        inside_reservation = _is_local_disc_reserved(
            profile, entry, CROWD_AGENT_RADIUS_CM
        )
        if not inside_clearance and not inside_reservation:
            failures.append(
                "{0}: Entry[0] plus the {1:g}cm Crowd radius is outside both "
                "the footprint clearance and QueueReservedRects".format(
                    definition_id, CROWD_AGENT_RADIUS_CM
                )
            )
    if failures:
        raise RuntimeError("Invalid movement-target profile: " + " | ".join(failures))


def _validate_record(record, profile, phase):
    spec = record["definition_object"].get_editor_property("spec")
    failures = []
    expected_suffix = "before" if phase == "before" else "after"

    if int(spec.get_editor_property("schema_version")) != 2:
        failures.append("SchemaVersion is not 2")
    if int(spec.get_editor_property("clearance_cells")) != AUDITED_CLEARANCE_CM:
        failures.append("ClearanceCells is not the audited 100cm")
    footprint = spec.get_editor_property("footprint_size_cells")
    if _int_point_tuple(footprint) != profile["footprint"]:
        failures.append(
            "Footprint {0} != {1}".format(
                _int_point_tuple(footprint), profile["footprint"]
            )
        )

    try:
        _anchors, by_key = _find_anchors(spec)
        expected_locations = {
            ("ENTRY", 0): profile["entry_" + expected_suffix],
            ("SERVICE", 0): profile["service_" + expected_suffix],
            ("EXIT", 0): profile["exit"],
        }
        for key, expected_location in expected_locations.items():
            actual_location = _vector_tuple(
                _transform_translation(
                    by_key[key].get_editor_property("local_transform")
                )
            )
            if not _vectors_match((actual_location,), (expected_location,)):
                failures.append(
                    "{0}[{1}] {2} != {3}".format(
                        key[0], key[1], actual_location, expected_location
                    )
                )
    except Exception as error:
        failures.append(str(error))

    queue_points = tuple(
        _vector_tuple(point)
        for point in spec.get_editor_property("queue_spline_points_cm")
    )
    expected_queue_points = profile["queue_points_" + expected_suffix]
    if not _vectors_match(queue_points, expected_queue_points):
        failures.append(
            "QueueSplinePoints {0} != {1}".format(
                queue_points, expected_queue_points
            )
        )

    rules = spec.get_editor_property("queue_rules")
    actual_spacing = float(rules.get_editor_property("slot_spacing_cm"))
    expected_spacing = float(profile["spacing_" + expected_suffix])
    if abs(actual_spacing - expected_spacing) > 0.001:
        failures.append(
            "SlotSpacing {0} != {1}".format(actual_spacing, expected_spacing)
        )
    actual_waiting = int(rules.get_editor_property("max_waiting_agents"))
    if actual_waiting != profile["max_waiting_agents"]:
        failures.append(
            "MaxWaitingAgents {0} != {1}".format(
                actual_waiting, profile["max_waiting_agents"]
            )
        )
    if int(rules.get_editor_property("service_capacity")) != 1:
        failures.append("ServiceCapacity is not 1")

    actual_rects = _queue_rect_tuples(spec)
    expected_rects = profile["queue_rects_" + expected_suffix]
    if actual_rects != expected_rects:
        failures.append(
            "QueueReservedRects {0} != {1}".format(actual_rects, expected_rects)
        )
    if phase == "after" and not _is_local_disc_reserved(
        profile, profile["entry_after"], CROWD_AGENT_RADIUS_CM
    ):
        failures.append(
            "Entry[0] plus the 34cm Crowd radius is outside QueueReservedRects"
        )
    return failures


def _apply_profile(definition, profile):
    spec = definition.get_editor_property("spec")
    anchors, by_key = _find_anchors(spec)
    for key, new_location in (
        (("ENTRY", 0), profile["entry_after"]),
        (("SERVICE", 0), profile["service_after"]),
    ):
        anchor = by_key[key]
        transform = anchor.get_editor_property("local_transform")
        _set_transform_translation(transform, new_location)
        anchor.set_editor_property("local_transform", transform)
    spec.set_editor_property("anchors", anchors)

    spec.set_editor_property(
        "queue_spline_points_cm",
        [unreal.Vector(*point) for point in profile["queue_points_after"]],
    )
    rules = spec.get_editor_property("queue_rules")
    rules.set_editor_property("slot_spacing_cm", profile["spacing_after"])
    spec.set_editor_property("queue_rules", rules)

    rects = list(spec.get_editor_property("queue_reserved_rects"))
    expected_rects = profile["queue_rects_after"]
    if len(rects) != len(expected_rects):
        raise RuntimeError("QueueReservedRects changed after preflight")
    for rect, (minimum, maximum) in zip(rects, expected_rects):
        rect.set_editor_property("min", unreal.IntPoint(*minimum))
        rect.set_editor_property("max", unreal.IntPoint(*maximum))
    spec.set_editor_property("queue_reserved_rects", rects)
    definition.set_editor_property("spec", spec)


def run():
    require_unattended_commandlet()
    _validate_profile_protection()
    records = read_facility_definitions(require_visual_mesh=False)
    records_by_id = {record["facility_definition_id"]: record for record in records}
    actual_ids = set(records_by_id)
    expected_ids = set(EXPECTED_DEFINITIONS)
    if actual_ids != expected_ids:
        raise RuntimeError(
            "Production Definition set differs from audited 16 assets; "
            "missing={0}, unexpected={1}".format(
                sorted(expected_ids - actual_ids), sorted(actual_ids - expected_ids)
            )
        )

    ordered_records = []
    preflight_failures = []
    for definition_id in sorted(EXPECTED_DEFINITIONS):
        expected_path, profile_name = EXPECTED_DEFINITIONS[definition_id]
        record = records_by_id[definition_id]
        if record["definition"] != expected_path:
            preflight_failures.append(
                "{0}: path {1} != {2}".format(
                    definition_id, record["definition"], expected_path
                )
            )
            continue
        profile = PROFILES[profile_name]
        for failure in _validate_record(record, profile, "before"):
            preflight_failures.append("{0}: {1}".format(definition_id, failure))
        ordered_records.append((record, profile))

    if preflight_failures:
        raise RuntimeError(
            "Movement-target migration preflight failed before mutation: "
            + " | ".join(preflight_failures)
        )

    transaction = AssetSaveTransaction(
        [record["definition"] for record, _profile_data in ordered_records],
        "FacilityMovementTargets",
    )
    try:
        for record, profile in ordered_records:
            _apply_profile(record["definition_object"], profile)

        verification_failures = []
        for record, profile in ordered_records:
            for failure in _validate_record(record, profile, "after"):
                verification_failures.append(
                    "{0}: {1}".format(record["facility_definition_id"], failure)
                )
        if verification_failures:
            raise RuntimeError(
                "Movement-target in-memory verification failed: "
                + " | ".join(verification_failures)
            )

        for record, _profile_data in ordered_records:
            definition = record["definition_object"]
            if not unreal.EditorAssetLibrary.save_loaded_asset(
                definition, only_if_is_dirty=False
            ):
                raise RuntimeError("Could not save " + record["definition"])
            unreal.log(
                "[FF_FACILITY_MOVEMENT] Saved {0}: {1}".format(
                    record["facility_definition_id"], record["definition"]
                )
            )
        transaction.commit()
    except Exception:
        transaction.rollback()
        raise

    unreal.log(
        "[FF_FACILITY_MOVEMENT] Migrated and verified {0} production Definitions".format(
            len(ordered_records)
        )
    )


run()
