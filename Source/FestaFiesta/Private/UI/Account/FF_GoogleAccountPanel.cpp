#include "UI/Account/FF_GoogleAccountPanel.h"

#include "Authentication/FF_GoogleAuthSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Font.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "ImageUtils.h"
#include "Misc/Paths.h"
#include "UI/Frontend/FF_FrontendCasualStyle.h"

#define LOCTEXT_NAMESPACE "FFGoogleAccountPanel"

TSharedRef<SWidget> UFF_GoogleAccountPanel::RebuildWidget()
{
	BuildAccountTree();
	return Super::RebuildWidget();
}

void UFF_GoogleAccountPanel::BuildAccountTree()
{
	if (!WidgetTree || WidgetTree->RootWidget) { return; }
	const FFFrontendCasual::FTokens Colors = FFFrontendCasual::Get();
	UFont* FontObject = LoadObject<UFont>(nullptr,
		TEXT("/Game/FestaFiesta/UI/Fonts/F_UI_Pretendard.F_UI_Pretendard"));
	auto MakeText = [this, FontObject](const TCHAR* Name, int32 Size, FLinearColor Color)
	{
		UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), FName(Name));
		FSlateFontInfo Font = Text->GetFont();
		if (FontObject) { Font.FontObject = FontObject; }
		Font.Size = Size;
		Font.TypefaceFontName = TEXT("Medium");
		Text->SetFont(Font);
		Text->SetColorAndOpacity(Color);
		Text->SetAutoWrapText(true);
		Text->SetWrappingPolicy(ETextWrappingPolicy::AllowPerCharacterWrapping);
		Text->SetVisibility(ESlateVisibility::HitTestInvisible);
		return Text;
	};
	UBorder* Root = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Border_GoogleAccountPanel"));
	Root->SetBrush(FFFrontendCasual::Rounded(FLinearColor::White, Colors.Line, 14.f));
	Root->SetBrushColor(FLinearColor::White);
	Root->SetPadding(FMargin(20.f, 18.f));
	Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	WidgetTree->RootWidget = Root;
	UVerticalBox* Content = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("VerticalBox_GoogleAccountPanel"));
	Root->SetContent(Content);
	UTextBlock* Heading = MakeText(TEXT("Text_GoogleAccountHeading"), 20, Colors.Ink);
	Heading->SetText(LOCTEXT("Heading", "Google 계정"));
	Content->AddChildToVerticalBox(Heading);
	StateValue = MakeText(TEXT("Text_GoogleAccountState"), 15, Colors.InkMuted);
	Content->AddChildToVerticalBox(StateValue)->SetPadding(FMargin(0.f, 6.f, 0.f, 14.f));
	auto AddProfileRow = [&](const TCHAR* RowName, const TCHAR* ValueName, const FText& Label)
	{
		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), FName(RowName));
		Content->AddChildToVerticalBox(Row)->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));
		USizeBox* LabelWidth = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		LabelWidth->SetWidthOverride(80.f);
		UTextBlock* LabelText = MakeText(*(FString(RowName) + TEXT("Label")), 15, Colors.InkMuted);
		LabelText->SetText(Label);
		LabelWidth->SetContent(LabelText);
		Row->AddChildToHorizontalBox(LabelWidth)->SetVerticalAlignment(VAlign_Top);
		UTextBlock* Value = MakeText(ValueName, 17, Colors.Ink);
		UHorizontalBoxSlot* ValueSlot = Row->AddChildToHorizontalBox(Value);
		ValueSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		ValueSlot->SetVerticalAlignment(VAlign_Top);
		return Value;
	};
	NameValue = AddProfileRow(TEXT("Row_GoogleAccountName"), TEXT("Text_GoogleAccountName"), LOCTEXT("NameLabel", "이름"));
	EmailValue = AddProfileRow(TEXT("Row_GoogleAccountEmail"), TEXT("Text_GoogleAccountEmail"), LOCTEXT("EmailLabel", "이메일"));
	StatusMessage = MakeText(TEXT("Text_GoogleAccountStatus"), 14, Colors.InkMuted);
	Content->AddChildToVerticalBox(StatusMessage)->SetPadding(FMargin(0.f, 4.f, 0.f, 16.f));

	AccountAction = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("Button_GoogleAccount"));
	AccountAction->SetBackgroundColor(FLinearColor::White);
	AccountAction->SetPressMethod(EButtonPressMethod::DownAndUp);
	SignInTexture = FImageUtils::ImportFileAsTexture2D(
		FPaths::Combine(FPaths::ProjectContentDir(), TEXT("GoogleLogin/google-signin-light.png")));
	USizeBox* Frame = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("SizeBox_GoogleAccountAction"));
	Frame->SetWidthOverride(SignInTexture && SignInTexture->GetSizeY() > 0
		? 40.f * SignInTexture->GetSizeX() / SignInTexture->GetSizeY() : 180.f);
	Frame->SetHeightOverride(40.f);
	AccountAction->SetContent(Frame);
	UOverlay* ActionContent = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("Overlay_GoogleAccountAction"));
	Frame->SetContent(ActionContent);
	UScaleBox* Scale = WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass(), TEXT("ScaleBox_GoogleSignIn"));
	Scale->SetStretch(EStretch::ScaleToFit);
	Scale->SetStretchDirection(EStretchDirection::DownOnly);
	SignInArtwork = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Image_GoogleSignIn"));
	SignInArtwork->SetBrushFromTexture(SignInTexture, true);
	SignInArtwork->SetColorAndOpacity(FLinearColor::White);
	SignInArtwork->SetVisibility(ESlateVisibility::HitTestInvisible);
	Scale->SetContent(SignInArtwork);
	ActionContent->AddChildToOverlay(Scale);
	ActionLabel = MakeText(TEXT("Text_GoogleAccountAction"), 15, Colors.Ink);
	ActionLabel->SetJustification(ETextJustify::Center);
	UOverlaySlot* LabelPlacement = ActionContent->AddChildToOverlay(ActionLabel);
	LabelPlacement->SetHorizontalAlignment(HAlign_Center);
	LabelPlacement->SetVerticalAlignment(VAlign_Center);
	Content->AddChildToVerticalBox(AccountAction)->SetHorizontalAlignment(HAlign_Center);
	UTextBlock* Optional = MakeText(TEXT("Text_GoogleAccountOptional"), 12, Colors.InkMuted);
	Optional->SetText(LOCTEXT("OptionalLogin", "로그인하지 않아도 플레이할 수 있습니다."));
	Content->AddChildToVerticalBox(Optional)->SetPadding(FMargin(0.f, 12.f, 0.f, 0.f));
	RefreshAccount();
}

