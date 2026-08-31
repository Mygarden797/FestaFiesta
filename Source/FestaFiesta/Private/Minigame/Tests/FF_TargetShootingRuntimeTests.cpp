// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Minigame/FF_MinigameDefinition.h"
#include "Minigame/FF_TargetShootingRuntime.h"
#include "Minigame/TargetShooting/FF_TargetShootingRules.h"
#include "UObject/Package.h"

namespace
{
	constexpr EAutomationTestFlags TargetShootingTestFlags =
		EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

	FFF_MinigameSessionContext MakeContext(const int32 Seed = 20260824)
	{
		FFF_MinigameSessionContext Context;
		Context.SessionId = FGuid(1, 2, 3, 4);
		Context.FacilityInstanceId = FGuid(5, 6, 7, 8);
		Context.FacilityDefinitionId = TEXT("Booth_ActivityGame_Shoot");
		Context.MinigameId = TEXT("TargetShooting");
		Context.RandomSeed = Seed;
		return Context;
	}

	UFF_TargetShootingDefinition* MakeDefinition()
	{
		return NewObject<UFF_TargetShootingDefinition>(GetTransientPackage());
	}

	UFF_TargetShootingRuntime* MakeRuntime(
		UFF_TargetShootingDefinition* Definition,
		const FFF_MinigameSessionContext& Context,
		FString& OutFailureReason)
	{
		UFF_TargetShootingRuntime* Runtime =
			NewObject<UFF_TargetShootingRuntime>(GetTransientPackage());
		return Runtime->Initialize(Definition, Context, &OutFailureReason)
			? Runtime
			: nullptr;
	}

	FVector2D MakeGuaranteedMissPoint(const FFF_TargetShootingSnapshot& Snapshot)
	{
		return FVector2D(
			Snapshot.TargetCenterNormalized.X < 0.5f ? 1.0f : 0.0f,
			Snapshot.TargetCenterNormalized.Y);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFF_TargetShootingScoringTest,
	"FestaFiesta.Minigame.TargetShooting.Scoring",
	TargetShootingTestFlags)

bool FFF_TargetShootingScoringTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	FFF_TargetShootingRules Rules;
	TestTrue(TEXT("Default rules validate"), Rules.IsValid());
	TestEqual(TEXT("First hit has base score"),
		FFTargetShootingRules::CalculateHitScore(Rules, 0), 100);
	TestEqual(TEXT("Combo bonus is capped"),
		FFTargetShootingRules::CalculateHitScore(Rules, 100), 150);
	TestTrue(TEXT("Ellipse boundary is a hit"),
		FFTargetShootingRules::IsInsideTarget(
			FVector2D(0.6f, 0.5f),
			FVector2D(0.5f, 0.5f),
			FVector2D(0.1f, 0.2f)));
	TestFalse(TEXT("Point outside ellipse is a miss"),
		FFTargetShootingRules::IsInsideTarget(
			FVector2D(0.61f, 0.5f),
			FVector2D(0.5f, 0.5f),
			FVector2D(0.1f, 0.2f)));
	TestEqual(TEXT("Center is a bullseye"),
		FFTargetShootingRules::GetHitQuality(
			Rules,
			FVector2D(0.5f, 0.5f),
			FVector2D(0.5f, 0.5f),
			FVector2D(0.1f, 0.2f)),
		EFF_TargetShootingFeedbackType::Bullseye);
	TestEqual(TEXT("Middle ring is precision"),
		FFTargetShootingRules::GetHitQuality(
			Rules,
			FVector2D(0.54f, 0.5f),
			FVector2D(0.5f, 0.5f),
			FVector2D(0.1f, 0.2f)),
		EFF_TargetShootingFeedbackType::Precision);
	TestEqual(TEXT("Outer ring is a normal hit"),
		FFTargetShootingRules::GetHitQuality(
			Rules,
			FVector2D(0.58f, 0.5f),
			FVector2D(0.5f, 0.5f),
			FVector2D(0.1f, 0.2f)),
		EFF_TargetShootingFeedbackType::Hit);
	TestEqual(TEXT("Sub-200ms reaction earns the full bonus"),
		FFTargetShootingRules::CalculateReactionScoreBonus(Rules, 0.19f), 50);
	TestEqual(TEXT("Mid-window reaction bonus interpolates"),
		FFTargetShootingRules::CalculateReactionScoreBonus(Rules, 0.50f), 25);
	TestEqual(TEXT("Slow reaction earns no speed bonus"),
		FFTargetShootingRules::CalculateReactionScoreBonus(Rules, 0.80f), 0);
	TestEqual(TEXT("Fast bullseye combines base precision and reaction score"),
		FFTargetShootingRules::CalculateShotScore(
			Rules, 0, EFF_TargetShootingFeedbackType::Bullseye, 0.19f, false),
		200);
	TestEqual(TEXT("Golden target multiplies the full shot value"),
		FFTargetShootingRules::CalculateShotScore(
			Rules, 0, EFF_TargetShootingFeedbackType::Bullseye, 0.19f, true),
		400);
	TestEqual(TEXT("Aim trainer accuracy uses accepted shots"),
		FMath::RoundToInt(FFTargetShootingRules::CalculateAccuracyPercent(2, 1)), 67);
	TestTrue(TEXT("Aim trainer TPS is stable"),
		FMath::IsNearlyEqual(FFTargetShootingRules::CalculateHitsPerSecond(5, 2.0f), 2.5f));
	FRandomStream SafeMarginStream(77);
	const FVector2D SafeCenter = FFTargetShootingRules::GenerateTargetCenter(
		SafeMarginStream,
		FVector2D(0.05f, 0.08f),
		Rules.SpawnSafeMarginNormalized);
	TestTrue(TEXT("Authored HUD safe margin excludes top-overlay target spawns"),
		SafeCenter.Y - 0.08f >= Rules.SpawnSafeMarginNormalized.Top);
	FFF_TargetShootingRules InvalidRules = Rules;
	InvalidRules.TargetLifetimeEndSeconds = 0.001f;
	TestFalse(TEXT("Sub-frame target lifetime is rejected before runtime advance"),
		InvalidRules.IsValid());
	InvalidRules = Rules;
	InvalidRules.BonusTargetScoreMultiplier = 101.0f;
	TestFalse(TEXT("Unbounded bonus multiplier is rejected"), InvalidRules.IsValid());
	InvalidRules = Rules;
	InvalidRules.SpawnSafeMarginNormalized.Top = 0.80f;
	TestFalse(TEXT("Safe margin must leave room for the complete target ellipse"),
		InvalidRules.IsValid());
	InvalidRules = Rules;
	InvalidRules.ProjectileOriginNormalized.Y = 1.1f;
	TestFalse(TEXT("Projectile origin must stay inside normalized play space"),
		InvalidRules.IsValid());
	InvalidRules = Rules;
	InvalidRules.ProjectileSpeedNormalizedPerSecond = 0.0f;
	TestFalse(TEXT("Projectile speed must be positive"), InvalidRules.IsValid());

