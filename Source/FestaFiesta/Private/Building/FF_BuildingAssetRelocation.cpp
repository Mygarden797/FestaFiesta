// FF_BuildingAssetRelocation.cpp
// 역할: 기존 건설 에셋 이동 기록을 재현·검증하는 에디터 전용 도구이다.

// One explicit editor migration for the construction asset organization requested in September 2026.
// Keep the historical source paths in this utility unchanged when rewriting live project references.
#include "CoreMinimal.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Dom/JsonObject.h"
#include "Editor.h"
#include "Engine/Blueprint.h"
#include "Engine/World.h"
#include "FileHelpers.h"
#include "HAL/IConsoleManager.h"
#include "HAL/FileManager.h"
#include "IAssetTools.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "ObjectTools.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/ObjectRedirector.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UnrealType.h"

DEFINE_LOG_CATEGORY_STATIC(LogFFBuildingAssetRelocation, Log, All);

namespace FFBuildingAssetRelocation
{
static FString WorkDir()
{ return FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("BuildingAssetRelocation_20260921")); }
static FString ObjectPath(const FString& Package)
{ return Package + TEXT(".") + FPackageName::GetLongPackageAssetName(Package); }

struct FEntry
{
    FString Old;
    FString New;
    FString ClassPath;
    UObject* Asset = nullptr;
    bool AlreadyMoved = false;
};

struct FRun
{
    explicit FRun(const TCHAR* InPhase) : Phase(InPhase), Report(MakeShared<FJsonObject>())
    {
        Report->SetStringField(TEXT("phase"), Phase);
        Report->SetStringField(TEXT("started_utc"), FDateTime::UtcNow().ToIso8601());
        Report->SetStringField(TEXT("manifest"), WorkDir() / TEXT("manifest.json"));
    }
    bool Error(const FString& Message)
    {
        Errors.Add(MakeShared<FJsonValueString>(Message));
        UE_LOG(LogFFBuildingAssetRelocation, Error, TEXT("%s"), *Message);
        return false;
    }
    void Finish(bool Success)
    {
        Report->SetBoolField(TEXT("success"), Success && Errors.IsEmpty());
        Report->SetStringField(TEXT("completed_utc"), FDateTime::UtcNow().ToIso8601());
        Report->SetArrayField(TEXT("errors"), Errors);
        TArray<TSharedPtr<FJsonValue>> Rows;
        for (const FEntry& Entry : Entries)
        {
            auto Row = MakeShared<FJsonObject>();
            Row->SetStringField(TEXT("old"), Entry.Old); Row->SetStringField(TEXT("new"), Entry.New);
            Row->SetStringField(TEXT("class"), Entry.ClassPath);
            Row->SetBoolField(TEXT("already_moved_at_start"), Entry.AlreadyMoved);
            Row->SetBoolField(TEXT("destination_on_disk"), FPackageName::DoesPackageExist(Entry.New));
            Rows.Add(MakeShared<FJsonValueObject>(Row));
        }
        Report->SetArrayField(TEXT("assets"), Rows);
        FString Json;
        FJsonSerializer::Serialize(Report, TJsonWriterFactory<>::Create(&Json));
        IFileManager::Get().MakeDirectory(*WorkDir(), true);
        const bool Written = FFileHelper::SaveStringToFile(Json, *(WorkDir() / (Phase + TEXT("-result.json"))), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
        UE_LOG(LogFFBuildingAssetRelocation, Display, TEXT("FF_BUILDING_ASSET_%s_%s"),
            *Phase.ToUpper(), Success && Errors.IsEmpty() && Written ? TEXT("PASS") : TEXT("FAIL"));
    }
    FString Phase;
    TSharedRef<FJsonObject> Report;
    TArray<TSharedPtr<FJsonValue>> Errors;
    TArray<FEntry> Entries;
    TArray<TStrongObjectPtr<UObject>> AssetPins;
    TArray<TStrongObjectPtr<UPackage>> PackagePins;
    TArray<FString> AdditionalReferencers;
};

static bool EntryWorld(FRun& Run)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    return World && !GEditor->PlayWorld && World->GetOutermost()->GetName() == TEXT("/Engine/Maps/Entry")
        ? true : Run.Error(TEXT("Requires a dedicated idle Entry editor; no PIE or game map may be open."));
}

