#pragma once

#include "CoreMinimal.h"

#if WITH_EDITOR
class UDataTable;

namespace FFFacilitySynergyTableBuilder
{
/** Resolves every enabled row's actual primary asset ID; display names are designer authored. */
bool ValidateReferencedDefinitions(const UDataTable& Table, FString& OutFailureReason);

/** Preflights the merged data before mutation; never replaces existing row keys or saves an asset. */
bool AppendMissingRows(UDataTable& Destination, const UDataTable& Source,
	TArray<FName>& OutAddedRows, FString& OutFailureReason);
}
#endif
