// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Title/FF_TitleScreen.h"

#include "Authentication/FF_GoogleAuthSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
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
#include "InputCoreTypes.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/Paths.h"
#include "UI/FF_CommonButtonBase.h"
#include "UI/FF_UILog.h"
#include "UI/Frontend/FF_FrontendCasualStyle.h"
#include "UI/Account/FF_GoogleAccountPanel.h"

#define LOCTEXT_NAMESPACE "FFTitleGoogleAccount"

void UFF_TitleScreen::NativeConstruct()
{
	Super::NativeConstruct();

	// 🔴 구독은 `NativeConstruct` 다 — `NativeOnInitialized` 가 아니다.
	// CommonUI 스택이 위젯을 풀에서 재사용하므로, 인스턴스당 한 번인 `NativeOnInitialized` 에
	// 구독하고 화면이 내려갈 때마다 도는 `NativeDestruct` 에서 해제하면
	// **뒤로 갔다 다시 들어온 화면의 버튼이 죽는다.** 구독과 해제는 수명이 같은 짝이어야 한다.
	//
	// AddUniqueDynamic만 쓴다. AddDynamic은 중복 구독을 막아주지 않는다.
	if (Btn_Play)
	{
		Btn_Play->OnFFButtonClicked.AddUniqueDynamic(this, &UFF_TitleScreen::HandleButtonClicked);
	}
	if (Btn_Options)
	{
		Btn_Options->OnFFButtonClicked.AddUniqueDynamic(this, &UFF_TitleScreen::HandleButtonClicked);
	}
	if (Btn_Quit)
	{
		Btn_Quit->OnFFButtonClicked.AddUniqueDynamic(this, &UFF_TitleScreen::HandleButtonClicked);
	}

	EnsureGoogleAccountPanel();
	if (GoogleAccountButton)
	{
		GoogleAccountButton->OnClicked.AddUniqueDynamic(this, &UFF_TitleScreen::HandleGoogleAccountClicked);
		GoogleAccountButton->OnHovered.AddUniqueDynamic(this, &UFF_TitleScreen::RefreshGoogleAccountButtonStyle);
		GoogleAccountButton->OnUnhovered.AddUniqueDynamic(this, &UFF_TitleScreen::RefreshGoogleAccountButtonStyle);
		GoogleAccountButton->OnReceivedFocus.BindUObject(this, &UFF_TitleScreen::RefreshGoogleAccountButtonStyle);
		GoogleAccountButton->OnLostFocus.BindUObject(this, &UFF_TitleScreen::RefreshGoogleAccountButtonStyle);
	}
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		GoogleAuth = GameInstance->GetSubsystem<UFF_GoogleAuthSubsystem>();
	}
	if (GoogleAuth)
	{
		GoogleAuth->OnAuthStateChanged.AddUniqueDynamic(this, &UFF_TitleScreen::RefreshGoogleAccount);
	}
	RefreshGoogleAccount();
	EnsureGoogleSignInPrompt();
	if (GooglePromptAccount) { GooglePromptAccount->SetAuthSubsystem(GoogleAuth); }
	if (GoogleContinueOffline)
	{
		GoogleContinueOffline->OnClicked.AddUniqueDynamic(this, &UFF_TitleScreen::DismissGoogleSignInPrompt);
	}
	const bool bPresentInitialPrompt = GoogleAuth
		? GoogleAuth->ConsumeInitialSignInPrompt() : !bInitialGooglePromptHandled;
	bInitialGooglePromptHandled = true;
	if (bPresentInitialPrompt) { ShowGoogleSignInPrompt(); }
}

void UFF_TitleScreen::NativeOnInitialized()
{
	Super::NativeOnInitialized();
}

