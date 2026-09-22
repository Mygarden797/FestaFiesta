// FF_PackagingInteractionRepair.cpp
// 역할: 에디터에서 확인된 상호작용 Blueprint 연결 오류를 수정한다.

// Editor-only repair for stale native interaction references in the release map.
#if WITH_EDITOR

#include "CoreMinimal.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimData/IAnimationDataModel.h"
#include "Animation/Skeleton.h"
#include "EdGraph/EdGraph.h"
#include "Engine/Level.h"
#include "Engine/LevelScriptBlueprint.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Interaction/FF_TriggerInteractionActor.h"
#include "K2Node_CallFunction.h"
#include "K2Node_DynamicCast.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UObject/SavePackage.h"

namespace
{
void RepairPackagingInteractionGraph(const TArray<FString>& Args)
{
    const bool bApply = Args.Num() == 1 && Args[0] == TEXT("apply");
    const FString PackageName = TEXT("/Game/FestaFiesta/Maps/Test_Main");
    UWorld* World = LoadObject<UWorld>(nullptr, *(PackageName + TEXT(".Test_Main")));
    ULevelScriptBlueprint* Blueprint = World && World->PersistentLevel
        ? World->PersistentLevel->GetLevelScriptBlueprint(true) : nullptr;
    if (!Blueprint)
    {
        UE_LOG(LogTemp, Error, TEXT("FF_PACKAGING_REPAIR_FAIL: Missing Test_Main level blueprint."));
        return;
    }

    TArray<UEdGraph*> Graphs;
    Blueprint->GetAllGraphs(Graphs);
    TArray<UK2Node_DynamicCast*> Casts;
    TArray<UK2Node_CallFunction*> Calls;
    for (UEdGraph* Graph : Graphs)
    {
        for (UEdGraphNode* Node : Graph->Nodes)
        {
            if (UK2Node_DynamicCast* CastNode = Cast<UK2Node_DynamicCast>(Node))
            {
                if (!CastNode->TargetType)
                {
                    for (const UEdGraphPin* Pin : CastNode->Pins)
                    {
                        if (Pin && Pin->PinName == TEXT("AsFF Trigger Interaction Actor"))
                        {
                            Casts.AddUnique(CastNode);
                        }
                        UE_LOG(LogTemp, Display, TEXT("FF_PACKAGING_REPAIR_CAST: %s pin=%s"),
                            *CastNode->NodeGuid.ToString(), Pin ? *Pin->PinName.ToString() : TEXT("null"));
                    }
                }
            }
            if (UK2Node_CallFunction* Call = Cast<UK2Node_CallFunction>(Node))
            {
                if (Call->FunctionReference.GetMemberName() == TEXT("TryStartInteraction")
                    && !Call->GetTargetFunction())
                {
                    Calls.Add(Call);
                }
            }
        }
    }
    UE_LOG(LogTemp, Display, TEXT("FF_PACKAGING_REPAIR_AUDIT: brokenCasts=%d brokenCalls=%d apply=%d"),
        Casts.Num(), Calls.Num(), bApply);
    if (!bApply || (Casts.IsEmpty() && Calls.IsEmpty()))
    {
        return;
    }
    // Fail closed if this is a different graph defect. Never remove nodes or links.
    if (Casts.Num() != 1 || Calls.Num() != 1)
    {
        UE_LOG(LogTemp, Error, TEXT("FF_PACKAGING_REPAIR_FAIL: Expected exactly one stale cast/call pair."));
        return;
    }
    const FString Filename = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetMapPackageExtension());
    const FString BackupDir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("PackagingRepair"), FGuid::NewGuid().ToString());
    IFileManager::Get().MakeDirectory(*BackupDir, true);
    const FString Backup = FPaths::Combine(BackupDir, TEXT("Test_Main.umap"));
    if (IFileManager::Get().Copy(*Backup, *Filename, false) != COPY_OK)
    {
        UE_LOG(LogTemp, Error, TEXT("FF_PACKAGING_REPAIR_FAIL: Could not back up map."));
        return;
    }
    Casts[0]->Modify();
    Casts[0]->TargetType = AFF_TriggerInteractionActor::StaticClass();
    Casts[0]->ReconstructNode();
    Calls[0]->Modify();
    Calls[0]->SetFromFunction(AFF_TriggerInteractionActor::StaticClass()->FindFunctionByName(TEXT("TryStartInteraction")));
    Calls[0]->ReconstructNode();
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    FKismetEditorUtilities::CompileBlueprint(Blueprint);
    if (Blueprint->Status == BS_Error)
    {
        UE_LOG(LogTemp, Error, TEXT("FF_PACKAGING_REPAIR_FAIL: Compile failed; original disk asset unchanged."));
        return;
    }
    FSavePackageArgs SaveArgs;
    SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
    SaveArgs.SaveFlags = SAVE_NoError;
    if (!UPackage::SavePackage(World->GetPackage(), World, *Filename, SaveArgs))
    {
        UE_LOG(LogTemp, Error, TEXT("FF_PACKAGING_REPAIR_FAIL: Save failed; backup=%s"), *Backup);
        return;
    }
    UE_LOG(LogTemp, Display, TEXT("FF_PACKAGING_REPAIR_DONE: Test_Main native references restored. backup=%s"), *Backup);
}

