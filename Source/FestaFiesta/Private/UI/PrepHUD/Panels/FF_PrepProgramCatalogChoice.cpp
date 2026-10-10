#include "UI/PrepHUD/Panels/FF_PrepProgramCatalogChoice.h"

#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "HAL/PlatformTime.h"

void UFF_PrepProgramCatalogChoice::ConfigureCatalogChoice(int32 InIndex, const FText& FacilityName)
{
	ConfigureChoice(InIndex, FacilityName);
	if (Text_Subtitle) Text_Subtitle->SetText(ShortcutText);
}

void UFF_PrepProgramCatalogChoice::NativePreConstruct()
{
	Super::NativePreConstruct();
	if (Text_Subtitle) Text_Subtitle->SetText(ShortcutText);
	if (Border_Attention) Border_Attention->SetRenderOpacity(0.f);
}

void UFF_PrepProgramCatalogChoice::SetInstalledCount(int32 Count)
{
	if (Text_InstallationStatus)
	{
		Text_InstallationStatus->SetText(Count > 0
			? FText::Format(InstalledTextFormat, FText::AsNumber(Count)) : NotInstalledText);
		if (Count > 0) Text_InstallationStatus->SetColorAndOpacity(InstalledTextColor);
	}
	if (Count > 0 && Border_InstallationStatus)
	{
		FSlateBrush Brush = Border_InstallationStatus->Background;
		Brush.TintColor = InstalledBackgroundColor;
		Border_InstallationStatus->SetBrush(Brush);
	}
}

void UFF_PrepProgramCatalogChoice::SetAttention(bool bAttention)
{
	if (bAttention && !bAttentionRequested && AttentionSeconds > 0.f)
		AttentionStart = FPlatformTime::Seconds();
	if (!bAttention)
	{
		AttentionStart = -1.0;
		if (Border_Attention) Border_Attention->SetRenderOpacity(0.f);
	}
	bAttentionRequested = bAttention;
}

void UFF_PrepProgramCatalogChoice::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (AttentionStart < 0.0 || !Border_Attention) return;
	const double Elapsed = FPlatformTime::Seconds() - AttentionStart;
	if (AttentionSeconds <= 0.f || Elapsed >= AttentionSeconds)
	{
		Border_Attention->SetRenderOpacity(0.f);
		AttentionStart = -1.0;
		return;
	}
	const float Phase = static_cast<float>(Elapsed / AttentionSeconds) * FMath::Clamp(AttentionCycles, 1, 3);
	const float Strength = 0.5f - 0.5f * FMath::Cos(Phase * 2.f * PI);
	Border_Attention->SetRenderOpacity(Strength * FMath::Clamp(AttentionOpacity, 0.f, 1.f));
}

void UFF_PrepProgramCatalogChoice::NativeDestruct()
{
	SetAttention(false);
	Super::NativeDestruct();
}