static UObject* LoadExactAsset(const FString& Package)
{
    // LoadObject<UObject> follows redirectors; find by full name after loading the package instead.
    if (!FPackageName::DoesPackageExist(Package)) return nullptr;
    if (!LoadPackage(nullptr, *Package, LOAD_None)) return nullptr;
    return FindObject<UObject>(nullptr, *ObjectPath(Package));
}

static bool ReadAndPreflight(FRun& Run, IAssetRegistry& Registry, bool RequireMoved)
{
    FString Source;
    TSharedPtr<FJsonObject> Manifest;
    if (!FFileHelper::LoadFileToString(Source, *(WorkDir() / TEXT("manifest.json"))) ||
        !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Source), Manifest) || !Manifest)
        return Run.Error(TEXT("Cannot read the relocation manifest JSON."));
    const TArray<TSharedPtr<FJsonValue>>* Rows = nullptr;
    if (!Manifest->TryGetArrayField(TEXT("assets"), Rows) || !Rows || Rows->IsEmpty())
        return Run.Error(TEXT("Manifest assets must be a non-empty array of old/new package names."));
    const TArray<TSharedPtr<FJsonValue>>* Extra = nullptr;
    if (Manifest->TryGetArrayField(TEXT("additional_referencers"), Extra) && Extra)
        for (const auto& Value : *Extra)
        {
            FString Name;
            if (!Value->TryGetString(Name) || !Name.StartsWith(TEXT("/Game/")) || !FPackageName::IsValidLongPackageName(Name))
                return Run.Error(TEXT("additional_referencers must contain valid /Game package names."));
            Run.AdditionalReferencers.AddUnique(Name);
        }
    TSet<FName> Sources, Destinations;
    for (const auto& Value : *Rows)
    {
        const TSharedPtr<FJsonObject>* Row = nullptr;
        FEntry Entry;
        if (!Value->TryGetObject(Row) || !Row || !(*Row)->TryGetStringField(TEXT("old"), Entry.Old) ||
            !(*Row)->TryGetStringField(TEXT("new"), Entry.New))
            return Run.Error(TEXT("Manifest row is missing old/new package names."));
        const bool Data = Entry.Old.StartsWith(TEXT("/Game/FestaFiesta/Zone/Data/FacilityDefinitions/"));
        const FString DestinationRoot = Data ? TEXT("/Game/FestaFiesta/Building/FF_Data_Building/")
            : TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/");
        if (!FPackageName::IsValidLongPackageName(Entry.Old) || !FPackageName::IsValidLongPackageName(Entry.New) ||
            !Entry.Old.StartsWith(TEXT("/Game/")) || !Entry.New.StartsWith(DestinationRoot) ||
            Entry.Old == Entry.New || FPackageName::GetLongPackageAssetName(Entry.Old) != FPackageName::GetLongPackageAssetName(Entry.New) ||
            Sources.Contains(FName(Entry.Old)) || Destinations.Contains(FName(Entry.New)))
            return Run.Error(FString::Printf(TEXT("Invalid, duplicate, out-of-scope or asset-name-changing mapping: %s -> %s"), *Entry.Old, *Entry.New));
        Sources.Add(FName(Entry.Old)); Destinations.Add(FName(Entry.New));
        Run.Entries.Add(MoveTemp(Entry));
    }
    for (const FName Destination : Destinations)
        if (Sources.Contains(Destination)) return Run.Error(TEXT("Source/destination chains and cycles are not permitted in this manifest."));
    Registry.SearchAllAssets(true);
    Registry.WaitForCompletion();
    for (FEntry& Entry : Run.Entries)
    {
        UObject* Old = LoadExactAsset(Entry.Old);
        UObject* New = LoadExactAsset(Entry.New);
        auto* Redirector = Cast<UObjectRedirector>(Old);
        if (New)
        {
            if (Cast<UObjectRedirector>(New) || (Old && !Redirector) || (Redirector && Redirector->DestinationObject != New))
                return Run.Error(FString::Printf(TEXT("Destination collision or unrelated redirector: %s"), *Entry.New));
            Entry.Asset = New; Entry.AlreadyMoved = true;
        }
        else
        {
            if (RequireMoved || !Old || Redirector || FPackageName::DoesPackageExist(Entry.New))
                return Run.Error(FString::Printf(TEXT("Missing source, unreadable destination, or move has not completed: %s -> %s"), *Entry.Old, *Entry.New));
            Entry.Asset = Old;
        }
        if (!Entry.Asset->IsAsset() || !Entry.Asset->GetClass() || Entry.Asset->GetClass()->HasAnyClassFlags(CLASS_NewerVersionExists))
            return Run.Error(FString::Printf(TEXT("Asset or asset class failed to load: %s"), *Entry.Old));
        Entry.ClassPath = Entry.Asset->GetClass()->GetPathName();
        Run.AssetPins.Emplace(Entry.Asset);
        FString Filename;
        if (FPackageName::DoesPackageExist(Entry.AlreadyMoved ? Entry.New : Entry.Old, &Filename) && IFileManager::Get().IsReadOnly(*Filename))
            return Run.Error(FString::Printf(TEXT("Source asset is read-only: %s"), *Filename));
    }
    return true;
}

