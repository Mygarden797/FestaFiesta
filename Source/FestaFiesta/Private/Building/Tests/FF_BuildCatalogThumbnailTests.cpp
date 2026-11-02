// FF_BuildCatalogThumbnailTests.cpp

// 썸네일 Bundle 선언, 비동기 프리로드 세대 보호, 카드 표시 폴백 계약을 검증한다.
// 흐름: 합성 FacilityDefinition 구성 -> 카탈로그 세션 구동 -> 로드/해제 전후 상태와 UI 반영 확인.
// 불변식: 실제 프로젝트 콘텐츠는 변경하지 않고 transient 정의·텍스처와 격리된 GameInstance만 사용한다.
// 실패/수명: Fixture는 assertion 동안 Root에 고정하고 종료 전에 핸들·객체 참조를 모두 해제한다.

#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AssetRegistry/AssetBundleData.h"
#include "Building/FF_BuildCatalogSubsystem.h"
#include "Building/UI/FF_BuildFacilityCard.h"
#include "Components/Image.h"
#include "Engine/AssetManager.h"
#include "Engine/GameInstance.h"
#include "Engine/StreamableManager.h"
#include "Engine/Texture2D.h"
#include "FF_FFZSFacilityDefinition.h"
#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"

namespace
{
	UTexture2D* MakeTransientThumbnail(const TCHAR* Name)
	{
		return UTexture2D::CreateTransient(
			1,
			1,
			PF_B8G8R8A8,
			FName(Name));
	}

	TSoftObjectPtr<UTexture2D>* FindDefaultThumbnailValue(
		FAutomationTestBase& Test,
		UFF_BuildFacilityCard& Card)
	{
		FProperty* DefaultThumbnailProperty = FindFProperty<FProperty>(
			UFF_BuildFacilityCard::StaticClass(),
			TEXT("DefaultThumbnail"));
		Test.TestNotNull(
			TEXT("Facility card exposes its default thumbnail setting"),
			DefaultThumbnailProperty);
		if (!DefaultThumbnailProperty)
		{
			return nullptr;
		}

		TSoftObjectPtr<UTexture2D>* DefaultValue =
			DefaultThumbnailProperty->ContainerPtrToValuePtr<TSoftObjectPtr<UTexture2D>>(&Card);
		Test.TestNotNull(
			TEXT("The facility card default thumbnail value is addressable"),
			DefaultValue);
		return DefaultValue;
	}