void UFF_TitleScreen::NativeDestruct()
{
	// A disappearing title must not leave pooled menu controls disabled. The auth attempt remains GI-owned.
	bGooglePromptVisible = false;
	if (GooglePromptRoot) { GooglePromptRoot->SetVisibility(ESlateVisibility::Collapsed); }
	SetTitleMenuEnabled(true);
	if (GoogleContinueOffline)
	{
		GoogleContinueOffline->OnClicked.RemoveDynamic(this, &UFF_TitleScreen::DismissGoogleSignInPrompt);
	}
	// 해제는 조건부, 정리는 무조건. 조기 return으로 빠져나가면 아래 해제를 못 한다.
	if (Btn_Play)
	{
		Btn_Play->OnFFButtonClicked.RemoveDynamic(this, &UFF_TitleScreen::HandleButtonClicked);
	}
	if (Btn_Options)
	{
		Btn_Options->OnFFButtonClicked.RemoveDynamic(this, &UFF_TitleScreen::HandleButtonClicked);
	}
	if (Btn_Quit)
	{
		Btn_Quit->OnFFButtonClicked.RemoveDynamic(this, &UFF_TitleScreen::HandleButtonClicked);
	}
	if (GoogleAccountButton)
	{
		GoogleAccountButton->OnClicked.RemoveDynamic(this, &UFF_TitleScreen::HandleGoogleAccountClicked);
		GoogleAccountButton->OnHovered.RemoveDynamic(this, &UFF_TitleScreen::RefreshGoogleAccountButtonStyle);
		GoogleAccountButton->OnUnhovered.RemoveDynamic(this, &UFF_TitleScreen::RefreshGoogleAccountButtonStyle);
		GoogleAccountButton->OnReceivedFocus.Unbind();
		GoogleAccountButton->OnLostFocus.Unbind();
	}
	if (GoogleAuth)
	{
		GoogleAuth->OnAuthStateChanged.RemoveDynamic(this, &UFF_TitleScreen::RefreshGoogleAccount);
	}
	// The GameInstance owns the sign-in attempt; leaving this screen must not cancel it.
	GoogleAuth = nullptr;

	Super::NativeDestruct();
}

UWidget* UFF_TitleScreen::NativeGetDesiredFocusTarget() const
{
	if (bGooglePromptVisible)
	{
		if (GooglePromptAccount)
		{
			if (UWidget* Action = GooglePromptAccount->GetPrimaryFocusTarget()) { return Action; }
		}
		return GoogleContinueOffline;
	}
	if (Btn_Play)
	{
		return Btn_Play;
	}

	UE_LOG(LogFestaFiestaUI, Warning,
		TEXT("[%s] Btn_Play가 없어 초기 포커스를 지정하지 못했다. 게임패드로 아무것도 선택할 수 없는 상태가 된다."),
		*GetName());

	return Super::NativeGetDesiredFocusTarget();
}

void UFF_TitleScreen::HandleButtonClicked(UFF_CommonButtonBase* Button)
{
	if (bGooglePromptVisible) { return; }
	if (!Button)
	{
		UE_LOG(LogFestaFiestaUI, Warning, TEXT("[%s] 클릭 알림에 버튼이 없다."), *GetName());
		return;
	}

	if (Button == Btn_Play)
	{
		UE_LOG(LogFestaFiestaUI, Log, TEXT("[%s] 플레이 요청."), *GetName());
		// TODO: 플레이 허브 화면이 만들어지면 UFF_UIRootLayout::PushScreen으로 교체한다.
		//
		// 둘 다 쏜다. BP 이벤트는 이 WBP 안에서 쓰는 기존 경로이고,
		// 델리게이트는 화면을 띄운 쪽(레벨 BP·PlayerController)이 구독하는 경로다.
		BP_OnPlayRequested();
		OnPlayRequested.Broadcast();
		return;
	}

	if (Button == Btn_Options)
	{
		UE_LOG(LogFestaFiestaUI, Log, TEXT("[%s] 옵션 요청."), *GetName());
		// TODO: 옵션 화면이 만들어지면 UFF_UIRootLayout::PushScreen으로 교체한다.
		BP_OnOptionsRequested();
		OnOptionsRequested.Broadcast();
		return;
	}

	if (Button == Btn_Quit)
	{
		UE_LOG(LogFestaFiestaUI, Log, TEXT("[%s] 게임 종료 요청."), *GetName());

		// TODO: 확인 다이얼로그를 PanelStack에 띄운 뒤 종료하도록 바꾼다.
		APlayerController* OwningPC = UGameplayStatics::GetPlayerController(this, 0);
		UKismetSystemLibrary::QuitGame(this, OwningPC, EQuitPreference::Quit, false);
		return;
	}

	// 어느 버튼도 아니면 조용히 넘어가지 않고 남긴다. 버튼을 추가하고 처리를 빠뜨린 경우다.
	UE_LOG(LogFestaFiestaUI, Warning,
		TEXT("[%s] 처리되지 않은 버튼 클릭: %s"), *GetName(), *Button->GetName());
}

