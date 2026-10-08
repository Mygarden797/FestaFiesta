"""Idempotent Building-only reference layout authoring. No PIE or test execution.

Run via Unreal Tools > Execute Python Script after compiling the native module.
All modified packages are backed up before edits; other UI packages are guarded.
"""
from pathlib import Path
from datetime import datetime,timezone
import hashlib,json,shutil,sys,traceback
import unreal

# Authoring helpers must not generate Python cache files inside packaged Content.
sys.dont_write_bytecode = True

HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[4]
OUT=ROOT/'Saved/BuildingReference20261008'
RUN=OUT/datetime.now(timezone.utc).strftime('apply-%Y%m%d-%H%M%S')
MODULES=('back_button_patch','catalog_reference_patch','discovery_patch','facility_reference_patch','gold_icon_patch')
BASE='/Game/FestaFiesta/Building/FF_WBP_Building/'
CATALOG=BASE+'Catalog/WBP_FF_PrepPanelBuild'
sys.path.insert(0,str(HERE))
sys.path.insert(0,str(HERE.parent/'WebUI'))
REPORT={'status':'RUNNING','modules':{},'saved':[],'runtimeTestsRun':False,'backup':str(RUN/'Backup')}
PACKAGES=[]
def asset_file(package):
    return ROOT/'Content'/(package.removeprefix('/Game/')+'.uasset')
def digest(path):
    h=hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda:stream.read(1024*1024),b''):h.update(block)
    return h.hexdigest()
def protected_hashes():
    paths=set((ROOT/'Content').rglob('WBP*.uasset'))
    for relative in ['Content/FestaFiesta/UI','Content/FestaFiesta/Building/FF_WBP_Building']:
        paths.update(p for p in (ROOT/relative).rglob('*') if p.is_file())
    return {p.relative_to(ROOT).as_posix():digest(p) for p in sorted(paths)}
def widgets(package):
    assert package in PACKAGES,'Out-of-scope package: '+package
    bp=unreal.load_asset(package);assert bp,package
    return bp,{w.get_name():w for w in unreal.ObjectIterator(unreal.Widget)
        if w.get_outer() and w.get_outer().get_class().get_name()=='WidgetTree' and w.get_outer().get_outer()==bp}
def color(value,alpha=1):
    value=value.lstrip('#');channels=[int(value[i:i+2],16)/255 for i in (0,2,4)]
    channels=[v/12.92 if v<=0.04045 else ((v+0.055)/1.055)**2.4 for v in channels]
    return unreal.LinearColor(*channels,float(alpha))
def slate(value,alpha=1):return unreal.SlateColor(specified_color=color(value,alpha))
def brush(value,radius=12):
    outline=unreal.SlateBrushOutlineSettings(corner_radii=unreal.Vector4(radius,radius,radius,radius),
        color=slate('#DBDEED'),width=1,rounding_type=unreal.SlateBrushRoundingType.FIXED_RADIUS)
    return unreal.SlateBrush(draw_as=unreal.SlateBrushDrawType.ROUNDED_BOX,tint_color=slate(value),outline_settings=outline)
def run():
    assert Path(unreal.Paths.project_dir()).resolve()==ROOT
    assert not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor(),'Stop PIE before applying'
    modules=[__import__(name) for name in MODULES]
    for module in modules:PACKAGES.extend(module.PACKAGES)
    PACKAGES.append(CATALOG);PACKAGES[:]=list(dict.fromkeys(PACKAGES))
    assert all(p.startswith(BASE) and '/Shared/' not in p and '/Legacy/' not in p for p in PACKAGES)
    RUN.mkdir(parents=True,exist_ok=True)
    before=protected_hashes();REPORT['beforeHashes']=before;original={}
    for package in PACKAGES:
        src=asset_file(package);dest=RUN/'Backup'/src.relative_to(ROOT)
        dest.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(src,dest)
        bp,w=widgets(package);original[package]={name:obj.get_class().get_name() for name,obj in w.items()}
    reference=ROOT/'Design/UXUI/Building_Review_20261007_v1/design-tokens.json'
    reference_colors=json.loads(reference.read_text(encoding='utf-8-sig'))['color']
    tokens={name:value['$value'] for name,value in reference_colors.items()}
    tokens.update(muted=tokens['secondary'],line=tokens['border'])
    context={'widgets':widgets,'color':color,'slate':slate,'brush':brush,'tokens':tokens}
    for module in modules:
        touched,summary=module.apply(context)
        assert set(touched)<=set(PACKAGES)
        assert not summary.get('errors'),str(summary['errors'])
        REPORT['modules'][module.__name__]=summary
    # Update only HTML. Existing embedded facility thumbnails remain unchanged.
    from sync_to_umg import embedded_document
    html,font_uri=embedded_document()
    bp,w=widgets(CATALOG);bp.modify();w['Web_CatalogRibbon'].set_editor_property('document_html',html)
    for package in PACKAGES:
        bp,w=widgets(package)
        assert all(name in w and w[name].get_class().get_name()==cls for name,cls in original[package].items()),'Removed/changed an existing bound widget: '+package
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        assert bp.generated_class(),package
        unreal.EditorAssetLibrary.set_metadata_tag(bp,'FF.Building.ReferenceLayout.20261008','1')
    for module in modules:
        if hasattr(module,'apply_defaults'):
            REPORT['modules'][module.__name__]['defaults']=module.apply_defaults(context)
    for package in PACKAGES:
        bp,_=widgets(package)
        assert unreal.EditorAssetLibrary.save_loaded_asset(bp,only_if_is_dirty=False),package
        REPORT['saved'].append(package)
    after=protected_hashes();allowed={asset_file(p).relative_to(ROOT).as_posix() for p in PACKAGES}
    unexpected=[p for p in before.keys()|after.keys() if p not in allowed and before.get(p)!=after.get(p)]
    REPORT.update(unexpectedChanges=unexpected,protectedFileCount=len(before)-len(allowed),changedAssets=[p for p in allowed if before.get(p)!=after.get(p)])
    assert not unexpected,str(unexpected)
    REPORT['status']='PASS'
def execute(module_names=MODULES):
    """Apply all modules, or a named subset for a narrow follow-up edit."""
    global MODULES
    assert module_names and set(module_names) <= set(MODULES), module_names
    MODULES = tuple(module_names)
    try:run()
    except Exception:
        REPORT['status']='FAIL';REPORT['error']=traceback.format_exc();unreal.log_error(REPORT['error'])
        # Restore only this operation's explicitly scoped packages after partial save.
        for package in REPORT['saved']:
            target=asset_file(package);backup=RUN/'Backup'/target.relative_to(ROOT)
            if backup.is_file():shutil.copy2(backup,target)
    finally:
        OUT.mkdir(parents=True,exist_ok=True);RUN.mkdir(parents=True,exist_ok=True)
        text=json.dumps(REPORT,ensure_ascii=False,indent=2)
        (OUT/'latest-result.json').write_text(text,encoding='utf8');(RUN/'report.json').write_text(text,encoding='utf8')
    if REPORT['status']!='PASS':raise RuntimeError(REPORT['error'])


if __name__ == '__main__':execute()
