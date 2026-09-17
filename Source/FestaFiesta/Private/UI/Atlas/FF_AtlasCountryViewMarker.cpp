#include "UI/Atlas/FF_AtlasCountryViewMarker.h"

#include "Components/ArrowComponent.h"
#include "Components/BillboardComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "UI/Atlas/FF_AtlasGlobeActor.h"
#include "UI/Atlas/FF_AtlasCameraRig.h"
#include "UI/Atlas/FF_AtlasSettings.h"
#include "UI/Atlas/FF_FestivalAtlasCatalog.h"
#include "UI/FF_UILog.h"

#if WITH_EDITOR
#include "Camera/CameraActor.h"
#include "Editor.h"
#include "EngineUtils.h"
#include "FileHelpers.h"
#include "LevelEditorViewport.h"
#include "ScopedTransaction.h"
#endif

AFF_AtlasCountryViewMarker::AFF_AtlasCountryViewMarker()
{
	PrimaryActorTick.bCanEverTick = false;
	SetActorEnableCollision(false);
	SetActorHiddenInGame(true);
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("MarkerRoot")));
#if WITH_EDITORONLY_DATA
	bIsEditorOnlyActor = true;
	UBillboardComponent* Icon = CreateEditorOnlyDefaultSubobject<UBillboardComponent>(TEXT("ViewPoint"));
	Icon->SetupAttachment(GetRootComponent());
	Icon->bIsScreenSizeScaled = true;
	Icon->ScreenSize = 0.003f;
	UArrowComponent* Arrow = CreateEditorOnlyDefaultSubobject<UArrowComponent>(TEXT("SurfaceNormal"));
	Arrow->SetupAttachment(GetRootComponent());
	Arrow->ArrowColor = FColor(255, 128, 32);
	Arrow->ArrowSize = 2.f;
	Arrow->bIsScreenSizeScaled = true;
#endif
}

USceneComponent* AFF_AtlasCountryViewMarker::FindGlobeRoot() const
{
	if (!IsValid(Globe)) return nullptr;
	TInlineComponentArray<USceneComponent*> Components(Globe);
	for (USceneComponent* Component : Components)
	{
		if (Component->GetFName() == TEXT("GlobeRoot")) return Component;
	}
	return nullptr;
}

FFF_AtlasLatLong AFF_AtlasCountryViewMarker::GetMarkerLocation() const
{
	if (const USceneComponent* SpinRoot = FindGlobeRoot())
	{
		return FFF_AtlasGeo::UnitVectorToLatLong(SpinRoot->GetComponentTransform().InverseTransformPosition(GetActorLocation()));
	}
	return FFF_AtlasLatLong();
}

TArray<FString> AFF_AtlasCountryViewMarker::GetCountryOptions() const
{
	TArray<FString> Result;
	if (Catalog) for (const auto& Country : Catalog->GetCountries()) Result.Add(Country.CountryId.ToString());
	return Result;
}

void AFF_AtlasCountryViewMarker::SnapToSurface()
{
	USceneComponent* SpinRoot = FindGlobeRoot();
	if (!SpinRoot) return;
	const FTransform Frame = SpinRoot->GetComponentTransform();
	const FVector Direction = Frame.InverseTransformPosition(GetActorLocation()).GetSafeNormal();
	if (Direction.IsNearlyZero() || Direction.ContainsNaN()) return;
	const FVector Normal = Frame.TransformVectorNoScale(Direction).GetSafeNormal();
	SetActorLocationAndRotation(Frame.TransformPosition(Direction * (Globe->GetGlobeRadius() + FMath::Max(0.f, MarkerSurfaceOffset))), Normal.Rotation());
}

void AFF_AtlasCountryViewMarker::LoadCountryView()
{
#if WITH_EDITOR
	if (!GetWorld() || GetWorld()->IsGameWorld()) return;
	const FFF_AtlasCountryEntry* Country = Catalog ? Catalog->FindCountry(CountryId) : nullptr;
	USceneComponent* SpinRoot = FindGlobeRoot();
	if (!Country || !SpinRoot) { LastResult = TEXT("대상 지구·카탈로그·나라를 먼저 지정하세요."); return; }
	const FScopedTransaction Transaction(NSLOCTEXT("Atlas", "LoadCountryView", "국가 보기 마커 불러오기"));
	Modify();
	AttachToComponent(SpinRoot, FAttachmentTransformRules::KeepWorldTransform);
	ApproachFraction = Country->bOverrideInspectionView ? Country->InspectionApproachFraction : UFF_AtlasSettings::Get()->CountryInspectionApproachFraction;
	const FFF_AtlasLatLong Location = Country->bOverrideInspectionView ? Country->InspectionLocation : Country->Location;
	SetActorLocation(SpinRoot->GetComponentTransform().TransformPosition(FFF_AtlasGeo::LatLongToSurfacePosition(Location, Globe->GetGlobeRadius() + MarkerSurfaceOffset)));
	SnapToSurface();
	LastResult = TEXT("불러왔습니다. 이동 도구(W)로 마커를 옮긴 뒤 저장하세요.");
#endif
}

