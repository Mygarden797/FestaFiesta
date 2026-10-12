#include "UI/LiveHUD/Panels/FF_LiveSnsPostEntry.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Framework/Application/SlateApplication.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "Layout/Geometry.h"
#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"
#endif
#define LOCTEXT_NAMESPACE "FestaFiestaLiveSnsEntry"

/** 기존 WBP의 내용과 여백을 그대로 감싸는 입력 전용 버튼. */
class SFF_LiveSnsReadButton : public SButton
{
public:
	SLATE_BEGIN_ARGS(SFF_LiveSnsReadButton) {}
		SLATE_DEFAULT_SLOT(FArguments, Content)
		SLATE_EVENT(FSimpleDelegate, OnPressed)
		SLATE_EVENT(FOnClicked, OnClicked)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		SButton::Construct(SButton::FArguments()
			.ButtonStyle(&FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("NoBorder"))
			.ContentPadding(FMargin(0.f))
			.NormalPaddingOverride(FMargin(0.f))
			.PressedPaddingOverride(FMargin(0.f))
			.ForegroundColor(FSlateColor::UseForeground())
			.ClickMethod(EButtonClickMethod::PreciseClick)
			.TouchMethod(EButtonTouchMethod::PreciseTap)
			.PressMethod(EButtonPressMethod::DownAndUp)
			.OnPressed(InArgs._OnPressed)
			.OnClicked(InArgs._OnClicked)
			[InArgs._Content.Widget]);
	}

	void SetReadInteractionEnabled(bool bEnabled)
	{
		bReadInteractionEnabled = bEnabled;
		SetIsFocusable(bEnabled);
		if (!bEnabled) { Release(); }
		// 읽음 기능을 쓰지 않는 기존 SNS 행도 색과 내용은 원래대로 표시한다.
		SetVisibility(bEnabled ? EVisibility::Visible : EVisibility::SelfHitTestInvisible);
	}

	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		return bReadInteractionEnabled ? SButton::OnMouseButtonDown(MyGeometry, MouseEvent) : FReply::Unhandled();
	}

	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		return bReadInteractionEnabled ? SButton::OnMouseButtonUp(MyGeometry, MouseEvent) : FReply::Unhandled();
	}

	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override
	{
		return bReadInteractionEnabled ? SButton::OnKeyDown(MyGeometry, InKeyEvent) : FReply::Unhandled();
	}

	virtual FReply OnKeyUp(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override
	{
		return bReadInteractionEnabled ? SButton::OnKeyUp(MyGeometry, InKeyEvent) : FReply::Unhandled();
	}

private:
	bool bReadInteractionEnabled = false;
};

UFF_LiveSnsPostEntry::UFF_LiveSnsPostEntry(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SentimentLabels = {
		{EFF_LiveSnsSentiment::Positive, LOCTEXT("Positive", "긍정")},
		{EFF_LiveSnsSentiment::Negative, LOCTEXT("Negative", "아쉬움")},
		{EFF_LiveSnsSentiment::Neutral, LOCTEXT("Neutral", "중립")},
		{EFF_LiveSnsSentiment::Unknown, LOCTEXT("Unknown", "반응")}};
	SentimentSymbols = {
		{EFF_LiveSnsSentiment::Positive, LOCTEXT("PositiveSymbol", "+")},
		{EFF_LiveSnsSentiment::Negative, LOCTEXT("NegativeSymbol", "!")},
		{EFF_LiveSnsSentiment::Neutral, LOCTEXT("NeutralSymbol", "·")},
		{EFF_LiveSnsSentiment::Unknown, LOCTEXT("UnknownSymbol", "·")}};
	SentimentBackgroundColors = {
		{EFF_LiveSnsSentiment::Positive, FLinearColor::FromSRGBColor(FColor(229, 245, 233))},
		{EFF_LiveSnsSentiment::Negative, FLinearColor::FromSRGBColor(FColor(255, 243, 218))},
		{EFF_LiveSnsSentiment::Neutral, FLinearColor::FromSRGBColor(FColor(231, 244, 250))},
		{EFF_LiveSnsSentiment::Unknown, FLinearColor::FromSRGBColor(FColor(238, 242, 243))}};
}

