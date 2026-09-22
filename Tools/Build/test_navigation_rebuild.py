# test_navigation_rebuild.py
# 역할: 내비게이션 재생성에서 도달 가능 영역 손실과 제작 입력 변경을 엄격히 차단하는지 검사한다.
"""Unreal 없이 실행: python -m unittest discover -s Tools/Build -p test_navigation_rebuild.py"""

import importlib.util
from copy import deepcopy
from pathlib import Path
import sys
from types import ModuleType
import unittest
from unittest.mock import patch


# 에디터 API와 main()을 실행하지 않고 순수 비교 함수만 불러온다.
_spec = importlib.util.spec_from_file_location(
    "navigation_rebuild_under_test", Path(__file__).with_name("rebuild_packaging_navigation.py"))
_module = importlib.util.module_from_spec(_spec)
with patch.dict(sys.modules, {"unreal": ModuleType("unreal")}):
    _spec.loader.exec_module(_module)
coverage_regressions = _module.coverage_regressions


def snapshot():
    """각 존에 도달 가능한 표본 두 개가 있는 최소 입력을 만든다."""
    zones = []
    for zone in ("North", "South"):
        samples = [{"sample_id": str(index), "status": "reachable", "inside_polygon": True,
                    "local": [index, 0, 0], "requested_world": [index + 10, 20, 30]}
                   for index in range(2)]
        zones.append({"zone": zone, "polygon_local": [[0, 0], [10, 0], [0, 10]],
                      "samples": samples, "reachable": [[10, 20, 30], [11, 20, 30]]})
    return {"zones": zones, "exit_requested": [100, 200, 300], "exit": [100, 200, 300]}


def set_status(audit, zone="North", sample_id="0", status="unprojected"):
    """표본 결과와 존 요약을 함께 바꿔 일관된 재생성 결과를 구성한다."""
    row = next(row for row in audit["zones"] if row["zone"] == zone)
    sample = next(item for item in row["samples"] if item["sample_id"] == sample_id)
    sample["status"] = status
    row["reachable"] = [item["requested_world"] for item in row["samples"] if item["status"] == "reachable"]