FAutoConsoleCommand RepairPackagingInteractionGraphCommand(
    TEXT("FF.Packaging.RepairInteractionGraph"),
    TEXT("Audit Test_Main stale native interaction references. Pass 'apply' to back up, repair and save the known pair."),
    FConsoleCommandWithArgsDelegate::CreateStatic(&RepairPackagingInteractionGraph));

// Deliberately does not save. The Python repair validates compatible reference
// assets and creates a disk backup before invoking this native-only API.
void RestorePackagingAnimationSkeleton(const TArray<FString>& Args)
{
    if (Args.Num() != 2)
    {
        UE_LOG(LogTemp, Error, TEXT("FF_PACKAGING_ANIM_FAIL: Expected animation and skeleton object paths."));
        return;
    }
    const FString ManRoot = TEXT("/Game/FestaFiesta/Character/Crowd/50_Man/");
    const FString ElderRoot = TEXT("/Game/FestaFiesta/Character/Crowd/Elder_Woman/");
    const bool bKnownPair =
        ((Args[0] == ManRoot + TEXT("Breathing_Idle.Breathing_Idle")
          || Args[0] == ManRoot + TEXT("Walk_With_BriefcaseTake_001.Walk_With_BriefcaseTake_001"))
          && Args[1] == ManRoot + TEXT("50_Man.50_Man"))
        || (Args[0] == ElderRoot + TEXT("Idle__2_Take_001.Idle__2_Take_001")
          && Args[1] == ElderRoot + TEXT("Elder_woman.Elder_woman"));
    if (!bKnownPair)
    {
        UE_LOG(LogTemp, Error, TEXT("FF_PACKAGING_ANIM_FAIL: Pair is outside the three audited assets."));
        return;
    }
    UAnimSequence* Animation = LoadObject<UAnimSequence>(nullptr, *Args[0]);
    USkeleton* Skeleton = LoadObject<USkeleton>(nullptr, *Args[1]);
    if (!Animation || !Skeleton || !Animation->GetDataModel()
        || (Animation->GetSkeleton() && Animation->GetSkeleton() != Skeleton))
    {
        UE_LOG(LogTemp, Error, TEXT("FF_PACKAGING_ANIM_FAIL: Missing asset/data or a different existing skeleton."));
        return;
    }
    TArray<FName> TrackNames;
    Animation->GetDataModel()->GetBoneTrackNames(TrackNames);
    if (TrackNames.IsEmpty())
    {
        UE_LOG(LogTemp, Error, TEXT("FF_PACKAGING_ANIM_FAIL: No bone tracks."));
        return;
    }
    for (FName Bone : TrackNames)
    {
        if (Skeleton->GetReferenceSkeleton().FindBoneIndex(Bone) == INDEX_NONE)
        {
            UE_LOG(LogTemp, Error, TEXT("FF_PACKAGING_ANIM_FAIL: Incompatible bone %s."), *Bone.ToString());
            return;
        }
    }
    Animation->Modify();
    if (!Animation->ReplaceSkeleton(Skeleton, false))
    {
        UE_LOG(LogTemp, Error, TEXT("FF_PACKAGING_ANIM_FAIL: ReplaceSkeleton failed."));
        return;
    }
    Animation->PostEditChange();
    UE_LOG(LogTemp, Display, TEXT("FF_PACKAGING_ANIM_READY: %s (not saved)"), *Args[0]);
}

FAutoConsoleCommand RestorePackagingAnimationSkeletonCommand(
    TEXT("FF.Packaging.RestoreAnimationSkeleton"),
    TEXT("Restore an audited missing animation skeleton in memory. Python handles compatibility audit, backup and save."),
    FConsoleCommandWithArgsDelegate::CreateStatic(&RestorePackagingAnimationSkeleton));
}
#endif