TSharedRef<SWidget> UFF_LiveSnsPostEntry::RebuildWidget()
{
	bReadPressValid = false;
	bReactionPressValid = false;
	const TSharedRef<SWidget> Content = Super::RebuildWidget();
	if (IsDesignTime()) { return Content; }
	SAssignNew(ReadButton, SFF_LiveSnsReadButton)
		.OnPressed(FSimpleDelegate::CreateUObject(this, &ThisClass::HandleReadPressed))
		.OnClicked(FOnClicked::CreateUObject(this, &ThisClass::HandleReadClicked))
		[Content];
	ReadButton->SetReadInteractionEnabled(bReadInteractionEnabled);
	return ReadButton.ToSharedRef();
}

void UFF_LiveSnsPostEntry::ReleaseSlateResources(bool bReleaseChildren)
{
	bReadPressValid = false;
	bReactionPressValid = false;
	ReadButton.Reset();
	Super::ReleaseSlateResources(bReleaseChildren);
}

void UFF_LiveSnsPostEntry::NativeDestruct()
{
	bReadPressValid = false;
	bReactionPressValid = false;
	if (Button_React)
	{
		Button_React->OnPressed.RemoveDynamic(this, &ThisClass::HandleReactionPressed);
		Button_React->OnClicked.RemoveDynamic(this, &ThisClass::HandleReactionClicked);
	}
	Super::NativeDestruct();
}

void UFF_LiveSnsPostEntry::NativeConstruct()
{
	Super::NativeConstruct();
	if (Button_React)
	{
		Button_React->SetClickMethod(EButtonClickMethod::PreciseClick);
		Button_React->SetTouchMethod(EButtonTouchMethod::PreciseTap);
		Button_React->SetPressMethod(EButtonPressMethod::DownAndUp);
		Button_React->OnPressed.AddUniqueDynamic(this, &ThisClass::HandleReactionPressed);
		Button_React->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleReactionClicked);
	}
	RefreshReaction();
}

void UFF_LiveSnsPostEntry::ConfigureReadInteraction(int64 InSerial, bool bEnabled)
{
	// 누르는 사이 새 글이 들어와 행이 재사용되면 그 입력 전체를 취소한다.
	if (PostSerial != InSerial || bReadInteractionEnabled != bEnabled)
	{
		bReadPressValid = false;
		bReactionPressValid = false;
	}
	PostSerial = InSerial;
	bReadInteractionEnabled = bEnabled;
	// 기존 표시 전용 WBP에서도 바깥 UserWidget이 내부 버튼의 입력을 막지 않게 한다.
	if (bEnabled && GetVisibility() == ESlateVisibility::HitTestInvisible) { SetVisibility(ESlateVisibility::SelfHitTestInvisible); }
	if (ReadButton) { ReadButton->SetReadInteractionEnabled(bEnabled); }
	RefreshReadToolTip();
}

void UFF_LiveSnsPostEntry::ConfigureReaction(bool bVisible, bool bReacted, bool bCanReact, const FText& Status, const FText& Hint)
{
	if (bReactionVisible != bVisible || bHasReacted != bReacted || bReactionAllowed != bCanReact)
	{
		bReactionPressValid = false;
	}
	bReactionVisible = bVisible;
	bHasReacted = bReacted;
	bReactionAllowed = bCanReact;
	ReactionStatus = Status;
	ReactionHint = Hint;
	RefreshReaction();
}