static TMap<FSoftObjectPath, FSoftObjectPath> RedirectMap(const FRun& Run)
{
    TMap<FSoftObjectPath, FSoftObjectPath> Map;
    for (const FEntry& Entry : Run.Entries)
    {
        const FString Old = ObjectPath(Entry.Old), New = ObjectPath(Entry.New);
        Map.Add(FSoftObjectPath(Old), FSoftObjectPath(New));
        if (Cast<UBlueprint>(Entry.Asset))
        {
            Map.Add(FSoftObjectPath(Old + TEXT("_C")), FSoftObjectPath(New + TEXT("_C")));
            Map.Add(FSoftObjectPath(Entry.Old + TEXT(".Default__") + Entry.Asset->GetName() + TEXT("_C")),
                FSoftObjectPath(Entry.New + TEXT(".Default__") + Entry.Asset->GetName() + TEXT("_C")));
        }
    }
    return Map;
}

static bool LoadReferencers(FRun& Run, IAssetRegistry& Registry, TArray<UPackage*>& Packages)
{
    TSet<FName> Names;
    TSet<FName> MovedNames;
    for (const FEntry& Entry : Run.Entries)
    {
        TArray<FName> Referencers;
        Registry.GetReferencers(FName(Entry.Old), Referencers);
        Names.Append(Referencers);
        Names.Add(FName(Entry.AlreadyMoved ? Entry.New : Entry.Old));
        MovedNames.Add(FName(Entry.Old)); MovedNames.Add(FName(Entry.New));
    }
    for (const FString& Name : Run.AdditionalReferencers) Names.Add(FName(Name));
    TArray<FName> Sorted = Names.Array();
    Sorted.Sort(FNameLexicalLess());
    TArray<TSharedPtr<FJsonValue>> ReportNames;
    TArray<TSharedPtr<FJsonValue>> Backups;
    for (const FName Name : Sorted)
    {
        const FString Path = Name.ToString();
        if (!Path.StartsWith(TEXT("/Game/"))) continue;
        FString Filename;
        if (!FPackageName::DoesPackageExist(Path, &Filename)) return Run.Error(TEXT("Referencer package is missing: ") + Path);
        if (IFileManager::Get().IsReadOnly(*Filename)) return Run.Error(TEXT("Referencer package is read-only: ") + Filename);
        if (!MovedNames.Contains(Name))
        {
            Filename = FPaths::ConvertRelativePathToFull(Filename);
            FString Relative = Filename;
            const FString ProjectRoot = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
            if (!FPaths::MakePathRelativeTo(Relative, *ProjectRoot) || !Relative.StartsWith(TEXT("Content/")) || Relative.Contains(TEXT("..")))
                return Run.Error(TEXT("External referencer filename is outside project Content: ") + Filename);
            const FString Destination = WorkDir() / TEXT("referencer_backup") / Relative;
            for (const FString& Extension : {FPaths::GetExtension(Filename, true), FString(TEXT(".uexp")), FString(TEXT(".ubulk"))})
            {
                const FString From = FPaths::ChangeExtension(Filename, Extension);
                const FString To = FPaths::ChangeExtension(Destination, Extension);
                if (!IFileManager::Get().FileExists(*From) || IFileManager::Get().FileExists(*To)) continue;
                IFileManager::Get().MakeDirectory(*FPaths::GetPath(To), true);
                if (IFileManager::Get().Copy(*To, *From, false) != COPY_OK)
                    return Run.Error(TEXT("External referencer backup failed: ") + From);
            }
            Backups.Add(MakeShared<FJsonValueString>(Destination));
        }
        UPackage* Package = LoadPackage(nullptr, *Path, LOAD_None);
        if (!Package) return Run.Error(TEXT("Could not load referencer package: ") + Path);
        Package->FullyLoad(); Packages.AddUnique(Package); Run.PackagePins.Emplace(Package);
        ReportNames.Add(MakeShared<FJsonValueString>(Path));
    }
    Run.Report->SetArrayField(TEXT("referencer_packages"), ReportNames);
    Run.Report->SetArrayField(TEXT("external_referencer_backups"), Backups);
    return true;
}

