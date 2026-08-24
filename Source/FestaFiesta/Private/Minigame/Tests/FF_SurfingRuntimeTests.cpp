// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/DataTable.h"
#include "Misc/AutomationTest.h"
#include "Minigame/FF_MinigameDefinition.h"
#include "Minigame/FF_SurfingRuntime.h"
#include "Minigame/Surfing/FF_SurfingRules.h"
#include "UObject/Package.h"

#include <limits>

namespace
{
	constexpr EAutomationTestFlags SurfingTestFlags =
		EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;
	constexpr float FixedStepSeconds = 1.0f / 60.0f;

	FFF_MinigameSessionContext MakeSurfingContext(const int32 Seed = 20260824)
	{
		FFF_MinigameSessionContext Context;
		Context.SessionId = FGuid(11, 12, 13, 14);
		Context.FacilityInstanceId = FGuid(21, 22, 23, 24);
		Context.FacilityDefinitionId = TEXT("Booth_ActivityGame_Surfing");
		Context.MinigameId = TEXT("Surfing");
		Context.RandomSeed = Seed;
		return Context;
	}

	UDataTable* MakeCourseTable(UObject* Outer)
	{
		UDataTable* Table = NewObject<UDataTable>(Outer);
		Table->RowStruct = FFF_SurfingCourseRow::StaticStruct();
		return Table;
	}

	void AddCourseRow(
		UDataTable& Table,
		const FName RowName,
		const EFF_SurfingObjectType Type,
		const FVector2D Position,
		const float Radius = 0.05f,
		const float ScoreOverride = 0.0f,
		const bool bEnabled = true)
	{
		FFF_SurfingCourseRow Row;
		Row.Type = Type;
		Row.PositionNormalized = Position;
		Row.CollisionRadiusNormalized = Radius;
		Row.ScoreValueOverride = ScoreOverride;
		Row.bEnabled = bEnabled;
		Table.AddRow(RowName, Row);
	}

	void PopulateMinimumCourse(UDataTable& Table, const bool bReverseInsertion = false)
	{
		if (bReverseInsertion)
		{
			AddCourseRow(
				Table,
				TEXT("Z_Coin"),
				EFF_SurfingObjectType::Coin,
				FVector2D(-0.8, -0.8));
			AddCourseRow(
				Table,
				TEXT("A_Rock"),
				EFF_SurfingObjectType::Rock,
				FVector2D(0.8, 0.8));
			return;
		}

		AddCourseRow(
			Table,
			TEXT("A_Rock"),
			EFF_SurfingObjectType::Rock,
			FVector2D(0.8, 0.8));
		AddCourseRow(
			Table,
			TEXT("Z_Coin"),
			EFF_SurfingObjectType::Coin,
			FVector2D(-0.8, -0.8));
	}

	UFF_SurfingDefinition* MakeSurfingDefinition(const bool bReverseInsertion = false)
	{
		UFF_SurfingDefinition* Definition = NewObject<UFF_SurfingDefinition>(GetTransientPackage());
		UDataTable* Table = MakeCourseTable(Definition);
		PopulateMinimumCourse(*Table, bReverseInsertion);
		Definition->CourseLayoutTable = Table;
		return Definition;
	}

	UFF_SurfingRuntime* MakeSurfingRuntime(
		UFF_SurfingDefinition* Definition,
		const FFF_MinigameSessionContext& Context,
		FString& OutFailureReason)
	{
		UFF_SurfingRuntime* Runtime = NewObject<UFF_SurfingRuntime>(GetTransientPackage());
		return Runtime->Initialize(Definition, Context, &OutFailureReason)
			? Runtime
			: nullptr;
	}

	bool IsNearlyEqualVector(
		const FVector2D& A,
		const FVector2D& B,
		const double Tolerance = 1.0e-6)
	{
		return FMath::IsNearlyEqual(A.X, B.X, Tolerance)
			&& FMath::IsNearlyEqual(A.Y, B.Y, Tolerance);
	}

	double GetArenaMetricLength(const FVector2D& Value, const double HorizontalScale)
	{
		const double MetricX = Value.X * HorizontalScale;
		return FMath::Sqrt(MetricX * MetricX + Value.Y * Value.Y);
	}

	bool FindVisibleType(
		const FFF_SurfingSnapshot& Snapshot,
		const EFF_SurfingObjectType Type,
		FFF_SurfingObjectSnapshot* OutObject = nullptr)
	{
		for (const FFF_SurfingObjectSnapshot& Object : Snapshot.VisibleObjects)
		{
			if (Object.Type == Type)
			{
				if (OutObject)
				{
					*OutObject = Object;
				}
				return true;
			}
		}
		return false;
	}

	UFF_SurfingDefinition* MakeVerticalContactDefinition(
		const EFF_SurfingObjectType ContactType,
		const int32 InitialLives = 3,
		const int32 InitialBoostCharges = 3)
	{
		UFF_SurfingDefinition* Definition = MakeSurfingDefinition();
		UDataTable* Table = MakeCourseTable(Definition);
		if (FFSurfingRules::IsObstacleType(ContactType))
		{
			AddCourseRow(
				*Table,
				TEXT("A_Contact"),
				ContactType,
				FVector2D(0.0, 0.5),
				0.02f);
			AddCourseRow(
				*Table,
				TEXT("Z_RequiredPickup"),
				EFF_SurfingObjectType::Coin,
				FVector2D(0.9, -0.9),
				0.02f);
		}
		else
		{
			AddCourseRow(
				*Table,
				TEXT("A_Contact"),
				ContactType,
				FVector2D(0.0, 0.5),
				0.02f);
			AddCourseRow(
				*Table,
				TEXT("Z_RequiredObstacle"),
				EFF_SurfingObjectType::Rock,
				FVector2D(0.9, -0.9),
				0.02f);
		}
		Definition->CourseLayoutTable = Table;
		Definition->Rules.CharacterSpeedNormalizedPerSecond = 60.0f;
		Definition->Rules.PlayerCollisionRadiusNormalized = 0.02f;
		Definition->Rules.InitialLives = InitialLives;
		Definition->Rules.InitialBoostCharges = InitialBoostCharges;
		return Definition;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFF_SurfingRulesTest,
	"FestaFiesta.Minigame.Surfing.RulesAndDefinition",
	SurfingTestFlags)

bool FFF_SurfingRulesTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	FFF_SurfingRules Rules;
	FString FailureReason;
	TestTrue(TEXT("Default endless-course rules validate"), Rules.IsValid(&FailureReason));
	TestTrue(TEXT("Successful validation clears its reason"), FailureReason.IsEmpty());
	TestTrue(TEXT("Both normalized movement axes validate"),
		FFSurfingRules::IsValidMovementInput(-1.0f, 1.0f));
	TestFalse(TEXT("An out-of-range movement component is rejected"),
		FFSurfingRules::IsValidMovementInput(0.0f, 1.01f));
	TestFalse(TEXT("A non-finite movement component is rejected"),
		FFSurfingRules::IsValidMovementInput(
			std::numeric_limits<float>::quiet_NaN(),
			0.0f));