void UFF_TitleScreen::EnsureGoogleAccountPanel()
{
	if (GoogleAccountPanel || !WidgetTree)
	{
		return;
	}
	UOverlay* Root = Cast<UOverlay>(WidgetTree->FindWidget(TEXT("Overlay_Root")));
	if (!Root)
	{
		UE_LOG(LogFestaFiestaUI, Warning,
			TEXT("[%s] Overlay_Root가 없어 Google 계정 패널을 표시할 수 없다."), *GetName());
		return;
	}

	const FFFrontendCasual::FTokens Colors = FFFrontendCasual::Get();
	UFont* UiFont = LoadObject<UFont>(nullptr,
		TEXT("/Game/FestaFiesta/UI/Fonts/F_UI_Pretendard.F_UI_Pretendard"));
	auto MakeText = [this, UiFont](const TCHAR* Name, int32 FontSize, const FLinearColor& Color)
	{
		UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), FName(Name));
		FSlateFontInfo Font = Text->GetFont();
		if (UiFont)
		{
			Font.FontObject = UiFont;
		}
		Font.Size = FontSize;
		Font.TypefaceFontName = TEXT("Medium");
		Text->SetFont(Font);
		Text->SetColorAndOpacity(Color);
		Text->SetVisibility(ESlateVisibility::HitTestInvisible);
		return Text;
	};

	GoogleAccountPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Border_GoogleAccount"));
	GoogleAccountPanel->SetBrush(FFFrontendCasual::Rounded(Colors.Paper, Colors.Line, 16.f));
	GoogleAccountPanel->SetBrushColor(FLinearColor::White);
	GoogleAccountPanel->SetPadding(FMargin(18.f, 16.f));
	GoogleAccountPanel->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

	USizeBox* Width = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("SizeBox_GoogleAccount"));
	Width->SetWidthOverride(292.f);
	GoogleAccountPanel->SetContent(Width);
	UVerticalBox* Content = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("VerticalBox_GoogleAccount"));
	Width->SetContent(Content);

	UTextBlock* Heading = MakeText(TEXT("Text_GoogleAccountHeading"), 13, Colors.InkMuted);
	Heading->SetText(LOCTEXT("Heading", "Google 계정"));
	Content->AddChildToVerticalBox(Heading);

	GoogleAccountProfile = MakeText(TEXT("Text_GoogleAccountProfile"), 17, Colors.Ink);
	GoogleAccountProfile->SetAutoWrapText(true);
	GoogleAccountProfile->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
	Content->AddChildToVerticalBox(GoogleAccountProfile)->SetPadding(FMargin(0.f, 7.f, 0.f, 0.f));

	GoogleAccountStatus = MakeText(TEXT("Text_GoogleAccountStatus"), 13, Colors.InkMuted);
	GoogleAccountStatus->SetAutoWrapText(true);
	Content->AddChildToVerticalBox(GoogleAccountStatus)->SetPadding(FMargin(0.f, 8.f, 0.f, 12.f));

	GoogleAccountButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("Button_GoogleAccount"));
	GoogleAccountButton->SetBackgroundColor(FLinearColor::White);
	GoogleAccountButton->SetPressMethod(EButtonPressMethod::DownAndUp);
	// Google's complete preapproved artwork preserves the G, font, spacing and border.
	// The PNG is staged as UFS and imported into a transient texture in packaged builds too.
	GoogleSignInTexture = FImageUtils::ImportFileAsTexture2D(
		FPaths::Combine(FPaths::ProjectContentDir(), TEXT("GoogleLogin/google-signin-light.png")));
	USizeBox* ButtonFrame = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("SizeBox_GoogleAccountAction"));
	const float ArtworkWidth = GoogleSignInTexture && GoogleSignInTexture->GetSizeY() > 0
		? 40.f * GoogleSignInTexture->GetSizeX() / GoogleSignInTexture->GetSizeY() : 180.f;
	ButtonFrame->SetWidthOverride(ArtworkWidth);
	ButtonFrame->SetHeightOverride(40.f);
	GoogleAccountButton->SetContent(ButtonFrame);
	UOverlay* ButtonContent = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("Overlay_GoogleAccountAction"));
	ButtonFrame->SetContent(ButtonContent);
	UScaleBox* ArtworkScale = WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass(), TEXT("ScaleBox_GoogleSignIn"));
	ArtworkScale->SetStretch(EStretch::ScaleToFit);
	ArtworkScale->SetStretchDirection(EStretchDirection::DownOnly);
	GoogleSignInImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Image_GoogleSignIn"));
	GoogleSignInImage->SetBrushFromTexture(GoogleSignInTexture, true);
	GoogleSignInImage->SetColorAndOpacity(FLinearColor::White);
	GoogleSignInImage->SetVisibility(ESlateVisibility::HitTestInvisible);
	ArtworkScale->SetContent(GoogleSignInImage);
	UOverlaySlot* ArtworkSlot = ButtonContent->AddChildToOverlay(ArtworkScale);
	ArtworkSlot->SetHorizontalAlignment(HAlign_Fill);
	ArtworkSlot->SetVerticalAlignment(VAlign_Fill);
	GoogleAccountButtonLabel = MakeText(TEXT("Text_GoogleAccountAction"), 15, FFFrontendCasual::Hex(TEXT("1F1F1F")));
	GoogleAccountButtonLabel->SetJustification(ETextJustify::Center);
	UOverlaySlot* LabelSlot = ButtonContent->AddChildToOverlay(GoogleAccountButtonLabel);
	LabelSlot->SetHorizontalAlignment(HAlign_Center);
	LabelSlot->SetVerticalAlignment(VAlign_Center);
	Content->AddChildToVerticalBox(GoogleAccountButton)->SetHorizontalAlignment(HAlign_Center);

	UTextBlock* OptionalHint = MakeText(TEXT("Text_GoogleAccountOptional"), 12, Colors.InkMuted);
	OptionalHint->SetText(LOCTEXT("OptionalHint", "로그인하지 않아도 플레이할 수 있습니다."));
	OptionalHint->SetAutoWrapText(true);
	Content->AddChildToVerticalBox(OptionalHint)->SetPadding(FMargin(0.f, 10.f, 0.f, 0.f));

	UOverlaySlot* Placement = Root->AddChildToOverlay(GoogleAccountPanel);
	Placement->SetHorizontalAlignment(HAlign_Right);
	Placement->SetVerticalAlignment(VAlign_Top);
	Placement->SetPadding(FMargin(24.f));
	RefreshGoogleAccountButtonStyle();
}

