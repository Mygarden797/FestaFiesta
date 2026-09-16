// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tutorial/FF_DialogueRow.h"

namespace
{
	// 문자열을 열거형으로. 못 찾으면 기본값을 쓰고 경고를 남긴다.
	//
	// 조용히 넘어가면 오타 하나 때문에 대사가 엉뚱하게 흐르는데,
	// 화면만 봐서는 원인을 알 수 없다.
	template <typename EnumType>
	EnumType ParseEnum(const FString& Value, const EnumType Fallback, const TCHAR* FieldName, const FName RowName)
	{
		if (Value.IsEmpty())
		{
			return Fallback;
		}

		const UEnum* EnumClass = StaticEnum<EnumType>();
		if (!EnumClass)
		{
			return Fallback;
		}

		const int64 Found = EnumClass->GetValueByNameString(Value);
		if (Found == INDEX_NONE)
		{
			UE_LOG(LogFestaFiestaTutorial, Warning,
				TEXT("[대사] %s 행의 %s 값 '%s' 를 알 수 없다. 기본값을 쓴다."),
				*RowName.ToString(), FieldName, *Value);
			return Fallback;
		}

		return static_cast<EnumType>(Found);
	}

	// 선택지 하나를 더한다. 텍스트가 비어 있으면 아무것도 안 한다.
	void AppendChoice(
		TArray<FFF_DialogueChoice>& OutChoices,
		const FString& Text,
		const FString& JumpTo)
	{
		if (Text.IsEmpty())
		{
			return;
		}

		FFF_DialogueChoice Choice;
		Choice.Text = FText::FromString(Text);
		Choice.JumpToLabel = JumpTo.IsEmpty() ? NAME_None : FName(*JumpTo);
		OutChoices.Add(Choice);
	}
}

namespace FFF_DialogueTable
{
	FFF_DialogueLine BuildLine(const FName RowName, const FFF_DialogueRow& Row)
	{
		FFF_DialogueLine Line;

		// 행 이름이 곧 라벨이다. CSV 는 어차피 이름이 필수라 따로 둘 이유가 없다.
		Line.Label = RowName;
		Line.NextLabel = Row.NextJump.IsEmpty() ? NAME_None : FName(*Row.NextJump);

		Line.Kind = ParseEnum<EFF_DialogueLineKind>(
			Row.Kind, EFF_DialogueLineKind::Speech, TEXT("Kind"), RowName);

		Line.WaitCondition = ParseEnum<EFF_TutorialWaitCondition>(
			Row.WaitCondition, EFF_TutorialWaitCondition::None, TEXT("WaitCondition"), RowName);

		Line.Speaker = Row.Speaker.IsEmpty() ? FText::GetEmpty() : FText::FromString(Row.Speaker);
		Line.Text = Row.Text.IsEmpty() ? FText::GetEmpty() : FText::FromString(Row.Text);

		Line.HighlightAnchor = Row.HighlightAnchor.IsEmpty()
			? NAME_None : FName(*Row.HighlightAnchor);

		Line.bSkippable = Row.Skippable;
		Line.SkipSummary = Row.SkipSummary.IsEmpty()
			? FText::GetEmpty() : FText::FromString(Row.SkipSummary);

		AppendChoice(Line.Choices, Row.Choice1Text, Row.Choice1Jump);
		AppendChoice(Line.Choices, Row.Choice2Text, Row.Choice2Jump);
		AppendChoice(Line.Choices, Row.Choice3Text, Row.Choice3Jump);

		// 선택지를 적어놓고 종류를 Speech 로 둔 경우가 흔하다. 여기서 바로잡는다.
		if (Line.Choices.Num() > 0 && Line.Kind != EFF_DialogueLineKind::Choice)
		{
			UE_LOG(LogFestaFiestaTutorial, Warning,
				TEXT("[대사] %s 행에 선택지가 있는데 Kind 가 Choice 가 아니다. Choice 로 바꾼다."),
				*RowName.ToString());
			Line.Kind = EFF_DialogueLineKind::Choice;
		}

		// 반대로 조건 없는 조작 대기 줄은 영영 멈춘다. 대사로 낮춰서 진행을 막지 않는다.
		if (Line.Kind == EFF_DialogueLineKind::WaitForAction
			&& Line.WaitCondition == EFF_TutorialWaitCondition::None)
		{
			UE_LOG(LogFestaFiestaTutorial, Warning,
				TEXT("[대사] %s 행이 조작 대기인데 조건이 없다. 대사로 처리한다."),
				*RowName.ToString());
			Line.Kind = EFF_DialogueLineKind::Speech;
		}

		return Line;
	}

	bool BuildLines(const UDataTable* Table, TArray<FFF_DialogueLine>& OutLines)
	{
		OutLines.Reset();

		if (!Table)
		{
			return false;
		}

		// 행 순서를 CSV 그대로 유지한다. 이름순으로 정렬하면 대사가 뒤섞인다.
		const TArray<FName> RowNames = Table->GetRowNames();
		OutLines.Reserve(RowNames.Num());

		for (const FName& RowName : RowNames)
		{
			const FFF_DialogueRow* Row =
				Table->FindRow<FFF_DialogueRow>(RowName, TEXT("FFF_DialogueTable::BuildLines"));
			if (!Row)
			{
				continue;
			}
			OutLines.Add(BuildLine(RowName, *Row));
		}

		return OutLines.Num() > 0;
	}
}