void UFF_GoogleAccountPanel::NativeConstruct()
{
	Super::NativeConstruct();
	bConstructed = true;
	if (!bHasExplicitSession)
	{
		if (UGameInstance* Game = GetGameInstance()) { Auth = Game->GetSubsystem<UFF_GoogleAuthSubsystem>(); }
	}
	if (AccountAction)
	{
		AccountAction->OnClicked.AddUniqueDynamic(this, &UFF_GoogleAccountPanel::HandleAccountAction);
		AccountAction->OnHovered.AddUniqueDynamic(this, &UFF_GoogleAccountPanel::RefreshActionStyle);
		AccountAction->OnUnhovered.AddUniqueDynamic(this, &UFF_GoogleAccountPanel::RefreshActionStyle);
		AccountAction->OnReceivedFocus.BindUObject(this, &UFF_GoogleAccountPanel::RefreshActionStyle);
		AccountAction->OnLostFocus.BindUObject(this, &UFF_GoogleAccountPanel::RefreshActionStyle);
	}
	BindSession();
	RefreshAccount();
}

void UFF_GoogleAccountPanel::NativeDestruct()
{
	UnbindSession();
	if (AccountAction)
	{
		AccountAction->OnClicked.RemoveDynamic(this, &UFF_GoogleAccountPanel::HandleAccountAction);
		AccountAction->OnHovered.RemoveDynamic(this, &UFF_GoogleAccountPanel::RefreshActionStyle);
		AccountAction->OnUnhovered.RemoveDynamic(this, &UFF_GoogleAccountPanel::RefreshActionStyle);
		AccountAction->OnReceivedFocus.Unbind();
		AccountAction->OnLostFocus.Unbind();
	}
	bConstructed = false;
	Super::NativeDestruct();
}

void UFF_GoogleAccountPanel::SetAuthSubsystem(UFF_GoogleAuthSubsystem* InAuth)
{
	UnbindSession();
	bHasExplicitSession = true;
	Auth = InAuth;
	if (bConstructed) { BindSession(); }
	RefreshAccount();
}

void UFF_GoogleAccountPanel::BindSession()
{
	if (Auth) { Auth->OnAuthStateChanged.AddUniqueDynamic(this, &UFF_GoogleAccountPanel::RefreshAccount); }
}

void UFF_GoogleAccountPanel::UnbindSession()
{
	if (Auth) { Auth->OnAuthStateChanged.RemoveDynamic(this, &UFF_GoogleAccountPanel::RefreshAccount); }
}