	bool BindThumbnailTestWidgets(
		FAutomationTestBase& Test,
		UFF_BuildFacilityCard& Card,
		UImage& ThumbnailImage,
		UTexture2D& DefaultThumbnail)
	{
		FObjectPropertyBase* ImageProperty = FindFProperty<FObjectPropertyBase>(
			UFF_BuildFacilityCard::StaticClass(),
			TEXT("Image_Thumbnail"));

		Test.TestNotNull(TEXT("Facility card exposes its thumbnail image binding"), ImageProperty);
		if (!ImageProperty)
		{
			return false;
		}

		ImageProperty->SetObjectPropertyValue_InContainer(&Card, &ThumbnailImage);
		TSoftObjectPtr<UTexture2D>* DefaultValue = FindDefaultThumbnailValue(Test, Card);
		if (!DefaultValue)
		{
			return false;
		}

		*DefaultValue = TSoftObjectPtr<UTexture2D>(&DefaultThumbnail);
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFF_BuildCatalogThumbnailDefinitionContractTest,
	"FestaFiesta.Building.Catalog.ThumbnailDefinitionContract",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FFF_BuildCatalogThumbnailDefinitionContractTest::RunTest(const FString& Parameters)
{
	// 리플렉션 필드와 Asset Bundle 메타데이터를 함께 고정해 선언은 남고 프리로드 경로만 깨지는 회귀를 막는다.
	(void)Parameters;

	FSoftObjectProperty* CatalogThumbnailProperty = FindFProperty<FSoftObjectProperty>(
		UFFZSFacilityDefinition::StaticClass(),
		TEXT("CatalogThumbnail"));
	TestNotNull(
		TEXT("Facility definitions expose CatalogThumbnail as a soft object reference"),
		CatalogThumbnailProperty);
	if (!CatalogThumbnailProperty)
	{
		return false;
	}

	TestEqual(
		TEXT("CatalogThumbnail accepts Texture2D assets"),
		CatalogThumbnailProperty->PropertyClass.Get(),
		UTexture2D::StaticClass());
	TestEqual(
		TEXT("CatalogThumbnail belongs directly to the facility definition asset"),
		CatalogThumbnailProperty->GetOwnerClass(),
		UFFZSFacilityDefinition::StaticClass());
	TestTrue(
		TEXT("CatalogThumbnail is editable in facility definition assets"),
		CatalogThumbnailProperty->HasAnyPropertyFlags(CPF_Edit));
	TestTrue(
		TEXT("CatalogThumbnail is readable from Blueprint"),
		CatalogThumbnailProperty->HasAllPropertyFlags(
			CPF_BlueprintVisible | CPF_BlueprintReadOnly));
	TestFalse(
		TEXT("CatalogThumbnail is presentation metadata, not SaveGame state"),
		CatalogThumbnailProperty->HasAnyPropertyFlags(CPF_SaveGame));
	// 프로퍼티 메타데이터 스캔은 에디터 전용 Asset Manager 기능이다. 런타임 빌드는 위의 리플렉션
	// 필드 계약을 검증하고, 에디터 자동화는 지정 Bundle에 Soft Path가 포함되는지 추가로 확인한다.
#if WITH_EDITOR
	TestEqual(
		TEXT("CatalogThumbnail participates in the build-catalog UI asset bundle"),
		CatalogThumbnailProperty->GetMetaData(TEXT("AssetBundles")),
		FString(TEXT("BuildCatalogUI")));

	UFFZSFacilityDefinition* Definition = NewObject<UFFZSFacilityDefinition>();
	UTexture2D* BundledThumbnail = MakeTransientThumbnail(TEXT("CatalogBundledThumbnail"));
	TestNotNull(TEXT("A transient facility definition can be created"), Definition);
	TestNotNull(TEXT("A transient bundled thumbnail can be created"), BundledThumbnail);
	if (Definition && BundledThumbnail)
	{
		Definition->CatalogThumbnail = TSoftObjectPtr<UTexture2D>(BundledThumbnail);

		FAssetBundleData BundleData;
		UAssetManager::Get().InitializeAssetBundlesFromMetadata(Definition, BundleData);
		FAssetBundleEntry* BundleEntry = BundleData.FindEntry(TEXT("BuildCatalogUI"));
		TestNotNull(
			TEXT("Asset Manager metadata scanning emits the build-catalog UI bundle"),
			BundleEntry);
		if (BundleEntry)
		{
			const FTopLevelAssetPath ThumbnailAssetPath =
				Definition->CatalogThumbnail.ToSoftObjectPath().GetAssetPath();
			TestTrue(
				TEXT("The generated bundle contains the configured catalog thumbnail"),
				BundleEntry->AssetPaths.Contains(ThumbnailAssetPath));
		}
	}
#endif

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFF_BuildCatalogThumbnailSessionLifetimeTest,
	"FestaFiesta.Building.Catalog.ThumbnailSessionLifetime",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FFF_BuildCatalogThumbnailSessionLifetimeTest::RunTest(const FString& Parameters)
{
	// 중첩 화면 세션의 참조 수가 0이 되는 마지막 Release에서만 썸네일 수명이 끝나는지 확인한다.
	(void)Parameters;

	// GameInstanceSubsystem은 UGameInstance Outer가 필요하다. 우발적인 자동화 GC가 격리된
	// 서브시스템을 무효화하지 않도록 전체 assertion 동안 두 Fixture 객체를 Root에 유지한다.
	TStrongObjectPtr<UGameInstance> TestGameInstance(NewObject<UGameInstance>());
	TestNotNull(TEXT("A transient game instance can own the catalog fixture"), TestGameInstance.Get());
	if (!TestGameInstance.IsValid())
	{
		return false;
	}
	TStrongObjectPtr<UFF_BuildCatalogSubsystem> Catalog(
		NewObject<UFF_BuildCatalogSubsystem>(TestGameInstance.Get()));
	TestNotNull(TEXT("A transient build catalog subsystem can be created"), Catalog.Get());
	if (!Catalog.IsValid())
	{
		return false;
	}

	TestEqual(
		TEXT("No thumbnail bundle is requested before a build session"),
		Catalog->GetThumbnailLoadState(),
		EFF_BuildCatalogThumbnailLoadState::NotRequested);

	Catalog->AcquireThumbnailPreload();
	TestEqual(
		TEXT("A session waits for the definition catalog before preloading thumbnails"),
		Catalog->GetThumbnailLoadState(),
		EFF_BuildCatalogThumbnailLoadState::WaitingForCatalog);

	Catalog->AcquireThumbnailPreload();
	Catalog->ReleaseThumbnailPreload();
	TestEqual(
		TEXT("Releasing one of two consumers keeps the thumbnail session alive"),
		Catalog->GetThumbnailLoadState(),
		EFF_BuildCatalogThumbnailLoadState::WaitingForCatalog);

	Catalog->ReleaseThumbnailPreload();
	TestEqual(
		TEXT("Releasing the final consumer returns the thumbnail session to idle"),
		Catalog->GetThumbnailLoadState(),
		EFF_BuildCatalogThumbnailLoadState::NotRequested);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFF_BuildCatalogLoadingScreenCompletionTest,
	"FestaFiesta.Building.Catalog.LoadingScreenCompletion",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FFF_BuildCatalogLoadingScreenCompletionTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	TStrongObjectPtr<UGameInstance> TestGameInstance(NewObject<UGameInstance>());
	TStrongObjectPtr<UFF_BuildCatalogSubsystem> Catalog(
		NewObject<UFF_BuildCatalogSubsystem>(TestGameInstance.Get()));

	TestFalse(TEXT("세션을 획득하지 않은 로딩 완료 요청은 로드를 시작하지 않는다"),
		Catalog->CompleteThumbnailPreloadForLoadingScreen());
	TestEqual(TEXT("완료 API가 숨은 세션 참조를 추가하지 않는다"), Catalog->ThumbnailPreloadSessionRefs, 0);
	TestEqual(TEXT("획득 전에는 정의 목록 상태도 유지한다"), Catalog->GetLoadState(),
		EFF_BuildCatalogLoadState::NotRequested);

	Catalog->AcquireThumbnailPreload();
	Catalog->LoadState = EFF_BuildCatalogLoadState::Loading;
	const uint64 Generation = Catalog->RequestGeneration;
	// 엔진도 할 일이 없는 combined 요청에 null 핸들을 반환하고 완료 알림만 다음 틱으로 미룬다.
	FStreamableAsyncLoadParams LoadParams;
	LoadParams.OnComplete = FStreamableDelegateWithHandle::CreateUObject(Catalog.Get(),
		&UFF_BuildCatalogSubsystem::HandleCatalogLoadCompleted, Generation);
	const TArray<TSharedPtr<FStreamableHandle>> EmptyHandles;
	Catalog->ActiveLoadHandle = UAssetManager::GetStreamableManager().CreateCombinedHandle(
		EmptyHandles, TEXT("CatalogLoadingScreenEmptyTest"),
		EStreamableManagerCombinedHandleOptions::None, MoveTemp(LoadParams));

	TestTrue(TEXT("게임 틱 없이 빈 정의 목록과 썸네일 준비를 마무리한다"),
		Catalog->CompleteThumbnailPreloadForLoadingScreen());
	TestEqual(TEXT("완료 뒤에도 로딩 소유자의 세션 참조 하나를 유지한다"), Catalog->ThumbnailPreloadSessionRefs, 1);
	const uint64 CompletedRevision = Catalog->GetCatalogRevision();
	Catalog->HandleCatalogLoadCompleted(nullptr, Generation);
	Catalog->HandleCatalogLoadCanceled(nullptr, Generation);
	TestEqual(TEXT("뒤늦은 같은 세대 완료/취소가 이미 완료된 목록을 다시 구성하지 않는다"),
		Catalog->GetCatalogRevision(), CompletedRevision);
	TestTrue(TEXT("중복 완료/취소 후에도 준비 상태를 유지한다"), Catalog->IsCatalogReady());
	TestTrue(TEXT("반복 완료 요청은 이미 준비된 결과를 재사용한다"),
		Catalog->CompleteThumbnailPreloadForLoadingScreen());
	TestEqual(TEXT("반복 완료 요청은 정의 목록을 새로고침하지 않는다"),
		Catalog->GetCatalogRevision(), CompletedRevision);

	Catalog->ReleaseThumbnailPreload();
	TestFalse(TEXT("마지막 참조 반환 뒤에는 완료 API가 세션을 되살리지 않는다"),
		Catalog->CompleteThumbnailPreloadForLoadingScreen());
	TestEqual(TEXT("마지막 참조 반환 뒤에는 썸네일 준비 상태도 해제된다"), Catalog->GetThumbnailLoadState(),
		EFF_BuildCatalogThumbnailLoadState::NotRequested);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFF_BuildCatalogLoadingScreenThumbnailHandleTest,
	"FestaFiesta.Building.Catalog.LoadingScreenThumbnailHandle",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FFF_BuildCatalogLoadingScreenThumbnailHandleTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	TStrongObjectPtr<UGameInstance> TestGameInstance(NewObject<UGameInstance>());
	TStrongObjectPtr<UFF_BuildCatalogSubsystem> Catalog(
		NewObject<UFF_BuildCatalogSubsystem>(TestGameInstance.Get()));
	TStrongObjectPtr<UTexture2D> Thumbnail(MakeTransientThumbnail(TEXT("CatalogLoadingScreenThumbnail")));
	if (!TestNotNull(TEXT("썸네일 핸들 검증용 임시 텍스처를 만든다"), Thumbnail.Get()))
	{
		return false;
	}

	Catalog->AcquireThumbnailPreload();
	Catalog->LoadState = EFF_BuildCatalogLoadState::Ready;
	Catalog->bThumbnailPreloadStarted = true;
	Catalog->ThumbnailLoadState = EFF_BuildCatalogThumbnailLoadState::Loading;
	const uint64 Generation = Catalog->ThumbnailRequestGeneration;
	FStreamableAsyncLoadParams LoadParams;
	LoadParams.TargetsToStream.Add(FSoftObjectPath(Thumbnail.Get()));
	LoadParams.OnComplete = FStreamableDelegateWithHandle::CreateUObject(Catalog.Get(),
		&UFF_BuildCatalogSubsystem::HandleThumbnailPreloadCompleted, Generation);
	LoadParams.OnCancel = FStreamableDelegateWithHandle::CreateUObject(Catalog.Get(),
		&UFF_BuildCatalogSubsystem::HandleThumbnailPreloadCanceled, Generation);
	const TSharedPtr<FStreamableHandle> Handle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
		MoveTemp(LoadParams), TEXT("CatalogLoadingScreenThumbnailTest"));
	Catalog->ThumbnailPreloadHandle = Handle;
	if (!TestTrue(TEXT("실제 Streamable 썸네일 요청 핸들이 만들어진다"), Handle.IsValid()))
	{
		Catalog->ReleaseThumbnailPreload();
		return false;
	}

	TestTrue(TEXT("게임 틱 없이 썸네일 핸들의 완료를 표시 상태에 반영한다"),
		Catalog->CompleteThumbnailPreloadForLoadingScreen());
	TestTrue(TEXT("로딩 완료 뒤에도 핸들이 텍스처를 잡고 있다"), Handle->IsActive());
	Catalog->HandleThumbnailPreloadCompleted(Handle, Generation);
	Catalog->HandleThumbnailPreloadCanceled(Handle, Generation);
	TestEqual(TEXT("같은 세대의 늦은 완료/취소가 준비된 썸네일을 되돌리지 않는다"),
		Catalog->GetThumbnailLoadState(), EFF_BuildCatalogThumbnailLoadState::Ready);
	TestTrue(TEXT("중복 알림 뒤에도 소유한 핸들을 유지한다"), Catalog->ThumbnailPreloadHandle == Handle);

	Catalog->ReleaseThumbnailPreload();
	TestFalse(TEXT("마지막 세션 해제에서만 핸들의 강한 참조를 반환한다"), Handle->IsActive());
	Catalog->HandleThumbnailPreloadCompleted(Handle, Generation);
	TestEqual(TEXT("해제 뒤 도착한 이전 세대 완료를 무시한다"), Catalog->GetThumbnailLoadState(),
		EFF_BuildCatalogThumbnailLoadState::NotRequested);

	Catalog->AcquireThumbnailPreload();
	Catalog->bThumbnailPreloadStarted = true;
	Catalog->ThumbnailLoadState = EFF_BuildCatalogThumbnailLoadState::Loading;
	FStreamableAsyncLoadParams CanceledLoadParams;
	CanceledLoadParams.TargetsToStream.Add(FSoftObjectPath(Thumbnail.Get()));
	CanceledLoadParams.bStartStalled = true;
	Catalog->ThumbnailPreloadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
		MoveTemp(CanceledLoadParams), TEXT("CatalogLoadingScreenCanceledTest"));
	if (TestTrue(TEXT("취소 경로 검증용 정지된 요청 핸들이 만들어진다"), Catalog->ThumbnailPreloadHandle.IsValid()))
	{
		Catalog->ThumbnailPreloadHandle->CancelHandle();
		TestFalse(TEXT("취소된 요청은 기다리거나 재시도하지 않고 실패로 마무리한다"),
			Catalog->CompleteThumbnailPreloadForLoadingScreen());
		TestEqual(TEXT("취소된 썸네일은 폴백 상태를 노출한다"), Catalog->GetThumbnailLoadState(),
			EFF_BuildCatalogThumbnailLoadState::Failed);
	}
	Catalog->ReleaseThumbnailPreload();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFF_BuildFacilityCardThumbnailSelectionTest,
	"FestaFiesta.Building.UI.FacilityCardThumbnailSelection",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FFF_BuildFacilityCardThumbnailSelectionTest::RunTest(const FString& Parameters)
{
	// 카드는 Asset Manager가 이미 올린 텍스처만 선택하고 Soft Path만 있는 상태에서는 폴백을 유지해야 한다.
	(void)Parameters;

	UFF_BuildFacilityCard* Card = NewObject<UFF_BuildFacilityCard>();
	UImage* ThumbnailImage = NewObject<UImage>(Card);
	UTexture2D* ExplicitThumbnail = MakeTransientThumbnail(TEXT("CatalogExplicitThumbnail"));
	UTexture2D* DefaultThumbnail = MakeTransientThumbnail(TEXT("CatalogDefaultThumbnail"));
	const TSoftObjectPtr<UTexture2D> UnresolvedThumbnail(FSoftObjectPath(TEXT(
		"/Game/FestaFiesta/Automation/UnresolvedCatalogThumbnail.UnresolvedCatalogThumbnail")));

	TestNotNull(TEXT("A native facility card can be created for the unit test"), Card);
	TestNotNull(TEXT("A thumbnail image can be created for the unit test"), ThumbnailImage);
	TestNotNull(TEXT("The explicit transient thumbnail can be created"), ExplicitThumbnail);
	TestNotNull(TEXT("The default transient thumbnail can be created"), DefaultThumbnail);
	if (!Card || !ThumbnailImage || !ExplicitThumbnail || !DefaultThumbnail)
	{
		return false;
	}

	if (!BindThumbnailTestWidgets(*this, *Card, *ThumbnailImage, *DefaultThumbnail))
	{
		return false;
	}

	TestEqual(
		TEXT("A loaded entry-specific thumbnail wins over a loaded fallback"),
		UFF_BuildFacilityCard::ResolveLoadedThumbnail(
			TSoftObjectPtr<UTexture2D>(ExplicitThumbnail),
			TSoftObjectPtr<UTexture2D>(DefaultThumbnail)),
		ExplicitThumbnail);
	TestEqual(
		TEXT("An unresolved entry-specific thumbnail uses the loaded fallback without loading"),
		UFF_BuildFacilityCard::ResolveLoadedThumbnail(
			UnresolvedThumbnail,
			TSoftObjectPtr<UTexture2D>(DefaultThumbnail)),
		DefaultThumbnail);
	TestNull(
		TEXT("No thumbnail is returned when neither reference is loaded"),
		UFF_BuildFacilityCard::ResolveLoadedThumbnail(
			UnresolvedThumbnail,
			TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT(
				"/Game/FestaFiesta/Automation/UnresolvedCatalogFallback.UnresolvedCatalogFallback")))));

