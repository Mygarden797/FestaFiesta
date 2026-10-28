#include "Animation/FF_NPCAnimationEditorLibrary.h"

#include "Animation/BlendSpace.h"

bool UFF_NPCAnimationEditorLibrary::RebuildBlendSpaceRuntimeData(UBlendSpace* BlendSpace)
{
	if (BlendSpace == nullptr)
	{
		return false;
	}

	BlendSpace->Modify();
	BlendSpace->ValidateSampleData();
	BlendSpace->ResampleData();
	BlendSpace->MarkPackageDirty();

	const FBlendSpaceData& Data = BlendSpace->GetBlendSpaceData();
	return Data.Segments.Num() > 0 || Data.Triangles.Num() > 0;
}
