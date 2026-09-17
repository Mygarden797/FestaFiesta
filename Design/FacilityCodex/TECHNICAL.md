# Facility codex implementation

- Rules remain in `DT_FacilitySynergies`; no duplicate authored recipe database. The collection model delegates distance, direction, stacking and cap calculation to `FFF_FacilitySynergyEvaluator`.
- `UFF_FacilityCodexSubsystem` projects the world read model after relevant lifecycle events. It caches Blueprint view data, subscribes to catalog/progress changes, and has no Tick or per-frame Blueprint bindings.
- `UFF_FacilityCodexProgressSubsystem` records stable rule IDs in the local `GameUserSettings.ini` section `FestaFiesta.FacilityCodex`. It writes only a newly discovered batch, independently of festival save slots. Completing installation of both required facilities unlocks a recipe regardless of distance, current operation, relocation requirements or revenue caps. Same-definition recipes require two distinct instance IDs. Distance, eligibility, stacking and caps continue to control current effects separately.
- The screen uses a virtualized TileView and reusable UObject entries. Images load through UMG soft texture requests. Native code updates data and state; the saved WBP owns layout and styling.
- The facility-inspector launcher opens all registered rules by default. A selected inspector facility never silently narrows the collection. Specialized callers may still explicitly request a facility filter.
- Undiscovered cards stay visible and selectable, using a configurable neutral palette and dim thumbnails. `Appearance|Undiscovered` on the Card WBP exposes those choices. The original WBP brush, text colors and thumbnail enabled state are captured once and restored when discovery changes or a virtualized card is reused. Existing WBP assets do not need regeneration.
- `UFF_FacilityCodexPresenterSubsystem` owns a modal input session without pausing time. It uses existing camera, build-mode and facility-inspector APIs and closes on stage changes, snapshot restore, controller teardown or widget removal.
- Two pre-existing facility-owned assets receive an authored launcher child: `WBP_FF_FacilityInfoPanel` in its synergy section, and `WBP_FF_PrepPanelBuild` immediately after header search. Original widgets, slots and styling are preserved. No existing C++ files, gameplay definitions, balance tables or other team assets are edited. Existing dirty files must retain their initial hashes.
- `FF.BuildFacilityCodexUMG` creates missing widgets only. `FF.MigrateFacilityCodexInspector` and `FF.MigrateFacilityCodexCatalog` validate their seams and add launchers only when absent; a conflicting type/name is an error rather than a reason to rebuild.

## Reproduce asset generation

After building the Development Editor target, run UnrealEditor-Cmd with the project and `-run=pythonscript -script=Tools/FacilityCodex/create_and_verify.py -unattended -nop4 -nullrhi`. The script checks generated classes and byte-for-byte preservation on a second run. Its report is written to `Saved/FacilityCodex/asset_verification.json`.

## Validation

Model tests: `FestaFiesta.Building.Codex`. UI tests: `FestaFiesta.FacilityCodex.UI` (AuthoredWidgets and CatalogHeader). Modal lifecycle tests: `FestaFiesta.UI.FacilityCodex.PresenterLifecycle`. Run with `-RenderOffscreen` for actual WBP PNGs in `Saved/Automation/FacilityCodex`; `-nullrhi` validates logic without rendering. The 120-recipe fixture is presentation-only and never enters gameplay tables or discovery storage.

The existing facility synergy, input and pause contracts should also pass after migration. Results for this change are recorded in `VALIDATION.md` after execution.
