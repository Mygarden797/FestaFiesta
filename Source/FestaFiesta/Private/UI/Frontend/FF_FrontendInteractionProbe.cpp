// PIE에서 실제 Slate 마우스 경로로 프런트엔드 버튼을 검사하는 에디터 도구.
#include "CoreMinimal.h"
#if WITH_EDITOR
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "CommonActivatableWidget.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/IConsoleManager.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "Editor.h"
#include "PlayInEditorDataTypes.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "UI/FF_CommonButtonBase.h"
#include "UObject/UObjectIterator.h"
#include "Widgets/SWindow.h"

namespace
{
    void BeginFrontendPIE(const TArray<FString>& Args)
    {
        if(!GEditor || GEditor->PlayWorld)return;
        auto* Settings=DuplicateObject<ULevelEditorPlaySettings>(GetDefault<ULevelEditorPlaySettings>(),GetTransientPackage());
        Settings->NewWindowWidth=Args.Num()>0?FCString::Atoi(*Args[0]):1600;
        Settings->NewWindowHeight=Args.Num()>1?FCString::Atoi(*Args[1]):900;
        Settings->CenterNewWindow=true;
        FRequestPlaySessionParams Params;Params.EditorPlaySettings=Settings;
        GEditor->RequestPlaySession(Params);
    }
    FAutoConsoleCommand BeginCommand(TEXT("FF.FrontendBeginPIE"),
        TEXT("프런트엔드 검증용 별도 PIE 뷰포트: 가로 세로. 사용자 에디터 설정 파일은 저장하지 않는다."),
        FConsoleCommandWithArgsDelegate::CreateStatic(&BeginFrontendPIE));
    void ProbePointer(const TArray<FString>& Args)
    {
        if(Args.Num()<1 || !FSlateApplication::IsInitialized()) return;
        FSlateApplication& App=FSlateApplication::Get();
        FVector2D Position(1800,100);
        if(Args[0]!=TEXT("Away"))
        {
            UWidget* Target=nullptr;
            for(TObjectIterator<UCommonActivatableWidget> It;It;++It)
            {
                if(It->GetWorld() && It->GetWorld()->WorldType==EWorldType::PIE && It->IsActivated())
                {
                    if(UWidget* Found=It->GetWidgetFromName(FName(*Args[0]))) {Target=Found;break;}
                }
            }
            if(!Target) {UE_LOG(LogTemp,Error,TEXT("FF_POINTER: 위젯을 찾지 못했다: %s"),*Args[0]);return;}
            const auto& Geometry=Target->GetCachedGeometry();
            Position=Geometry.LocalToAbsolute(Geometry.GetLocalSize()*0.5f);
        }
        const FVector2D Old=App.GetCursorPos();App.SetCursorPos(Position);
        const TSet<FKey> Empty;
        App.ProcessMouseMoveEvent(FPointerEvent(0,Position,Old,Empty,EKeys::Invalid,0,FModifierKeysState()),true);
        if(Args.Num()>1 && Args[1]==TEXT("Click"))
        {
            TSet<FKey> Down;Down.Add(EKeys::LeftMouseButton);
            TSharedPtr<SWindow> Window=App.GetActiveTopLevelWindow();
            App.ProcessMouseButtonDownEvent(Window?Window->GetNativeWindow():nullptr,
                FPointerEvent(0,Position,Position,Down,EKeys::LeftMouseButton,0,FModifierKeysState()));
            App.ProcessMouseButtonUpEvent(FPointerEvent(0,Position,Position,Empty,EKeys::LeftMouseButton,0,FModifierKeysState()));
        }
        UE_LOG(LogTemp,Display,TEXT("FF_POINTER: %s %s"),*Args[0],*Position.ToString());
    }
    FAutoConsoleCommand PointerCommand(TEXT("FF.FrontendPointer"),
        TEXT("PIE 프런트엔드 검증: 위젯이름 [Click], 또는 Away. 실제 Slate 입력 경로로 전달한다."),
        FConsoleCommandWithArgsDelegate::CreateStatic(&ProbePointer));
}
#endif
