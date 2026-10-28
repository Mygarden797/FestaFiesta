// Editor entry points for automatic Zone welding. Opening/previewing is read-only.
#pragma once

#include "CoreMinimal.h"

class UWorld;
class FTransactionObjectEvent;

namespace FFZASelectedWeldTool
{
    void Open(UWorld* ExpectedWorld, float InitialToleranceCm = 50.0f);
    void RegisterMenus(void* Owner);
    void HandleObjectTransacted(UObject* Object, const FTransactionObjectEvent& Event);
    void HandleUndoRedo();
    void Shutdown();
}