	UFF_TargetShootingDefinition* BonusDefinition = MakeDefinition();
	BonusDefinition->Rules.BonusTargetChance = 1.0f;
	FString BonusFailureReason;
	UFF_TargetShootingRuntime* BonusRuntime = MakeRuntime(
		BonusDefinition,
		MakeContext(314159),
		BonusFailureReason);
	if (!TestNotNull(TEXT("Forced golden-target runtime initializes"), BonusRuntime))
	{
		return false;
	}
	const FFF_TargetShootingSnapshot BonusSnapshot = BonusRuntime->GetTargetSnapshot();
	TestTrue(TEXT("Chance one produces a golden target"), BonusSnapshot.bBonusTarget);
	TestTrue(TEXT("Golden target uses its authored shorter lifetime"),
		FMath::IsNearlyEqual(
			BonusSnapshot.TargetLifetimeSeconds,
			BonusDefinition->Rules.TargetLifetimeStartSeconds
				* BonusDefinition->Rules.BonusTargetLifetimeScale));
	BonusRuntime->SubmitShot(
		BonusSnapshot.Session.Context.SessionId,
		BonusSnapshot.TargetCenterNormalized,
		BonusSnapshot.TargetSerial);
	TestEqual(TEXT("Forced fast golden bullseye applies the full multiplier"),
		BonusRuntime->GetSnapshot().Score, 400);
	TestTrue(TEXT("Golden hit feedback preserves target kind"),
		BonusRuntime->GetTargetSnapshot().bLatestFeedbackWasBonusTarget);

	UFF_TargetShootingDefinition* Definition = MakeDefinition();
	Definition->Rules.BonusTargetChance = 0.0f;
	FString FailureReason;
	UFF_TargetShootingRuntime* Runtime = MakeRuntime(Definition, MakeContext(), FailureReason);
	if (!TestNotNull(*FString::Printf(TEXT("Runtime initializes: %s"), *FailureReason), Runtime))
	{
		return false;
	}

