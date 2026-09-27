# 역할: 에디터 제작 코드의 중복과 비공개 헤더 의존성을 컴파일 전에 찾아낸다.
"""실행: python -m unittest discover -s Tools/Build -p 'test_*.py'"""
from pathlib import Path
import json
import re
import unittest


ROOT = Path(__file__).resolve().parents[2]
RUNTIME = ROOT / "Source/FestaFiesta"
EDITOR = ROOT / "Source/FestaFiestaEditor"


class EditorModuleBoundaryTests(unittest.TestCase):
    def test_editor_module_is_not_a_runtime_module(self):
        project = json.loads((ROOT / "FestaFiesta.uproject").read_text(encoding="utf-8-sig"))
        modules = [module for module in project["Modules"] if module["Name"] == "FestaFiestaEditor"]
        self.assertEqual(len(modules), 1)
        self.assertEqual(modules[0]["Type"], "Editor")

    def test_moved_implementations_have_only_one_owner(self):
        # 이동 전 파일이 남으면 같은 제작 명령이 두 DLL에 들어가고 링크 의존성도 다시 생긴다.
        runtime_sources = {path.relative_to(RUNTIME / "Private")
                           for path in (RUNTIME / "Private").rglob("*.cpp")}
        editor_sources = {path.relative_to(EDITOR / "Private")
                          for path in (EDITOR / "Private").rglob("*.cpp")}
        self.assertTrue(editor_sources, "Editor 소스 폴더가 비어 있습니다.")
        duplicates = sorted(path.as_posix() for path in runtime_sources & editor_sources)
        self.assertEqual(duplicates, [], "두 모듈에서 중복 컴파일하는 파일: " + ", ".join(duplicates))

    def test_shared_layout_headers_are_public(self):
        for relative in ("UI/Frontend/FF_FrontendCasualStyle.h", "UI/Settings/FF_KeyBindingLayoutMetrics.h"):
            with self.subTest(header=relative):
                self.assertTrue((RUNTIME / "Public" / relative).is_file(), relative)
                self.assertFalse((RUNTIME / "Private" / relative).exists(), relative)

    def test_authoring_helpers_remain_editor_private(self):
        for relative in ("UI/Core/FF_UIWbpBuilderCommon.h", "UI/Frontend/FF_FrontendCasualBuilder.h",
                         "UI/Facility/FF_FacilityInfoWbpBuilder.h"):
            with self.subTest(header=relative):
                self.assertTrue((EDITOR / "Private" / relative).is_file(), relative)
                self.assertFalse((RUNTIME / "Private" / relative).exists(), relative)

    def test_runtime_does_not_include_editor_builder_helpers(self):
        # Private include 경로를 억지로 추가해 모듈 경계 오류를 숨기지 못하게 한다.
        include = re.compile(r'^\s*#\s*include\s*[<"]UI/Core/FF_UIWbpBuilderCommon\.h[>"]', re.MULTILINE)
        offenders = [path.relative_to(ROOT).as_posix() for path in sorted((RUNTIME / "Private").rglob("*.cpp"))
                     if include.search(path.read_text(encoding="utf-8-sig"))]
        self.assertEqual(offenders, [])


if __name__ == "__main__":
    unittest.main()
