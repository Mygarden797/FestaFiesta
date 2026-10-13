// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Animation/AnimBlueprintGeneratedClass.h"
#include "Components/SkeletalMeshComponent.h"
#include "CrowdSystem/Components/FF_CrowdAnimInstance.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "CrowdSystem/Types/FF_SimulationSettings.h"
#include "DataTables/FF_CrowdAppearenceRow.h"
#include "Engine/DataTable.h"
#include "Engine/SkeletalMesh.h"

namespace FFF_CrowdAppearenceRowTestHelpers
{
	TSoftObjectPtr<USkeletalMesh> MeshPath(const TCHAR* Name)
	{
		return TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(Name));
	}

	TSoftClassPtr<UFF_CrowdAnimInstance> ClassPath(const TCHAR* Name)
	{
		return TSoftClassPtr<UFF_CrowdAnimInstance>(FSoftObjectPath(Name));
	}

	constexpr int32 SeedCount = 512;

	bool SkeletonsMatch(const TSoftObjectPtr<USkeletalMesh>& Mesh, const TSoftClassPtr<UFF_CrowdAnimInstance>& AnimClass, FString& OutReason)
	{
		USkeletalMesh* LoadedMesh = Mesh.LoadSynchronous();
		UClass* LoadedClass = AnimClass.LoadSynchronous();
		if (LoadedMesh == nullptr || LoadedClass == nullptr)
		{
			OutReason = FString::Printf(TEXT("로드 실패 mesh=%s class=%s"), *Mesh.ToString(), *AnimClass.ToString());
			return false;
		}
		const UAnimBlueprintGeneratedClass* AnimBPClass = Cast<UAnimBlueprintGeneratedClass>(LoadedClass);
		if (AnimBPClass == nullptr || AnimBPClass->GetTargetSkeleton() == nullptr)
		{
			OutReason = FString::Printf(TEXT("%s 의 대상 스켈레톤을 알 수 없다"), *LoadedClass->GetName());
			return false;
		}
		if (AnimBPClass->GetTargetSkeleton() != LoadedMesh->GetSkeleton())
		{
			OutReason = FString::Printf(TEXT("%s 의 스켈레톤(%s)과 %s 의 스켈레톤(%s)이 다르다"),
				*LoadedClass->GetName(), *GetNameSafe(AnimBPClass->GetTargetSkeleton()), *LoadedMesh->GetName(), *GetNameSafe(LoadedMesh->GetSkeleton()));
			return false;
		}
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdAppearancePickKeepsMeshAndAnimClassPaired,
	"FestaFiesta.DataTables.CrowdAppearance.PickKeepsMeshAndAnimClassPaired",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFF_CrowdAppearancePickKeepsMeshAndAnimClassPaired::RunTest(const FString& Parameters)
{
	using namespace FFF_CrowdAppearenceRowTestHelpers;

	FFF_CrowdAppearenceRow Row;
	Row.Mesh = MeshPath(TEXT("/Game/Test/SK_Base.SK_Base"));
	Row.AnimClass = ClassPath(TEXT("/Game/Test/ABP_Base.ABP_Base_C"));

	FFF_CrowdAppearanceVariant VariantA;
	VariantA.Mesh = MeshPath(TEXT("/Game/Test/SK_A.SK_A"));
	VariantA.AnimClass = ClassPath(TEXT("/Game/Test/ABP_A.ABP_A_C"));
	FFF_CrowdAppearanceVariant VariantB;
	VariantB.Mesh = MeshPath(TEXT("/Game/Test/SK_B.SK_B"));
	VariantB.AnimClass = ClassPath(TEXT("/Game/Test/ABP_B.ABP_B_C"));
	Row.Variants = { VariantA, VariantB };

	TSet<FString> PickedMeshes;
	for (int32 Seed = 0; Seed < SeedCount; ++Seed)
	{
		TSoftObjectPtr<USkeletalMesh> Mesh;
		TSoftClassPtr<UFF_CrowdAnimInstance> AnimClass;
		Row.PickAppearance(Seed, Mesh, AnimClass);
		PickedMeshes.Add(Mesh.ToString());

		const FString Expected = Mesh.ToString().EndsWith(TEXT("SK_A")) ? TEXT("/Game/Test/ABP_A.ABP_A_C")
			: Mesh.ToString().EndsWith(TEXT("SK_B")) ? TEXT("/Game/Test/ABP_B.ABP_B_C")
			: TEXT("/Game/Test/ABP_Base.ABP_Base_C");
		if (AnimClass.ToString() != Expected)
		{
			AddError(FString::Printf(TEXT("Seed %d: 메시 %s 에 AnimClass %s 가 붙었다 (기대 %s)"), Seed, *Mesh.ToString(), *AnimClass.ToString(), *Expected));
			return false;
		}
	}
	TestEqual(TEXT("세 후보가 모두 뽑힌다"), PickedMeshes.Num(), 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdAppearanceVariantWithoutAnimClassUsesRowAnimClass,
	"FestaFiesta.DataTables.CrowdAppearance.VariantWithoutAnimClassUsesRowAnimClass",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFF_CrowdAppearanceVariantWithoutAnimClassUsesRowAnimClass::RunTest(const FString& Parameters)
{
	using namespace FFF_CrowdAppearenceRowTestHelpers;

	FFF_CrowdAppearenceRow Row;
	Row.AnimClass = ClassPath(TEXT("/Game/Test/ABP_Base.ABP_Base_C"));
	FFF_CrowdAppearanceVariant Variant;
	Variant.Mesh = MeshPath(TEXT("/Game/Test/SK_A.SK_A"));
	Row.Variants = { Variant };

	TSoftObjectPtr<USkeletalMesh> Mesh;
	TSoftClassPtr<UFF_CrowdAnimInstance> AnimClass;
	Row.PickAppearance(7, Mesh, AnimClass);
	TestEqual(TEXT("메시"), Mesh.ToString(), FString(TEXT("/Game/Test/SK_A.SK_A")));
	TestEqual(TEXT("AnimClass 는 행 값"), AnimClass.ToString(), FString(TEXT("/Game/Test/ABP_Base.ABP_Base_C")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdAppearancePickMeshMatchesPickAppearanceAndIsStable,
	"FestaFiesta.DataTables.CrowdAppearance.PickMeshMatchesPickAppearanceAndIsStable",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFF_CrowdAppearancePickMeshMatchesPickAppearanceAndIsStable::RunTest(const FString& Parameters)
{
	using namespace FFF_CrowdAppearenceRowTestHelpers;

	FFF_CrowdAppearenceRow Row;
	Row.Mesh = MeshPath(TEXT("/Game/Test/SK_Base.SK_Base"));
	FFF_CrowdAppearanceVariant Variant;
	Variant.Mesh = MeshPath(TEXT("/Game/Test/SK_A.SK_A"));
	Row.Variants = { Variant };

	for (int32 Seed = 0; Seed < SeedCount; ++Seed)
	{
		TSoftObjectPtr<USkeletalMesh> Mesh;
		TSoftClassPtr<UFF_CrowdAnimInstance> AnimClass;
		Row.PickAppearance(Seed, Mesh, AnimClass);
		if (Row.PickMesh(Seed) != Mesh)
		{
			AddError(FString::Printf(TEXT("Seed %d: PickMesh 와 PickAppearance 의 메시가 다르다"), Seed));
			return false;
		}
		TSoftObjectPtr<USkeletalMesh> Again;
		TSoftClassPtr<UFF_CrowdAnimInstance> AnimAgain;
		Row.PickAppearance(Seed, Again, AnimAgain);
		if (Again != Mesh || AnimAgain != AnimClass)
		{
			AddError(FString::Printf(TEXT("Seed %d: 같은 Seed 가 다른 결과를 냈다"), Seed));
			return false;
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdAppearanceEmptyPoolReturnsNullMesh,
	"FestaFiesta.DataTables.CrowdAppearance.EmptyPoolReturnsNullMesh",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFF_CrowdAppearanceEmptyPoolReturnsNullMesh::RunTest(const FString& Parameters)
{
	FFF_CrowdAppearenceRow Row;
	TSoftObjectPtr<USkeletalMesh> Mesh;
	TSoftClassPtr<UFF_CrowdAnimInstance> AnimClass;
	Row.PickAppearance(3, Mesh, AnimClass);
	TestTrue(TEXT("후보가 없으면 빈 메시"), Mesh.IsNull());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdAppearanceTableVariantsMatchMeshSkeleton,
	"FestaFiesta.DataTables.CrowdAppearance.TableVariantsMatchMeshSkeleton",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFF_CrowdAppearanceTableVariantsMatchMeshSkeleton::RunTest(const FString& Parameters)
{
	using namespace FFF_CrowdAppearenceRowTestHelpers;

	const UFF_SimulationSettings* Settings = GetDefault<UFF_SimulationSettings>();
	const UDataTable* Table = Settings ? Settings->CrowdAppearanceTable.LoadSynchronous() : nullptr;
	if (!TestNotNull(TEXT("설정에 지정된 외형 표를 로드한다"), Table))
	{
		return false;
	}

	int32 Checked = 0;
	for (const TPair<FName, uint8*>& Pair : Table->GetRowMap())
	{
		const FFF_CrowdAppearenceRow* Row = reinterpret_cast<const FFF_CrowdAppearenceRow*>(Pair.Value);
		FString Reason;
		for (const FFF_CrowdAppearanceVariant& Variant : Row->Variants)
		{
			++Checked;
			if (!SkeletonsMatch(Variant.Mesh, Variant.AnimClass.IsNull() ? Row->AnimClass : Variant.AnimClass, Reason))
			{
				AddError(FString::Printf(TEXT("행 %s 의 Variants: %s"), *Pair.Key.ToString(), *Reason));
			}
		}
	}
	TestTrue(TEXT("검사한 variant 가 있다"), Checked > 0);
	return true;
}

namespace FFF_CrowdAppearenceRowTestHelpers
{
	float MeasureMotion(UWorld* World, USkeletalMesh* Mesh, UClass* AnimClass, float Speed, bool& bOutHasInstance)
	{
		AActor* Actor = World->SpawnActor<AActor>();
		USkeletalMeshComponent* Comp = NewObject<USkeletalMeshComponent>(Actor);
		Actor->SetRootComponent(Comp);
		Comp->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
		Comp->RegisterComponent();
		Comp->SetSkeletalMeshAsset(Mesh);
		Comp->SetAnimInstanceClass(AnimClass);

		UFF_CrowdAnimInstance* Instance = Cast<UFF_CrowdAnimInstance>(Comp->GetAnimInstance());
		bOutHasInstance = Instance != nullptr;
		if (Instance != nullptr)
		{
			Instance->SetSpeed(Speed);
		}

		TArray<FTransform> First;
		float MaxDelta = 0.f;
		for (int32 Tick = 0; Tick < 90; ++Tick)
		{
			Comp->TickAnimation(1.f / 30.f, false);
			Comp->RefreshBoneTransforms();
			const TArray<FTransform>& Now = Comp->GetComponentSpaceTransforms();
			if (Tick == 0)
			{
				First = Now;
				continue;
			}
			for (int32 Bone = 0; Bone < Now.Num() && Bone < First.Num(); ++Bone)
			{
				MaxDelta = FMath::Max(MaxDelta, FVector::Dist(Now[Bone].GetLocation(), First[Bone].GetLocation()));
			}
		}
		Actor->Destroy();
		return MaxDelta;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdAppearanceTableVariantsAnimate,
	"FestaFiesta.DataTables.CrowdAppearance.TableVariantsAnimate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFF_CrowdAppearanceTableVariantsAnimate::RunTest(const FString& Parameters)
{
	using namespace FFF_CrowdAppearenceRowTestHelpers;

	const UFF_SimulationSettings* Settings = GetDefault<UFF_SimulationSettings>();
	const UDataTable* Table = Settings ? Settings->CrowdAppearanceTable.LoadSynchronous() : nullptr;
	if (!TestNotNull(TEXT("설정에 지정된 외형 표를 로드한다"), Table))
	{
		return false;
	}

	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
	Context.SetCurrentWorld(World);
	World->InitializeActorsForPlay(FURL());
	World->BeginPlay();

	for (const TPair<FName, uint8*>& Pair : Table->GetRowMap())
	{
		const FFF_CrowdAppearenceRow* Row = reinterpret_cast<const FFF_CrowdAppearenceRow*>(Pair.Value);
		for (const FFF_CrowdAppearanceVariant& Variant : Row->Variants)
		{
			USkeletalMesh* Mesh = Variant.Mesh.LoadSynchronous();
			UClass* AnimClass = (Variant.AnimClass.IsNull() ? Row->AnimClass : Variant.AnimClass).LoadSynchronous();
			if (Mesh == nullptr || AnimClass == nullptr)
			{
				continue;
			}
			for (const float Speed : { 0.f, 100.f })
			{
				bool bHasInstance = false;
				const float Motion = MeasureMotion(World, Mesh, AnimClass, Speed, bHasInstance);
				AddInfo(FString::Printf(TEXT("%s %s speed=%.0f instance=%d 최대 이동량=%.2f"), *Pair.Key.ToString(), *Mesh->GetName(), Speed, bHasInstance ? 1 : 0, Motion));
				if (!bHasInstance || Motion < 0.5f)
				{
					AddError(FString::Printf(TEXT("%s / %s speed=%.0f : AnimInstance=%d, 뼈가 움직이지 않는다(%.3f) — T-포즈"), *Pair.Key.ToString(), *Mesh->GetName(), Speed, bHasInstance ? 1 : 0, Motion));
				}
			}
		}
	}

	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);
	return true;
}

#endif