	FFF_TargetShootingSnapshot Snapshot = Runtime->GetTargetSnapshot();
	TestEqual(TEXT("Center hit is accepted"),
		Runtime->SubmitShot(
			Snapshot.Session.Context.SessionId,
			Snapshot.TargetCenterNormalized,
			Snapshot.TargetSerial),
		EFF_TargetShootingShotResult::Hit);
	TestEqual(TEXT("First center hit includes bullseye and reaction bonuses"),
		Runtime->GetSnapshot().Score, 200);
	TestEqual(TEXT("First hit feedback tier"),
		Runtime->GetTargetSnapshot().LatestFeedbackType,
		EFF_TargetShootingFeedbackType::Bullseye);
	TestEqual(TEXT("First hit feedback score"),
		Runtime->GetTargetSnapshot().LatestScoreDelta, 200);

	Snapshot = Runtime->GetTargetSnapshot();
	TestEqual(TEXT("Second center hit is accepted"),
		Runtime->SubmitShot(
			Snapshot.Session.Context.SessionId,
			Snapshot.TargetCenterNormalized,
			Snapshot.TargetSerial),
		EFF_TargetShootingShotResult::Hit);
	TestEqual(TEXT("Second hit includes combo bonus"), Runtime->GetSnapshot().Score, 410);

	Snapshot = Runtime->GetTargetSnapshot();
	const int32 TargetSerialBeforeMiss = Snapshot.TargetSerial;
	TestEqual(TEXT("Outside shot is an authored miss"),
		Runtime->SubmitShot(
			Snapshot.Session.Context.SessionId,
			MakeGuaranteedMissPoint(Snapshot),
			Snapshot.TargetSerial),
		EFF_TargetShootingShotResult::Miss);
	TestEqual(TEXT("Miss penalty is applied"), Runtime->GetSnapshot().Score, 385);
	TestEqual(TEXT("Miss resets combo"), Runtime->GetTargetSnapshot().CurrentCombo, 0);
	TestEqual(TEXT("Miss keeps the same target for aim correction"),
		Runtime->GetTargetSnapshot().TargetSerial, TargetSerialBeforeMiss);
	TestEqual(TEXT("Miss creates one feedback event"),
		Runtime->GetTargetSnapshot().LatestFeedbackType,
		EFF_TargetShootingFeedbackType::Miss);
	TestEqual(TEXT("Two hits and one miss report rounded accuracy"),
		FMath::RoundToInt(Runtime->GetTargetSnapshot().AccuracyPercent), 67);

	TestEqual(TEXT("Half threshold maps to half reward"),
		FFTargetShootingRules::CalculateRewardGold(500, 1000, 150), 75);
	TestEqual(TEXT("Reward cannot exceed economy cap"),
		FFTargetShootingRules::CalculateRewardGold(MAX_int32, 1000, 150), 150);
	TestEqual(TEXT("Negative score never creates reward"),
		FFTargetShootingRules::CalculateRewardGold(-1, 1000, 150), 0);
	Definition->RewardGoldAtMaxScore = 90;
	TestEqual(TEXT("Per-minigame DataAsset reward overrides the festival fallback"),
		Definition->ResolveRewardGoldAtMaxScore(150), 90);
	Definition->RewardGoldAtMaxScore = 0;
	TestEqual(TEXT("Zero explicitly disables the minigame reward"),
		Definition->ResolveRewardGoldAtMaxScore(150), 0);
	Definition->RewardGoldAtMaxScore = -1;
	TestEqual(TEXT("Minus one preserves the festival-wide reward fallback"),
		Definition->ResolveRewardGoldAtMaxScore(150), 150);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFF_TargetShootingDeterministicTargetsTest,
	"FestaFiesta.Minigame.TargetShooting.DeterministicTargets",
	TargetShootingTestFlags)

