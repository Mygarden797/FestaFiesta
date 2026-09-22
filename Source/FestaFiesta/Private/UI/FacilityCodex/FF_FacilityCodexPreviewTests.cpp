// FF_FacilityCodexPreviewTests.cpp
// 역할: 시설 도감의 동작과 저장된 화면 연결을 자동 검증한다.

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "UI/FacilityCodex/FF_FacilityCodexPreview.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/LocalPlayer.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "RenderingThread.h"
#include "Slate/WidgetRenderer.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
	AStaticMeshActor* FindPreviewActor(UWorld* World)
	{
		if (World)
			for (TActorIterator<AStaticMeshActor> It(World); It; ++It) return *It;
		return nullptr;
	}

	int32 CountPreviewActors(UWorld* World)
	{
		int32 Count = 0;
		if (World)
			for (TActorIterator<AStaticMeshActor> It(World); It; ++It) ++Count;
		return Count;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_FacilityCodexPreviewLifecycleTest,
	"FestaFiesta.FacilityCodex.UI.PreviewLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_FacilityCodexPreviewLifecycleTest::RunTest(const FString& Parameters)
{
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (!TestNotNull(TEXT("Engine mesh fixture"), Cube)) return false;
	TStrongObjectPtr<UFF_FacilityCodexPreview> Preview(NewObject<UFF_FacilityCodexPreview>());
	int32 ReadyEvents = 0;
	Preview->OnPreviewReadyChanged.AddLambda([&ReadyEvents]() { ++ReadyEvents; });
	Preview->ShowMesh(TSoftObjectPtr<UStaticMesh>(Cube));
	TestFalse(TEXT("An unbuilt widget retains its request without creating a preview world"), Preview->IsPreviewReady());
	TestNull(TEXT("No world is borrowed from the active game"), Preview->GetViewportWorld());

	TSharedPtr<SWidget> Slate = Preview->TakeWidget();
	UWorld* World = Preview->GetViewportWorld();
	if (!TestNotNull(TEXT("Rebuild creates the isolated preview world"), World)) return false;
	TestEqual(TEXT("The preview is a GamePreview world"), World->WorldType.GetValue(), EWorldType::GamePreview);
	TestNull(TEXT("No game instance is attached to the preview"), World->GetGameInstance());
	TestNull(TEXT("No gameplay mode is spawned"), World->GetAuthGameMode());
	TestTrue(TEXT("A mesh requested before Slate construction becomes ready"), Preview->IsPreviewReady());
	TestEqual(TEXT("A single ready notification is sent"), ReadyEvents, 1);
	TestEqual(TEXT("The world contains exactly one display mesh actor"), CountPreviewActors(World), 1);
	AStaticMeshActor* Actor = FindPreviewActor(World);
	if (!TestNotNull(TEXT("Display mesh actor"), Actor)) return false;
	TestTrue(TEXT("The preview actor is transient"), Actor->HasAnyFlags(RF_Transient));
	TestFalse(TEXT("Preview actor cannot tick"), Actor->PrimaryActorTick.bCanEverTick);
	TestFalse(TEXT("Preview actor has no collision"), Actor->GetActorEnableCollision());
	TestEqual(TEXT("Preview mesh collision is disabled"),
		Actor->GetStaticMeshComponent()->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
	TestFalse(TEXT("Preview mesh does not simulate physics"), Actor->GetStaticMeshComponent()->IsSimulatingPhysics());

	Preview->ShowMesh(TSoftObjectPtr<UStaticMesh>(Cube));
	TestEqual(TEXT("Showing the same ready mesh does not spawn a second actor"), CountPreviewActors(World), 1);
	TestEqual(TEXT("Same mesh does not flicker readiness"), ReadyEvents, 1);
	Preview->RotatePreview(450.f);
	TestEqual(TEXT("Rotation wraps around the centered mesh"), Preview->GetPreviewYaw(), 90.f);
	const double DistanceBeforeZoom = Preview->GetViewLocation().Length();
	Preview->SetZoom(.15f);
	TestTrue(TEXT("A positive zoom step moves the camera closer"), Preview->GetViewLocation().Length() < DistanceBeforeZoom);
	Preview->SetZoom(100.f);
	TestTrue(TEXT("Zoom cannot place the camera inside the normalized mesh"), Preview->GetViewLocation().Length() >= 139.999);
	Preview->ResetView();
	TestEqual(TEXT("Reset restores the initial yaw"), Preview->GetPreviewYaw(), 0.f);
	TestEqual(TEXT("Reset restores the initial zoom"), Preview->GetPreviewZoom(), 1.f);
	if (!FParse::Param(FCommandLine::Get(), TEXT("nullrhi")))
	{
		TGuardValue<TEnumAsByte<EAspectRatioAxisConstraint>> MaintainY(
			GetMutableDefault<ULocalPlayer>()->AspectRatioAxisConstraint, AspectRatio_MaintainYFOV);
		FWidgetRenderer Renderer(false, true);
		TStrongObjectPtr<UTextureRenderTarget2D> Target(FWidgetRenderer::CreateTargetFor(FVector2D(900, 300), TF_Bilinear, true));
		Renderer.DrawWidget(Target.Get(), Slate.ToSharedRef(), FVector2D(300, 300), .016f, false);
		Preview->RefreshViewForSize();
		const double SquareDistance = Preview->GetViewLocation().Length();
		Renderer.DrawWidget(Target.Get(), Slate.ToSharedRef(), FVector2D(900, 300), .016f, false);
		Preview->RefreshViewForSize();
		TestTrue(TEXT("MaintainYFOV keeps the model equally large when the stage widens to 3:1"),
			FMath::IsNearlyEqual(Preview->GetViewLocation().Length(), SquareDistance, .001));
		Renderer.DrawWidget(Target.Get(), Slate.ToSharedRef(), FVector2D(150, 300), .016f, false);
		Preview->RefreshViewForSize();
		TestTrue(TEXT("A portrait stage moves the camera back only when its horizontal field becomes narrower"),
			Preview->GetViewLocation().Length() > SquareDistance * 1.5);
		const FVector FramedLocation = Preview->GetViewLocation();
		Preview->RefreshViewForSize();
		TestTrue(TEXT("An unchanged layout retains the camera frame"), Preview->GetViewLocation().Equals(FramedLocation));
		TestEqual(TEXT("Resize preserves the preview actor"), FindPreviewActor(World), Actor);
		TestEqual(TEXT("Resize does not change preview readiness"), ReadyEvents, 1);
		FlushRenderingCommands();
	}

	Preview->ResetPreview();
	TestFalse(TEXT("Clearing selection restores fallback readiness"), Preview->IsPreviewReady());
	TestNull(TEXT("Clearing selection releases the hard mesh reference"), Preview->GetLoadedMesh());
	TestEqual(TEXT("Clearing selection destroys its display actor"), CountPreviewActors(World), 0);
	TestEqual(TEXT("Clearing selection emits one not-ready notification"), ReadyEvents, 2);
	Preview->OnPreviewReadyChanged.Clear();
	Slate.Reset();
	Preview->ReleaseSlateResources(true);
	TestNull(TEXT("Closing the viewport releases its world"), Preview->GetViewportWorld());
	Preview->ShowMesh(TSoftObjectPtr<UStaticMesh>(Cube));
	Preview->ResetPreview();
	Slate = Preview->TakeWidget();
	TestFalse(TEXT("A request canceled while closed is not resurrected by rebuilding"), Preview->IsPreviewReady());
	TestEqual(TEXT("Rebuilding after cancellation spawns no mesh"), CountPreviewActors(Preview->GetViewportWorld()), 0);
	Slate.Reset();
	Preview->ReleaseSlateResources(true);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_FacilityCodexPreviewBoundsTest,
	"FestaFiesta.FacilityCodex.UI.PreviewFacilityBounds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_FacilityCodexPreviewBoundsTest::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<UFF_FacilityCodexPreview> Preview(NewObject<UFF_FacilityCodexPreview>());
	TSharedPtr<SWidget> Slate = Preview->TakeWidget();
	const TCHAR* Paths[] = {
		TEXT("/Game/Asset/FacilitiesAsset_HKH/FacilityExpansion70/Booth/MiniArcheryRange/SM_FF_Booth_ActivityGame_MiniArcheryRange.SM_FF_Booth_ActivityGame_MiniArcheryRange"),
		TEXT("/Game/Asset/FacilitiesAsset_HKH/FacilityExpansion70/Lighting/LighthouseStreetlamp/SM_FF_Lighting_Streetlamp_LighthouseStreetlamp.SM_FF_Lighting_Streetlamp_LighthouseStreetlamp"),
		TEXT("/Game/Asset/FacilitiesAsset_HKH/FacilityExpansion70/Performance/AudienceDanceChallengeStage/SM_FF_Performance_ExperienceStage_AudienceDanceChallengeStage.SM_FF_Performance_ExperienceStage_AudienceDanceChallengeStage")
	};
	for (const TCHAR* Path : Paths)
	{
		UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, Path);
		if (!TestNotNull(TEXT("Production facility visual mesh"), Mesh)) continue;
		const FBoxSphereBounds SourceBounds = Mesh->GetBounds();
		Preview->ShowMesh(TSoftObjectPtr<UStaticMesh>(Mesh));
		if (!TestTrue(FString::Printf(TEXT("%s is ready"), *Mesh->GetName()), Preview->IsPreviewReady())) continue;
		TestEqual(TEXT("Changing facilities replaces the display actor"), CountPreviewActors(Preview->GetViewportWorld()), 1);
		AStaticMeshActor* Actor = FindPreviewActor(Preview->GetViewportWorld());
		if (!Actor) continue;
		for (const float Yaw : { 0.f, 37.f, 143.f })
		{
			Preview->RotatePreview(Yaw);
			const FTransform Transform = Actor->GetActorTransform();
			TestTrue(TEXT("An offset mesh pivot remains centered while rotating"),
				Transform.TransformPosition(SourceBounds.Origin).IsNearlyZero(.001));
			TestTrue(TEXT("Different facility sizes fit the same 100-unit sphere"),
				FMath::IsNearlyEqual(Transform.GetScale3D().GetMax() * SourceBounds.BoxExtent.Size(), 100.0, .001));
			TestFalse(TEXT("Preview camera stays finite"), Preview->GetViewLocation().ContainsNaN());
		}
		TestTrue(TEXT("Preview framing does not modify the source asset bounds"),
			Mesh->GetBounds().Origin.Equals(SourceBounds.Origin) && Mesh->GetBounds().BoxExtent.Equals(SourceBounds.BoxExtent));
	}
	Preview->ResetPreview();
	Slate.Reset();
	Preview->ReleaseSlateResources(true);
	return true;
}
#endif
