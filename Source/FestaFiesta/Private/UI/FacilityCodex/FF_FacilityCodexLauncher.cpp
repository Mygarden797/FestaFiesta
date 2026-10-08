// FF_FacilityCodexLauncher.cpp
// 역할: 시설 도감을 여는 버튼을 도감 화면 관리자에 연결한다.

#include "UI/FacilityCodex/FF_FacilityCodexLauncher.h"

#include "Building/Codex/FF_FacilityCodexProgressSubsystem.h"
#include "Building/Codex/FF_FacilityCodexSubsystem.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "UI/FacilityCodex/FF_FacilityCodexPresenterSubsystem.h"

void UFF_FacilityCodexLauncher::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (Button_OpenCodex) Button_OpenCodex->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleOpen);
}

void UFF_FacilityCodexLauncher::NativeConstruct()
{
	Super::NativeConstruct();
	if (IsDesignTime()) return;
	UGameInstance* Game = GetGameInstance();
	Progress = Game ? Game->GetSubsystem<UFF_FacilityCodexProgressSubsystem>() : nullptr;
	if (Progress) Progress->OnProgressChanged.AddUniqueDynamic(this, &ThisClass::RefreshNotificationHint);
	UWorld* World = GetWorld();
	Codex = World ? World->GetSubsystem<UFF_FacilityCodexSubsystem>() : nullptr;
	// Discovery can broadcast progress before the codex publishes its updated entries.
	if (Codex) Codex->OnCodexChanged.AddUniqueDynamic(this, &ThisClass::RefreshNotificationHint);
	RefreshNotificationHint();
}

void UFF_FacilityCodexLauncher::NativeDestruct()
{
	if (Progress) Progress->OnProgressChanged.RemoveDynamic(this, &ThisClass::RefreshNotificationHint);
	if (Codex) Codex->OnCodexChanged.RemoveDynamic(this, &ThisClass::RefreshNotificationHint);
	Progress = nullptr;
	Codex = nullptr;
	Super::NativeDestruct();
}

void UFF_FacilityCodexLauncher::RefreshNotificationHint()
{
	const bool bUnseen = Progress && !Progress->HasSeenIntroduction();
	const bool bHasRewards = Codex && Codex->HasUnclaimedRewards();
	if (Border_IntroductionHint)
	{
		Border_IntroductionHint->SetVisibility(bUnseen ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		Border_IntroductionHint->SetRenderScale(FVector2D(1.f));
	}
	if (Border_RewardHint)
		Border_RewardHint->SetVisibility(bHasRewards
			? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (Button_OpenCodex)
		Button_OpenCodex->SetToolTipText(bHasRewards
			? NSLOCTEXT("FFFacilityCodex", "LauncherRewards", "조합 도감에 받을 수 있는 보상이 있어요.")
			: bUnseen ? NSLOCTEXT("FFFacilityCodex", "LauncherIntroduction", "조합 도감의 사용 방법을 확인해 보세요.")
			: NSLOCTEXT("FFFacilityCodex", "LauncherOpen", "시설 조합과 발견 기록을 확인합니다."));
}

void UFF_FacilityCodexLauncher::HandleOpen()
{
	UWorld* World = GetWorld();
	if (!World) return;
	if (auto* Presenter = World->GetSubsystem<UFF_FacilityCodexPresenterSubsystem>())
	{
		// An inspector selection is an entry point, not an implicit collection filter.
		Presenter->OpenCodex(GetOwningPlayer(), ExplicitFacilityId);
	}
}