bool FFF_TargetShootingDeterministicTargetsTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UFF_TargetShootingDefinition* DefinitionA = MakeDefinition();
	UFF_TargetShootingDefinition* DefinitionB = MakeDefinition();
	const FFF_MinigameSessionContext Context = MakeContext(123456789);
	FString FailureA;
	FString FailureB;
	UFF_TargetShootingRuntime* RuntimeA = MakeRuntime(DefinitionA, Context, FailureA);
	UFF_TargetShootingRuntime* RuntimeB = MakeRuntime(DefinitionB, Context, FailureB);
	if (!TestNotNull(TEXT("First runtime initializes"), RuntimeA)
		|| !TestNotNull(TEXT("Second runtime initializes"), RuntimeB))
	{
		return false;
	}

	FFF_TargetShootingSnapshot SnapshotA = RuntimeA->GetTargetSnapshot();
	FFF_TargetShootingSnapshot SnapshotB = RuntimeB->GetTargetSnapshot();
	TestTrue(TEXT("Same seed creates the same first center"),
		SnapshotA.TargetCenterNormalized.Equals(SnapshotB.TargetCenterNormalized, 0.0f));
	TestTrue(TEXT("Same progress creates the same first radius"),
		SnapshotA.TargetRadiusNormalized.Equals(SnapshotB.TargetRadiusNormalized, 0.0f));
	TestEqual(TEXT("Same seed creates the same first bonus-target kind"),
		SnapshotA.bBonusTarget, SnapshotB.bBonusTarget);

	RuntimeA->SubmitShot(Context.SessionId, SnapshotA.TargetCenterNormalized, SnapshotA.TargetSerial);
	RuntimeB->SubmitShot(Context.SessionId, SnapshotB.TargetCenterNormalized, SnapshotB.TargetSerial);
	SnapshotA = RuntimeA->GetTargetSnapshot();
	SnapshotB = RuntimeB->GetTargetSnapshot();
	TestTrue(TEXT("Same accepted input creates the same next center"),
		SnapshotA.TargetCenterNormalized.Equals(SnapshotB.TargetCenterNormalized, 0.0f));
	TestEqual(TEXT("Same accepted input creates the same score"),
		SnapshotA.Session.Score, SnapshotB.Session.Score);
	TestEqual(TEXT("Same accepted input creates the same feedback tier"),
		SnapshotA.LatestFeedbackType, SnapshotB.LatestFeedbackType);

	RuntimeA->Advance(SnapshotA.TargetRemainingSeconds + 0.01f);
	RuntimeB->Advance(SnapshotB.TargetRemainingSeconds + 0.01f);
	SnapshotA = RuntimeA->GetTargetSnapshot();
	SnapshotB = RuntimeB->GetTargetSnapshot();
	TestTrue(TEXT("Same time step creates the same post-expiry center"),
		SnapshotA.TargetCenterNormalized.Equals(SnapshotB.TargetCenterNormalized, 0.0f));
	TestEqual(TEXT("Expired target is counted"), SnapshotA.EscapedTargetCount, 1);

	TestTrue(TEXT("Target stays inside horizontal bounds"),
		SnapshotA.TargetCenterNormalized.X - SnapshotA.TargetRadiusNormalized.X
			>= DefinitionA->Rules.SpawnSafeMarginNormalized.Left
		&& SnapshotA.TargetCenterNormalized.X + SnapshotA.TargetRadiusNormalized.X
			<= 1.0f - DefinitionA->Rules.SpawnSafeMarginNormalized.Right);
	TestTrue(TEXT("Target stays inside vertical bounds"),
		SnapshotA.TargetCenterNormalized.Y - SnapshotA.TargetRadiusNormalized.Y
			>= DefinitionA->Rules.SpawnSafeMarginNormalized.Top
		&& SnapshotA.TargetCenterNormalized.Y + SnapshotA.TargetRadiusNormalized.Y
			<= 1.0f - DefinitionA->Rules.SpawnSafeMarginNormalized.Bottom);
	TestEqual(TEXT("Expired target emits explicit too-slow feedback"),
		SnapshotA.LatestFeedbackType, EFF_TargetShootingFeedbackType::Escaped);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFF_TargetShootingProjectileFlightTest,
	"FestaFiesta.Minigame.TargetShooting.ProjectileFlightAndImpact",
	TargetShootingTestFlags)

bool FFF_TargetShootingProjectileFlightTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UFF_TargetShootingDefinition* Definition = MakeDefinition();
	Definition->Rules.BonusTargetChance = 0.0f;
	Definition->Rules.ProjectileSpeedNormalizedPerSecond = 100.0f;
	Definition->Rules.ProjectileMinTravelSeconds = 0.30f;
	const FFF_MinigameSessionContext Context = MakeContext(86420);
	FString FailureReason;
	UFF_TargetShootingRuntime* Runtime = MakeRuntime(Definition, Context, FailureReason);
	if (!TestNotNull(*FString::Printf(TEXT("Projectile runtime initializes: %s"), *FailureReason), Runtime))
	{
		return false;
	}

	const FFF_TargetShootingSnapshot Original = Runtime->GetTargetSnapshot();
	const FVector2D AuthoredMuzzle(0.44f, 0.76f);
	TestEqual(TEXT("Fire request starts a projectile"),
		Runtime->FireProjectileFromMuzzle(
			Context.SessionId,
			AuthoredMuzzle,
			Original.TargetCenterNormalized,
			Original.TargetSerial),
		EFF_TargetShootingShotResult::ProjectileFired);
	FFF_TargetShootingSnapshot Projectile = Runtime->GetTargetSnapshot();
	TestTrue(TEXT("Accepted projectile is in flight"), Projectile.bProjectileInFlight);
	TestEqual(TEXT("Projectile starts at the weapon-authored muzzle"),
		Projectile.ProjectileStartNormalized, AuthoredMuzzle);
	TestEqual(TEXT("Projectile captures the target serial"),
		Projectile.ProjectileTargetSerial, Original.TargetSerial);
	TestEqual(TEXT("Score is deferred until impact"), Projectile.Session.Score, 0);
	TestEqual(TEXT("Target remains visible during projectile travel"),
		Projectile.TargetSerial, Original.TargetSerial);
	TestEqual(TEXT("A second projectile cannot overlap the active shot"),
		Runtime->FireProjectile(
			Context.SessionId,
			Original.TargetCenterNormalized,
			Original.TargetSerial),
		EFF_TargetShootingShotResult::RejectedProjectileInFlight);
	TestEqual(TEXT("Immediate resolution cannot bypass an active projectile"),
		Runtime->SubmitShot(
			Context.SessionId,
			Original.TargetCenterNormalized,
			Original.TargetSerial),
		EFF_TargetShootingShotResult::RejectedProjectileInFlight);

	Runtime->Advance(Projectile.ProjectileTravelSeconds * 0.5f);
	const FFF_TargetShootingSnapshot MidFlight = Runtime->GetTargetSnapshot();
	TestTrue(TEXT("Projectile remains in flight before arrival"), MidFlight.bProjectileInFlight);
	TestEqual(TEXT("Mid-flight still has no score"), MidFlight.Session.Score, 0);
	TestTrue(TEXT("Projectile position advances away from the muzzle"),
		!MidFlight.ProjectilePositionNormalized.Equals(
			MidFlight.ProjectileStartNormalized,
			UE_KINDA_SMALL_NUMBER));

	Runtime->Advance(MidFlight.ProjectileRemainingSeconds + 0.001f);
	const FFF_TargetShootingSnapshot Impact = Runtime->GetTargetSnapshot();
	TestFalse(TEXT("Projectile is removed after arrival"), Impact.bProjectileInFlight);
	TestEqual(TEXT("Impact performs one authoritative hit"), Impact.HitCount, 1);
	TestEqual(TEXT("Center impact is a bullseye"),
		Impact.LatestFeedbackType, EFF_TargetShootingFeedbackType::Bullseye);
	TestEqual(TEXT("Impact applies the fire-time reaction score after travel"),
		Impact.Session.Score, 200);
	TestTrue(TEXT("Reaction metric excludes projectile travel time"),
		FMath::IsNearlyZero(Impact.LatestReactionSeconds));
	TestTrue(TEXT("A hit spawns the next target after impact"),
		Impact.TargetSerial != Original.TargetSerial);

	const int32 TargetSerialBeforeMiss = Impact.TargetSerial;
	const FVector2D MissPoint = MakeGuaranteedMissPoint(Impact);
	TestEqual(TEXT("Miss trajectory also starts as a projectile"),
		Runtime->FireProjectile(Context.SessionId, MissPoint, TargetSerialBeforeMiss),
		EFF_TargetShootingShotResult::ProjectileFired);
	Projectile = Runtime->GetTargetSnapshot();
	Runtime->Advance(Projectile.ProjectileTravelSeconds + 0.001f);
	const FFF_TargetShootingSnapshot MissImpact = Runtime->GetTargetSnapshot();
	TestEqual(TEXT("Miss is counted at projectile arrival"), MissImpact.MissCount, 1);
	TestEqual(TEXT("Miss impact keeps the same target"),
		MissImpact.TargetSerial, TargetSerialBeforeMiss);
	TestEqual(TEXT("Miss impact emits miss feedback"),
		MissImpact.LatestFeedbackType, EFF_TargetShootingFeedbackType::Miss);

	UFF_TargetShootingDefinition* FastDefinition = MakeDefinition();
	UFF_TargetShootingDefinition* SlowDefinition = MakeDefinition();
	FastDefinition->Rules.BonusTargetChance = 0.0f;
	SlowDefinition->Rules.BonusTargetChance = 0.0f;
	FastDefinition->Rules.ProjectileSpeedNormalizedPerSecond = 100.0f;
	SlowDefinition->Rules.ProjectileSpeedNormalizedPerSecond = 100.0f;
	FastDefinition->Rules.ProjectileMinTravelSeconds = 0.06f;
	SlowDefinition->Rules.ProjectileMinTravelSeconds = 0.35f;
	const FFF_MinigameSessionContext ReactionContext = MakeContext(24680);
	FString FastFailure;
	FString SlowFailure;
	UFF_TargetShootingRuntime* FastRuntime = MakeRuntime(
		FastDefinition, ReactionContext, FastFailure);
	UFF_TargetShootingRuntime* SlowRuntime = MakeRuntime(
		SlowDefinition, ReactionContext, SlowFailure);
	if (!TestNotNull(TEXT("Fast reaction-control runtime initializes"), FastRuntime)
		|| !TestNotNull(TEXT("Slow reaction-control runtime initializes"), SlowRuntime))
	{
		return false;
	}
	FastRuntime->Advance(0.15f);
	SlowRuntime->Advance(0.15f);
	const FFF_TargetShootingSnapshot FastTarget = FastRuntime->GetTargetSnapshot();
	const FFF_TargetShootingSnapshot SlowTarget = SlowRuntime->GetTargetSnapshot();
	FastRuntime->FireProjectile(
		ReactionContext.SessionId,
		FastTarget.TargetCenterNormalized,
		FastTarget.TargetSerial);
	SlowRuntime->FireProjectile(
		ReactionContext.SessionId,
		SlowTarget.TargetCenterNormalized,
		SlowTarget.TargetSerial);
	const float FastTravelSeconds = FastRuntime->GetTargetSnapshot().ProjectileTravelSeconds;
	const float SlowTravelSeconds = SlowRuntime->GetTargetSnapshot().ProjectileTravelSeconds;
	TestTrue(TEXT("Control runtimes use different projectile travel times"),
		SlowTravelSeconds > FastTravelSeconds);
	FastRuntime->Advance(FastTravelSeconds + 0.001f);
	SlowRuntime->Advance(SlowTravelSeconds + 0.001f);
	const FFF_TargetShootingSnapshot FastImpact = FastRuntime->GetTargetSnapshot();
	const FFF_TargetShootingSnapshot SlowImpact = SlowRuntime->GetTargetSnapshot();
	TestTrue(TEXT("Reaction metric is identical across projectile speeds"),
		FMath::IsNearlyEqual(
			FastImpact.LatestReactionSeconds,
			SlowImpact.LatestReactionSeconds));
	TestEqual(TEXT("Reaction score is identical across projectile speeds"),
		FastImpact.Session.Score, SlowImpact.Session.Score);

	UFF_TargetShootingDefinition* ExpiryDefinition = MakeDefinition();
	ExpiryDefinition->Rules.BonusTargetChance = 0.0f;
	ExpiryDefinition->Rules.TargetLifetimeStartSeconds = 0.1f;
	ExpiryDefinition->Rules.TargetLifetimeEndSeconds = 0.1f;
	ExpiryDefinition->Rules.ProjectileSpeedNormalizedPerSecond = 100.0f;
	ExpiryDefinition->Rules.ProjectileMinTravelSeconds = 0.15f;
	FString ExpiryFailure;
	UFF_TargetShootingRuntime* ExpiryRuntime = MakeRuntime(
		ExpiryDefinition,
		MakeContext(97531),
		ExpiryFailure);
	if (!TestNotNull(
		*FString::Printf(TEXT("Expiry-race runtime initializes: %s"), *ExpiryFailure),
		ExpiryRuntime))
	{
		return false;
	}
	const FFF_TargetShootingSnapshot ExpiringTarget = ExpiryRuntime->GetTargetSnapshot();
	ExpiryRuntime->FireProjectile(
		ExpiringTarget.Session.Context.SessionId,
		ExpiringTarget.TargetCenterNormalized,
		ExpiringTarget.TargetSerial);
	ExpiryRuntime->Advance(0.08f);
	const FFF_TargetShootingSnapshot ReservedMidFlight = ExpiryRuntime->GetTargetSnapshot();
	TestTrue(TEXT("Accepted projectile keeps its target reserved while travelling"),
		ReservedMidFlight.bProjectileInFlight);
	TestTrue(TEXT("Reserved target lifetime does not drain during cosmetic travel"),
		FMath::IsNearlyEqual(
			ReservedMidFlight.TargetRemainingSeconds,
			ExpiringTarget.TargetRemainingSeconds));
	ExpiryRuntime->Advance(0.08f);
	const FFF_TargetShootingSnapshot ReservedImpact = ExpiryRuntime->GetTargetSnapshot();
	TestFalse(TEXT("Reserved projectile finishes at impact"),
		ReservedImpact.bProjectileInFlight);
	TestEqual(TEXT("A timely shot cannot lose its target to cosmetic travel"),
		ReservedImpact.HitCount, 1);
	TestEqual(TEXT("Reserved target is not counted as escaped"),
		ReservedImpact.EscapedTargetCount, 0);
	TestEqual(TEXT("Reserved hit does not add a miss"),
		ReservedImpact.MissCount, 0);

	UFF_TargetShootingDefinition* DeadlineDefinition = MakeDefinition();
	DeadlineDefinition->Rules.BonusTargetChance = 0.0f;
	DeadlineDefinition->Rules.TargetLifetimeStartSeconds = 0.2f;
	DeadlineDefinition->Rules.TargetLifetimeEndSeconds = 0.2f;
	DeadlineDefinition->Rules.ProjectileSpeedNormalizedPerSecond = 100.0f;
	DeadlineDefinition->Rules.ProjectileMinTravelSeconds = 0.2f;
	FString DeadlineFailure;
	UFF_TargetShootingRuntime* DeadlineRuntime = MakeRuntime(
		DeadlineDefinition,
		MakeContext(11223),
		DeadlineFailure);
	if (!TestNotNull(TEXT("Target-deadline runtime initializes"), DeadlineRuntime))
	{
		return false;
	}
	const FFF_TargetShootingSnapshot DeadlineTarget = DeadlineRuntime->GetTargetSnapshot();
	DeadlineRuntime->FireProjectile(
		DeadlineTarget.Session.Context.SessionId,
		DeadlineTarget.TargetCenterNormalized,
		DeadlineTarget.TargetSerial);
	DeadlineRuntime->Advance(0.2f);
	const FFF_TargetShootingSnapshot DeadlineImpact = DeadlineRuntime->GetTargetSnapshot();
	TestEqual(TEXT("Projectile impact wins an exact target-deadline tie"),
		DeadlineImpact.HitCount, 1);
	TestEqual(TEXT("Exact-deadline hit does not also count as escaped"),
		DeadlineImpact.EscapedTargetCount, 0);

	UFF_TargetShootingDefinition* SessionEndDefinition = MakeDefinition();
	SessionEndDefinition->DurationSeconds = 0.2f;
	SessionEndDefinition->Rules.BonusTargetChance = 0.0f;
	SessionEndDefinition->Rules.TargetLifetimeStartSeconds = 0.5f;
	SessionEndDefinition->Rules.TargetLifetimeEndSeconds = 0.5f;
	SessionEndDefinition->Rules.ProjectileSpeedNormalizedPerSecond = 100.0f;
	SessionEndDefinition->Rules.ProjectileMinTravelSeconds = 0.2f;
	FString SessionEndFailure;
	UFF_TargetShootingRuntime* SessionEndRuntime = MakeRuntime(
		SessionEndDefinition,
		MakeContext(44556),
		SessionEndFailure);
	if (!TestNotNull(TEXT("Session-end boundary runtime initializes"), SessionEndRuntime))
	{
		return false;
	}
	const FFF_TargetShootingSnapshot SessionEndTarget = SessionEndRuntime->GetTargetSnapshot();
	SessionEndRuntime->FireProjectile(
		SessionEndTarget.Session.Context.SessionId,
		SessionEndTarget.TargetCenterNormalized,
		SessionEndTarget.TargetSerial);
	SessionEndRuntime->Advance(0.2f);
	const FFF_TargetShootingSnapshot SessionEndImpact = SessionEndRuntime->GetTargetSnapshot();
	TestEqual(TEXT("Impact at session end still counts"), SessionEndImpact.HitCount, 1);
	TestEqual(TEXT("Session-end impact does not spawn a phantom target"),
		SessionEndImpact.TargetSerial, SessionEndTarget.TargetSerial);
	TestEqual(TEXT("Session completes after exact-boundary impact"),
		SessionEndImpact.Session.State, EFF_MinigameSessionState::Completed);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFF_TargetShootingDuplicateInputTest,
	"FestaFiesta.Minigame.TargetShooting.DuplicateTargetInputIgnored",
	TargetShootingTestFlags)

bool FFF_TargetShootingDuplicateInputTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UFF_TargetShootingDefinition* Definition = MakeDefinition();
	const FFF_MinigameSessionContext Context = MakeContext();
	FString FailureReason;
	UFF_TargetShootingRuntime* Runtime = MakeRuntime(Definition, Context, FailureReason);
	if (!TestNotNull(TEXT("Runtime initializes"), Runtime))
	{
		return false;
	}

	const FFF_TargetShootingSnapshot Original = Runtime->GetTargetSnapshot();
	TestEqual(TEXT("Original serial is accepted"),
		Runtime->SubmitShot(
			Context.SessionId,
			Original.TargetCenterNormalized,
			Original.TargetSerial),
		EFF_TargetShootingShotResult::Hit);
	const FFF_TargetShootingSnapshot AfterHit = Runtime->GetTargetSnapshot();

	TestEqual(TEXT("Repeated serial is rejected"),
		Runtime->SubmitShot(
			Context.SessionId,
			Original.TargetCenterNormalized,
			Original.TargetSerial),
		EFF_TargetShootingShotResult::RejectedStaleTarget);
	const FFF_TargetShootingSnapshot AfterDuplicate = Runtime->GetTargetSnapshot();
	TestEqual(TEXT("Duplicate cannot add score"),
		AfterDuplicate.Session.Score, AfterHit.Session.Score);
	TestEqual(TEXT("Duplicate cannot add a hit"),
		AfterDuplicate.HitCount, AfterHit.HitCount);
	TestEqual(TEXT("Duplicate cannot consume the new target"),
		AfterDuplicate.TargetSerial, AfterHit.TargetSerial);
	TestEqual(TEXT("Rejected duplicate cannot create presentation feedback"),
		AfterDuplicate.LatestFeedbackSerial, AfterHit.LatestFeedbackSerial);

	TestEqual(TEXT("Wrong session is rejected before mutation"),
		Runtime->SubmitShot(
			FGuid(9, 9, 9, 9),
			AfterHit.TargetCenterNormalized,
			AfterHit.TargetSerial),
		EFF_TargetShootingShotResult::RejectedWrongSession);
	TestEqual(TEXT("Rejected wrong-session shot cannot create presentation feedback"),
		Runtime->GetTargetSnapshot().LatestFeedbackSerial,
		AfterHit.LatestFeedbackSerial);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFF_TargetShootingSessionRewardOnceTest,
	"FestaFiesta.Minigame.TargetShooting.SessionRewardOnce",
	TargetShootingTestFlags)