static int32 RewriteStringValue(FProperty* Property, void* Value, const TMap<FString, FString>& Replacements, int32 Depth)
{
    if (Depth > 32) return 0;
    if (auto* String = CastField<FStrProperty>(Property))
    {
        if (const FString* Replacement = Replacements.Find(String->GetPropertyValue(Value)))
        { String->SetPropertyValue(Value, *Replacement); return 1; }
        return 0;
    }
    int32 Count = 0;
    if (auto* Struct = CastField<FStructProperty>(Property))
        for (TFieldIterator<FProperty> It(Struct->Struct); It; ++It)
            for (int32 Index = 0; Index < It->ArrayDim; ++Index)
                Count += RewriteStringValue(*It, It->ContainerPtrToValuePtr<void>(Value, Index), Replacements, Depth + 1);
    else if (auto* Array = CastField<FArrayProperty>(Property))
    {
        FScriptArrayHelper Helper(Array, Value);
        for (int32 Index = 0; Index < Helper.Num(); ++Index)
            Count += RewriteStringValue(Array->Inner, Helper.GetRawPtr(Index), Replacements, Depth + 1);
    }
    else if (auto* Map = CastField<FMapProperty>(Property))
    {
        FScriptMapHelper Helper(Map, Value);
        for (int32 Index = 0; Index < Helper.GetMaxIndex(); ++Index) if (Helper.IsValidIndex(Index))
        {
            Count += RewriteStringValue(Map->KeyProp, Helper.GetKeyPtr(Index), Replacements, Depth + 1);
            Count += RewriteStringValue(Map->ValueProp, Helper.GetValuePtr(Index), Replacements, Depth + 1);
        }
        if (Count) Helper.Rehash();
    }
    else if (auto* Set = CastField<FSetProperty>(Property))
    {
        FScriptSetHelper Helper(Set, Value);
        for (int32 Index = 0; Index < Helper.GetMaxIndex(); ++Index) if (Helper.IsValidIndex(Index))
            Count += RewriteStringValue(Set->ElementProp, Helper.GetElementPtr(Index), Replacements, Depth + 1);
        if (Count) Helper.Rehash();
    }
    return Count;
}

static void RewriteExactStringPaths(FRun& Run, const TArray<UPackage*>& Packages)
{
    TMap<FString, FString> Replacements;
    for (const FEntry& Entry : Run.Entries)
    {
        Replacements.Add(Entry.Old, Entry.New);
        const FString OldObject = ObjectPath(Entry.Old), NewObject = ObjectPath(Entry.New);
        Replacements.Add(OldObject, NewObject);
        if (Cast<UBlueprint>(Entry.Asset))
        {
            Replacements.Add(OldObject + TEXT("_C"), NewObject + TEXT("_C"));
            Replacements.Add(TEXT("WidgetBlueprintGeneratedClass'") + OldObject + TEXT("_C'"),
                TEXT("WidgetBlueprintGeneratedClass'") + NewObject + TEXT("_C'"));
            Replacements.Add(TEXT("Class'") + OldObject + TEXT("_C'"), TEXT("Class'") + NewObject + TEXT("_C'"));
        }
    }
    TArray<TSharedPtr<FJsonValue>> ChangedObjects;
    int32 Count = 0;
    for (UPackage* Package : Packages)
    {
        TArray<UObject*> Objects;
        GetObjectsWithPackage(Package, Objects);
        for (UObject* Object : Objects)
        {
            if (!IsValid(Object)) continue;
            int32 ObjectCount = 0;
            for (TFieldIterator<FProperty> It(Object->GetClass()); It; ++It)
                for (int32 Index = 0; Index < It->ArrayDim; ++Index)
                    ObjectCount += RewriteStringValue(*It, It->ContainerPtrToValuePtr<void>(Object, Index), Replacements, 0);
            if (ObjectCount)
            {
                Object->MarkPackageDirty(); Count += ObjectCount;
                ChangedObjects.Add(MakeShared<FJsonValueString>(Object->GetPathName()));
            }
        }
    }
    Run.Report->SetNumberField(TEXT("exact_string_path_replacements"), Count);
    Run.Report->SetArrayField(TEXT("string_path_objects"), ChangedObjects);
}