void UFF_TitleScreen::RefreshGoogleAccount()
{
	if (!GoogleAccountPanel || !GoogleAccountButton || !GoogleAccountButtonLabel)
	{
		return;
	}
	const bool bSignedIn = GoogleAuth && GoogleAuth->GetState() == EFF_GoogleAuthState::SignedIn;
	const bool bBusy = GoogleAuth && GoogleAuth->IsBusy();
	GoogleAccountProfile->SetVisibility(bSignedIn ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (bSignedIn)
	{
		const FFF_GoogleAccount Account = GoogleAuth->GetAccount();
		const FString Name = Account.DisplayName.IsEmpty() ? Account.Email : Account.DisplayName;
		GoogleAccountProfile->SetText(Name.IsEmpty()
			? LOCTEXT("ConnectedAccount", "Google 계정 연결됨")
			: FText::FromString(!Account.Email.IsEmpty() && Account.Email != Name
				? Name + TEXT("\n") + Account.Email : Name));
	}
	GoogleAccountStatus->SetText(GoogleAuth ? GoogleAuth->GetStatusMessage()
		: LOCTEXT("Unavailable", "지금은 Google 로그인을 사용할 수 없습니다."));
	GoogleAccountButtonLabel->SetText(bSignedIn ? LOCTEXT("SignOut", "로그아웃")
		: bBusy ? LOCTEXT("CancelSignIn", "로그인 취소") : LOCTEXT("SignIn", "Google로 로그인"));
	const bool bShowSignInArtwork = !bSignedIn && !bBusy && GoogleSignInTexture;
	if (GoogleSignInImage)
	{
		GoogleSignInImage->SetVisibility(bShowSignInArtwork ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	GoogleAccountButtonLabel->SetVisibility(bShowSignInArtwork ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	GoogleAccountButton->SetIsEnabled(GoogleAuth && (bSignedIn || bBusy || GoogleAuth->IsConfigured()));
	GoogleAccountButton->SetToolTipText(bSignedIn ? LOCTEXT("SignOutHint", "이 게임에서 Google 계정 연결을 해제합니다.")
		: bBusy ? LOCTEXT("CancelHint", "진행 중인 Google 로그인을 취소합니다.")
		: LOCTEXT("SignInHint", "기본 브라우저에서 Google 계정을 선택합니다."));
#if WITH_ACCESSIBILITY
	if (const TSharedPtr<SWidget> Slate = GoogleAccountButton->GetCachedWidget())
	{
		// The artwork's text is pixels; supply the Korean action name to assistive technology.
		Slate->SetAccessibleBehavior(EAccessibleBehavior::Custom,
			TAttribute<FText>(GoogleAccountButtonLabel->GetText()), EAccessibleType::Main);
	}
#endif
	RefreshGoogleAccountButtonStyle();
	if (bSignedIn && bGooglePromptVisible) { DismissGoogleSignInPrompt(); }
}

void UFF_TitleScreen::HandleGoogleAccountClicked()
{
	if (!GoogleAuth)
	{
		return;
	}
	if (GoogleAuth->IsBusy())
	{
		GoogleAuth->CancelSignIn();
	}
	else if (GoogleAuth->GetState() == EFF_GoogleAuthState::SignedIn)
	{
		GoogleAuth->SignOut();
	}
	else
	{
		GoogleAuth->SignIn();
	}
}

void UFF_TitleScreen::RefreshGoogleAccountButtonStyle()
{
	if (!GoogleAccountButton)
	{
		return;
	}
	const FLinearColor Border = FFFrontendCasual::Hex(TEXT("747775"));
	const FLinearColor FocusBorder = FFFrontendCasual::Hex(TEXT("0B57D0"));
	const bool bFocused = GoogleAccountButton->HasKeyboardFocus() || GoogleAccountButton->HasUserFocus(GetOwningPlayer());
	FButtonStyle Style = GoogleAccountButton->GetStyle();
	Style.Normal = FFFrontendCasual::Rounded(FLinearColor::White, bFocused ? FocusBorder : Border, 6.f, bFocused ? 2.f : 1.f);
	Style.Hovered = FFFrontendCasual::Rounded(FFFrontendCasual::Hex(TEXT("F8FAFD")), FocusBorder, 6.f, 2.f);
	Style.Pressed = FFFrontendCasual::Rounded(FFFrontendCasual::Hex(TEXT("E8F0FE")), FocusBorder, 6.f, 2.f);
	Style.Disabled = FFFrontendCasual::Rounded(FFFrontendCasual::Hex(TEXT("F1F3F4")), Border, 6.f);
	if (GoogleSignInTexture && GoogleSignInImage && GoogleSignInImage->IsVisible())
	{
		// Do not paint a second button, crop, or tint the official full-button artwork.
		Style.Normal = bFocused ? FFFrontendCasual::Rounded(FLinearColor::Transparent, FocusBorder, 6.f, 2.f) : FSlateBrush();
		if (!bFocused) { Style.Normal.DrawAs = ESlateBrushDrawType::NoDrawType; }
		Style.Hovered = Style.Normal;
		Style.Pressed = Style.Normal;
		Style.Disabled = Style.Normal;
	}
	Style.NormalPadding = FMargin(0.f);
	Style.PressedPadding = Style.NormalPadding;
	GoogleAccountButton->SetStyle(Style);
}

void UFF_TitleScreen::NativeOnActivated()
{
	Super::NativeOnActivated();
	FocusAccountOrPlay();
}

FReply UFF_TitleScreen::NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (bGooglePromptVisible && InKeyEvent.GetKey() == EKeys::Escape)
	{
		DismissGoogleSignInPrompt();
		return FReply::Handled();
	}
	return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
}

void UFF_TitleScreen::EnsureGoogleSignInPrompt()
{
	if (GooglePromptRoot || !WidgetTree) { return; }
	UOverlay* Root = Cast<UOverlay>(WidgetTree->FindWidget(TEXT("Overlay_Root")));
	if (!Root) { return; }
	const FFFrontendCasual::FTokens Colors = FFFrontendCasual::Get();
	UFont* FontObject = LoadObject<UFont>(nullptr,
		TEXT("/Game/FestaFiesta/UI/Fonts/F_UI_Pretendard.F_UI_Pretendard"));
	auto MakeText = [this, FontObject](const TCHAR* Name, const FText& Value, int32 Size)
	{
		UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), FName(Name));
		FSlateFontInfo Font = Text->GetFont();
		if (FontObject) { Font.FontObject = FontObject; }
		Font.Size = Size;
		Font.TypefaceFontName = TEXT("Medium");
		Text->SetFont(Font);
		Text->SetText(Value);
		Text->SetAutoWrapText(true);
		Text->SetColorAndOpacity(FFFrontendCasual::Get().Ink);
		Text->SetVisibility(ESlateVisibility::HitTestInvisible);
		return Text;
	};
	GooglePromptRoot = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Border_GoogleSignInPrompt"));
	GooglePromptRoot->SetBrush(FFFrontendCasual::Rounded(
		FLinearColor(0.025f, 0.09f, 0.065f, 0.42f), FLinearColor::Transparent, 0.f));
	GooglePromptRoot->SetBrushColor(FLinearColor::White);
	GooglePromptRoot->SetPadding(FMargin(24.f));
	GooglePromptRoot->SetHorizontalAlignment(HAlign_Center);
	GooglePromptRoot->SetVerticalAlignment(VAlign_Center);
	GooglePromptRoot->SetVisibility(ESlateVisibility::Collapsed);
	USizeBox* Width = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("SizeBox_GoogleSignInDialog"));
	Width->SetWidthOverride(480.f);
	GooglePromptRoot->SetContent(Width);
	UBorder* Card = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Border_GoogleSignInDialog"));
	Card->SetBrush(FFFrontendCasual::Rounded(Colors.Paper, Colors.Line, 22.f));
	Card->SetBrushColor(FLinearColor::White);
	Card->SetPadding(FMargin(24.f));
	Card->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	Width->SetContent(Card);
	UVerticalBox* Content = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("VerticalBox_GoogleSignInDialog"));
	Card->SetContent(Content);
	Content->AddChildToVerticalBox(MakeText(TEXT("Text_GoogleSignInWelcome"),
		LOCTEXT("Welcome", "Google 계정으로 시작하기"), 24));
	UTextBlock* Intro = MakeText(TEXT("Text_GoogleSignInIntro"),
		LOCTEXT("PromptIntro", "Google 계정을 연결하면 이름과 이메일을 게임에서 확인할 수 있습니다."), 15);
	Intro->SetColorAndOpacity(Colors.InkMuted);
	Content->AddChildToVerticalBox(Intro)->SetPadding(FMargin(0.f, 8.f, 0.f, 16.f));
	GooglePromptAccount = CreateWidget<UFF_GoogleAccountPanel>(this, UFF_GoogleAccountPanel::StaticClass(), TEXT("GoogleAccountPromptPanel"));
	if (GooglePromptAccount) { Content->AddChildToVerticalBox(GooglePromptAccount); }
	GoogleContinueOffline = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("Button_GoogleContinueOffline"));
	FButtonStyle ButtonStyle = GoogleContinueOffline->GetStyle();
	ButtonStyle.Normal = FFFrontendCasual::Rounded(Colors.Paper, Colors.Line, 10.f);
	ButtonStyle.Hovered = FFFrontendCasual::Rounded(Colors.Paper, Colors.Ink, 10.f, 2.f);
	ButtonStyle.Pressed = ButtonStyle.Hovered;
	ButtonStyle.NormalPadding = FMargin(16.f, 12.f);
	ButtonStyle.PressedPadding = ButtonStyle.NormalPadding;
	GoogleContinueOffline->SetStyle(ButtonStyle);
	GoogleContinueOffline->SetContent(MakeText(TEXT("Text_GoogleContinueOffline"), LOCTEXT("ContinueOffline", "로그인 없이 계속"), 15));
	Content->AddChildToVerticalBox(GoogleContinueOffline)->SetPadding(FMargin(0.f, 14.f, 0.f, 0.f));
	UOverlaySlot* Placement = Root->AddChildToOverlay(GooglePromptRoot);
	Placement->SetHorizontalAlignment(HAlign_Fill);
	Placement->SetVerticalAlignment(VAlign_Fill);
}

