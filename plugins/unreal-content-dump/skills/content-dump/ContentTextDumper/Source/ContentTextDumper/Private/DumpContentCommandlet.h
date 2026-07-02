#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "DumpContentCommandlet.generated.h"

/**
 * Editor commandlet that dumps logic-bearing Content assets to readable text.
 *
 * Run headless:
 *   UnrealEditor-Cmd <Project.uproject> -run=DumpContent -OutDir=<dir> [-Root=/Game/Path]
 *
 * For each Blueprint under the root it writes parent class, interfaces, components,
 * variables (+ defaults) and every graph (event/function/macro) using the engine's own
 * FEdGraphUtilities::ExportNodesToText, so the output is byte-for-byte what the editor's
 * copy-nodes-as-text produces.
 */
UCLASS()
class UDumpContentCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UDumpContentCommandlet();

	virtual int32 Main(const FString& Params) override;
};