class NavigationCoverageRegressionTests(unittest.TestCase):
    def test_identical_audits_have_no_regression(self):
        before = snapshot()
        self.assertFalse(any(coverage_regressions(before, deepcopy(before)).values()))

    def test_reordering_records_preserves_zone_and_sample_identity(self):
        before, after = snapshot(), snapshot()
        after["zones"].reverse()
        for zone in after["zones"]:
            zone["samples"].reverse()
        self.assertFalse(any(coverage_regressions(before, after).values()))

    def test_any_previously_reachable_sample_loss_blocks_save(self):
        for status in ("unprojected", "unreachable", "target_unprojected"):
            before, after = snapshot(), snapshot()
            set_status(after, status=status)
            with self.subTest(status=status):
                gate = coverage_regressions(before, after)
                self.assertEqual(gate["lost_reachable_samples"], [("North", "0")])
                self.assertEqual(gate["lost_reachable_zones"], [])
                self.assertTrue(any(gate.values()))

    def test_a_newly_reachable_sample_cannot_compensate_for_an_old_loss(self):
        before = snapshot()
        set_status(before, sample_id="1")
        after = deepcopy(before)
        set_status(after, sample_id="0")
        set_status(after, sample_id="1", status="reachable")
        gate = coverage_regressions(before, after)
        self.assertEqual(gate["lost_reachable_samples"], [("North", "0")])
        self.assertTrue(any(gate.values()))

    def test_the_same_sample_id_in_another_zone_does_not_hide_a_loss(self):
        before, after = snapshot(), snapshot()
        set_status(after, zone="South")
        gate = coverage_regressions(before, after)
        self.assertEqual(gate["lost_reachable_samples"], [("South", "0")])
        self.assertTrue(any(gate.values()))

    def test_losing_all_reachable_samples_in_a_zone_blocks_save(self):
        before, after = snapshot(), snapshot()
        for index in range(2):
            set_status(after, sample_id=str(index))
        gate = coverage_regressions(before, after)
        self.assertEqual(gate["lost_reachable_zones"], ["North"])
        self.assertEqual(gate["lost_reachable_samples"], [("North", "0"), ("North", "1")])
        self.assertFalse(gate["no_reachable_samples_after"])
        self.assertTrue(any(gate.values()))

    def test_even_tiny_input_changes_or_polygon_membership_changes_block_save(self):
        for field, value in (("local", [0.000001, 0, 0]), ("requested_world", [10, 20, 30.000001]),
                             ("inside_polygon", False)):
            before, after = snapshot(), snapshot()
            after["zones"][0]["samples"][0][field] = value
            with self.subTest(field=field):
                gate = coverage_regressions(before, after)
                self.assertEqual(gate["changed_sample_inputs"], [("North", "0")])
                self.assertEqual(gate["lost_reachable_samples"], [])
                self.assertTrue(any(gate.values()))

    def test_sample_addition_and_removal_are_input_changes(self):
        for remove in (True, False):
            before, after = snapshot(), snapshot()
            zone = after["zones"][0]
            if remove:
                zone["samples"].pop(0)
                changed = ("North", "0")
            else:
                added = deepcopy(zone["samples"][0])
                added["sample_id"] = "New"
                zone["samples"].append(added)
                changed = ("North", "New")
            zone["reachable"] = [sample["requested_world"] for sample in zone["samples"]]
            with self.subTest(remove=remove):
                gate = coverage_regressions(before, after)
                self.assertIn(changed, gate["changed_sample_inputs"])
                self.assertTrue(any(gate.values()))

    def test_even_tiny_zone_boundary_changes_block_save(self):
        before, after = snapshot(), snapshot()
        after["zones"][0]["polygon_local"][1] = [10.000001, 0]
        gate = coverage_regressions(before, after)
        self.assertEqual(gate["changed_zone_boundaries"], ["North"])
        self.assertTrue(any(gate.values()))

    def test_zone_addition_and_removal_cannot_pass_unchanged_boundaries(self):
        for remove in (True, False):
            before, after = snapshot(), snapshot()
            if remove:
                after["zones"].pop(0)
                changed = "North"
            else:
                added = deepcopy(after["zones"][0])
                added["zone"] = "New"
                after["zones"].append(added)
                changed = "New"
            with self.subTest(remove=remove):
                gate = coverage_regressions(before, after)
                self.assertEqual(gate["changed_zone_boundaries"], [changed])
                self.assertEqual(gate["lost_reachable_zones"], ["North"] if remove else [])
                self.assertTrue(any(gate.values()))

    def test_changed_exit_input_and_failed_exit_projection_independently_block_save(self):
        for field, value, guard in (("exit_requested", [100, 200, 300.000001], "changed_exit_input"),
                                    ("exit", None, "exit_unprojected_after")):
            before, after = snapshot(), snapshot()
            after[field] = value
            with self.subTest(guard=guard):
                gate = coverage_regressions(before, after)
                self.assertTrue(gate[guard])
                self.assertTrue(any(gate.values()))

    def test_no_reachable_sample_after_rebuild_blocks_save(self):
        before, after = snapshot(), snapshot()
        for zone in ("North", "South"):
            for index in range(2):
                set_status(after, zone=zone, sample_id=str(index), status="unreachable")
        gate = coverage_regressions(before, after)
        self.assertTrue(gate["no_reachable_samples_after"])
        self.assertEqual(gate["lost_reachable_zones"], ["North", "South"])
        self.assertEqual(len(gate["lost_reachable_samples"]), 4)
        self.assertTrue(any(gate.values()))

    def test_a_world_with_no_reachable_samples_still_blocks_without_any_new_loss(self):
        before = snapshot()
        for zone in ("North", "South"):
            for index in range(2):
                set_status(before, zone=zone, sample_id=str(index))
        gate = coverage_regressions(before, deepcopy(before))
        self.assertEqual(gate["lost_reachable_samples"], [])
        self.assertEqual(gate["lost_reachable_zones"], [])
        self.assertTrue(gate["no_reachable_samples_after"])
        self.assertTrue(any(gate.values()))

    def test_comparison_and_report_edits_do_not_mutate_the_original_audits(self):
        before, after = snapshot(), snapshot()
        set_status(after)
        after["zones"][0]["samples"][0]["requested_world"][2] += 0.000001
        before_copy, after_copy = deepcopy(before), deepcopy(after)
        gate = coverage_regressions(before, after)
        self.assertEqual(before, before_copy)
        self.assertEqual(after, after_copy)
        gate["lost_reachable_samples"].append(("Other", "9"))
        gate["changed_sample_inputs"].clear()
        self.assertEqual(before, before_copy)
        self.assertEqual(after, after_copy)


if __name__ == "__main__":
    unittest.main()