void UFF_TitleScreen::ShowGoogleSignInPrompt()
{
	if (GoogleAuth && GoogleAuth->GetState() == EFF_GoogleAuthState::SignedIn) { return; }
	EnsureGoogleSignInPrompt();
	if (!GooglePromptRoot) { return; }
	bGooglePromptVisible = true;
	GooglePromptRoot->SetVisibility(ESlateVisibility::Visible);
	SetTitleMenuEnabled(false);
	if (GooglePromptAccount)
	{
		GooglePromptAccount->SetAuthSubsystem(GoogleAuth);
		GooglePromptAccount->RefreshAccount();
	}
	if (GoogleContinueOffline)
	{
		UWidget* Primary = GooglePromptAccount ? GooglePromptAccount->GetPrimaryFocusTarget() : nullptr;
		UWidget* Other = Primary ? Primary : GoogleContinueOffline.Get();
		for (const EUINavigation Direction : { EUINavigation::Next, EUINavigation::Previous, EUINavigation::Up, EUINavigation::Down })
		{
			GoogleContinueOffline->SetNavigationRuleExplicit(Direction, Other);
			if (Primary) { Primary->SetNavigationRuleExplicit(Direction, GoogleContinueOffline); }
		}
	}
	FocusAccountOrPlay();
}

void UFF_TitleScreen::DismissGoogleSignInPrompt()
{
	bGooglePromptVisible = false;
	if (GooglePromptRoot) { GooglePromptRoot->SetVisibility(ESlateVisibility::Collapsed); }
	SetTitleMenuEnabled(true);
	FocusAccountOrPlay();
}

void UFF_TitleScreen::SetTitleMenuEnabled(const bool bEnabled)
{
	if (Btn_Play) { Btn_Play->SetIsEnabled(bEnabled); }
	if (Btn_Options) { Btn_Options->SetIsEnabled(bEnabled); }
	if (Btn_Quit) { Btn_Quit->SetIsEnabled(bEnabled); }
	if (GoogleAccountPanel)
	{
		GoogleAccountPanel->SetVisibility(bEnabled ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void UFF_TitleScreen::FocusAccountOrPlay()
{
	APlayerController* Player = GetOwningPlayer();
	if (!Player || !Player->GetLocalPlayer()) { return; }
	if (UWidget* Focus = NativeGetDesiredFocusTarget()) { Focus->SetUserFocus(Player); }
}

#undef LOCTEXT_NAMESPACE