bool AFF_AtlasCountryViewMarker::ApplyCountryView()
{
#if WITH_EDITOR
	if (!GetWorld() || GetWorld()->IsGameWorld() || !Catalog || !FindGlobeRoot() || !FMath::IsFinite(ApproachFraction)) return false;
	FFF_AtlasCountryEntry* Country = Catalog->Countries.FindByPredicate([this](const auto& Entry) { return Entry.CountryId == CountryId && !CountryId.IsNone(); });
	const FVector Direction = FindGlobeRoot()->GetComponentTransform().InverseTransformPosition(GetActorLocation());
	if (!Country || Direction.ContainsNaN() || Direction.IsNearlyZero()) return false;
	const FScopedTransaction Transaction(NSLOCTEXT("Atlas", "SaveCountryView", "국가 보기 위치와 거리 설정"));
	Catalog->Modify();
	Country->bOverrideInspectionView = true;
	Country->InspectionLocation = GetMarkerLocation();
	Country->InspectionApproachFraction = FMath::Clamp(ApproachFraction, 0.f, 0.5f);
	Catalog->PostEditChange();
	Catalog->MarkPackageDirty();
	return true;
#else
	return false;
#endif
}

void AFF_AtlasCountryViewMarker::SaveCountryView()
{
#if WITH_EDITOR
	if (!ApplyCountryView()) { LastResult = TEXT("저장 실패: 편집 월드에서 지구·카탈로그·나라·위치를 확인하세요."); return; }
	const bool bSaved = UEditorLoadingAndSavingUtils::SavePackages({Catalog->GetOutermost()}, false);
	LastResult = bSaved ? TEXT("국가 보기 위치·접근 비율을 카탈로그에 저장했습니다. PIE에서 나라를 선택해 확인하세요.") : TEXT("메모리에 반영됐으나 파일 저장에 실패했습니다. 카탈로그를 직접 저장하세요.");
	UE_LOG(LogFestaFiestaUI, Log, TEXT("Country View %s: %s"), *CountryId.ToString(), *LastResult);
#endif
}

void AFF_AtlasCountryViewMarker::PreviewCountryView()
{
#if WITH_EDITOR
	if (!GetWorld() || GetWorld()->IsGameWorld() || !FindGlobeRoot() || !GCurrentLevelEditingViewportClient) return;
	FLevelEditorViewportClient* Viewport = GCurrentLevelEditingViewportClient;
	if (!Viewport->IsPerspective()) { LastResult = TEXT("원근 뷰포트에서 미리보기를 실행하세요."); return; }
	const FVector Center = Globe->GetActorLocation();
	const FVector Normal = (GetActorLocation() - Center).GetSafeNormal();
	if (Normal.ContainsNaN() || Normal.IsNearlyZero() || !FMath::IsFinite(ApproachFraction))
	{
		LastResult = TEXT("미리보기 실패: 먼저 저장된 보기를 불러오고 유효한 접근 비율을 입력하세요.");
		return;
	}
	const double WorldRadius = Globe->GetGlobeRadius() * Globe->GetActorScale3D().GetAbsMax();
	double BaseDistance = WorldRadius * 2.5;
	// 미리보기는 실제 나라 선택용 카메라의 기본 거리를 읽고, 카메라 액터를 수정하지 않는다.
	if (AFF_AtlasCameraRig* Rig = AFF_AtlasCameraRig::Find(this, false))
	{
		const ACameraActor* Camera = Rig->GetGlobeCamera();
		if (Camera) BaseDistance = FVector::Distance(Camera->GetActorLocation(), Center);
	}
	const double Distance = FMath::Max(BaseDistance * (1. - FMath::Clamp(ApproachFraction, 0.f, .5f)), WorldRadius * 1.35);
	const FVector Up = FindGlobeRoot()->GetUpVector();
	Viewport->SetViewLocation(Center + Normal * Distance);
	Viewport->SetViewRotation(FRotationMatrix::MakeFromXZ(-Normal, Up).Rotator());
	Viewport->Invalidate();
	LastResult = TEXT("지형 미리보기입니다. UI를 포함한 최종 구도는 저장 후 PIE에서 확인하세요.");
#endif
}

#if WITH_EDITOR
void AFF_AtlasCountryViewMarker::PostEditMove(bool bFinished)
{
	Super::PostEditMove(bFinished);
	if (bFinished) SnapToSurface();
}
#endif
