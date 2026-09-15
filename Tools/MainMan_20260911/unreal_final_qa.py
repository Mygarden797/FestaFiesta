from pathlib import Path
for script in ['unreal_import_test.py','unreal_material_test.py']:
    path=Path(__file__).with_name(script)
    exec(compile(path.read_text(encoding='utf-8'),str(path),'exec'),{'__file__':str(path),'__name__':'__main__'})
