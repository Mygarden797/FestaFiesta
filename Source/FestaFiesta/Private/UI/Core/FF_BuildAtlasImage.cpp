#include "UI/Core/FF_BuildAtlasImage.h"

#include "Engine/Texture2D.h"

bool FFBuildAtlas::TryApply(FSlateBrush& Brush, UTexture2D* Texture,
	const int32 Columns, const int32 Rows, const int32 Index)
{
	if (!Texture || Columns < 1 || Columns > 128 || Rows < 1 || Rows > 128 ||
		Index < 0 || Index >= Columns * Rows) return false;
	const int32 Column = Index % Columns;
	const int32 Row = Index / Columns;
	Brush.SetResourceObject(Texture);
	Brush.SetUVRegion(FBox2f(
		FVector2f(float(Column) / Columns, float(Row) / Rows),
		FVector2f(float(Column + 1) / Columns, float(Row + 1) / Rows)));
	return true;
}

bool UFF_BuildAtlasImage::SetAtlasIndex(const int32 NewIndex)
{
	FSlateBrush Updated = GetBrush();
	if (!FFBuildAtlas::TryApply(Updated, AtlasTexture, AtlasColumns, AtlasRows, NewIndex)) return false;
	AtlasIndex = NewIndex;
	SetBrush(Updated);
	return true;
}

bool UFF_BuildAtlasImage::SetAtlasSource(UTexture2D* NewTexture, const int32 NewColumns,
	const int32 NewRows, const int32 NewIndex)
{
	FSlateBrush Updated = GetBrush();
	if (!FFBuildAtlas::TryApply(Updated, NewTexture, NewColumns, NewRows, NewIndex)) return false;
	AtlasTexture = NewTexture;
	AtlasColumns = NewColumns;
	AtlasRows = NewRows;
	AtlasIndex = NewIndex;
	SetBrush(Updated);
	return true;
}

void UFF_BuildAtlasImage::SynchronizeProperties()
{
	Super::SynchronizeProperties();
	UpdateAtlasBrush();
}

void UFF_BuildAtlasImage::UpdateAtlasBrush()
{
	FSlateBrush Updated = GetBrush();
	if (FFBuildAtlas::TryApply(Updated, AtlasTexture, AtlasColumns, AtlasRows, AtlasIndex)) SetBrush(Updated);
}
