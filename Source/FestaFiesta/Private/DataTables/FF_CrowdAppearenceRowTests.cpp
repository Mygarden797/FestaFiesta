// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Animation/AnimBlueprintGeneratedClass.h"
#include "Animation/AnimBoneCompressionCodec.h"
#include "Animation/AnimNode_AssetPlayerBase.h"
#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace.h"
#include "Animation/Skeleton.h"
#include "Components/SkeletalMeshComponent.h"
#include "CrowdSystem/Components/FF_CrowdAnimInstance.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "CrowdSystem/Types/FF_SimulationSettings.h"
#include "DataTables/FF_CrowdAppearenceRow.h"
#include "Engine/DataTable.h"
#include "Engine/SkeletalMesh.h"
#include "Misc/ScopeExit.h"
#include "UObject/UnrealType.h"
#if WITH_EDITOR
#include "SkinnedAssetCompiler.h"
#endif

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
	struct FMotionSample
	{
		float MaxDelta = 0.f;
		int32 FirstPoseCount = 0;
		int32 LastPoseCount = 0;
	};

	FMotionSample SampleComponentMotion(USkeletalMeshComponent& Component)
	{
		FMotionSample Result;
		TArray<FTransform> First;
		// 초기 참조 포즈에서 한 번 바뀌는 현상을 지속적인 움직임으로 오인하지 않는다.
		// 처음 10프레임을 준비한 뒤 90프레임 동안의 실제 움직임을 측정한다.
		constexpr int32 WarmupFrames = 10;
		for (int32 Tick = 0; Tick < WarmupFrames + 90; ++Tick)
		{
			++GFrameCounter;
			Component.TickAnimation(1.f / 30.f, false);
			Component.RefreshBoneTransforms();
			const TArray<FTransform>& Now = Component.GetComponentSpaceTransforms();
			Result.LastPoseCount = Now.Num();
			if (Tick < WarmupFrames) continue;
			if (Tick == WarmupFrames)
			{
				First = Now;
				Result.FirstPoseCount = Now.Num();
				continue;
			}
			for (int32 Bone = 0; Bone < Now.Num() && Bone < First.Num(); ++Bone)
			{
				Result.MaxDelta = FMath::Max(Result.MaxDelta,
					FVector::Dist(Now[Bone].GetLocation(), First[Bone].GetLocation()));
			}
		}
		return Result;
	}

	float SampleSequenceMotion(const UAnimSequence& Sequence, const bool bUseRawData)
	{
		const USkeleton* Skeleton = Sequence.GetSkeleton();
		if (!Skeleton || Sequence.GetPlayLength() <= 0.f) return 0.f;
		const FReferenceSkeleton& Reference = Skeleton->GetReferenceSkeleton();
		TArray<FTransform> First;
		float MaxDelta = 0.f;
		for (int32 Frame = 0; Frame < 90; ++Frame)
		{
			const double Time = FMath::Fmod(Frame / 30.0, static_cast<double>(Sequence.GetPlayLength()));
			const FAnimExtractContext Context(Time);
			TArray<FTransform> Pose;
			Pose.SetNum(Reference.GetNum());
			for (int32 Bone = 0; Bone < Reference.GetNum(); ++Bone)
			{
				FTransform Local = Reference.GetRefBonePose()[Bone];
				Sequence.GetBoneTransform(Local, FSkeletonPoseBoneIndex(Bone), Context, bUseRawData);
				const int32 Parent = Reference.GetParentIndex(Bone);
				Pose[Bone] = Parent == INDEX_NONE ? Local : Local * Pose[Parent];
				if (Frame > 0)
					MaxDelta = FMath::Max(MaxDelta, FVector::Dist(Pose[Bone].GetLocation(), First[Bone].GetLocation()));
			}
			if (Frame == 0) First = MoveTemp(Pose);
		}
		return MaxDelta;
	}

	void PrepareSequenceForEvaluation(FAutomationTestBase& Test, UAnimSequence& Sequence)
	{
#if WITH_EDITOR
		// UE 5.7은 플랫폼 압축 데이터가 아직 없을 때도 빈 임시 데이터를 valid로
		// 취급할 수 있다. 메시 준비와 별도로 원본 클립의 실제 압축 결과를 기다린다.
		// 아래 읽기 잠금을 잡은 채 컴파일을 기다리면 교착될 수 있으므로 순서를 지킨다.
		Sequence.WaitOnExistingCompression();
		Sequence.CacheDerivedDataForCurrentPlatform();
		Sequence.WaitOnExistingCompression();
#endif
		const FAnimExtractContext ExtractionContext(0.0);
		const auto CompressedScope = Sequence.GetCompressedData(ExtractionContext);
		const FCompressedAnimSequence& Compressed = CompressedScope.Get();
		Test.AddInfo(FString::Printf(TEXT("Animation ready: asset=%s tracks=%d bytes=%d codec=%s data=%d"),
			*Sequence.GetPathName(), Compressed.CompressedTrackToSkeletonMapTable.Num(),
			Compressed.CompressedByteStream.Num(), *GetNameSafe(Compressed.BoneCompressionCodec),
			Compressed.CompressedDataStructure.IsValid()));
		Test.TestTrue(FString::Printf(TEXT("%s has resident compressed bone tracks, codec and data before evaluation"), *Sequence.GetName()),
			!Compressed.CompressedTrackToSkeletonMapTable.IsEmpty() &&
			Compressed.BoneCompressionCodec != nullptr && Compressed.CompressedDataStructure.IsValid());
	}

	float MeasureMotion(FAutomationTestBase& Test, UWorld* World, USkeletalMesh* Mesh, UClass* AnimClass, float Speed,
		bool& bOutHasInstance, TSet<UAnimSequence*>& PreparedSequences)
	{
#if WITH_EDITOR
		// 에셋 로드는 비동기 스킨 메시 빌드까지 기다리지 않는다. 컴파일 중에는
		// TickAnimation이 평가를 생략하므로, 측정할 메시의 준비가 끝난 뒤 시작한다.
		TArray<USkinnedAsset*> AssetsToCompile{Mesh};
		FSkinnedAssetCompilingManager::Get().FinishCompilation(AssetsToCompile);
#endif
		AActor* Actor = World->SpawnActor<AActor>();
		USkeletalMeshComponent* Comp = NewObject<USkeletalMeshComponent>(Actor);
		Actor->AddInstanceComponent(Comp);
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

		const auto* Generated = Cast<UAnimBlueprintGeneratedClass>(AnimClass);
		if (Generated && Instance)
		{
			for (const FStructProperty* Property : Generated->GetAnimNodeProperties())
			{
				if (!Property || !Property->Struct->IsChildOf(FAnimNode_AssetPlayerBase::StaticStruct())) continue;
				const auto* Player = Property->ContainerPtrToValuePtr<FAnimNode_AssetPlayerBase>(Instance);
				UAnimationAsset* Asset = Player->GetAnimAsset();
				auto Prepare = [&Test, &PreparedSequences](UAnimSequence* Sequence)
				{
					if (Sequence && !PreparedSequences.Contains(Sequence))
					{
						PrepareSequenceForEvaluation(Test, *Sequence);
						PreparedSequences.Add(Sequence);
					}
				};
				if (const auto* BlendSpace = Cast<UBlendSpace>(Asset))
					for (const FBlendSample& Sample : BlendSpace->GetBlendSamples()) Prepare(Sample.Animation.Get());
				else Prepare(Cast<UAnimSequence>(Asset));
			}
		}
		const uint64 PreviousFrameCounter = GFrameCounter;
		ON_SCOPE_EXIT { GFrameCounter = PreviousFrameCounter; };
		const FMotionSample Motion = SampleComponentMotion(*Comp);
		Test.AddInfo(FString::Printf(TEXT("Animation evaluation: mesh=%s class=%s refBones=%d pose=%d/%d nodes=%d registered=%d updateEnabled=%d updateCounter=%d state=%s stateTime=%.3f finalSpeed=%.1f"),
			*Mesh->GetName(), *AnimClass->GetPathName(), Mesh->GetRefSkeleton().GetNum(),
			Motion.FirstPoseCount, Motion.LastPoseCount, Generated ? Generated->GetAnimNodeProperties().Num() : 0,
			Comp->IsRegistered(), Instance && Instance->IsUpdateAnimationEnabled(),
			Instance ? Instance->GetUpdateCounter().Get() : -1,
			Instance && Generated && !Generated->GetBakedStateMachines().IsEmpty()
				? *Instance->GetCurrentStateName(0).ToString() : TEXT("none"),
			Instance && Generated && !Generated->GetBakedStateMachines().IsEmpty()
				? Instance->GetInstanceCurrentStateElapsedTime(0) : -1.f,
			Instance ? Instance->GetSpeed() : -1.f));
		Test.TestEqual(TEXT("Animation sample contains every reference skeleton bone"), Motion.LastPoseCount, Mesh->GetRefSkeleton().GetNum());

		// 실패 때 실제 그래프의 재생 시간과 가중치를 기록한다. 같은 컴포넌트에서
		// 단일 원본 클립도 평가해 그래프/에셋 문제와 측정 하네스 문제를 구분한다.
		if (Motion.MaxDelta < 0.5f)
		{
			UAnimSequence* ControlSequence = nullptr;
			if (Generated && Instance)
			{
				for (const FStructProperty* Property : Generated->GetAnimNodeProperties())
				{
					if (!Property || !Property->Struct->IsChildOf(FAnimNode_AssetPlayerBase::StaticStruct())) continue;
					const auto* Player = Property->ContainerPtrToValuePtr<FAnimNode_AssetPlayerBase>(Instance);
					UAnimationAsset* Asset = Player->GetAnimAsset();
					const auto* BlendSpace = Cast<UBlendSpace>(Asset);
					Test.AddInfo(FString::Printf(TEXT("Animation player: node=%s asset=%s time=%.3f weight=%.3f samples=%d"),
						*Property->GetName(), *GetPathNameSafe(Asset), Player->GetAccumulatedTime(),
						Player->GetCachedBlendWeight(), BlendSpace ? BlendSpace->GetBlendSamples().Num() : -1));
					if (BlendSpace)
						for (const FBlendSample& Sample : BlendSpace->GetBlendSamples())
						{
							Test.AddInfo(FString::Printf(TEXT("Animation blend sample: asset=%s value=%s rate=%.3f singleFrame=%d frame=%u"),
								*GetPathNameSafe(Sample.Animation), *Sample.SampleValue.ToString(), Sample.RateScale,
								Sample.bUseSingleFrameForBlending, Sample.FrameIndexToSample));
							if (!ControlSequence && Sample.Animation && Sample.Animation->GetSkeleton() == Mesh->GetSkeleton())
								ControlSequence = Sample.Animation;
						}
				}
			}
			if (!ControlSequence && Mesh->GetName() == TEXT("SK_20_male_1"))
				ControlSequence = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/Level/Demo_Matereal/Mesh/anim/NPC/Anim/SK_20_male_1/A_20_male_1_walk_male.A_20_male_1_walk_male"));
			if (ControlSequence)
			{
				const float ExtractedMotion = SampleSequenceMotion(*ControlSequence, false);
				float RawExtractedMotion = -1.f;
#if WITH_EDITOR
				RawExtractedMotion = SampleSequenceMotion(*ControlSequence, true);
				Test.AddInfo(FString::Printf(TEXT("Animation compression: asset=%s compressedValid=%d keys=%d"),
					*ControlSequence->GetName(), ControlSequence->IsCompressedDataValid(), ControlSequence->GetNumberOfSampledKeys()));
#endif
				Comp->PlayAnimation(ControlSequence, true);
				const FMotionSample Control = SampleComponentMotion(*Comp);
				Test.AddInfo(FString::Printf(TEXT("Animation direct-clip control: asset=%s length=%.3f pose=%d/%d motion=%.3f runtimeExtractMotion=%.3f rawExtractMotion=%.3f"),
					*ControlSequence->GetPathName(), ControlSequence->GetPlayLength(),
					Control.FirstPoseCount, Control.LastPoseCount, Control.MaxDelta, ExtractedMotion, RawExtractedMotion));
				Test.TestTrue(TEXT("Direct source animation proves the skeletal motion harness evaluates a moving pose"), Control.MaxDelta >= 0.5f);
			}
		}
		Actor->Destroy();
		return Motion.MaxDelta;
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

	TSet<UAnimSequence*> PreparedSequences;
	for (const TPair<FName, uint8*>& Pair : Table->GetRowMap())
	{
		const FFF_CrowdAppearenceRow* Row = reinterpret_cast<const FFF_CrowdAppearenceRow*>(Pair.Value);
		for (const FFF_CrowdAppearanceVariant& Variant : Row->Variants)
		{
			USkeletalMesh* Mesh = Variant.Mesh.LoadSynchronous();
			UClass* AnimClass = (Variant.AnimClass.IsNull() ? Row->AnimClass : Variant.AnimClass).LoadSynchronous();
			if (!TestNotNull(*FString::Printf(TEXT("%s variant 메시를 로드한다: %s"),
				*Pair.Key.ToString(), *Variant.Mesh.ToString()), Mesh) ||
				!TestNotNull(*FString::Printf(TEXT("%s variant 애니메이션 클래스를 로드한다"), *Pair.Key.ToString()), AnimClass))
			{
				continue;
			}
			for (const float Speed : { 0.f, 100.f })
			{
				bool bHasInstance = false;
				const float Motion = MeasureMotion(*this, World, Mesh, AnimClass, Speed, bHasInstance, PreparedSequences);
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
