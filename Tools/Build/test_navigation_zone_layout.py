# test_navigation_zone_layout.py
# 역할: 존 분할의 면적·ID·앵커 여유와 변을 마주한 이웃 판정을 Unreal 없이 검사한다.
"""python -m unittest discover -s Tools/Build -p test_navigation_zone_layout.py"""

import importlib.util
from copy import deepcopy
from pathlib import Path
import sys
from types import ModuleType
import unittest
from unittest.mock import patch


_spec = importlib.util.spec_from_file_location(
    "navigation_zone_layout_under_test", Path(__file__).with_name("apply_navigation_zone_layout.py"))
_module = importlib.util.module_from_spec(_spec)
# 순수 함수만 검증하며 에디터를 띄우거나 main()을 실행하지 않는다.
with patch.dict(sys.modules, {"unreal": ModuleType("unreal")}):
    _spec.loader.exec_module(_module)
split_layout = _module.split_layout
adjacent = _module.adjacent


def plan(axis="x", recipient_side="high", cut=400, size=(1000, 800)):
    """분할 양쪽 중앙에 앵커를 둔 독립 검사 입력을 만든다."""
    origin = [100, 200, 30]
    dimension = 0 if axis == "x" else 1
    low_anchor = [origin[0] + size[0] / 2, origin[1] + size[1] / 2, 35]
    high_anchor = list(low_anchor)
    low_anchor[dimension] = origin[dimension] + cut / 2
    high_anchor[dimension] = origin[dimension] + (cut + size[dimension]) / 2
    low_id, high_id = ("Recipient", "Donor") if recipient_side == "low" else ("Donor", "Recipient")
    return {"minimum_side_cm": 100, "anchor_boundary_margin_cm": 25, "neighbor_gap_cm": 0,
            "splits": [{"donor": "Donor", "recipient": "Recipient", "axis": axis,
                        "origin": origin, "size": list(size), "cut_cm": cut,
                        "recipient_side": recipient_side,
                        "anchors_world": {low_id: low_anchor, high_id: high_anchor}}]}


def bounds(geometry):
    """도구의 rectangle 함수를 사용하지 않고 결과를 독립적으로 검산한다."""
    x, y, _ = geometry["origin"]
    width, height = geometry["size"]
    return (x, y, x + width, y + height)


