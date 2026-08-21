// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Finale/FF_FinaleShowTypes.h"

// 등급 수치는 화면을 봐야 정해지는 값이라 절대값을 고정하지 않는다.
// 대신 등급이 올라갈수록 커진다는 관계와, 나눗셈이 안전한지를 검증한다.

// 1. 등급이 올라갈수록 연출이 커진다.
//    이게 깨지면 돈을 더 쓰고도 초라해 보인다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_FinaleGradeScalesUpward, "FestaFiesta.Finale.Spec.GradeScalesUpward", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_FinaleGradeScalesUpward::RunTest(const FString& Parameters)
{
	const TArray<EFF_FinaleGrade> Order = {
		EFF_FinaleGrade::None, EFF_FinaleGrade::C, EFF_FinaleGrade::B,
		EFF_FinaleGrade::A,    EFF_FinaleGrade::S
	};

	bool bBurstsIncrease = true;
	bool bDurationIncreases = true;
	bool bShellsNonDecreasing = true;
	bool bColorsNonDecreasing = true;

	FFF_FinaleGradeSpec Previous = FFF_FinaleDefaults::MakeSpec(Order[0]);
	for (int32 Index = 1; Index < Order.Num(); ++Index)
	{
		const FFF_FinaleGradeSpec Current = FFF_FinaleDefaults::MakeSpec(Order[Index]);

		bBurstsIncrease &= (Current.BurstCount > Previous.BurstCount);
		bDurationIncreases &= (Current.DurationSeconds > Previous.DurationSeconds);
		bShellsNonDecreasing &= (Current.SimultaneousShells >= Previous.SimultaneousShells);
		bColorsNonDecreasing &= (Current.Colors.Num() >= Previous.Colors.Num());

		Previous = Current;
	}

	TestTrue(TEXT("등급이 오르면 발수가 늘어난다"), bBurstsIncrease);
	TestTrue(TEXT("등급이 오르면 지속 시간이 늘어난다"), bDurationIncreases);
	TestTrue(TEXT("동시 발사 수가 줄지 않는다"), bShellsNonDecreasing);
	TestTrue(TEXT("색 가짓수가 줄지 않는다"), bColorsNonDecreasing);
	return true;
}

// 2. 미구매 등급도 최소 연출이 남는다.
//    아무것도 안 터지면 축제가 그냥 끊긴 것처럼 보인다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_FinaleNoneGradeStillPlays, "FestaFiesta.Finale.Spec.NoneGradeStillPlays", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_FinaleNoneGradeStillPlays::RunTest(const FString& Parameters)
{
	const FFF_FinaleGradeSpec None = FFF_FinaleDefaults::MakeSpec(EFF_FinaleGrade::None);

	TestTrue(TEXT("미구매도 재생 가능하다"), None.IsPlayable());
	TestTrue(TEXT("최소한 한 발은 터진다"), None.BurstCount > 0);
	TestTrue(TEXT("색이 최소 하나는 있다"), None.Colors.Num() > 0);

	// 다만 가장 낮은 등급보다는 작아야 한다. 안 그러면 살 이유가 없다.
	const FFF_FinaleGradeSpec C = FFF_FinaleDefaults::MakeSpec(EFF_FinaleGrade::C);
	TestTrue(TEXT("미구매가 C 등급보다 작다"), None.BurstCount < C.BurstCount);
	return true;
}

// 3. 발사 간격 계산이 안전한지.
//    발수나 지속 시간이 0 이어도 0 으로 나누거나 무한 루프에 빠지면 안 된다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_FinaleIntervalIsSafe, "FestaFiesta.Finale.Spec.IntervalIsSafe", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_FinaleIntervalIsSafe::RunTest(const FString& Parameters)
{
	// 정상값.
	FFF_FinaleGradeSpec Normal;
	Normal.BurstCount = 30;
	Normal.DurationSeconds = 30.f;
	Normal.SimultaneousShells = 1;
	TestEqual(TEXT("30발을 30초에 쏘면 차례가 30번"), Normal.GetVolleyCount(), 30);
	TestEqual(TEXT("간격이 1초"), Normal.GetIntervalSeconds(), 1.f, 0.01f);

	// 동시 발사가 늘면 차례가 줄고 간격이 늘어난다.
	Normal.SimultaneousShells = 3;
	TestEqual(TEXT("3발씩이면 차례가 10번"), Normal.GetVolleyCount(), 10);
	TestEqual(TEXT("간격이 3초"), Normal.GetIntervalSeconds(), 3.f, 0.01f);

	// 나누어떨어지지 않는 경우. 남는 발을 버리면 등급 차이가 흐려진다.
	Normal.BurstCount = 10;
	Normal.SimultaneousShells = 3;
	TestEqual(TEXT("10발을 3발씩이면 4차례 (마지막은 1발)"), Normal.GetVolleyCount(), 4);

	// 극단값에서도 0 이나 음수가 나오면 안 된다.
	FFF_FinaleGradeSpec Empty;
	Empty.BurstCount = 0;
	Empty.DurationSeconds = 0.f;
	TestFalse(TEXT("빈 설정은 재생 불가로 판정된다"), Empty.IsPlayable());
	TestTrue(TEXT("발수 0 이어도 간격이 양수다"), Empty.GetIntervalSeconds() > 0.f);
	TestTrue(TEXT("차례 수가 음수가 아니다"), Empty.GetVolleyCount() >= 0);

	FFF_FinaleGradeSpec Single;
	Single.BurstCount = 1;
	Single.DurationSeconds = 5.f;
	Single.SimultaneousShells = 1;
	TestTrue(TEXT("한 발짜리도 간격이 양수다"), Single.GetIntervalSeconds() > 0.f);
	return true;
}

