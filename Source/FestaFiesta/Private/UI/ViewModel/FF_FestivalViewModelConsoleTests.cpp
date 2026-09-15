#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING

#include "Engine/GameInstance.h"
#include "HAL/IConsoleManager.h"
#include "Misc/OutputDeviceNull.h"
#include "Tests/AutomationCommon.h"
#include "UI/ViewModel/FF_FestivalViewModelSubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_ViewModelConsoleSurvivesOverlappingGameInstances,
    "FestaFiesta.UI.ViewModel.ConsoleCommandsSurviveOverlappingGameInstances",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FFF_ViewModelConsoleSurvivesOverlappingGameInstances::RunTest(const FString& Parameters)
{
    (void)Parameters;
    IConsoleManager& Console = IConsoleManager::Get();
    FOutputDeviceNull Output;
    const TCHAR* Names[] = { TEXT("FF.SetMetrics"), TEXT("FF.SetEconomy"), TEXT("FF.DumpMetrics") };
    TArray<IConsoleCommand*> RegisteredCommands;
    for (const TCHAR* Name : Names)
    {
        IConsoleObject* Object = Console.FindConsoleObject(Name);
        IConsoleCommand* Command = Object ? Object->AsCommand() : nullptr;
        if (!TestNotNull(*FString::Printf(TEXT("Module command exists: %s"), Name), Command))
        {
            return false;
        }
        TestTrue(*FString::Printf(TEXT("Command preserves its cheat flag: %s"), Name), Command->TestFlags(ECVF_Cheat));
        RegisteredCommands.Add(Command);
    }

    auto CheckRegistration = [&](const TCHAR* Context)
    {
        for (int32 Index = 0; Index < RegisteredCommands.Num(); ++Index)
        {
            IConsoleObject* Object = Console.FindConsoleObject(Names[Index]);
            TestTrue(*FString::Printf(TEXT("%s keeps the same live command: %s"), Context, Names[Index]),
                Object && Object->AsCommand() == RegisteredCommands[Index]);
        }
    };
    auto Run = [&](UWorld* World, const TCHAR* Command)
    {
        return TestTrue(*FString::Printf(TEXT("Console executes %s"), Command),
            Console.ProcessUserConsoleInput(Command, Output, World));
    };

    // Deliberately overlap the two GameInstances. Older per-instance command
    // registration replaced the first instance's global command handles here.
    FTestWorldWrapper First;
    FTestWorldWrapper Second;
    if (!First.CreateTestWorld(EWorldType::Game) || !Second.CreateTestWorld(EWorldType::Game))
    {
        First.ForwardErrorMessages(this);
        Second.ForwardErrorMessages(this);
        return false;
    }
    UFF_FestivalViewModelSubsystem* FirstView = First.GetTestWorld()->GetGameInstance()->GetSubsystem<UFF_FestivalViewModelSubsystem>();
    UFF_FestivalViewModelSubsystem* SecondView = Second.GetTestWorld()->GetGameInstance()->GetSubsystem<UFF_FestivalViewModelSubsystem>();
    if (!TestNotNull(TEXT("First game instance owns a ViewModel"), FirstView)
        || !TestNotNull(TEXT("Second game instance owns a ViewModel"), SecondView))
    {
        return false;
    }
    CheckRegistration(TEXT("Creating overlapping worlds"));
    FirstView->Debug_SetMetrics(11, 12, 13);
    SecondView->Debug_SetMetrics(21, 22, 23);
    FirstView->Debug_SetEconomy(1100, 1200, 1300);
    SecondView->Debug_SetEconomy(2100, 2200, 2300);

    Run(First.GetTestWorld(), TEXT("FF.SetMetrics -25 125 47"));
    TestEqual(TEXT("First world metric minimum is preserved"), FirstView->GetSimSatisfaction(), 0);
    TestEqual(TEXT("First world metric maximum is preserved"), FirstView->GetScoreSatisfaction(), 100);
    TestEqual(TEXT("First world receives its safety value"), FirstView->GetSafety(), 47);
    TestEqual(TEXT("Second world metrics are not changed by first world input"), SecondView->GetSafety(), 23);

    Run(Second.GetTestWorld(), TEXT("FF.SetEconomy 15000 2700 900"));
    TestEqual(TEXT("Second world economy is not clamped to metric range"), SecondView->GetGold(), 15000);
    TestEqual(TEXT("Second world receives total revenue"), SecondView->GetTotalRevenue(), 2700);
    TestEqual(TEXT("Second world receives today's revenue"), SecondView->GetTodayRevenue(), 900);
    TestEqual(TEXT("First world economy is not changed by second world input"), FirstView->GetGold(), 1100);
    Run(First.GetTestWorld(), TEXT("FF.DumpMetrics"));
    Run(Second.GetTestWorld(), TEXT("FF.DumpMetrics"));

    // Destroy the older instance while the newer one is still usable. This was
    // the dangling IConsoleCommand* unregister crash in the full test run.
    if (!TestTrue(TEXT("Older world shuts down safely"), First.DestroyTestWorld(false)))
    {
        First.ForwardErrorMessages(this);
        return false;
    }
    FirstView = nullptr;
    CheckRegistration(TEXT("Older world teardown"));
    Run(Second.GetTestWorld(), TEXT("FF.SetMetrics 31 32 33"));
    TestEqual(TEXT("Surviving world still receives console input"), SecondView->GetSafety(), 33);
    Run(Second.GetTestWorld(), TEXT("FF.SetEconomy 4100 4200 4300"));
    TestEqual(TEXT("Surviving world's economy command remains usable"), SecondView->GetGold(), 4100);

    if (!TestTrue(TEXT("Newer world shuts down safely"), Second.DestroyTestWorld(false)))
    {
        Second.ForwardErrorMessages(this);
        return false;
    }
    SecondView = nullptr;
    CheckRegistration(TEXT("Both game instances torn down"));
    Run(nullptr, TEXT("FF.SetMetrics 51 52 53"));
    Run(nullptr, TEXT("FF.SetEconomy 5100 5200 5300"));
    Run(nullptr, TEXT("FF.DumpMetrics"));

    FTestWorldWrapper Replacement;
    if (!Replacement.CreateTestWorld(EWorldType::Game))
    {
        Replacement.ForwardErrorMessages(this);
        return false;
    }
    UFF_FestivalViewModelSubsystem* ReplacementView = Replacement.GetTestWorld()->GetGameInstance()->GetSubsystem<UFF_FestivalViewModelSubsystem>();
    if (!TestNotNull(TEXT("Replacement game instance owns a ViewModel"), ReplacementView))
    {
        return false;
    }
    CheckRegistration(TEXT("Replacement world creation"));
    Run(Replacement.GetTestWorld(), TEXT("FF.SetMetrics 61 62 63"));
    TestEqual(TEXT("The same command routes to a replacement world"), ReplacementView->GetSafety(), 63);
    return true;
}

#endif