class SplitLayoutTests(unittest.TestCase):
    def test_both_axes_and_recipient_sides_preserve_area_without_overlap(self):
        for axis in ("x", "y"):
            for side in ("low", "high"):
                with self.subTest(axis=axis, side=side):
                    result = split_layout(plan(axis, side, cut=300))
                    low_id, high_id = ("Recipient", "Donor") if side == "low" else ("Donor", "Recipient")
                    low, high = result[low_id], result[high_id]
                    self.assertEqual(set(result), {"Donor", "Recipient"})
                    self.assertEqual(low["origin"], [100, 200, 30])
                    self.assertEqual(high["origin"], [400, 200, 30] if axis == "x" else [100, 500, 30])
                    self.assertEqual(low["size"], [300, 800] if axis == "x" else [1000, 300])
                    self.assertEqual(high["size"], [700, 800] if axis == "x" else [1000, 500])
                    self.assertEqual(sum(row["size"][0] * row["size"][1] for row in result.values()), 1000 * 800)
                    low_bounds, high_bounds = bounds(low), bounds(high)
                    overlap_x = min(low_bounds[2], high_bounds[2]) - max(low_bounds[0], high_bounds[0])
                    overlap_y = min(low_bounds[3], high_bounds[3]) - max(low_bounds[1], high_bounds[1])
                    self.assertEqual(max(0, overlap_x) * max(0, overlap_y), 0)
                    self.assertTrue(adjacent(low_bounds, high_bounds, 0))
                    self.assertEqual([min(low_bounds[0], high_bounds[0]), min(low_bounds[1], high_bounds[1]),
                                      max(low_bounds[2], high_bounds[2]), max(low_bounds[3], high_bounds[3])],
                                     [100, 200, 1100, 1000])

    def test_same_donor_and_recipient_is_rejected(self):
        candidate = plan()
        candidate["splits"][0]["recipient"] = "Donor"
        with self.assertRaises(AssertionError):
            split_layout(candidate)

    def test_any_identity_reused_in_another_split_is_rejected(self):
        for repeated in ("Donor", "Recipient"):
            for role in ("donor", "recipient"):
                candidate = plan()
                duplicate = deepcopy(candidate["splits"][0])
                duplicate.update(donor="OtherDonor", recipient="OtherRecipient")
                duplicate[role] = repeated
                candidate["splits"].append(duplicate)
                with self.subTest(repeated=repeated, role=role), self.assertRaises(AssertionError):
                    split_layout(candidate)

    def test_exact_minimum_width_is_allowed_on_either_side(self):
        for cut in (100, 900):
            with self.subTest(cut=cut):
                result = split_layout(plan(cut=cut))
                self.assertEqual(min(row["size"][0] for row in result.values()), 100)

    def test_a_split_below_minimum_width_on_either_side_is_rejected(self):
        for cut in (99, 901, 0, 1000):
            with self.subTest(cut=cut), self.assertRaises(AssertionError):
                split_layout(plan(cut=cut))

    def test_the_unsplit_dimension_must_also_meet_the_minimum(self):
        with self.assertRaises(AssertionError):
            split_layout(plan(size=(1000, 99)))

    def test_anchors_at_the_margin_are_allowed_but_crossing_any_side_is_rejected(self):
        for zone_id in ("Donor", "Recipient"):
            geometry = split_layout(plan())[zone_id]
            x0, y0, x1, y1 = bounds(geometry)
            for coordinate, value, outside in ((0, x0 + 25, -0.000001), (0, x1 - 25, 0.000001),
                                               (1, y0 + 25, -0.000001), (1, y1 - 25, 0.000001)):
                candidate = plan()
                anchor = candidate["splits"][0]["anchors_world"][zone_id]
                anchor[coordinate] = value
                with self.subTest(zone=zone_id, coordinate=coordinate, boundary=value):
                    self.assertEqual(split_layout(candidate)[zone_id]["anchor_world"], anchor)
                    anchor[coordinate] += outside
                    with self.assertRaises(AssertionError):
                        split_layout(candidate)

    def test_an_anchor_in_the_other_half_is_rejected(self):
        candidate = plan()
        anchors = candidate["splits"][0]["anchors_world"]
        anchors["Donor"], anchors["Recipient"] = anchors["Recipient"], anchors["Donor"]
        with self.assertRaises(AssertionError):
            split_layout(candidate)

    def test_margin_too_large_for_a_valid_anchor_is_rejected(self):
        candidate = plan(cut=100)
        candidate["anchor_boundary_margin_cm"] = 51
        with self.assertRaises(AssertionError):
            split_layout(candidate)

    def test_negative_anchor_margin_cannot_allow_an_anchor_outside_the_zone(self):
        candidate = plan()
        candidate["anchor_boundary_margin_cm"] = -10
        candidate["splits"][0]["anchors_world"]["Donor"][0] = 95
        with self.assertRaises(AssertionError):
            split_layout(candidate)

    def test_nonpositive_minimum_cannot_allow_a_zero_area_half(self):
        for minimum in (0, -1):
            candidate = plan(cut=0)
            candidate["minimum_side_cm"] = minimum
            candidate["anchor_boundary_margin_cm"] = 0
            with self.subTest(minimum=minimum), self.assertRaises(AssertionError):
                split_layout(candidate)

    def test_negative_neighbor_gap_and_nonfinite_policy_values_are_rejected(self):
        for field in ("minimum_side_cm", "anchor_boundary_margin_cm", "neighbor_gap_cm"):
            for value in (float("nan"), float("inf"), float("-inf")):
                candidate = plan()
                candidate[field] = value
                with self.subTest(field=field, value=value), self.assertRaises(AssertionError):
                    split_layout(candidate)
        candidate = plan()
        candidate["neighbor_gap_cm"] = -1
        with self.assertRaises(AssertionError):
            split_layout(candidate)

    def test_nonfinite_coordinates_and_cut_are_rejected(self):
        for field, index in (("origin", 0), ("origin", 1), ("origin", 2),
                             ("size", 0), ("size", 1), ("cut_cm", None),
                             ("anchor", 0), ("anchor", 1), ("anchor", 2)):
            for value in (float("nan"), float("inf"), float("-inf")):
                candidate = plan()
                split = candidate["splits"][0]
                if field == "anchor":
                    split["anchors_world"]["Donor"][index] = value
                elif index is None:
                    split[field] = value
                else:
                    split[field][index] = value
                with self.subTest(field=field, index=index, value=value), self.assertRaises(AssertionError):
                    split_layout(candidate)

    def test_empty_plan_and_incomplete_coordinate_vectors_are_rejected(self):
        candidate = plan()
        candidate["splits"] = []
        with self.assertRaises(AssertionError):
            split_layout(candidate)
        for field in ("origin", "size", "anchor"):
            candidate = plan()
            split = candidate["splits"][0]
            values = split["anchors_world"]["Donor"] if field == "anchor" else split[field]
            values.pop()
            with self.subTest(field=field), self.assertRaises(AssertionError):
                split_layout(candidate)

    def test_fractional_origin_size_or_cut_is_rejected(self):
        for field, index in (("origin", 0), ("origin", 1), ("size", 0), ("size", 1), ("cut_cm", None)):
            candidate = plan()
            split = candidate["splits"][0]
            if index is None:
                split[field] += 0.5
            else:
                split[field][index] += 0.5
            with self.subTest(field=field, index=index), self.assertRaises(AssertionError):
                split_layout(candidate)

    def test_invalid_axis_or_recipient_side_is_rejected(self):
        for field, value in (("axis", "z"), ("recipient_side", "middle")):
            candidate = plan()
            candidate["splits"][0][field] = value
            with self.subTest(field=field), self.assertRaises((AssertionError, KeyError)):
                split_layout(candidate)

    def test_repeated_evaluation_is_deterministic_and_does_not_modify_the_plan(self):
        candidate = plan()
        before = deepcopy(candidate)
        self.assertEqual(split_layout(candidate), split_layout(candidate))
        self.assertEqual(candidate, before)