void UFF_GoogleAccountPanel::RefreshAccount()
{
	if (!NameValue || !EmailValue || !StateValue || !StatusMessage || !AccountAction || !ActionLabel) { return; }
	const bool bSignedIn = Auth && Auth->GetState() == EFF_GoogleAuthState::SignedIn;
	const bool bBusy = Auth && Auth->IsBusy();
	const FFF_GoogleAccount Account = bSignedIn ? Auth->GetAccount() : FFF_GoogleAccount();
	NameValue->SetText(bSignedIn && !Account.DisplayName.IsEmpty()
		? FText::FromString(Account.DisplayName) : LOCTEXT("NoName", "로그인 후 표시됩니다"));
	EmailValue->SetText(bSignedIn && !Account.Email.IsEmpty()
		? FText::FromString(Account.Email) : LOCTEXT("NoEmail", "로그인 후 표시됩니다"));
	StateValue->SetText(bSignedIn ? LOCTEXT("SignedIn", "Google 계정으로 로그인됨")
		: bBusy ? LOCTEXT("SigningIn", "Google 로그인 진행 중")
		: LOCTEXT("SignedOut", "로그인하지 않음"));
	const FText Message = Auth ? Auth->GetStatusMessage() : FText::GetEmpty();
	StatusMessage->SetText(!Message.IsEmpty() ? Message : LOCTEXT("Unavailable", "Google 로그인 상태를 확인할 수 없습니다."));
	ActionLabel->SetText(bSignedIn ? LOCTEXT("SignOut", "로그아웃")
		: bBusy ? LOCTEXT("Cancel", "로그인 취소") : LOCTEXT("SignIn", "Google로 로그인"));
	const bool bUseArtwork = !bSignedIn && !bBusy && SignInTexture;
	SignInArtwork->SetVisibility(bUseArtwork ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	ActionLabel->SetVisibility(bUseArtwork ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	AccountAction->SetIsEnabled(Auth && (bSignedIn || bBusy || Auth->IsConfigured()));
	AccountAction->SetToolTipText(bSignedIn ? LOCTEXT("SignOutHint", "이 게임에서 Google 계정을 로그아웃합니다.")
		: bBusy ? LOCTEXT("CancelHint", "진행 중인 로그인을 취소합니다.")
		: LOCTEXT("SignInHint", "기본 브라우저에서 Google 계정을 선택합니다."));
#if WITH_ACCESSIBILITY
	if (const TSharedPtr<SWidget> Slate = AccountAction->GetCachedWidget())
	{
		Slate->SetAccessibleBehavior(EAccessibleBehavior::Custom,
			TAttribute<FText>(ActionLabel->GetText()), EAccessibleType::Main);
	}
#endif
	RefreshActionStyle();
}

UWidget* UFF_GoogleAccountPanel::GetPrimaryFocusTarget() const
{
	return AccountAction && AccountAction->GetIsEnabled() ? AccountAction.Get() : nullptr;
}

void UFF_GoogleAccountPanel::HandleAccountAction()
{
	if (!Auth) { return; }
	if (Auth->IsBusy()) { Auth->CancelSignIn(); }
	else if (Auth->GetState() == EFF_GoogleAuthState::SignedIn) { Auth->SignOut(); }
	else { Auth->SignIn(); }
}

void UFF_GoogleAccountPanel::RefreshActionStyle()
{
	if (!AccountAction) { return; }
	const FFFrontendCasual::FTokens Colors = FFFrontendCasual::Get();
	const FLinearColor Focus = FFFrontendCasual::Hex(TEXT("0B57D0"));
	const bool bFocused = AccountAction->HasKeyboardFocus()
		|| (GetOwningPlayer() && AccountAction->HasUserFocus(GetOwningPlayer()));
	FButtonStyle Style = AccountAction->GetStyle();
	Style.Normal = FFFrontendCasual::Rounded(Colors.Paper, bFocused ? Focus : Colors.Line, 6.f, bFocused ? 2.f : 1.f);
	Style.Hovered = FFFrontendCasual::Rounded(Colors.Paper, Focus, 6.f, 2.f);
	Style.Pressed = FFFrontendCasual::Rounded(Colors.Paper, Focus, 6.f, 2.f);
	Style.Disabled = Style.Normal;
	if (SignInTexture && SignInArtwork && SignInArtwork->IsVisible())
	{
		Style.Normal = bFocused ? FFFrontendCasual::Rounded(FLinearColor::Transparent, Focus, 6.f, 2.f) : FSlateBrush();
		if (!bFocused) { Style.Normal.DrawAs = ESlateBrushDrawType::NoDrawType; }
		Style.Hovered = Style.Normal;
		Style.Pressed = Style.Normal;
		Style.Disabled = Style.Normal;
	}
	Style.NormalPadding = FMargin(0.f);
	Style.PressedPadding = Style.NormalPadding;
	AccountAction->SetStyle(Style);
}

#undef LOCTEXT_NAMESPACE
