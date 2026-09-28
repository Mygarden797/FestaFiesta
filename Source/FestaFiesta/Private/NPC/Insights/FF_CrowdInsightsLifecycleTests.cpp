// The SNS template table is loaded from a soft setting before Live and must survive GC.
#include "Misc/AutomationTest.h"

// 역할: SNS 게시글 양식이 운영 시작 전에 로드되고 메모리 정리 후에도 유지되는지 검사한다.
// 전역 설정은 검사 종료 시 복원하고 실제 제작 에셋은 변경하지 않는다.

#if WITH_DEV_AUTOMATION_TESTS

#include "CrowdSystem/Types/FF_SimulationSettings.h"
#include "DataTables/FF_SnsPostTemplateRow.h"
#include "NPC/Insights/FF_CrowdInsightsSubsystem.h"
#include "Engine/DataTable.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Tests/AutomationCommon.h"
#include "UObject/GarbageCollection.h"
#include "UObject/UObjectGlobals.h"

namespace FFCrowdInsightsLifecycleTest
{
struct FScopedSnsTableSetting
{
    UFF_SimulationSettings* Settings = GetMutableDefault<UFF_SimulationSettings>();
    TSoftObjectPtr<UDataTable> Original = Settings->SnsPostTemplateTable;
    TWeakObjectPtr<UDataTable> RuntimeCopy;

    ~FScopedSnsTableSetting()
    {
        Settings->SnsPostTemplateTable = Original;
    }

    bool Install(FAutomationTestBase& Test)
    {
        UDataTable* Source = Original.LoadSynchronous();
        if (!Test.TestNotNull(TEXT("Project SNS template table resolves"), Source))
        {
            return false;
        }
        if (!Test.TestTrue(TEXT("Project SNS table uses the expected row struct"),
            Source->GetRowStruct() && Source->GetRowStruct()->IsChildOf(FFF_SnsPostTemplateRow::StaticStruct())))
        {
            return false;
        }
        if (!Test.TestTrue(TEXT("Project SNS table has authored rows"), Source->GetRowMap().Num() > 0))
        {
            return false;
        }

        // The authored asset may be held by an editor tab. An independent copy
        // proves the subsystem owns its runtime table instead of relying on that tab.
        UDataTable* Copy = DuplicateObject<UDataTable>(Source, GetTransientPackage(),
            MakeUniqueObjectName(GetTransientPackage(), UDataTable::StaticClass(), TEXT("FFSnsGcTest")));
        if (!Test.TestNotNull(TEXT("Independent SNS table copy exists"), Copy))
        {
            return false;
        }
        Copy->ClearFlags(RF_Standalone | RF_Public);
        Copy->SetFlags(RF_Transient);
        RuntimeCopy = Copy;
        Settings->SnsPostTemplateTable = Copy;
        return Test.TestFalse(TEXT("Runtime SNS copy is not rooted by the test"), Copy->IsRooted());
    }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdInsightsTemplatesSurviveGarbageCollection,
    "FestaFiesta.NPC.Insights.RuntimeTemplatesSurviveGarbageCollection",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FFF_CrowdInsightsTemplatesSurviveGarbageCollection::RunTest(const FString& Parameters)
{
    (void)Parameters;
    using namespace FFCrowdInsightsLifecycleTest;

    // The world shuts down before the global settings are restored.
    FScopedSnsTableSetting TableSetting;
    if (!TableSetting.Install(*this))
    {
        return false;
    }
    FTestWorldWrapper Fixture;
    if (!Fixture.CreateTestWorld(EWorldType::Game))
    {
        Fixture.ForwardErrorMessages(this);
        return false;
    }
    UWorld* World = Fixture.GetTestWorld();
    UGameInstance* Game = World ? World->GetGameInstance() : nullptr;
    UFF_CrowdInsightsSubsystem* Insights = Game ? Game->GetSubsystem<UFF_CrowdInsightsSubsystem>() : nullptr;
    if (!TestNotNull(TEXT("SNS game instance subsystem initializes"), Insights))
    {
        return false;
    }
    if (!Fixture.BeginPlayInTestWorld())
    {
        Fixture.ForwardErrorMessages(this);
        return false;
    }

    const TWeakObjectPtr<UWorld> WeakWorld(World);
    const TWeakObjectPtr<UFF_CrowdInsightsSubsystem> WeakInsights(Insights);
    auto CollectAndCheck = [&](const TCHAR* Phase)
    {
        // This control confirms that an actual collection took place.
        const TWeakObjectPtr<UDataTable> Control(NewObject<UDataTable>(GetTransientPackage(), NAME_None, RF_Transient));
        CollectGarbage(RF_NoFlags, true);
        TestFalse(*FString::Printf(TEXT("%s: unowned control was collected"), Phase), Control.IsValid());
        const bool bFixtureAlive = TestTrue(*FString::Printf(TEXT("%s: world and SNS subsystem survive"), Phase),
            WeakWorld.IsValid() && WeakInsights.IsValid());
        const bool bTableAlive = TestTrue(*FString::Printf(TEXT("%s: runtime SNS table survives"), Phase),
            TableSetting.RuntimeCopy.IsValid());
        return bFixtureAlive && bTableAlive;
    };
    auto GenerateAuthoredPost = [&](const TCHAR* Phase)
    {
        for (int32 Attempt = 0; Attempt < 32; ++Attempt)
        {
            const FFF_SnsPostRow Post = Insights->Debug_ForceGeneratePost();
            if (!Post.MessageTemplateId.IsNone() && !Post.Body.IsEmpty())
            {
                return TestTrue(*FString::Printf(TEXT("%s: authored SNS text is generated"), Phase), true);
            }
        }
        return TestTrue(*FString::Printf(TEXT("%s: authored SNS text is generated"), Phase), false);
    };

    if (!CollectAndCheck(TEXT("Before first post")))
    {
        return false;
    }
    const bool bFirstPostGenerated = GenerateAuthoredPost(TEXT("After Setup GC"));
    if (!CollectAndCheck(TEXT("After cached rows")))
    {
        return false;
    }
    const bool bSecondPostGenerated = GenerateAuthoredPost(TEXT("After cached-row GC"));
    return bFirstPostGenerated && bSecondPostGenerated;
}

#endif // WITH_DEV_AUTOMATION_TESTS