	const float HorizontalMetricScale = Rules.ArenaHorizontalMetricScale;
	TestTrue(TEXT("Default arena metric matches the authored aspect"),
		FMath::IsNearlyEqual(HorizontalMetricScale, 2.18f));
	const FVector2D Diagonal = FFSurfingRules::NormalizeHeading(
		1.0f,
		1.0f,
		HorizontalMetricScale);
	TestTrue(TEXT("Diagonal heading is unit length in display metric"),
		FMath::IsNearlyEqual(
			GetArenaMetricLength(Diagonal, HorizontalMetricScale),
			1.0,
			1.0e-8));
	TestTrue(TEXT("Metric normalization preserves the requested coordinate direction"),
		FMath::IsNearlyEqual(Diagonal.X, Diagonal.Y, 1.0e-8));
	TestTrue(TEXT("Zero input does not manufacture a heading"),
		FFSurfingRules::NormalizeHeading(
			0.0f,
			0.0f,
			HorizontalMetricScale).IsZero());

	double TimeOfImpact = -1.0;
	TestTrue(TEXT("Swept test detects tunneling through a circle"),
		FFSurfingRules::GetSegmentCircleHitTime(
			FVector2D(-1.0, 0.0),
			FVector2D(1.0, 0.0),
			FVector2D::ZeroVector,
			0.1f,
			HorizontalMetricScale,
			TimeOfImpact));
	TestTrue(TEXT("Swept hit publishes its aspect-correct earliest time"),
		FMath::IsNearlyEqual(
			TimeOfImpact,
			0.5 - 0.1 / (2.0 * HorizontalMetricScale),
			1.0e-6));
	TestFalse(TEXT("Separated segment does not collide"),
		FFSurfingRules::GetSegmentCircleHitTime(
			FVector2D(-1.0, 0.5),
			FVector2D(1.0, 0.5),
			FVector2D::ZeroVector,
			0.1f,
			HorizontalMetricScale,
			TimeOfImpact));
	TestTrue(TEXT("Metric-scaled horizontal radius includes a visually inside point"),
		FFSurfingRules::GetSegmentCircleHitTime(
			FVector2D(0.05, 0.0),
			FVector2D(0.05, 0.0),
			FVector2D::ZeroVector,
			0.15f,
			HorizontalMetricScale,
			TimeOfImpact));
	TestFalse(TEXT("Metric-scaled horizontal radius excludes a visually outside point"),
		FFSurfingRules::GetSegmentCircleHitTime(
			FVector2D(0.1, 0.0),
			FVector2D(0.1, 0.0),
			FVector2D::ZeroVector,
			0.15f,
			HorizontalMetricScale,
			TimeOfImpact));

	FFF_SurfingRules InvalidRules = Rules;
	InvalidRules.CharacterSpeedNormalizedPerSecond = 0.0f;
	TestFalse(TEXT("Zero character speed is rejected"), InvalidRules.IsValid());
	InvalidRules = Rules;
	InvalidRules.PlayerCollisionRadiusNormalized = 1.1f;
	TestFalse(TEXT("Out-of-arena player radius is rejected"), InvalidRules.IsValid());
	InvalidRules = Rules;
	InvalidRules.ArenaHorizontalMetricScale = 0.0f;
	TestFalse(TEXT("Non-positive arena horizontal metric is rejected"), InvalidRules.IsValid());
	InvalidRules = Rules;
	InvalidRules.CharacterSpeedNormalizedPerSecond = 1500.0f;
	InvalidRules.BoostSpeedMultiplier = 2.0f;
	InvalidRules.ForwardInputSpeedInfluence = 0.5f;
	TestFalse(TEXT("Maximum forward trim is included in bounded tick travel"),
		InvalidRules.IsValid());

	TestTrue(TEXT("Score subtraction saturates at zero"),
		FMath::IsNearlyZero(FFSurfingRules::AddScoreSaturated(20.0f, -100.0f)));
	TestEqual(TEXT("Partial points are not rounded up"),
		FFSurfingRules::QuantizeScore(10.99f), 10);

	UFF_SurfingDefinition* Definition = MakeSurfingDefinition();
	TestEqual(TEXT("Default ID"), Definition->MinigameId, FName(TEXT("Surfing")));
	TestEqual(TEXT("Default duration"), Definition->DurationSeconds, 30.0f);
	TestEqual(TEXT("Endless-course full-reward target"), Definition->ScoreForMaxReward, 3500);
	TestTrue(TEXT("Default facility is supported"),
		Definition->SupportsFacility(TEXT("Booth_ActivityGame_Surfing")));
	TArray<FText> Errors;
	TestTrue(TEXT("Definition with a valid course table validates"),
		Definition->IsDefinitionValid(Errors));

	UFF_SurfingDefinition* MissingTable = MakeSurfingDefinition();
	MissingTable->CourseLayoutTable.Reset();
	Errors.Reset();
	TestFalse(TEXT("Missing course table is rejected"),
		MissingTable->IsDefinitionValid(Errors));

	UFF_SurfingDefinition* WrongStruct = MakeSurfingDefinition();
	UDataTable* WrongTable = NewObject<UDataTable>(WrongStruct);
	WrongTable->RowStruct = FTableRowBase::StaticStruct();
	WrongStruct->CourseLayoutTable = WrongTable;
	Errors.Reset();
	TestFalse(TEXT("Wrong course row struct is rejected"),
		WrongStruct->IsDefinitionValid(Errors));