bool FFF_TargetShootingSessionRewardOnceTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UFF_TargetShootingDefinition* Definition = MakeDefinition();
	Definition->ScoreForMaxReward = 1;
	const FFF_MinigameSessionContext Context = MakeContext();
	FString FailureReason;
	UFF_TargetShootingRuntime* Runtime = MakeRuntime(Definition, Context, FailureReason);
	if (!TestNotNull(TEXT("Runtime initializes"), Runtime))
	{
		return false;
	}

	const FFF_TargetShootingSnapshot Target = Runtime->GetTargetSnapshot();
	Runtime->SubmitShot(
		Context.SessionId,
		Target.TargetCenterNormalized,
		Target.TargetSerial);

	int32 EndBroadcastCount = 0;
	Runtime->OnEnded.AddLambda([&EndBroadcastCount](UFF_MinigameRuntime*)
	{
		++EndBroadcastCount;
	});

	Runtime->Advance(29.999f);
	TestEqual(TEXT("Session is still playing before 30 seconds"),
		Runtime->GetSnapshot().State, EFF_MinigameSessionState::Playing);
	Runtime->Advance(1.0f);
	TestEqual(TEXT("Session completes at duration"),
		Runtime->GetSnapshot().State, EFF_MinigameSessionState::Completed);
	TestTrue(TEXT("Elapsed time is clamped to exactly 30 seconds"),
		FMath::IsNearlyEqual(Runtime->GetSnapshot().ElapsedSeconds, 30.0f));
	TestEqual(TEXT("End event broadcasts once"), EndBroadcastCount, 1);
	TestFalse(TEXT("Completed session hides target"),
		Runtime->GetTargetSnapshot().bTargetVisible);

	const FFF_MinigameResult FirstResult = Runtime->BuildResult(150);
	Runtime->Advance(100.0f);
	const FFF_MinigameResult SecondResult = Runtime->BuildResult(150);
	TestEqual(TEXT("Repeated advance cannot broadcast another completion"), EndBroadcastCount, 1);
	TestEqual(TEXT("Reward reaches but does not exceed cap"), FirstResult.RewardGold, 150);
	TestEqual(TEXT("Repeated result calculation is stable"),
		SecondResult.RewardGold, FirstResult.RewardGold);

	UFF_TargetShootingDefinition* CancelDefinition = MakeDefinition();
	FString CancelFailure;
	UFF_TargetShootingRuntime* CancelRuntime = MakeRuntime(
		CancelDefinition,
		MakeContext(7),
		CancelFailure);
	if (!TestNotNull(TEXT("Cancellation runtime initializes"), CancelRuntime))
	{
		return false;
	}
	const FFF_TargetShootingSnapshot CancelTarget = CancelRuntime->GetTargetSnapshot();
	CancelRuntime->SubmitShot(
		CancelTarget.Session.Context.SessionId,
		CancelTarget.TargetCenterNormalized,
		CancelTarget.TargetSerial);
	CancelRuntime->Cancel(EFF_MinigameEndReason::PlayerCancelled);
	TestEqual(TEXT("Cancelled sessions never grant reward"),
		CancelRuntime->BuildResult(150).RewardGold, 0);
	return true;
}

#endif