// 4. 데이터 애셋이 등급별로 올바른 항목을 돌려주는지.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_FinaleShowDataMapsGrades, "FestaFiesta.Finale.Spec.ShowDataMapsGrades", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_FinaleShowDataMapsGrades::RunTest(const FString& Parameters)
{
	UFF_FinaleShowData* Data = NewObject<UFF_FinaleShowData>();

	// 등급마다 구분되는 값을 넣고 제대로 찾아오는지 본다.
	Data->NoneGrade.BurstCount = 1;
	Data->GradeC.BurstCount = 2;
	Data->GradeB.BurstCount = 3;
	Data->GradeA.BurstCount = 4;
	Data->GradeS.BurstCount = 5;

	TestEqual(TEXT("미구매"), Data->GetSpec(EFF_FinaleGrade::None).BurstCount, 1);
	TestEqual(TEXT("C"), Data->GetSpec(EFF_FinaleGrade::C).BurstCount, 2);
	TestEqual(TEXT("B"), Data->GetSpec(EFF_FinaleGrade::B).BurstCount, 3);
	TestEqual(TEXT("A"), Data->GetSpec(EFF_FinaleGrade::A).BurstCount, 4);
	TestEqual(TEXT("S"), Data->GetSpec(EFF_FinaleGrade::S).BurstCount, 5);
	return true;
}

// 5. 스케일도 등급에 따라 커진다.
//    발수만 늘고 크기가 그대로면 화면에서 등급 차이가 잘 안 느껴진다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_FinaleScaleGrowsWithGrade, "FestaFiesta.Finale.Spec.ScaleGrowsWithGrade", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_FinaleScaleGrowsWithGrade::RunTest(const FString& Parameters)
{
	const TArray<EFF_FinaleGrade> Order = {
		EFF_FinaleGrade::None, EFF_FinaleGrade::C, EFF_FinaleGrade::B,
		EFF_FinaleGrade::A,    EFF_FinaleGrade::S
	};

	bool bScaleGrows = true;
	bool bTrailGrows = true;
	bool bAllPositive = true;

	FFF_FinaleGradeSpec Previous = FFF_FinaleDefaults::MakeSpec(Order[0]);
	bAllPositive &= (Previous.BurstScale > 0.f && Previous.TrailLength > 0.f);

	for (int32 Index = 1; Index < Order.Num(); ++Index)
	{
		const FFF_FinaleGradeSpec Current = FFF_FinaleDefaults::MakeSpec(Order[Index]);
		bScaleGrows &= (Current.BurstScale >= Previous.BurstScale);
		bTrailGrows &= (Current.TrailLength >= Previous.TrailLength);
		bAllPositive &= (Current.BurstScale > 0.f && Current.TrailLength > 0.f);
		Previous = Current;
	}

	TestTrue(TEXT("등급이 오르면 폭발 크기가 줄지 않는다"), bScaleGrows);
	TestTrue(TEXT("등급이 오르면 궤적 길이가 줄지 않는다"), bTrailGrows);
	TestTrue(TEXT("모든 등급에서 스케일이 양수다"), bAllPositive);

	// 최고와 최저 사이에 실제로 차이가 있어야 한다. 전부 1.0 이면 통과하지만 의미가 없다.
	const FFF_FinaleGradeSpec None = FFF_FinaleDefaults::MakeSpec(EFF_FinaleGrade::None);
	const FFF_FinaleGradeSpec S = FFF_FinaleDefaults::MakeSpec(EFF_FinaleGrade::S);
	TestTrue(TEXT("S 가 미구매보다 확실히 크다"), S.BurstScale > None.BurstScale * 1.5f);
	return true;
}

// 6. Niagara 파라미터 이름이 비어 있지 않은지.
//    이름이 틀리면 값이 조용히 무시된다. 크래시도 경고도 없어서 "왜 색이 안 바뀌지" 로만 보인다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_FinaleParamNamesAreValid, "FestaFiesta.Finale.Spec.ParamNamesAreValid", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_FinaleParamNamesAreValid::RunTest(const FString& Parameters)
{
	TestFalse(TEXT("색 파라미터 이름이 비어 있지 않다"), FFF_FinaleParams::BurstColor.IsNone());
	TestFalse(TEXT("크기 파라미터 이름이 비어 있지 않다"), FFF_FinaleParams::BurstScale.IsNone());
	TestFalse(TEXT("궤적 파라미터 이름이 비어 있지 않다"), FFF_FinaleParams::TrailLength.IsNone());

	// 서로 달라야 한다. 같으면 나중 값이 앞 값을 덮어쓴다.
	TestNotEqual(TEXT("색과 크기 이름이 다르다"), FFF_FinaleParams::BurstColor, FFF_FinaleParams::BurstScale);
	TestNotEqual(TEXT("크기와 궤적 이름이 다르다"), FFF_FinaleParams::BurstScale, FFF_FinaleParams::TrailLength);
	TestNotEqual(TEXT("색과 궤적 이름이 다르다"), FFF_FinaleParams::BurstColor, FFF_FinaleParams::TrailLength);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