	UFF_SurfingDefinition* InvalidPosition = MakeSurfingDefinition();
	UDataTable* InvalidPositionTable = MakeCourseTable(InvalidPosition);
	AddCourseRow(
		*InvalidPositionTable,
		TEXT("Rock"),
		EFF_SurfingObjectType::Rock,
		FVector2D(1.01, 0.0));
	AddCourseRow(
		*InvalidPositionTable,
		TEXT("Coin"),
		EFF_SurfingObjectType::Coin,
		FVector2D::ZeroVector);
	InvalidPosition->CourseLayoutTable = InvalidPositionTable;
	Errors.Reset();
	TestFalse(TEXT("Out-of-range course position is rejected"),
		InvalidPosition->IsDefinitionValid(Errors));

	UFF_SurfingDefinition* MissingPickup = MakeSurfingDefinition();
	UDataTable* ObstacleOnlyTable = MakeCourseTable(MissingPickup);
	AddCourseRow(
		*ObstacleOnlyTable,
		TEXT("Rock"),
		EFF_SurfingObjectType::Rock,
		FVector2D::ZeroVector);
	AddCourseRow(
		*ObstacleOnlyTable,
		TEXT("DisabledCoin"),
		EFF_SurfingObjectType::Coin,
		FVector2D(0.5, 0.5),
		0.05f,
		0.0f,
		false);
	MissingPickup->CourseLayoutTable = ObstacleOnlyTable;
	Errors.Reset();
	TestFalse(TEXT("An enabled pickup is required"),
		MissingPickup->IsDefinitionValid(Errors));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFF_SurfingDeterminismTest,
	"FestaFiesta.Minigame.Surfing.FixedTickDeterminism",
	SurfingTestFlags)

bool FFF_SurfingDeterminismTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UFF_SurfingDefinition* DefinitionA = MakeSurfingDefinition(false);
	UFF_SurfingDefinition* DefinitionB = MakeSurfingDefinition(true);
	const FFF_MinigameSessionContext Context = MakeSurfingContext(8675309);
	FString FailureA;
	FString FailureB;
	UFF_SurfingRuntime* RuntimeA = MakeSurfingRuntime(DefinitionA, Context, FailureA);
	UFF_SurfingRuntime* RuntimeB = MakeSurfingRuntime(DefinitionB, Context, FailureB);
	if (!TestNotNull(*FString::Printf(TEXT("First runtime initializes: %s"), *FailureA), RuntimeA)
		|| !TestNotNull(*FString::Printf(TEXT("Second runtime initializes: %s"), *FailureB), RuntimeB))
	{
		return false;
	}

	const FFF_SurfingSnapshot InitialA = RuntimeA->GetSurfingSnapshot();
	const FFF_SurfingSnapshot InitialB = RuntimeB->GetSurfingSnapshot();
	TestEqual(TEXT("Row insertion order does not change visible count"),
		InitialA.VisibleObjects.Num(), InitialB.VisibleObjects.Num());
	for (int32 Index = 0; Index < InitialA.VisibleObjects.Num(); ++Index)
	{
		TestEqual(TEXT("Lexically sorted rows keep deterministic IDs"),
			InitialA.VisibleObjects[Index].ObjectId,
			InitialB.VisibleObjects[Index].ObjectId);
		TestEqual(TEXT("Lexically sorted rows keep deterministic types"),
			InitialA.VisibleObjects[Index].Type,
			InitialB.VisibleObjects[Index].Type);
		TestTrue(TEXT("Lexically sorted rows keep deterministic positions"),
			IsNearlyEqualVector(
				InitialA.VisibleObjects[Index].PositionNormalized,
				InitialB.VisibleObjects[Index].PositionNormalized));
	}

	RuntimeA->SetMovementInput(Context.SessionId, 0.6f, 0.8f);
	RuntimeB->SetMovementInput(Context.SessionId, 0.6f, 0.8f);
	RuntimeA->SubmitBoost(Context.SessionId);
	RuntimeB->SubmitBoost(Context.SessionId);
	RuntimeA->Advance(10.0f);
	for (int32 Tick = 0; Tick < 600; ++Tick)
	{
		RuntimeB->Advance(FixedStepSeconds);
	}

	const FFF_SurfingSnapshot SnapshotA = RuntimeA->GetSurfingSnapshot();
	const FFF_SurfingSnapshot SnapshotB = RuntimeB->GetSurfingSnapshot();
	TestEqual(TEXT("Frame partition preserves the arena metric"),
		SnapshotA.ArenaHorizontalMetricScale,
		SnapshotB.ArenaHorizontalMetricScale);
	TestEqual(TEXT("Frame partition preserves exact integer score"),
		SnapshotA.Session.Score, SnapshotB.Session.Score);
	TestEqual(TEXT("Frame partition preserves lives"), SnapshotA.Lives, SnapshotB.Lives);
	TestEqual(TEXT("Frame partition preserves collisions"),
		SnapshotA.CollisionCount, SnapshotB.CollisionCount);
	TestEqual(TEXT("Frame partition preserves pickups"),
		SnapshotA.CoinCount, SnapshotB.CoinCount);
	TestTrue(TEXT("Fixed ticks keep path distance strictly close"),
		FMath::IsNearlyEqual(SnapshotA.DistanceMeters, SnapshotB.DistanceMeters, 1.0e-5f));
	TestTrue(TEXT("Fixed ticks keep player position strictly close"),
		IsNearlyEqualVector(
			SnapshotA.PlayerPositionNormalized,
			SnapshotB.PlayerPositionNormalized,
			1.0e-6));
	TestTrue(TEXT("Fixed ticks keep steering heading strictly close"),
		IsNearlyEqualVector(SnapshotA.CurrentHeading, SnapshotB.CurrentHeading, 1.0e-6));
	TestTrue(TEXT("Fixed ticks keep endless course progress strictly close"),
		FMath::IsNearlyEqual(
			SnapshotA.CourseProgressNormalized,
			SnapshotB.CourseProgressNormalized,
			1.0e-6));
	TestEqual(TEXT("Frame partition preserves latest event serial"),
		SnapshotA.LatestEventSerial, SnapshotB.LatestEventSerial);
	TestEqual(TEXT("Frame partition preserves visible object count"),
		SnapshotA.VisibleObjects.Num(), SnapshotB.VisibleObjects.Num());

	UFF_SurfingDefinition* DifferentSeedDefinition = MakeSurfingDefinition();
	FString DifferentSeedFailure;
	UFF_SurfingRuntime* DifferentSeedRuntime = MakeSurfingRuntime(
		DifferentSeedDefinition,
		MakeSurfingContext(-12345),
		DifferentSeedFailure);
	if (!TestNotNull(TEXT("Different-seed runtime initializes"), DifferentSeedRuntime))
	{
		return false;
	}
	const FFF_SurfingSnapshot DifferentSeedInitial = DifferentSeedRuntime->GetSurfingSnapshot();
	TestEqual(TEXT("Random seed cannot alter fixed layout count"),
		InitialA.VisibleObjects.Num(), DifferentSeedInitial.VisibleObjects.Num());
	for (int32 Index = 0; Index < InitialA.VisibleObjects.Num(); ++Index)
	{
		TestTrue(TEXT("Random seed cannot alter fixed layout position"),
			IsNearlyEqualVector(
				InitialA.VisibleObjects[Index].PositionNormalized,
				DifferentSeedInitial.VisibleObjects[Index].PositionNormalized));
	}

	UFF_SurfingDefinition* CardinalDefinition = MakeSurfingDefinition();
	UFF_SurfingDefinition* DiagonalDefinition = MakeSurfingDefinition();
	auto IsolatePathScore = [](UFF_SurfingDefinition& MetricDefinition)
	{
		MetricDefinition.Rules.CoinScore = 0.0f;
		MetricDefinition.Rules.ObstacleDestroyScore = 0.0f;
		MetricDefinition.Rules.CollisionPenalty = 0.0f;
	};
	IsolatePathScore(*CardinalDefinition);
	IsolatePathScore(*DiagonalDefinition);
	const FFF_MinigameSessionContext CardinalContext = MakeSurfingContext(9001);
	const FFF_MinigameSessionContext DiagonalContext = MakeSurfingContext(9002);
	FString CardinalFailure;
	FString DiagonalFailure;
	UFF_SurfingRuntime* CardinalRuntime = MakeSurfingRuntime(
		CardinalDefinition,
		CardinalContext,
		CardinalFailure);
	UFF_SurfingRuntime* DiagonalRuntime = MakeSurfingRuntime(
		DiagonalDefinition,
		DiagonalContext,
		DiagonalFailure);
	if (!TestNotNull(TEXT("Cardinal metric runtime initializes"), CardinalRuntime)
		|| !TestNotNull(TEXT("Diagonal metric runtime initializes"), DiagonalRuntime))
	{
		return false;
	}
	CardinalRuntime->SetMovementInput(CardinalContext.SessionId, 0.0f, 0.0f);
	DiagonalRuntime->SetMovementInput(DiagonalContext.SessionId, 1.0f, 0.0f);
	CardinalRuntime->SubmitBoost(CardinalContext.SessionId);
	DiagonalRuntime->SubmitBoost(DiagonalContext.SessionId);
	CardinalRuntime->Advance(30.0f);
	DiagonalRuntime->Advance(30.0f);
	TestTrue(TEXT("Straight and steered paths accumulate equal display-metric distance"),
		FMath::IsNearlyEqual(
			CardinalRuntime->GetSurfingSnapshot().DistanceMeters,
			DiagonalRuntime->GetSurfingSnapshot().DistanceMeters,
			1.0e-5f));
	TestEqual(TEXT("Straight and steered paths receive equal isolated path score"),
		CardinalRuntime->GetSurfingSnapshot().Session.Score,
		DiagonalRuntime->GetSurfingSnapshot().Session.Score);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFF_SurfingInputTest,
	"FestaFiesta.Minigame.Surfing.ContinuousInputAndEndlessCourse",
	SurfingTestFlags)