void UFF_LiveSnsPostEntry::RefreshReaction()
{
	const bool bShow = bReactionVisible && bHasPost && !CurrentPost.bOperationAlert;
	if (!bShow) { bReactionPressValid = false; }
	if (Box_Reaction) { Box_Reaction->SetVisibility(bShow ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed); }
	if (Button_React)
	{
		Button_React->SetVisibility(bShow ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		Button_React->SetIsEnabled(bShow && bReactionAllowed && !bHasReacted);
		Button_React->SetToolTipText(ReactionHint);
	}
	if (Text_ReactLabel)
	{
		Text_ReactLabel->SetText(bHasReacted ? LOCTEXT("Reacted", "♥ 공감함") : LOCTEXT("React", "♡ 공감"));
	}
	if (Text_ReactStatus)
	{
		Text_ReactStatus->SetText(ReactionStatus);
		Text_ReactStatus->SetToolTipText(ReactionHint);
		Text_ReactStatus->SetVisibility(ReactionStatus.IsEmpty() ? ESlateVisibility::Hidden : ESlateVisibility::Visible);
	}
}

void UFF_LiveSnsPostEntry::HandleReactionPressed()
{
	ReactionPressedPostSerial = PostSerial;
	bReactionPressValid = bReactionVisible && bReactionAllowed && !bHasReacted && bHasPost && !CurrentPost.bOperationAlert;
}

void UFF_LiveSnsPostEntry::HandleReactionClicked()
{
	const bool bActivate = bReactionPressValid && ReactionPressedPostSerial == PostSerial
		&& bReactionVisible && bReactionAllowed && !bHasReacted && bHasPost && !CurrentPost.bOperationAlert;
	const int64 ActivatedSerial = ReactionPressedPostSerial;
	bReactionPressValid = false;
	// 성공하기 전 하트나 보상 문구를 바꾸지 않는다. 행 읽음은 바깥 버튼의 별도 입력이다.
	if (bActivate) { OnReactionRequested.Broadcast(ActivatedSerial); }
}

void UFF_LiveSnsPostEntry::HandleReadPressed()
{
	PressedPostSerial = PostSerial;
	bReadPressValid = bReadInteractionEnabled && bHasPost;
}

FReply UFF_LiveSnsPostEntry::HandleReadClicked()
{
	// PreciseClick/PreciseTap은 드래그 시 클릭을 취소하고, 키보드는 누름·해제가 모두 있어야 도착한다.
	const bool bActivate = bReadPressValid && bReadInteractionEnabled && bHasPost && PressedPostSerial == PostSerial;
	const int64 ActivatedSerial = PressedPostSerial;
	bReadPressValid = false;
	if (bActivate) { OnPostActivated.Broadcast(ActivatedSerial); }
	return FReply::Handled();
}

void UFF_LiveSnsPostEntry::ConfigureIconRowDefaults(const TMap<EFF_LiveSnsSentiment, UTexture2D*>& Icons,
	const FLinearColor& Positive, const FLinearColor& Negative, const FLinearColor& Other)
{
	SentimentIcons.Reset();
	for (const TPair<EFF_LiveSnsSentiment, UTexture2D*>& Pair : Icons) { SentimentIcons.Add(Pair.Key, Pair.Value); }
	PositiveColor = Positive;
	NegativeColor = Negative;
	NeutralColor = UnknownColor = Other;
	bTintSentimentIcon = true;
}

void UFF_LiveSnsPostEntry::SetNewMark(bool bNew)
{
	bNewMark = bNew;
	// 점은 이름 바로 뒤에 붙는다. 없을 때는 자리도 없앤다(뒤 칸이 남은 폭을 채운다).
	if (Image_NewDot) { Image_NewDot->SetVisibility(bNew ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed); }
	RefreshReadToolTip();
}

void UFF_LiveSnsPostEntry::RefreshReadToolTip()
{
	const FText OperationDay = CurrentPost.bOperationAlert && CurrentPost.OperationDay > 0
		? FText::Format(LOCTEXT("OperationDay", "{0}일차 운영 알림"), FText::AsNumber(CurrentPost.OperationDay)) : FText::GetEmpty();
	if (!bReadInteractionEnabled) { SetToolTipText(OperationDay); return; }
	const FText ReadState = bNewMark
		? LOCTEXT("UnreadNewsHint", "읽지 않은 소식 · 클릭하면 읽음 처리됩니다.")
		: LOCTEXT("ReadNewsHint", "읽은 소식");
	SetToolTipText(OperationDay.IsEmpty() ? ReadState
		: FText::Format(LOCTEXT("OperationReadHint", "{0}\n{1}"), OperationDay, ReadState));
}

void UFF_LiveSnsPostEntry::ConfigureNewsRowDefaults(UTexture2D* InSocialIcon, UTexture2D* InOperationIcon)
{
	bShowSourceLabel = true;
	SocialIcon = InSocialIcon;
	OperationIcon = InOperationIcon;
}

void UFF_LiveSnsPostEntry::ConfigurePost(const FFF_LiveSocialPost& Post)
{
	CurrentPost = Post;
	bHasPost = true;
	RefreshPost();
}
void UFF_LiveSnsPostEntry::NativePreConstruct()
{
	Super::NativePreConstruct();
	if (bHasPost) { RefreshPost(); }
	if (!IsDesignTime()) { RefreshReaction(); }
}
void UFF_LiveSnsPostEntry::RefreshPost()
{
	if (Text_Author) { Text_Author->SetText(CurrentPost.Author); }
	if (Text_Body) { Text_Body->SetText(CurrentPost.Body); }
	RefreshReadToolTip();
	RefreshReaction();
	const EFF_LiveSnsSentiment Sentiment = FFF_LiveSnsFeedState::ResolveSentiment(CurrentPost);
	FLinearColor Color = UnknownColor;
	switch (Sentiment)
	{
	case EFF_LiveSnsSentiment::Positive:
		Color = PositiveColor; break;
	case EFF_LiveSnsSentiment::Negative:
		Color = NegativeColor; break;
	case EFF_LiveSnsSentiment::Neutral:
		Color = NeutralColor; break;
	default: break;
	}
	if (CurrentPost.bOperationAlert) { Color = FLinearColor::FromSRGBColor(FColor(183, 109, 0)); }
	if (Text_Sentiment)
	{
		const FText* Label = SentimentLabels.Find(Sentiment);
		const FText Mood = Label ? *Label : LOCTEXT("Unknown", "반응");
		Text_Sentiment->SetText(CurrentPost.bOperationAlert ? LOCTEXT("Operation", "운영 알림")
			: bShowSourceLabel ? FText::Format(LOCTEXT("SocialMood", "SNS · {0}"), Mood) : Mood);
		Text_Sentiment->SetColorAndOpacity(FSlateColor(Color));
	}
	if (Text_SentimentSymbol)
	{
		const FText* Symbol = SentimentSymbols.Find(Sentiment);
		Text_SentimentSymbol->SetText(Symbol ? *Symbol : LOCTEXT("UnknownSymbol", "·"));
		Text_SentimentSymbol->SetColorAndOpacity(FSlateColor(Color));
	}
	if (Image_Sentiment)
	{
		// 브러시를 복사해 그림만 바꾼다. Designer 의 크기·여백은 그대로다.
		const TObjectPtr<UTexture2D>* Icon = SentimentIcons.Find(Sentiment);
		UTexture2D* Texture = Icon ? Icon->Get() : nullptr;
		FSlateBrush IconBrush = Image_Sentiment->GetBrush();
		IconBrush.SetResourceObject(Texture);
		Image_Sentiment->SetBrush(IconBrush);
		if (bTintSentimentIcon) { Image_Sentiment->SetColorAndOpacity(Color); }
		Image_Sentiment->SetVisibility(Texture && !CurrentPost.bOperationAlert ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (bShowSourceLabel && Image_Avatar)
	{
		FSlateBrush Avatar = Image_Avatar->GetBrush();
		Avatar.SetResourceObject(CurrentPost.bOperationAlert ? OperationIcon.Get() : SocialIcon.Get());
		Image_Avatar->SetBrush(Avatar);
		Image_Avatar->SetColorAndOpacity(CurrentPost.bOperationAlert ? Color : NeutralColor);
	}
	if (bShowSourceLabel && Border_Avatar)
	{
		Border_Avatar->SetBrushColor(CurrentPost.bOperationAlert
			? FLinearColor::FromSRGBColor(FColor(255, 245, 224)) : FLinearColor::FromSRGBColor(FColor(236, 243, 244)));
	}
	// 색과 기호·라벨을 동시에 사용한다.
	if (Border_Sentiment)
	{
		const FLinearColor* Background = SentimentBackgroundColors.Find(Sentiment);
		Border_Sentiment->SetBrushColor(CurrentPost.bOperationAlert
			? FLinearColor::FromSRGBColor(FColor(255, 243, 218)) : Background ? *Background : FLinearColor::White);
	}
}
#undef LOCTEXT_NAMESPACE

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_LiveSnsPostReadInteractionTest, "FestaFiesta.UI.SNS.PostReadInteraction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FFF_LiveSnsPostReadInteractionTest::RunTest(const FString& Parameters)
{
	if (!TestTrue(TEXT("Slate 앱 초기화"), FSlateApplication::IsInitialized())) { return false; }
	UClass* EntryClass = LoadClass<UFF_LiveSnsPostEntry>(nullptr,
		TEXT("/Game/FestaFiesta/UI/LiveHUD/Panels/WBP_FF_LiveSnsFlyoutPostRow.WBP_FF_LiveSnsFlyoutPostRow_C"));
	if (!TestNotNull(TEXT("실제 도크 소식 행 WBP"), EntryClass)) { return false; }
	TStrongObjectPtr<UFF_LiveSnsPostEntry> Entry(NewObject<UFF_LiveSnsPostEntry>(GetTransientPackage(), EntryClass));
	if (!TestTrue(TEXT("행 WBP 바인딩 초기화"), Entry->Initialize())) { return false; }
	FFF_LiveSocialPost Post;
	Post.Author = FText::FromString(TEXT("입력 검증"));
	Post.Body = FText::FromString(TEXT("소식 본문"));
	Post.bOperationAlert = true;
	Post.OperationDay = 2;
	Entry->ConfigurePost(Post);
	Entry->ConfigureReadInteraction(101, true);
	const TSharedRef<SWidget> SlateRoot = Entry->TakeWidget();
	const TSharedPtr<SFF_LiveSnsReadButton> Button = Entry->ReadButton;
	if (!TestTrue(TEXT("실제 RebuildWidget의 입력 래퍼"), Button.IsValid())) { return false; }
	TestTrue(TEXT("UserWidget이 자식 입력을 허용"), Entry->GetVisibility() == ESlateVisibility::Visible
		|| Entry->GetVisibility() == ESlateVisibility::SelfHitTestInvisible);
	TestTrue(TEXT("TakeWidget 바깥 래퍼가 자식 입력을 허용"), SlateRoot->GetVisibility().AreChildrenHitTestVisible());
	TestTrue(TEXT("입력 버튼이 히트 테스트와 키보드 포커스를 허용"),
		Button->GetVisibility().IsHitTestVisible() && Button->SupportsKeyboardFocus());
	Entry->SetNewMark(true);
	TestTrue(TEXT("운영 일차와 읽지 않음 문구"), Entry->GetToolTipText().ToString().Contains(TEXT("2일차"))
		&& Entry->GetToolTipText().ToString().Contains(TEXT("읽지 않은 소식")));
	Entry->SetNewMark(false);
	TestTrue(TEXT("읽음 문구 갱신"), Entry->GetToolTipText().ToString().Contains(TEXT("읽은 소식")));

	TArray<int64> ActivatedSerials;
	const FDelegateHandle Handle = Entry->OnPostActivated.AddLambda([&ActivatedSerials](int64 Serial) { ActivatedSerials.Add(Serial); });
	const float DragDistance = FSlateApplication::Get().GetDragTriggerDistance() + 32.f;
	const FGeometry Geometry = FGeometry::MakeRoot(FVector2D(FMath::Max(512.f, DragDistance + 40.f), 128.f), FSlateLayoutTransform());
	const FVector2D Point(10.f, 10.f);
	const TSet<FKey> PressedButtons{EKeys::LeftMouseButton};
	const TSet<FKey> NoButtons;
	const FModifierKeysState Modifiers;
	const FPointerEvent Down(0, Point, Point, PressedButtons, EKeys::LeftMouseButton, 0.f, Modifiers);
	const FPointerEvent Up(0, Point, Point, NoButtons, EKeys::LeftMouseButton, 0.f, Modifiers);
	Button->OnMouseEnter(Geometry, Down);
	const FReply DownReply = Button->OnMouseButtonDown(Geometry, Down);
	TestTrue(TEXT("누름은 처리하되 포인터는 캡처하지 않음"), DownReply.IsEventHandled() && !DownReply.GetMouseCaptor().IsValid());
	TestEqual(TEXT("누르는 중에는 읽지 않음"), ActivatedSerials.Num(), 0);
	TestTrue(TEXT("놓음은 처리함"), Button->OnMouseButtonUp(Geometry, Up).IsEventHandled());
	TestTrue(TEXT("클릭한 도착 번호를 한 번 전달"), ActivatedSerials == TArray<int64>{101});
	Button->OnMouseButtonUp(Geometry, Up);
	TestEqual(TEXT("추가 놓음은 중복 전달하지 않음"), ActivatedSerials.Num(), 1);

	Button->OnMouseButtonDown(Geometry, Down);
	Entry->ConfigureReadInteraction(102, true);
	Entry->ConfigureReadInteraction(101, true);
	Button->OnMouseButtonUp(Geometry, Up);
	TestEqual(TEXT("누른 중 행이 바뀌었다 되돌아와도 취소"), ActivatedSerials.Num(), 1);
	Button->OnMouseButtonDown(Geometry, Down);
	const FPointerEvent Drag(0, Point + FVector2D(DragDistance, 0.f), Point, PressedButtons, EKeys::Invalid, 0.f, Modifiers);
	TestFalse(TEXT("이동은 부모 스크롤에 전달"), Button->OnMouseMove(Geometry, Drag).IsEventHandled());
	TestFalse(TEXT("드래그 임계치를 넘으면 누름 해제"), Button->IsPressed());
	Button->OnMouseButtonUp(Geometry, Up);
	TestEqual(TEXT("드래그 뒤 놓음은 읽지 않음"), ActivatedSerials.Num(), 1);

	const FKeyEvent Accept(EKeys::Enter, Modifiers, 0, false, 0, 0);
	const FKeyEvent RepeatAccept(EKeys::Enter, Modifiers, 0, true, 0, 0);
	Entry->ConfigureReadInteraction(103, true);
	TestTrue(TEXT("키보드 확인 누름 처리"), Button->OnKeyDown(Geometry, Accept).IsEventHandled());
	Button->OnKeyDown(Geometry, RepeatAccept);
	TestEqual(TEXT("키 반복도 놓기 전에는 읽지 않음"), ActivatedSerials.Num(), 1);
	TestTrue(TEXT("키보드 확인 놓음 처리"), Button->OnKeyUp(Geometry, Accept).IsEventHandled());
	TestTrue(TEXT("키보드로 현재 도착 번호를 한 번 전달"), ActivatedSerials == TArray<int64>({101, 103}));
	Button->OnKeyUp(Geometry, Accept);
	TestEqual(TEXT("키보드 추가 놓음은 중복 전달하지 않음"), ActivatedSerials.Num(), 2);
	Button->OnKeyDown(Geometry, Accept);
	Entry->ConfigureReadInteraction(104, true);
	Button->OnKeyDown(Geometry, RepeatAccept);
	Button->OnKeyUp(Geometry, Accept);
	TestEqual(TEXT("키를 누른 중 행 교체도 반복 키로 재활성화되지 않음"), ActivatedSerials.Num(), 2);
	Entry->SetVisibility(ESlateVisibility::HitTestInvisible);
	Entry->ConfigureReadInteraction(104, true);
	TestTrue(TEXT("표시 전용 외부 가시성도 자식 입력 허용으로 복구"), SlateRoot->GetVisibility().AreChildrenHitTestVisible());

	Entry->ConfigureReadInteraction(105, false);
	TestFalse(TEXT("기존 표시 전용 모드는 버튼 포커스 제외"), Button->SupportsKeyboardFocus());
	TestFalse(TEXT("기존 표시 전용 모드는 버튼 히트 테스트 제외"), Button->GetVisibility().IsHitTestVisible());
	TestFalse(TEXT("기존 표시 전용 모드는 누름 미처리"), Button->OnMouseButtonDown(Geometry, Down).IsEventHandled());
	Button->OnMouseButtonUp(Geometry, Up);
	Button->OnKeyDown(Geometry, Accept);
	Button->OnKeyUp(Geometry, Accept);
	TestEqual(TEXT("기존 표시 전용 모드는 읽음 요청 없음"), ActivatedSerials.Num(), 2);
	Button->OnMouseLeave(Up);
	Entry->OnPostActivated.Remove(Handle);
	Entry->ReleaseSlateResources(true);
	// 앱의 히트 테스트 경로·실제 스크롤 이동은 건드리지 않고, 실제 래퍼의 입력 처리만 검증한다.
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_LiveSnsPostReactionInteractionTest, "FestaFiesta.UI.SNS.PostReactionInteraction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FFF_LiveSnsPostReactionInteractionTest::RunTest(const FString& Parameters)
{
	if (!TestTrue(TEXT("Slate 앱 초기화"), FSlateApplication::IsInitialized())) { return false; }
	UClass* EntryClass = LoadClass<UFF_LiveSnsPostEntry>(nullptr,
		TEXT("/Game/FestaFiesta/UI/LiveHUD/Panels/WBP_FF_LiveSnsFlyoutPostRow.WBP_FF_LiveSnsFlyoutPostRow_C"));
	if (!TestNotNull(TEXT("실제 공감 행 WBP"), EntryClass)) { return false; }
	TStrongObjectPtr<UFF_LiveSnsPostEntry> Entry(NewObject<UFF_LiveSnsPostEntry>(GetTransientPackage(), EntryClass));
	if (!TestTrue(TEXT("행 바인딩 초기화"), Entry->Initialize())) { return false; }
	if (!TestNotNull(TEXT("공감 버튼"), Entry->Button_React.Get())
		|| !TestNotNull(TEXT("공감 문구"), Entry->Text_ReactLabel.Get())
		|| !TestNotNull(TEXT("공감 결과 문구"), Entry->Text_ReactStatus.Get())
		|| !TestNotNull(TEXT("공감 행"), Entry->Box_Reaction.Get())) { return false; }
	FFF_LiveSocialPost Post;
	Post.Author = FText::FromString(TEXT("방문객"));
	Post.Body = FText::FromString(TEXT("공감 입력 확인"));
	Entry->ConfigurePost(Post);
	Entry->ConfigureReadInteraction(201, true);
	Entry->ConfigureReaction(true, false, true, FText::GetEmpty(), FText::FromString(TEXT("공감 안내")));
	const TSharedRef<SWidget> Root = Entry->TakeWidget();
	const TSharedRef<SButton> React = StaticCastSharedRef<SButton>(Entry->Button_React->TakeWidget());
	TestTrue(TEXT("공감 자식 입력을 허용"), Root->GetVisibility().AreChildrenHitTestVisible()
		&& Entry->Box_Reaction->GetVisibility() == ESlateVisibility::SelfHitTestInvisible);
	TArray<int64> Requested;
	int32 ReadCount = 0;
	const FDelegateHandle ReactionHandle = Entry->OnReactionRequested.AddLambda([&Requested](int64 Serial) { Requested.Add(Serial); });
	const FDelegateHandle ReadHandle = Entry->OnPostActivated.AddLambda([&ReadCount](int64) { ++ReadCount; });
	const float DragDistance = FSlateApplication::Get().GetDragTriggerDistance() + 32.f;
	const FGeometry Geometry = FGeometry::MakeRoot(FVector2D(FMath::Max(512.f, DragDistance + 40.f), 128.f), FSlateLayoutTransform());
	const FVector2D Point(10.f, 10.f);
	const TSet<FKey> PressedButtons{EKeys::LeftMouseButton};
	const TSet<FKey> NoButtons;
	const FModifierKeysState Modifiers;
	const FPointerEvent Down(0, Point, Point, PressedButtons, EKeys::LeftMouseButton, 0.f, Modifiers);
	const FPointerEvent Up(0, Point, Point, NoButtons, EKeys::LeftMouseButton, 0.f, Modifiers);
	React->OnMouseEnter(Geometry, Down);
	const FReply DownReply = React->OnMouseButtonDown(Geometry, Down);
	TestTrue(TEXT("공감 누름은 스크롤을 위해 포인터 미캡처"), DownReply.IsEventHandled() && !DownReply.GetMouseCaptor().IsValid());
	TestEqual(TEXT("놓기 전 요청 없음"), Requested.Num(), 0);
	TestTrue(TEXT("공감 클릭은 자식에서 처리"), React->OnMouseButtonUp(Geometry, Up).IsEventHandled());
	TestTrue(TEXT("클릭한 글 번호 전달"), Requested == TArray<int64>{201});
	TestEqual(TEXT("공감은 읽음 델리게이트를 호출하지 않음"), ReadCount, 0);
	TestEqual(TEXT("명령 성공 전에는 빈 하트 유지"), Entry->Text_ReactLabel->GetText().ToString(), FString(TEXT("♡ 공감")));
	React->OnMouseButtonUp(Geometry, Up);
	TestEqual(TEXT("추가 놓음 중복 요청 방지"), Requested.Num(), 1);
	React->OnMouseButtonDown(Geometry, Down);
	Entry->ConfigureReadInteraction(202, true);
	Entry->ConfigureReadInteraction(201, true);
	React->OnMouseButtonUp(Geometry, Up);
	TestEqual(TEXT("누른 중 행이 교체되었다 돌아와도 공감 취소"), Requested.Num(), 1);
	React->OnMouseButtonDown(Geometry, Down);
	const FPointerEvent Drag(0, Point + FVector2D(DragDistance, 0.f), Point, PressedButtons, EKeys::Invalid, 0.f, Modifiers);
	React->OnMouseMove(Geometry, Drag);
	React->OnMouseButtonUp(Geometry, Up);
	TestEqual(TEXT("목록 드래그는 공감하지 않음"), Requested.Num(), 1);
	Entry->ConfigureReaction(true, true, false, FText::FromString(TEXT("화제성 +0.1")), FText::GetEmpty());
	TestEqual(TEXT("실제 성공 후 채운 하트"), Entry->Text_ReactLabel->GetText().ToString(), FString(TEXT("♥ 공감함")));
	TestEqual(TEXT("원본 결과 문구 그대로 표시"), Entry->Text_ReactStatus->GetText().ToString(), FString(TEXT("화제성 +0.1")));
	TestFalse(TEXT("공감한 글은 중복 버튼 비활성"), Entry->Button_React->GetIsEnabled());
	Entry->HandleReactionPressed(); Entry->HandleReactionClicked();
	TestEqual(TEXT("중복 요청 방어"), Requested.Num(), 1);
	Post.bOperationAlert = true;
	Entry->ConfigurePost(Post);
	Entry->ConfigureReadInteraction(203, true);
	Entry->ConfigureReaction(true, false, true, FText::GetEmpty(), FText::GetEmpty());
	TestEqual(TEXT("운영 알림에는 공감 행 없음"), Entry->Box_Reaction->GetVisibility(), ESlateVisibility::Collapsed);
	Entry->HandleReactionPressed(); Entry->HandleReactionClicked();
	TestEqual(TEXT("운영 알림 공감 요청 방지"), Requested.Num(), 1);
	React->OnMouseLeave(Up);
	Entry->OnReactionRequested.Remove(ReactionHandle);
	Entry->OnPostActivated.Remove(ReadHandle);
	Entry->ReleaseSlateResources(true);
	return true;
}
#endif