	FFF_BuildCatalogEntry Entry;
	Entry.FacilityDefinitionId = TEXT("Booth_FoodStall_ThumbnailContract");
	Entry.Thumbnail = TSoftObjectPtr<UTexture2D>(ExplicitThumbnail);
	Entry.InstallCost = 400;
	Entry.bHasResolvedInstallCost = true;
	Entry.RecoveryCost = 200;
	Entry.bHasResolvedRecoveryCost = true;
	Card->Configure(Entry);
	TestEqual(
		TEXT("A configured facility card exposes its resolved install and recovery costs on hover"),
		Card->GetToolTipText().ToString(),
		FString(TEXT("건설 비용: 400 Gold\n회수 비용: 200 Gold")));

	FFF_BuildCatalogEntry PartiallyResolvedEntry = Entry;
	PartiallyResolvedEntry.bHasResolvedRecoveryCost = false;
	TestEqual(
		TEXT("An unavailable recovery cost does not hide a resolved install cost"),
		UFF_BuildFacilityCard::MakeFacilityCostToolTip(PartiallyResolvedEntry).ToString(),
		FString(TEXT("건설 비용: 400 Gold\n회수 비용: 가격 정보 없음")));
	PartiallyResolvedEntry.bHasResolvedInstallCost = false;
	PartiallyResolvedEntry.bHasResolvedRecoveryCost = true;
	TestEqual(
		TEXT("An unavailable install cost does not hide a resolved recovery cost"),
		UFF_BuildFacilityCard::MakeFacilityCostToolTip(PartiallyResolvedEntry).ToString(),
		FString(TEXT("건설 비용: 가격 정보 없음\n회수 비용: 200 Gold")));
	TestEqual(
		TEXT("An entry-specific thumbnail takes priority over the card default"),
		ThumbnailImage->GetBrush().GetResourceObject(),
		static_cast<UObject*>(ExplicitThumbnail));