bool FFF_SurfingInputTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UFF_SurfingDefinition* Definition = MakeSurfingDefinition();
	Definition->Rules.CharacterSpeedNormalizedPerSecond = 60.0f;
	Definition->Rules.PlayerCollisionRadiusNormalized = 0.01f;
	const FFF_MinigameSessionContext Context = MakeSurfingContext();
	FString FailureReason;
	UFF_SurfingRuntime* Runtime = MakeSurfingRuntime(Definition, Context, FailureReason);
	if (!TestNotNull(TEXT("Runtime initializes"), Runtime))
	{
		return false;
	}

	TestTrue(TEXT("Default heading always advances the course"),
		IsNearlyEqualVector(Runtime->GetSurfingSnapshot().CurrentHeading, FVector2D(0.0, 1.0)));
	TestEqual(TEXT("Wrong session is rejected"),
		Runtime->SetMovementInput(FGuid(9, 9, 9, 9), 0.5f, 0.5f),
		EFF_SurfingSteerResult::RejectedWrongSession);
	TestEqual(TEXT("Non-finite forward axis is rejected"),
		Runtime->SetMovementInput(
			Context.SessionId,
			0.0f,
			std::numeric_limits<float>::infinity()),
		EFF_SurfingSteerResult::RejectedInvalidAxis);
	TestEqual(TEXT("Out-of-range lateral axis is rejected"),
		Runtime->SetMovementInput(Context.SessionId, -1.1f, 0.0f),
		EFF_SurfingSteerResult::RejectedInvalidAxis);
	TestEqual(TEXT("Compatibility wrapper rejects non-direction"),
		Runtime->SubmitSteer(Context.SessionId, 0),
		EFF_SurfingSteerResult::RejectedInvalidDirection);

	TestEqual(TEXT("Diagonal input is accepted"),
		Runtime->SetMovementInput(Context.SessionId, 0.3f, 0.4f),
		EFF_SurfingSteerResult::Applied);
	TestTrue(TEXT("Input waits for the next fixed tick"),
		Runtime->GetSurfingSnapshot().MovementInput.IsZero());
	Runtime->Advance(FixedStepSeconds);
	const FFF_SurfingSnapshot AfterDiagonal = Runtime->GetSurfingSnapshot();
	const FVector2D ExpectedDiagonal = FFSurfingRules::NormalizeHeading(
		0.3f * Definition->Rules.SteeringStrength,
		1.0f,
		Definition->Rules.ArenaHorizontalMetricScale);
	const double ExpectedTravel = Definition->Rules.CharacterSpeedNormalizedPerSecond
		* (1.0 + 0.4 * Definition->Rules.ForwardInputSpeedInfluence)
		* FixedStepSeconds;
	TestTrue(TEXT("Snapshot exposes the authoritative arena metric"),
		FMath::IsNearlyEqual(
			AfterDiagonal.ArenaHorizontalMetricScale,
			Definition->Rules.ArenaHorizontalMetricScale));
	TestTrue(TEXT("A/D steering keeps a positive auto-forward heading"),
		IsNearlyEqualVector(AfterDiagonal.CurrentHeading, ExpectedDiagonal, 1.0e-5));
	TestTrue(TEXT("Presentation stays camera-relative while logical progress advances"),
		FMath::IsNearlyZero(AfterDiagonal.PlayerPositionNormalized.Y)
		&& FMath::IsNearlyEqual(
			AfterDiagonal.PlayerPositionNormalized.X,
			ExpectedDiagonal.X * ExpectedTravel,
			1.0e-5)
		&& FMath::IsNearlyEqual(
			AfterDiagonal.CourseProgressNormalized,
			ExpectedDiagonal.Y * ExpectedTravel,
			1.0e-5));

	Runtime->SetSteerInput(Context.SessionId, -0.5f);
	Runtime->Advance(FixedStepSeconds);
	TestTrue(TEXT("Horizontal wrapper preserves the forward input component"),
		FMath::IsNearlyEqual(Runtime->GetSurfingSnapshot().MovementInput.Y, 0.4, 1.0e-6));

	Definition->Rules.CharacterSpeedNormalizedPerSecond = 1.0f;
	Runtime->SetMovementInput(Context.SessionId, 0.0f, 0.0f);
	const double ProgressBeforeZero = Runtime->GetSurfingSnapshot().CourseProgressNormalized;
	const float DistanceBeforeZero = Runtime->GetSurfingSnapshot().DistanceMeters;
	Runtime->Advance(FixedStepSeconds);
	TestTrue(TEXT("Zero input recentres steering and never stops the course"),
		IsNearlyEqualVector(
			Runtime->GetSurfingSnapshot().CurrentHeading,
			FVector2D(0.0, 1.0))
		&& Runtime->GetSurfingSnapshot().DistanceMeters > DistanceBeforeZero
		&& Runtime->GetSurfingSnapshot().CourseProgressNormalized > ProgressBeforeZero);

	UFF_SurfingDefinition* BounceDefinition = MakeSurfingDefinition();
	BounceDefinition->Rules.CharacterSpeedNormalizedPerSecond = 500.0f;
	FString BounceFailure;
	UFF_SurfingRuntime* BounceRuntime = MakeSurfingRuntime(
		BounceDefinition,
		MakeSurfingContext(3001),
		BounceFailure);
	if (!TestNotNull(TEXT("Bounce runtime initializes"), BounceRuntime))
	{
		return false;
	}
	BounceRuntime->SetMovementInput(MakeSurfingContext(3001).SessionId, 1.0f, 0.0f);
	BounceRuntime->Advance(FixedStepSeconds);
	const FFF_SurfingSnapshot FirstBoundaryContact = BounceRuntime->GetSurfingSnapshot();
	TestTrue(TEXT("Held steering slides along the lateral boundary without reversing"),
		FMath::IsNearlyEqual(FirstBoundaryContact.PlayerPositionNormalized.X, 1.0)
		&& FMath::IsNearlyZero(FirstBoundaryContact.PlayerPositionNormalized.Y)
		&& FirstBoundaryContact.CourseProgressNormalized > 1.0
		&& FirstBoundaryContact.CurrentHeading.X > 0.0
		&& FirstBoundaryContact.CurrentHeading.Y > 0.0);
	BounceRuntime->Advance(FixedStepSeconds);
	const FFF_SurfingSnapshot HeldAtBoundary = BounceRuntime->GetSurfingSnapshot();
	TestTrue(TEXT("Unchanged held input remains at the requested boundary and keeps advancing"),
		FMath::IsNearlyEqual(HeldAtBoundary.PlayerPositionNormalized.X, 1.0)
		&& HeldAtBoundary.CurrentHeading.X > 0.0
		&& HeldAtBoundary.CourseProgressNormalized
			> FirstBoundaryContact.CourseProgressNormalized);

	UFF_SurfingDefinition* EndlessDefinition = MakeVerticalContactDefinition(
		EFF_SurfingObjectType::Coin);
	EndlessDefinition->Rules.CharacterSpeedNormalizedPerSecond = 60.0f;
	const FFF_MinigameSessionContext EndlessContext = MakeSurfingContext(3002);
	FString EndlessFailure;
	UFF_SurfingRuntime* EndlessRuntime = MakeSurfingRuntime(
		EndlessDefinition,
		EndlessContext,
		EndlessFailure);
	if (!TestNotNull(TEXT("Endless segment runtime initializes"), EndlessRuntime))
	{
		return false;
	}
	const int32 InitialPoolObjects = EndlessRuntime->GetActiveCourseObjectCount();
	const int32 InitialPoolSegments = EndlessRuntime->GetActiveCourseSegmentCount();
	for (int32 Tick = 0; Tick < 130; ++Tick)
	{
		EndlessRuntime->Advance(FixedStepSeconds);
	}
	const FFF_SurfingSnapshot Endless = EndlessRuntime->GetSurfingSnapshot();
	TestTrue(TEXT("Course advances beyond fifty original arena lengths"),
		Endless.CourseProgressNormalized > 100.0);
	TestEqual(TEXT("Segment recycling keeps the active segment count constant"),
		EndlessRuntime->GetActiveCourseSegmentCount(), InitialPoolSegments);
	TestEqual(TEXT("Segment recycling keeps the active object pool bounded"),
		EndlessRuntime->GetActiveCourseObjectCount(), InitialPoolObjects);
	TestTrue(TEXT("Repeated logical segments continue to award new pickups"),
		Endless.CoinCount > 40);
	TestTrue(TEXT("Resolved pickup history is pruned behind the active window"),
		EndlessRuntime->GetResolvedCourseObjectCount() <= InitialPoolSegments);

	Runtime->Cancel(EFF_MinigameEndReason::PlayerCancelled);
	TestEqual(TEXT("Ended runtime rejects movement"),
		Runtime->SetMovementInput(Context.SessionId, -1.0f, 1.0f),
		EFF_SurfingSteerResult::RejectedNotPlaying);
	TestEqual(TEXT("Ended runtime rejects boost"),
		Runtime->SubmitBoost(Context.SessionId),
		EFF_SurfingBoostResult::RejectedNotPlaying);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFF_SurfingArenaLayoutSweptTest,
	"FestaFiesta.Minigame.Surfing.ArenaLayoutAndSweptCollision",
	SurfingTestFlags)

bool FFF_SurfingArenaLayoutSweptTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UFF_SurfingDefinition* Definition = MakeVerticalContactDefinition(
		EFF_SurfingObjectType::Rock);
	const FFF_MinigameSessionContext Context = MakeSurfingContext(4001);
	FString FailureReason;
	UFF_SurfingRuntime* Runtime = MakeSurfingRuntime(Definition, Context, FailureReason);
	if (!TestNotNull(TEXT("Swept runtime initializes"), Runtime))
	{
		return false;
	}

	const FFF_SurfingSnapshot Initial = Runtime->GetSurfingSnapshot();
	TestEqual(TEXT("All enabled fixed objects are initially visible"),
		Initial.VisibleObjects.Num(), 2);
	TestEqual(TEXT("Lexical row ordering assigns the first object ID"),
		Initial.VisibleObjects[0].ObjectId, 1);
	TestEqual(TEXT("Primary position projects exact authored coordinates"),
		Initial.VisibleObjects[0].PositionNormalized,
		FVector2D(0.0, 0.5));
	TestTrue(TEXT("Legacy object axes mirror primary position"),
		FMath::IsNearlyEqual(Initial.VisibleObjects[0].LateralPositionNormalized, 0.0f)
		&& FMath::IsNearlyEqual(Initial.VisibleObjects[0].ForwardPositionNormalized, 0.5f));

	// A full arena unit in one tick starts and ends outside the small rock circle,
	// so this hit proves that collision uses the swept segment rather than endpoints.
	Runtime->Advance(FixedStepSeconds);
	const FFF_SurfingSnapshot AfterSweep = Runtime->GetSurfingSnapshot();
	TestEqual(TEXT("Swept movement catches a tunneled obstacle"),
		AfterSweep.CollisionCount, 1);
	TestEqual(TEXT("Swept obstacle costs one life"), AfterSweep.Lives, 2);
	TestTrue(TEXT("Fixed obstacle remains visible after collision"),
		FindVisibleType(AfterSweep, EFF_SurfingObjectType::Rock));

	UFF_SurfingDefinition* MetricMissDefinition = MakeSurfingDefinition();
	UDataTable* MetricMissTable = MakeCourseTable(MetricMissDefinition);
	AddCourseRow(
		*MetricMissTable,
		TEXT("A_MetricSeparatedRock"),
		EFF_SurfingObjectType::Rock,
		FVector2D(0.03, 0.5),
		0.02f);
	AddCourseRow(
		*MetricMissTable,
		TEXT("Z_RequiredCoin"),
		EFF_SurfingObjectType::Coin,
		FVector2D(0.9, -0.9),
		0.02f);
	MetricMissDefinition->CourseLayoutTable = MetricMissTable;
	MetricMissDefinition->Rules.CharacterSpeedNormalizedPerSecond = 60.0f;
	MetricMissDefinition->Rules.PlayerCollisionRadiusNormalized = 0.02f;
	const FFF_MinigameSessionContext MetricMissContext = MakeSurfingContext(4002);
	FString MetricMissFailure;
	UFF_SurfingRuntime* MetricMissRuntime = MakeSurfingRuntime(
		MetricMissDefinition,
		MetricMissContext,
		MetricMissFailure);
	if (!TestNotNull(TEXT("Metric-separated runtime initializes"), MetricMissRuntime))
	{
		return false;
	}
	MetricMissRuntime->Advance(FixedStepSeconds);
	TestEqual(TEXT("Horizontal aspect metric prevents a visually separated collision"),
		MetricMissRuntime->GetSurfingSnapshot().CollisionCount,
		0);

	UFF_SurfingDefinition* MetricOverlapDefinition = MakeSurfingDefinition();
	UDataTable* MetricOverlapTable = MakeCourseTable(MetricOverlapDefinition);
	AddCourseRow(
		*MetricOverlapTable,
		TEXT("A_CenterRock"),
		EFF_SurfingObjectType::Rock,
		FVector2D::ZeroVector,
		0.02f);
	AddCourseRow(
		*MetricOverlapTable,
		TEXT("Z_RequiredCoin"),
		EFF_SurfingObjectType::Coin,
		FVector2D(0.9, -0.9),
		0.02f);
	MetricOverlapDefinition->CourseLayoutTable = MetricOverlapTable;
	MetricOverlapDefinition->Rules.PlayerCollisionRadiusNormalized = 0.02f;
	MetricOverlapDefinition->Rules.CharacterSpeedNormalizedPerSecond =
		0.03f * MetricOverlapDefinition->Rules.ArenaHorizontalMetricScale * 60.0f;
	MetricOverlapDefinition->Rules.HitInvincibilitySeconds = 0.5f * FixedStepSeconds;
	const FFF_MinigameSessionContext MetricOverlapContext = MakeSurfingContext(4003);
	FString MetricOverlapFailure;
	UFF_SurfingRuntime* MetricOverlapRuntime = MakeSurfingRuntime(
		MetricOverlapDefinition,
		MetricOverlapContext,
		MetricOverlapFailure);
	if (!TestNotNull(TEXT("Metric-overlap runtime initializes"), MetricOverlapRuntime))
	{
		return false;
	}
	MetricOverlapRuntime->SetMovementInput(MetricOverlapContext.SessionId, 1.0f, 0.0f);
	MetricOverlapRuntime->Advance(FixedStepSeconds);
	TestEqual(TEXT("Initial center departure records one contact"),
		MetricOverlapRuntime->GetSurfingSnapshot().CollisionCount,
		1);
	MetricOverlapRuntime->SetMovementInput(MetricOverlapContext.SessionId, -1.0f, 0.0f);
	MetricOverlapRuntime->Advance(FixedStepSeconds);
	TestEqual(TEXT("Auto-forward travel prevents a duplicate hit on the passed obstacle"),
		MetricOverlapRuntime->GetSurfingSnapshot().CollisionCount,
		1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFF_SurfingCollisionPickupBoostTest,
	"FestaFiesta.Minigame.Surfing.CollisionPickupsAndBoost",
	SurfingTestFlags)

bool FFF_SurfingCollisionPickupBoostTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UFF_SurfingDefinition* CoinDefinition = MakeVerticalContactDefinition(
		EFF_SurfingObjectType::Coin);
	const FFF_MinigameSessionContext CoinContext = MakeSurfingContext(5001);
	FString CoinFailure;
	UFF_SurfingRuntime* CoinRuntime = MakeSurfingRuntime(
		CoinDefinition,
		CoinContext,
		CoinFailure);
	if (!TestNotNull(TEXT("Coin runtime initializes"), CoinRuntime))
	{
		return false;
	}
	CoinRuntime->Advance(FixedStepSeconds);
	const FFF_SurfingSnapshot AfterCoin = CoinRuntime->GetSurfingSnapshot();
	TestEqual(TEXT("Swept coin is collected once"), AfterCoin.CoinCount, 1);
	TestFalse(TEXT("Collected coin is hidden"),
		FindVisibleType(AfterCoin, EFF_SurfingObjectType::Coin));
	CoinRuntime->Advance(FixedStepSeconds);
	TestEqual(TEXT("A pickup cannot be collected twice inside one logical segment"),
		CoinRuntime->GetSurfingSnapshot().CoinCount, 1);
	CoinRuntime->Advance(FixedStepSeconds);
	TestEqual(TEXT("The recycled slot becomes collectible in the next logical segment"),
		CoinRuntime->GetSurfingSnapshot().CoinCount, 2);

	UFF_SurfingDefinition* HeartDefinition = MakeVerticalContactDefinition(
		EFF_SurfingObjectType::Heart,
		0);
	const FFF_MinigameSessionContext HeartContext = MakeSurfingContext(5002);
	FString HeartFailure;
	UFF_SurfingRuntime* HeartRuntime = MakeSurfingRuntime(
		HeartDefinition,
		HeartContext,
		HeartFailure);
	if (!TestNotNull(TEXT("Heart runtime initializes"), HeartRuntime))
	{
		return false;
	}
	HeartRuntime->Advance(FixedStepSeconds);
	TestEqual(TEXT("Heart restores one life from zero"),
		HeartRuntime->GetSurfingSnapshot().Lives, 1);
	TestEqual(TEXT("Heart pickup is counted once"),
		HeartRuntime->GetSurfingSnapshot().HeartCount, 1);

	UFF_SurfingDefinition* PickupDefinition = MakeVerticalContactDefinition(
		EFF_SurfingObjectType::BoostPickup,
		3,
		0);
	const FFF_MinigameSessionContext PickupContext = MakeSurfingContext(5003);
	FString PickupFailure;
	UFF_SurfingRuntime* PickupRuntime = MakeSurfingRuntime(
		PickupDefinition,
		PickupContext,
		PickupFailure);
	if (!TestNotNull(TEXT("Boost-pickup runtime initializes"), PickupRuntime))
	{
		return false;
	}
	PickupRuntime->Advance(FixedStepSeconds);
	TestEqual(TEXT("Boost pickup adds one charge"),
		PickupRuntime->GetSurfingSnapshot().BoostCharges, 1);
	TestEqual(TEXT("Boost pickup is counted once"),
		PickupRuntime->GetSurfingSnapshot().BoostPickupCount, 1);

	UFF_SurfingDefinition* BoostDefinition = MakeVerticalContactDefinition(
		EFF_SurfingObjectType::Rock,
		3,
		1);
	const FFF_MinigameSessionContext BoostContext = MakeSurfingContext(5004);
	FString BoostFailure;
	UFF_SurfingRuntime* BoostRuntime = MakeSurfingRuntime(
		BoostDefinition,
		BoostContext,
		BoostFailure);
	if (!TestNotNull(TEXT("Boost runtime initializes"), BoostRuntime))
	{
		return false;
	}
	TestEqual(TEXT("Boost command queues activation"),
		BoostRuntime->SubmitBoost(BoostContext.SessionId),
		EFF_SurfingBoostResult::Activated);
	TestEqual(TEXT("Duplicate queued boost is rejected"),
		BoostRuntime->SubmitBoost(BoostContext.SessionId),
		EFF_SurfingBoostResult::RejectedAlreadyBoosting);
	BoostRuntime->Advance(FixedStepSeconds);
	const FFF_SurfingSnapshot BoostedContact = BoostRuntime->GetSurfingSnapshot();
	TestTrue(TEXT("Boost activates on the next fixed tick"), BoostedContact.bBoosting);
	TestEqual(TEXT("Activation consumes the charge"), BoostedContact.BoostCharges, 0);
	TestEqual(TEXT("Boosted obstacle contact is counted as smashed"),
		BoostedContact.SmashedCount, 1);
	TestEqual(TEXT("Boosted contact cannot damage lives"), BoostedContact.Lives, 3);
	TestTrue(TEXT("Boosted contact does not remove the obstacle"),
		FindVisibleType(BoostedContact, EFF_SurfingObjectType::Rock));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFF_SurfingHitInvincibilityTest,
	"FestaFiesta.Minigame.Surfing.HitInvincibilityAndZeroLives",
	SurfingTestFlags)

bool FFF_SurfingHitInvincibilityTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UFF_SurfingDefinition* Definition = MakeVerticalContactDefinition(
		EFF_SurfingObjectType::Rock,
		1);
	Definition->Rules.HitInvincibilitySeconds = 2.0f * FixedStepSeconds;
	const FFF_MinigameSessionContext Context = MakeSurfingContext(6001);
	FString FailureReason;
	UFF_SurfingRuntime* Runtime = MakeSurfingRuntime(Definition, Context, FailureReason);
	if (!TestNotNull(TEXT("Collision runtime initializes"), Runtime))
	{
		return false;
	}

	Runtime->Advance(FixedStepSeconds);
	const FFF_SurfingSnapshot FirstHit = Runtime->GetSurfingSnapshot();
	TestEqual(TEXT("First unprotected obstacle costs one life"), FirstHit.Lives, 0);
	TestEqual(TEXT("First collision is counted"), FirstHit.CollisionCount, 1);
	TestTrue(TEXT("Hit starts invincibility"), FirstHit.bInvincible);
	TestEqual(TEXT("Zero lives does not end the challenge"),
		FirstHit.Session.State, EFF_MinigameSessionState::Playing);

	Runtime->Advance(FixedStepSeconds);
	TestEqual(TEXT("Return crossing during invincibility cannot deal duplicate damage"),
		Runtime->GetSurfingSnapshot().CollisionCount, 1);
	TestEqual(TEXT("Lives never fall below zero"),
		Runtime->GetSurfingSnapshot().Lives, 0);

	// Three more unit paths reach the next recycled rock after invincibility expires.
	Runtime->Advance(3.0f * FixedStepSeconds);
	TestEqual(TEXT("A new logical segment obstacle can damage after invincibility"),
		Runtime->GetSurfingSnapshot().CollisionCount, 2);
	TestTrue(TEXT("Persistent obstacle is still rendered"),
		FindVisibleType(Runtime->GetSurfingSnapshot(), EFF_SurfingObjectType::Rock));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFF_SurfingEndRewardTest,
	"FestaFiesta.Minigame.Surfing.EndAndRewardOnce",
	SurfingTestFlags)