static bool SaveChangedPackages(FRun& Run, const TArray<UPackage*>& Packages)
{
    TArray<UPackage*> Changed;
    TArray<TSharedPtr<FJsonValue>> Names;
    for (UPackage* Package : Packages)
        if (Package && Package->GetName().StartsWith(TEXT("/Game/")) && Package->IsDirty())
        { Changed.AddUnique(Package); Names.Add(MakeShared<FJsonValueString>(Package->GetName())); }
    Run.Report->SetArrayField(TEXT("saved_packages"), Names);
    return Changed.IsEmpty() || UEditorLoadingAndSavingUtils::SavePackages(Changed, true)
        ? true : Run.Error(TEXT("One or more changed referencer packages failed to save; redirectors must be retained."));
}

static void Rescan(IAssetRegistry& Registry, const FRun& Run)
{
    TArray<FString> Paths;
    for (const FEntry& Entry : Run.Entries)
    {
        Paths.AddUnique(FPackageName::GetLongPackagePath(Entry.Old));
        Paths.AddUnique(FPackageName::GetLongPackagePath(Entry.New));
    }
    for (const TStrongObjectPtr<UPackage>& Package : Run.PackagePins)
        if (Package && Package->GetName().StartsWith(TEXT("/Game/")))
            Paths.AddUnique(FPackageName::GetLongPackagePath(Package->GetName()));
    Registry.ScanPathsSynchronous(Paths, true);
    Registry.WaitForCompletion();
}

static void Move()
{
    FRun Run(TEXT("move"));
    IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
    IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();
    TArray<UPackage*> Packages;
    if (!EntryWorld(Run) || !ReadAndPreflight(Run, Registry, false) || !LoadReferencers(Run, Registry, Packages))
    { Run.Finish(false); return; }
    const auto Map = RedirectMap(Run);
    // Update transient native defaults as part of the move. Never save script packages; source/config
    // paths are updated and rebuilt by the separate checked-in migration step before deleting redirects.
    TArray<UPackage*> NativePackages;
    for (const TCHAR* Name : {TEXT("/Script/FestaFiesta"), TEXT("/Script/FF_ZoneSystem")})
        if (UPackage* Package = FindPackage(nullptr, Name)) NativePackages.Add(Package);
    AssetTools.RenameReferencingSoftObjectPaths(NativePackages, Map);
    TArray<FAssetRenameData> Renames;
    for (const FEntry& Entry : Run.Entries)
        if (!Entry.AlreadyMoved) Renames.Emplace(Entry.Asset, FPackageName::GetLongPackagePath(Entry.New), Entry.Asset->GetName());
    Run.Report->SetNumberField(TEXT("requested_renames"), Renames.Num());
    const bool Renamed = Renames.IsEmpty() || AssetTools.RenameAssets(Renames);
    Run.Report->SetBoolField(TEXT("rename_api_result"), Renamed);
    if (!Renamed)
    {
        Run.Error(TEXT("AssetTools RenameAssets failed or declined a native CDO reference warning. Inspect the engine log and per-asset destinations; no redirector cleanup was attempted."));
        Run.Finish(false); return;
    }
    for (const FEntry& Entry : Run.Entries)
    {
        if (Entry.Asset->GetOutermost()->GetName() != Entry.New || Entry.Asset->GetClass()->GetPathName() != Entry.ClassPath)
        { Run.Error(TEXT("Renamed asset path/class mismatch: ") + Entry.New); Run.Finish(false); return; }
        Packages.AddUnique(Entry.Asset->GetOutermost());
    }
    AssetTools.RenameReferencingSoftObjectPaths(Packages, Map);
    RewriteExactStringPaths(Run, Packages);
    const bool Saved = SaveChangedPackages(Run, Packages);
    Rescan(Registry, Run);
    Run.Report->SetStringField(TEXT("next_step"), TEXT("Rewrite source/config/tool paths, rebuild the editor, then run FF.FixupBuildingAssetRedirectors."));
    Run.Finish(Saved);
}