	Entry.Thumbnail.Reset();
	Card->Configure(Entry);
	TestEqual(
		TEXT("A null entry thumbnail falls back to the card default"),
		ThumbnailImage->GetBrush().GetResourceObject(),
		static_cast<UObject*>(DefaultThumbnail));

	Entry.Thumbnail = UnresolvedThumbnail;
	Card->Configure(Entry);
	TestEqual(
		TEXT("A non-null but unloaded entry thumbnail still uses the loaded card default"),
		ThumbnailImage->GetBrush().GetResourceObject(),
		static_cast<UObject*>(DefaultThumbnail));

	TSoftObjectPtr<UTexture2D>* DefaultValue = FindDefaultThumbnailValue(*this, *Card);
	if (DefaultValue)
	{
		DefaultValue->Reset();
		Card->RefreshThumbnail();
		TestNull(
			TEXT("Refreshing with no loaded thumbnail clears the previously displayed brush"),
			ThumbnailImage->GetBrush().GetResourceObject());
	}

	Entry.InstallCost = 0;
	Entry.bHasResolvedInstallCost = false;
	Entry.RecoveryCost = 0;
	Entry.bHasResolvedRecoveryCost = false;
	Card->Configure(Entry);
	TestEqual(
		TEXT("A card never shows a misleading zero when price data is unavailable"),
		Card->GetToolTipText().ToString(),
		FString(TEXT("건설 비용: 가격 정보 없음\n회수 비용: 가격 정보 없음")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFF_BuildFacilityCardFreeInstallCostFallbackTest,
	"FestaFiesta.Building.UI.FacilityCardFreeInstallCostFallback",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FFF_BuildFacilityCardFreeInstallCostFallbackTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	UFF_BuildFacilityCard* Card = NewObject<UFF_BuildFacilityCard>();
	if (!TestNotNull(TEXT("A native facility card supports the text tooltip fallback"), Card))
	{
		return false;
	}
	FFF_BuildCatalogEntry Entry;
	Entry.FacilityDefinitionId = TEXT("Booth_FoodStall_FreeTooltipTest");
	Entry.InstallCost = 0;
	Entry.bHasResolvedInstallCost = true;
	Entry.RecoveryCost = 0;
	Entry.bHasResolvedRecoveryCost = true;
	Card->Configure(Entry);
	TestEqual(TEXT("The text fallback displays resolved zero install and recovery prices"),
		Card->GetToolTipText().ToString(), FString(TEXT("건설 비용: 0 Gold\n회수 비용: 0 Gold")));

	Entry.InstallCost = -1;
	Card->Configure(Entry);
	TestEqual(TEXT("A negative install price is unavailable even with the resolved flag"),
		Card->GetToolTipText().ToString(),
		FString(TEXT("건설 비용: 가격 정보 없음\n회수 비용: 0 Gold")));

	Entry.InstallCost = 0;
	Entry.bHasResolvedInstallCost = false;
	Card->Configure(Entry);
	TestEqual(TEXT("An unresolved zero install price is distinct from a free facility"),
		Card->GetToolTipText().ToString(),
		FString(TEXT("건설 비용: 가격 정보 없음\n회수 비용: 0 Gold")));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