bool FFF_SurfingEndRewardTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UFF_SurfingDefinition* Definition = MakeSurfingDefinition();
	Definition->ScoreForMaxReward = 1;
	const FFF_MinigameSessionContext Context = MakeSurfingContext(7001);
	FString FailureReason;
	UFF_SurfingRuntime* Runtime = MakeSurfingRuntime(Definition, Context, FailureReason);
	if (!TestNotNull(TEXT("Runtime initializes"), Runtime))
	{
		return false;
	}

	int32 EndBroadcastCount = 0;
	Runtime->OnEnded.AddLambda([&EndBroadcastCount](UFF_MinigameRuntime*)
	{
		++EndBroadcastCount;
	});

	Runtime->Advance(30.0f);
	TestEqual(TEXT("Session completes at thirty seconds"),
		Runtime->GetSnapshot().State, EFF_MinigameSessionState::Completed);
	TestTrue(TEXT("Continuous challenge travels a positive path distance"),
		Runtime->GetSurfingSnapshot().DistanceMeters > 0.0f);
	TestEqual(TEXT("Completion broadcasts once"), EndBroadcastCount, 1);
	TestEqual(TEXT("Completed score reaches the configured reward cap"),
		Runtime->BuildResult(150).RewardGold, 150);
	const int32 FinalScore = Runtime->GetSnapshot().Score;
	Runtime->Advance(100.0f);
	TestEqual(TEXT("Repeated advance cannot complete twice"), EndBroadcastCount, 1);
	TestEqual(TEXT("Repeated advance cannot change final score"),
		Runtime->GetSnapshot().Score, FinalScore);

	UFF_SurfingDefinition* CancelDefinition = MakeSurfingDefinition();
	FString CancelFailure;
	UFF_SurfingRuntime* CancelRuntime = MakeSurfingRuntime(
		CancelDefinition,
		MakeSurfingContext(7002),
		CancelFailure);
	if (!TestNotNull(TEXT("Cancellation runtime initializes"), CancelRuntime))
	{
		return false;
	}
	CancelRuntime->Advance(1.0f);
	CancelRuntime->Cancel(EFF_MinigameEndReason::PlayerCancelled);
	TestEqual(TEXT("Cancelled challenge never grants reward"),
		CancelRuntime->BuildResult(150).RewardGold, 0);

	float MinimumDistance = TNumericLimits<float>::Max();
	float MaximumDistance = 0.0f;
	int32 MinimumScore = MAX_int32;
	int32 MaximumScore = 0;
	for (int32 SampleIndex = 0; SampleIndex < 8; ++SampleIndex)
	{
		UFF_SurfingDefinition* SampleDefinition = MakeSurfingDefinition();
		const FFF_MinigameSessionContext SampleContext = MakeSurfingContext(8000 + SampleIndex);
		FString SampleFailure;
		UFF_SurfingRuntime* SampleRuntime = MakeSurfingRuntime(
			SampleDefinition,
			SampleContext,
			SampleFailure);
		if (!SampleRuntime)
		{
			AddError(FString::Printf(
				TEXT("Default arena sample failed to initialize: %s"),
				*SampleFailure));
			return false;
		}

		const float Angle = 2.0f * PI * static_cast<float>(SampleIndex) / 8.0f;
		SampleRuntime->SetMovementInput(
			SampleContext.SessionId,
			FMath::Cos(Angle),
			FMath::Sin(Angle));
		while (SampleRuntime->IsPlaying())
		{
			const FFF_SurfingSnapshot SampleBeforeTick = SampleRuntime->GetSurfingSnapshot();
			if (!SampleBeforeTick.bBoosting && SampleBeforeTick.BoostCharges > 0)
			{
				SampleRuntime->SubmitBoost(SampleContext.SessionId);
			}
			SampleRuntime->Advance(FixedStepSeconds);
		}

		const FFF_SurfingSnapshot Sample = SampleRuntime->GetSurfingSnapshot();
		MinimumDistance = FMath::Min(MinimumDistance, Sample.DistanceMeters);
		MaximumDistance = FMath::Max(MaximumDistance, Sample.DistanceMeters);
		MinimumScore = FMath::Min(MinimumScore, Sample.Session.Score);
		MaximumScore = FMath::Max(MaximumScore, Sample.Session.Score);
	}
	AddInfo(FString::Printf(
		TEXT("Default 30s endless-course simulation (8 inputs, all initial boosts): distance %.2f..%.2f, score %d..%d"),
		MinimumDistance,
		MaximumDistance,
		MinimumScore,
		MaximumScore));
	TestTrue(TEXT("All throttle samples keep uninterrupted bounded forward travel"),
		MinimumDistance > 0.0f
		&& MaximumDistance >= MinimumDistance
		&& MaximumDistance / MinimumDistance < 1.8f);
	return true;
}

#endif
