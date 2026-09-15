// Copyright Epic Games, Inc. All Rights Reserved.

#include "FestaFiesta.h"
#include "DynamicRHI.h"
#include "HAL/PlatformMisc.h"
#include "Misc/CommandLine.h"
#include "Misc/CoreDelegates.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"

class FFestaFiestaGameModule final : public FDefaultGameModuleImpl
{
public:
	virtual void StartupModule() override
	{
		FDefaultGameModuleImpl::StartupModule();
		if (!FParse::Value(FCommandLine::Get(), TEXT("FFGraphicsProbe="), ProbeReportPath))
		{
			return;
		}

		// Opt-in package verification only. Normal launches do not register a callback or write files.
		if (GIsEditor || IsRunningCommandlet() || ProbeReportPath.IsEmpty() || FPaths::IsRelative(ProbeReportPath)
			|| !FParse::Value(FCommandLine::Get(), TEXT("FFGraphicsProbeToken="), ProbeToken)
			|| ProbeToken.Len() != 32)
		{
			FPlatformMisc::RequestExitWithStatus(true, 21, TEXT("FFGraphicsProbe.InvalidArguments"));
			return;
		}
		for (const TCHAR Character : ProbeToken)
		{
			if (!FChar::IsHexDigit(Character))
			{
				FPlatformMisc::RequestExitWithStatus(true, 21, TEXT("FFGraphicsProbe.InvalidToken"));
				return;
			}
		}

		PostEngineInitHandle = FCoreDelegates::OnPostEngineInit.AddRaw(this, &FFestaFiestaGameModule::WriteGraphicsProbe);
	}

	virtual void ShutdownModule() override
	{
		if (PostEngineInitHandle.IsValid())
		{
			FCoreDelegates::OnPostEngineInit.Remove(PostEngineInitHandle);
			PostEngineInitHandle.Reset();
		}
		FDefaultGameModuleImpl::ShutdownModule();
	}

private:
	void WriteGraphicsProbe()
	{
		const FString RhiName = GDynamicRHI ? GDynamicRHI->GetName() : TEXT("");
		const bool bSM5 = GMaxRHIFeatureLevel == ERHIFeatureLevel::SM5;
		const bool bSM6 = GMaxRHIFeatureLevel == ERHIFeatureLevel::SM6;
		if (!((RhiName == TEXT("D3D11") && bSM5) || (RhiName == TEXT("D3D12") && bSM6)))
		{
			FPlatformMisc::RequestExitWithStatus(true, 22, TEXT("FFGraphicsProbe.UnsupportedRHI"));
			return;
		}

		// All interpolated values are constrained above, so no arbitrary text enters this JSON.
		const FString Report = FString::Printf(
			TEXT("{\"token\":\"%s\",\"rhi\":\"%s\",\"shaderModel\":\"%s\",\"initialized\":true}\n"),
			*ProbeToken, *RhiName, bSM6 ? TEXT("SM6") : TEXT("SM5"));
		if (!FFileHelper::SaveStringToFile(Report, *ProbeReportPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
		{
			FPlatformMisc::RequestExitWithStatus(true, 23, TEXT("FFGraphicsProbe.ReportWriteFailed"));
			return;
		}

		// The marker proves this callback was reached after engine/RHI initialization.
		// It does not certify rendered gameplay, map traversal, or performance.
		FPlatformMisc::RequestExit(false, TEXT("FFGraphicsProbe.Completed"));
	}

	FString ProbeReportPath;
	FString ProbeToken;
	FDelegateHandle PostEngineInitHandle;
};

IMPLEMENT_PRIMARY_GAME_MODULE(FFestaFiestaGameModule, FestaFiesta, "FestaFiesta");
