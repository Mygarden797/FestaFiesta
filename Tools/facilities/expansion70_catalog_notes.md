# Facility Expansion 70 catalog

`expansion70_catalog.json` is the checked-in initial authoring plan for exactly the 70 numbered rows in `Design/Guides/Facility70_BlenderMCP_Unreal_ProductionBrief_KO.md`. Korean names, subtypes, descriptions, and production emphases are copied verbatim. All 140 named main/exploded reference images exist in `Design/Facitilies` (the repository's actual spelling).

The initial centimetre dimensions are authored targets based on the brief and visual proportions, not dimensions measured from the concept art. When definitions are created, each `UFFZSFacilityDefinition.Spec` becomes authoritative for runtime and subsequent model revisions. Keep the catalog, saved definition, and model reports synchronized; do not silently resize existing definitions on reruns.

## Fields

- `number`: the stable brief number, 1–70.
- `category` / `subtype`: existing category enum and ID naming tokens. There is no separate subtype property on `FFFZSFacilitySpec`; subtype is encoded in `id`.
- `slug` / `id`: meaningful PascalCase unique token and `Category_Subtype_Slug` ID.
- `footprint_cm` / `height_cm`: proposed `FootprintSizeCells(X,Y)` and `PlacementObstacleHeightCm`; 1 Cell is 1 cm. Blender geometry uses metres.
- `name_ko`: `Spec.DisplayName`. `description_ko` and `emphasis_ko` remain production documentation because the current Spec has no description property.
- `reference_image` / `exploded_reference_image`: repository-relative evidence paths, not automatic Unreal texture imports.
- `template_definition`: verified existing subtype template package path. It is a source for compatible policies, not permission to inherit geometry, identity, thumbnails, anchor positions, external economic keys, or light rigs blindly.
- `followup_features`: per-facility runtime work or checks that the model alone does not implement.

## Definition construction

The definition class is `/Script/FF_ZoneSystem.FFZSFacilityDefinition`, a `UPrimaryDataAsset`. The expected package is `/Game/FestaFiesta/Building/FF_Data_Building/<Category>/PDA_FFZS_<id>`; the primary asset identity comes from `Spec.FacilityDefinitionId`.

Use current `FFFZSFacilitySpec::CurrentSchemaVersion` (2). For exact visual sizing, use zero `VisualOffsetCm`, a permitted right-angle Yaw, and a ground-centred mesh whose front is local +Y. Road/Tile retain +X travel direction and 400×400×8 cm source geometry. Keep new artwork separate from the legacy procedural mesh ownership flag and metadata unless deliberately integrating that ownership contract.

Service anchors use facility-centred local centimetres; queue reservations use the footprint's lower-left origin and half-open cm cell rectangles. Rebuild both for the new dimensions and validate unique movement targets. Lighting, Road, and Tile require zero service capacity and empty queue/service/audience data. Road blocks facilities above it; Tile permits them. Copying a subtype template does not establish that any new economic key, sound, animation, program, or service effect exists.

Run `IsDefinitionValid` and Unreal Data Validation after assigning the real mesh and rebuilding usage geometry. Names and files are checked for collisions during catalog authoring, but the loaded Unreal registry must check actual serialized IDs before creation because legacy package names need not match their serialized IDs.

## Reference interpretation

All 70 main images were visually inspected as contact sheets. #2, #8, and #10 show truck bodies; #27 has a circular kiosk and lighthouse roof; #62 includes a freestanding four-post shell-lamp support. These details inform distinct silhouettes.

Where concept illustrations conflict with the brief's placement constraints, apply the brief: #50 needs an open evacuation surface without a blocking central ornament; #56 needs an open sunset frame rather than an opaque sunset painting; #68 and #69 stay flat and omit tall corner decorations/railings that would obstruct upper facilities. The white presentation plinth and printed captions in the references are presentation devices, not mandatory geometry. Do not infer a reference render's light, text label, or game effect is already functional in Unreal.