class AdjacentRectangleTests(unittest.TestCase):
    def assert_symmetric_adjacency(self, a, b, gap, expected):
        self.assertEqual(adjacent(a, b, gap), expected)
        self.assertEqual(adjacent(b, a, gap), expected)

    def test_shared_vertical_and_horizontal_edges_are_neighbors(self):
        self.assert_symmetric_adjacency([0, 0, 100, 100], [100, 25, 200, 75], 0, True)
        self.assert_symmetric_adjacency([0, 0, 100, 100], [25, 100, 75, 200], 0, True)

    def test_corner_contact_and_diagonal_nearness_are_not_neighbors(self):
        self.assert_symmetric_adjacency([0, 0, 100, 100], [100, 100, 200, 200], 0, False)
        self.assert_symmetric_adjacency([0, 0, 100, 100], [105, 105, 200, 200], 10, False)

    def test_gap_limit_is_inclusive_and_any_greater_gap_is_rejected(self):
        for other in ([110, 20, 200, 80], [20, 110, 80, 200]):
            with self.subTest(other=other):
                self.assert_symmetric_adjacency([0, 0, 100, 100], other, 10, True)
                self.assert_symmetric_adjacency([0, 0, 100, 100], other, 9.999999, False)

    def test_partial_edge_overlap_requires_positive_length(self):
        self.assert_symmetric_adjacency([0, 0, 100, 100], [100, 99.999999, 200, 200], 0, True)
        self.assert_symmetric_adjacency([0, 0, 100, 100], [100, 100.000001, 200, 200], 0, False)

    def test_existing_narrow_overlap_is_still_recognized_as_adjacent(self):
        # 기존 donor 경계에 작은 겹침이 있을 수 있다. 인접 판정은 이를 연결로 인정하며,
        # 새로운 공간 생성 여부는 위 분할 면적·동일 외곽 검사가 별도로 확인한다.
        self.assert_symmetric_adjacency([0, 0, 1000, 800], [970, 0, 1970, 800], 0, True)


if __name__ == "__main__":
    unittest.main()
