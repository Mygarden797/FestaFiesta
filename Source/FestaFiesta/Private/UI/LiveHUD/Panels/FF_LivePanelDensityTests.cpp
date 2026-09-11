// Fill out your copyright notice in the Description page of Project Settings.

#include "Misc/AutomationTest.h"
#include "UI/LiveHUD/Panels/FF_LivePanelDensity.h"

#if WITH_DEV_AUTOMATION_TESTS

/**
 * 밀집 범례 테스트.
 *
 * === 🔴 왜 이 테스트가 있어야 하나 ===
 * 범례는 Fruin 경계값(`0.31 / 0.43 / 0.72 / 1.08 / 2.17`)을 **베끼지 않고**
 * `FCrowdDensityGrid::DensityToLOS()` 를 훑어서 알아낸다. 베껴 두면 이정원 님이 값을
 * 바꿨을 때 **범례만 조용히 틀려지기** 때문이다 — 색은 새 경계로 칠해지는데 적힌 수치는
 * 옛날 것이라, 화면은 멀쩡해 보이고 내용만 거짓이 된다.
 *
 * 훑어서 알아내는 방식이라 **경계값 자체를 여기에 적으면 안 된다.** 그러면 결국 베낀 것과
 * 같아진다. 그래서 이 테스트는 값이 아니라 **성질**을 본다 —
 *   · 등급이 여섯 개 다 나온다
 *   · 구간이 낮은 쪽부터 순서대로다
 *   · 구간 사이에 틈이 없다 (앞 줄의 끝 = 다음 줄의 시작)
 *   · 마지막 줄만 위가 열려 있다
 *
 * 이정원 님이 경계를 바꿔도 이 성질은 그대로 참이고, 등급을 하나 없애거나 순서를 뒤집으면
 * 여기서 걸린다.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_DensityRules_Legend, "FestaFiesta.UI.Density.Legend", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_DensityRules_Legend::RunTest(const FString& Parameters)
{
	const TArray<FFF_DensityLegendRow> Rows = FFF_DensityRules::BuildLegend();

	// --- 여섯 등급이 다 나온다 ---
	TestEqual(TEXT("등급이 여섯 줄"), Rows.Num(), 6);
	if (Rows.Num() != 6)
	{
		// 아래 검사가 전부 무의미해진다. 여기서 끝낸다.
		return false;
	}

	TestEqual(TEXT("첫 줄은 A"), Rows[0].Grade, ECrowdLOS::A);
	TestEqual(TEXT("마지막 줄은 F"), Rows.Last().Grade, ECrowdLOS::F);

	// --- 첫 줄은 0 에서 시작한다 ---
	TestEqual(TEXT("A 는 0 부터"), Rows[0].MinDensity, 0.f);

	// --- 🔴 마지막 줄만 위가 열려 있다 ---
	//
	// `MaxDensity` 를 0 으로 두면 화면이 `0명/㎡ 까지` 로 적어서 **정반대 뜻**이 된다.
	// 음수로 두고 `HasUpperBound()` 가 가른다.
	TestFalse(TEXT("F 는 위가 없다"), Rows.Last().HasUpperBound());

	for (int32 Index = 0; Index < Rows.Num() - 1; ++Index)
	{
		TestTrue(
			*FString::Printf(TEXT("%d 번째 줄은 위가 있다"), Index),
			Rows[Index].HasUpperBound());
	}

	// --- 낮은 쪽부터 순서대로, 틈 없이 이어진다 ---
	for (int32 Index = 1; Index < Rows.Num(); ++Index)
	{
		const FFF_DensityLegendRow& Prev = Rows[Index - 1];
		const FFF_DensityLegendRow& Curr = Rows[Index];

		TestTrue(
			*FString::Printf(TEXT("%d 번째 구간이 앞보다 높다"), Index),
			Curr.MinDensity > Prev.MinDensity);

		// 🔴 틈이 있으면 그 사이 밀도는 **어느 등급도 아니게** 된다. 화면에 빈 구간이
		//    생기고 플레이어는 자기 값이 어디에 드는지 못 찾는다.
		TestEqual(
			*FString::Printf(TEXT("%d 번째 구간이 앞 구간과 붙어 있다"), Index),
			Curr.MinDensity, Prev.MaxDensity);
	}

	// --- 문자와 이름이 비어 있지 않다 ---
	//
	// 색을 못 보는 사람에게는 이 둘이 **유일한 단서**다 (AT-011).
	for (const FFF_DensityLegendRow& Row : Rows)
	{
		TestFalse(TEXT("등급 문자가 있다"), Row.GradeLetter.IsEmpty());
		TestFalse(TEXT("등급 이름이 있다"), Row.GradeName.IsEmpty());
	}

	return true;
}

/**
 * 잘못된 인자로 무한 루프를 돌지 않는다.
 *
 * `BuildLegend` 는 `Density += Step` 으로 훑는다. `Step` 이 0 이면 영원히 안 끝난다 —
 * 에디터가 멈추고 원인을 찾기 어렵다.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_DensityRules_BadArgs, "FestaFiesta.UI.Density.BadArgs", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_DensityRules_BadArgs::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("간격 0 이면 빈 목록"),
		FFF_DensityRules::BuildLegend(/*MaxScanDensity=*/4.f, /*StepDensity=*/0.f).IsEmpty());

	TestTrue(TEXT("간격 음수면 빈 목록"),
		FFF_DensityRules::BuildLegend(4.f, -0.1f).IsEmpty());

	TestTrue(TEXT("상한 0 이면 빈 목록"),
		FFF_DensityRules::BuildLegend(0.f, 0.001f).IsEmpty());

	// ★ 상한이 너무 낮으면 위쪽 등급을 못 찾는다. 그때도 **크래시하지 않고** 찾은 만큼만
	//   돌려준다 — 범례가 짧은 것은 화면에서 보이지만 멈추는 것은 안 보인다.
	const TArray<FFF_DensityLegendRow> Shallow = FFF_DensityRules::BuildLegend(0.2f, 0.001f);
	TestTrue(TEXT("상한이 낮으면 찾은 만큼만"), Shallow.Num() >= 1 && Shallow.Num() < 6);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
