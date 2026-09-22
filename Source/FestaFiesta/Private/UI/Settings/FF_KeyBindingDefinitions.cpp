// FF_KeyBindingDefinitions.cpp
// 역할: 변경 가능한 조작 항목의 이름·기본 키·입력 유형을 정의한다.

// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/Settings/FF_KeyBindingDefinitions.h"
#include "UserSettings/EnhancedInputUserSettings.h"

#define LOCTEXT_NAMESPACE "FFKeyBindingDefinitions"

namespace FFKeyBindingDefinitions
{
	const TArray<FFF_KeyBindingDefinition>& GetAll()
	{
		static const TArray<FFF_KeyBindingDefinition> Definitions =
		{
			{
				TEXT("FF.System.Options.Primary"),
				LOCTEXT("OptionsPrimary", "옵션 열기 / 닫기 (기본)"),
				EKeys::Escape,
				NAME_None,
				EFF_KeyBindingContext::System
			},
			{
				TEXT("FF.System.Options.Secondary"),
				LOCTEXT("OptionsSecondary", "옵션 열기 / 닫기 (보조)"),
				EKeys::F5,
				NAME_None,
				EFF_KeyBindingContext::System
			},
			{
				TEXT("FF.Camera.Move.Forward"),
				LOCTEXT("CameraMoveForward", "카메라 앞으로 이동"),
				EKeys::W,
				TEXT("IA_Pan"),
				EFF_KeyBindingContext::Camera
			},
			{
				TEXT("FF.Camera.Move.Backward"),
				LOCTEXT("CameraMoveBackward", "카메라 뒤로 이동"),
				EKeys::S,
				TEXT("IA_Pan"),
				EFF_KeyBindingContext::Camera
			},
			{
				TEXT("FF.Camera.Move.Left"),
				LOCTEXT("CameraMoveLeft", "카메라 왼쪽 이동"),
				EKeys::A,
				TEXT("IA_Pan"),
				EFF_KeyBindingContext::Camera
			},
			{
				TEXT("FF.Camera.Move.Right"),
				LOCTEXT("CameraMoveRight", "카메라 오른쪽 이동"),
				EKeys::D,
				TEXT("IA_Pan"),
				EFF_KeyBindingContext::Camera
			},
			{
				TEXT("FF.Camera.Rotate.Left"),
				LOCTEXT("CameraRotateLeft", "카메라 왼쪽 회전"),
				EKeys::Q,
				TEXT("IA_SnapRotate"),
				EFF_KeyBindingContext::Camera
			},
			{
				TEXT("FF.Camera.Rotate.Right"),
				LOCTEXT("CameraRotateRight", "카메라 오른쪽 회전"),
				EKeys::E,
				TEXT("IA_SnapRotate"),
				EFF_KeyBindingContext::Camera
			},
			{
				TEXT("FF.Camera.ResetView"),
				LOCTEXT("CameraResetView", "카메라 시점 초기화"),
				EKeys::Home,
				TEXT("IA_ResetView"),
				EFF_KeyBindingContext::Camera
			},
			{
				TEXT("FF.Camera.ToggleView"),
				LOCTEXT("CameraToggleView", "카메라 시점 전환"),
				EKeys::Tab,
				TEXT("IA_ToggleView"),
				EFF_KeyBindingContext::Camera
			},
			{
				TEXT("FF.Character.Move.Forward"),
				LOCTEXT("CharacterMoveForward", "캐릭터 앞으로 이동"),
				EKeys::W,
				TEXT("IA_Move"),
				EFF_KeyBindingContext::Character
			},
			{
				TEXT("FF.Character.Move.Backward"),
				LOCTEXT("CharacterMoveBackward", "캐릭터 뒤로 이동"),
				EKeys::S,
				TEXT("IA_Move"),
				EFF_KeyBindingContext::Character
			},
			{
				TEXT("FF.Character.Move.Left"),
				LOCTEXT("CharacterMoveLeft", "캐릭터 왼쪽 이동"),
				EKeys::A,
				TEXT("IA_Move"),
				EFF_KeyBindingContext::Character
			},
			{
				TEXT("FF.Character.Move.Right"),
				LOCTEXT("CharacterMoveRight", "캐릭터 오른쪽 이동"),
				EKeys::D,
				TEXT("IA_Move"),
				EFF_KeyBindingContext::Character
			},
			{
				TEXT("FF.Character.Jump"),
				LOCTEXT("CharacterJump", "점프"),
				EKeys::SpaceBar,
				TEXT("IA_Jump"),
				EFF_KeyBindingContext::Character
			},
			{
				TEXT("FF.Character.ShiftLock"),
				LOCTEXT("CharacterShiftLock", "시점 고정 켜기 / 끄기 (Shift Lock)"),
				EKeys::LeftShift,
				TEXT("IA_ShiftLock"),
				EFF_KeyBindingContext::Character
			}
		};
		return Definitions;
	}

	const FFF_KeyBindingDefinition* Find(const FName MappingName)
	{
		return GetAll().FindByPredicate([MappingName](const FFF_KeyBindingDefinition& Definition)
		{
			return Definition.MappingName == MappingName;
		});
	}

	FKey ResolveCurrentKey(const UEnhancedInputUserSettings* UserSettings, const FName MappingName)
	{
		const FFF_KeyBindingDefinition* Definition = Find(MappingName);
		if (!Definition)
		{
			// 제거된 동작의 저장 행은 보존하되 실행하거나 새 동작의 키로 옮기지 않는다.
			// 잘못 노출되던 Sprint와 ShiftLock은 서로 다른 기능이므로 자동 변환하지 않는다.
			return EKeys::Invalid;
		}
		if (UserSettings)
		{
			if (const FPlayerKeyMapping* Mapping = UserSettings->FindCurrentMappingForSlot(
				MappingName, EPlayerMappableKeySlot::First))
			{
				if (Mapping->GetCurrentKey().IsValid())
				{
					return Mapping->GetCurrentKey();
				}
			}
		}
		return Definition->DefaultKey;
	}

	bool IsSystemMapping(const FName MappingName)
	{
		const FFF_KeyBindingDefinition* Definition = Find(MappingName);
		return Definition && Definition->Context == EFF_KeyBindingContext::System;
	}

	FText GetContextLabel(const EFF_KeyBindingContext Context)
	{
		switch (Context)
		{
		case EFF_KeyBindingContext::System:
			return LOCTEXT("SystemContext", "시스템");
		case EFF_KeyBindingContext::Camera:
			return LOCTEXT("CameraContext", "관찰 카메라");
		case EFF_KeyBindingContext::Character:
			return LOCTEXT("CharacterContext", "캐릭터");
		default:
			return FText::GetEmpty();
		}
	}
}

#undef LOCTEXT_NAMESPACE