static void Fixup()
{
    FRun Run(TEXT("fixup"));
    IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
    IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();
    TArray<UPackage*> Packages;
    if (!EntryWorld(Run) || !ReadAndPreflight(Run, Registry, true) || !LoadReferencers(Run, Registry, Packages))
    { Run.Finish(false); return; }
    AssetTools.RenameReferencingSoftObjectPaths(Packages, RedirectMap(Run));
    RewriteExactStringPaths(Run, Packages);
    // Hard references resolved through redirectors also need serialization even when loading did not mark dirty.
    for (UPackage* Package : Packages) Package->MarkPackageDirty();
    if (!SaveChangedPackages(Run, Packages)) { Run.Finish(false); return; }
    Rescan(Registry, Run);
    TArray<UObject*> Redirectors;
    for (const FEntry& Entry : Run.Entries)
    {
        UObject* Old = LoadExactAsset(Entry.Old);
        if (!Old) continue;
        auto* Redirector = Cast<UObjectRedirector>(Old);
        if (!Redirector || Redirector->DestinationObject != Entry.Asset)
        { Run.Error(TEXT("Unexpected object remains at the old asset path: ") + Entry.Old); continue; }
        TArray<FName> Referencers;
        Registry.GetReferencers(FName(Entry.Old), Referencers);
        Referencers.Remove(FName(Entry.Old));
        if (!Referencers.IsEmpty())
        {
            for (const FName Name : Referencers) Run.Error(TEXT("Redirector retained because a referencer remains: ") + Entry.Old + TEXT(" <- ") + Name.ToString());
            continue;
        }
        Redirectors.Add(Redirector);
    }
    Run.Report->SetNumberField(TEXT("verified_unreferenced_redirectors"), Redirectors.Num());
    if (!Run.Errors.IsEmpty()) { Run.Finish(false); return; }
    // UE 5.7 IAssetTools::FixupReferencers always opens SFixupRedirectorsReport even with
    // DeleteFixedUpRedirectors and checkout prompts disabled. Use its non-modal delete API only
    // after the exact same load/soft-rewrite/save sequence and an explicit zero-referencer check.
    Run.PackagePins.Reset(); // Do not keep empty source packages alive during the editor deletion API.
    const int32 Deleted = Redirectors.IsEmpty() ? 0 : ObjectTools::DeleteObjects(Redirectors, false);
    Run.Report->SetNumberField(TEXT("deleted_redirectors"), Deleted);
    Rescan(Registry, Run);
    if (Deleted != Redirectors.Num()) Run.Error(TEXT("Editor asset deletion did not remove every verified redirector."));
    for (const FEntry& Entry : Run.Entries)
    {
        if (FPackageName::DoesPackageExist(Entry.Old)) Run.Error(TEXT("Old redirector package still exists: ") + Entry.Old);
        if (!FPackageName::DoesPackageExist(Entry.New)) Run.Error(TEXT("Destination package is missing: ") + Entry.New);
    }
    Run.Finish(Run.Errors.IsEmpty());
}

static FAutoConsoleCommand MoveCommand(TEXT("FF.MoveBuildingAssets"),
    TEXT("Move only assets in Saved/BuildingAssetRelocation_20260921/manifest.json using AssetTools; keep redirectors until source paths are rebuilt."),
    FConsoleCommandDelegate::CreateStatic(&Move));
static FAutoConsoleCommand FixupCommand(TEXT("FF.FixupBuildingAssetRedirectors"),
    TEXT("After the path rewrite/rebuild, resave manifest referencers and remove only redirectors proven to have no remaining referencers."),
    FConsoleCommandDelegate::CreateStatic(&Fixup));
}
#endif
