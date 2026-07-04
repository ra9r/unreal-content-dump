#include "DumpContentCommandlet.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Engine/Blueprint.h"
#include "Engine/DataAsset.h"
#include "Engine/DataTable.h"
#include "Engine/Level.h"
#include "Engine/LevelScriptBlueprint.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "Engine/UserDefinedEnum.h"
#include "Engine/UserDefinedStruct.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraphUtilities.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UObject/UnrealType.h"

DEFINE_LOG_CATEGORY_STATIC(LogContentDump, Log, All);

namespace
{
	// Remove transient compiler-state properties from exported node T3D text.
	// ErrorType / bHasCompilerMessage / ErrorMsg are recomputed during this
	// headless per-asset load and false-positive on cross-asset references, so
	// they are misleading noise in a static dump. Each is a whole-line node
	// property, so drop matching lines.
	FString StripCompilerNoise(const FString& In)
	{
		TArray<FString> Lines;
		In.ParseIntoArrayLines(Lines, /*CullEmpty=*/false);

		FString Out;
		Out.Reserve(In.Len());
		for (const FString& Line : Lines)
		{
			const FString Trimmed = Line.TrimStart();
			if (Trimmed.StartsWith(TEXT("ErrorType=")) ||
				Trimmed.StartsWith(TEXT("bHasCompilerMessage=")) ||
				Trimmed.StartsWith(TEXT("ErrorMsg=")))
			{
				continue;
			}
			Out += Line;
			Out += TEXT("\n");
		}
		return Out;
	}

	void AppendGraphs(const TArray<UEdGraph*>& Graphs, const TCHAR* Section, FString& Out)
	{
		for (UEdGraph* Graph : Graphs)
		{
			if (!Graph)
			{
				continue;
			}

			Out += FString::Printf(TEXT("\n=== %s: %s ===\n"), Section, *Graph->GetName());

			TSet<UObject*> NodeSet;
			NodeSet.Reserve(Graph->Nodes.Num());
			for (UEdGraphNode* Node : Graph->Nodes)
			{
				if (Node)
				{
					NodeSet.Add(Node);
				}
			}

			FString Exported;
			FEdGraphUtilities::ExportNodesToText(NodeSet, Exported);
			Out += StripCompilerNoise(Exported);
			Out += TEXT("\n");
		}
	}

	// Iterate every property on an object and export its current value as text.
	FString DumpObjectProperties(UObject* Obj)
	{
		FString Out;
		if (!Obj)
		{
			return Out;
		}
		for (TFieldIterator<FProperty> It(Obj->GetClass()); It; ++It)
		{
			FProperty* Prop = *It;
			FString Value;
			Prop->ExportText_InContainer(0, Value, Obj, /*Delta=*/nullptr, /*Parent=*/Obj, PPF_None);
			Out += FString::Printf(TEXT("- %s (%s) = %s\n"),
				*Prop->GetAuthoredName(), *Prop->GetCPPType(), *Value);
		}
		return Out;
	}

	// Export inherited/native CDO property values that the Blueprint OVERRIDES relative to its
	// parent class default — i.e. the "Class Defaults" panel content (e.g. a GameMode's
	// DefaultPawnClass/HUDClass, an Actor's bReplicates). BP-added variables are listed under
	// "## Variables" already, so they are skipped here. Only properties whose value differs from
	// the parent class's CDO are emitted, to avoid dumping every inherited engine default.
	FString DumpClassDefaultOverrides(UClass* GenClass, UObject* CDO)
	{
		FString Out;
		if (!GenClass || !CDO)
		{
			return Out;
		}

		UClass* SuperClass = GenClass->GetSuperClass();
		UObject* SuperCDO = SuperClass ? SuperClass->GetDefaultObject() : nullptr;

		for (TFieldIterator<FProperty> It(GenClass); It; ++It)
		{
			FProperty* Prop = *It;

			// BP-added variable (owned by the generated class itself) — already dumped.
			if (Prop->GetOwnerClass() == GenClass)
			{
				continue;
			}
			if (Prop->HasAnyPropertyFlags(CPF_Transient | CPF_DuplicateTransient | CPF_Deprecated))
			{
				continue;
			}
			// Unchanged from the parent default — not an override.
			if (SuperCDO && Prop->Identical_InContainer(CDO, SuperCDO))
			{
				continue;
			}

			FString Value;
			Prop->ExportText_InContainer(0, Value, CDO, /*Delta=*/nullptr, /*Parent=*/CDO, PPF_None);
			Out += FString::Printf(TEXT("- %s (%s) = %s\n"), *Prop->GetName(), *Prop->GetCPPType(), *Value);
		}
		return Out;
	}

	FString DumpBlueprint(UBlueprint* BP)
	{
		FString Out;
		Out += FString::Printf(TEXT("# Blueprint: %s\n"), *BP->GetName());
		Out += FString::Printf(TEXT("Path: %s\n"), *BP->GetPathName());
		Out += FString::Printf(TEXT("ParentClass: %s\n"),
			BP->ParentClass ? *BP->ParentClass->GetPathName() : TEXT("None"));

		Out += TEXT("\n## Implemented Interfaces\n");
		for (const FBPInterfaceDescription& Iface : BP->ImplementedInterfaces)
		{
			Out += FString::Printf(TEXT("- %s\n"),
				Iface.Interface ? *Iface.Interface->GetPathName() : TEXT("None"));
		}

		// Variable defaults live on the generated class's CDO, not in NewVariables, so read them there.
		UClass* GenClass = BP->GeneratedClass;
		UObject* CDO = GenClass ? GenClass->GetDefaultObject() : nullptr;

		Out += TEXT("\n## Variables\n");
		for (const FBPVariableDescription& Var : BP->NewVariables)
		{
			FString Default;
			if (CDO)
			{
				if (FProperty* Prop = GenClass->FindPropertyByName(Var.VarName))
				{
					Prop->ExportText_InContainer(0, Default, CDO, /*Delta=*/nullptr, /*Parent=*/CDO, PPF_None);
				}
			}
			Out += FString::Printf(TEXT("- %s : %s%s = %s\n"),
				*Var.VarName.ToString(),
				*Var.VarType.PinCategory.ToString(),
				Var.VarType.IsArray() ? TEXT("[]") : TEXT(""),
				*Default);
		}

		Out += TEXT("\n## Components (SimpleConstructionScript)\n");
		if (BP->SimpleConstructionScript)
		{
			for (USCS_Node* Node : BP->SimpleConstructionScript->GetAllNodes())
			{
				if (!Node)
				{
					continue;
				}
				Out += FString::Printf(TEXT("- %s : %s\n"),
					*Node->GetVariableName().ToString(),
					Node->ComponentClass ? *Node->ComponentClass->GetName() : TEXT("?"));
			}
		}

		Out += TEXT("\n## Class Defaults (overrides vs parent)\n");
		Out += DumpClassDefaultOverrides(GenClass, CDO);

		AppendGraphs(BP->UbergraphPages, TEXT("EventGraph"), Out);
		AppendGraphs(BP->FunctionGraphs, TEXT("Function"), Out);
		AppendGraphs(BP->MacroGraphs, TEXT("Macro"), Out);

		// Interface implementations with a graph body live in
		// FBPInterfaceDescription::Graphs, not FunctionGraphs — without this
		// they are invisible in the dump.
		for (const FBPInterfaceDescription& Iface : BP->ImplementedInterfaces)
		{
			const FString Section = FString::Printf(TEXT("Interface Graph: %s"),
				Iface.Interface ? *Iface.Interface->GetName() : TEXT("None"));
			AppendGraphs(Iface.Graphs, *Section, Out);
		}

		return Out;
	}

	FString DumpStruct(UUserDefinedStruct* Struct)
	{
		FString Out;
		Out += FString::Printf(TEXT("# Struct: %s\n"), *Struct->GetName());
		Out += FString::Printf(TEXT("Path: %s\n\n## Fields\n"), *Struct->GetPathName());
		for (TFieldIterator<FProperty> It(Struct); It; ++It)
		{
			FProperty* Prop = *It;
			Out += FString::Printf(TEXT("- %s : %s\n"), *Prop->GetAuthoredName(), *Prop->GetCPPType());
		}
		return Out;
	}

	FString DumpEnum(UUserDefinedEnum* Enum)
	{
		FString Out;
		Out += FString::Printf(TEXT("# Enum: %s\n"), *Enum->GetName());
		Out += FString::Printf(TEXT("Path: %s\n\n## Values\n"), *Enum->GetPathName());
		// Last entry is the implicit _MAX sentinel; skip it.
		const int32 Num = Enum->NumEnums();
		for (int32 i = 0; i < Num - 1; ++i)
		{
			Out += FString::Printf(TEXT("- %s = %lld (%s)\n"),
				*Enum->GetNameStringByIndex(i),
				(long long)Enum->GetValueByIndex(i),
				*Enum->GetDisplayNameTextByIndex(i).ToString());
		}
		return Out;
	}

	FString DumpDataTable(UDataTable* Table)
	{
		FString Out;
		Out += FString::Printf(TEXT("# DataTable: %s\n"), *Table->GetName());
		Out += FString::Printf(TEXT("Path: %s\n"), *Table->GetPathName());
		Out += FString::Printf(TEXT("RowStruct: %s\n\n## Rows (JSON)\n"),
			Table->RowStruct ? *Table->RowStruct->GetPathName() : TEXT("None"));
		Out += Table->GetTableAsJSON();
		Out += TEXT("\n");
		return Out;
	}

	FString DumpDataAsset(UDataAsset* Asset)
	{
		FString Out;
		Out += FString::Printf(TEXT("# DataAsset: %s\n"), *Asset->GetName());
		Out += FString::Printf(TEXT("Path: %s\n"), *Asset->GetPathName());
		Out += FString::Printf(TEXT("Class: %s\n\n## Properties\n"), *Asset->GetClass()->GetPathName());
		Out += DumpObjectProperties(Asset);
		return Out;
	}
}

UDumpContentCommandlet::UDumpContentCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
}

int32 UDumpContentCommandlet::Main(const FString& Params)
{
	FString OutDir;
	if (!FParse::Value(*Params, TEXT("OutDir="), OutDir))
	{
		OutDir = FPaths::Combine(FPaths::ProjectDir(), TEXT("Docs"), TEXT("ContentText"));
	}
	OutDir = FPaths::ConvertRelativePathToFull(OutDir);

	FString RootPath;
	if (!FParse::Value(*Params, TEXT("Root="), RootPath))
	{
		RootPath = TEXT("/Game");
	}

	UE_LOG(LogContentDump, Display, TEXT("Dumping '%s' -> '%s'"), *RootPath, *OutDir);

	FAssetRegistryModule& ARM =
		FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	IAssetRegistry& AR = ARM.Get();
	AR.SearchAllAssets(/*bSynchronousSearch=*/true);

	// Only logic-bearing asset classes, so we never load pure-art assets just to skip them.
	// Maps (UWorld) are included solely for their Level Blueprint.
	FARFilter Filter;
	Filter.PackagePaths.Add(FName(*RootPath));
	Filter.bRecursivePaths = true;
	Filter.bRecursiveClasses = true;
	Filter.ClassPaths.Add(UBlueprint::StaticClass()->GetClassPathName());
	Filter.ClassPaths.Add(UWorld::StaticClass()->GetClassPathName());
	Filter.ClassPaths.Add(UUserDefinedStruct::StaticClass()->GetClassPathName());
	Filter.ClassPaths.Add(UUserDefinedEnum::StaticClass()->GetClassPathName());
	Filter.ClassPaths.Add(UDataTable::StaticClass()->GetClassPathName());
	Filter.ClassPaths.Add(UDataAsset::StaticClass()->GetClassPathName());

	TArray<FAssetData> Assets;
	AR.GetAssets(Filter, Assets);

	UE_LOG(LogContentDump, Display, TEXT("Found %d candidate asset(s)"), Assets.Num());

	int32 NumDumped = 0;
	for (const FAssetData& AssetData : Assets)
	{
		UObject* Obj = AssetData.GetAsset();
		if (!Obj)
		{
			continue;
		}

		FString Text;
		const TCHAR* Ext = TEXT(".txt");
		if (UWorld* World = Cast<UWorld>(Obj))
		{
			// A map's Level Blueprint is a sub-object of its persistent ULevel, not a
			// standalone asset, so it is only reachable through the loaded world.
			// bDontCreate: levels whose script BP was never opened have none — skip them.
			ULevelScriptBlueprint* LevelBP = World->PersistentLevel
				? World->PersistentLevel->GetLevelScriptBlueprint(/*bDontCreate=*/true)
				: nullptr;
			if (!LevelBP)
			{
				continue;
			}
			Text = FString::Printf(TEXT("# Level Blueprint of map: %s\n\n"), *World->GetPathName());
			Text += DumpBlueprint(LevelBP);
			Ext = TEXT(".levelbp.txt");
		}
		else if (UBlueprint* BP = Cast<UBlueprint>(Obj))
		{
			Text = DumpBlueprint(BP);
			Ext = TEXT(".bp.txt");
		}
		else if (UUserDefinedStruct* Struct = Cast<UUserDefinedStruct>(Obj))
		{
			Text = DumpStruct(Struct);
			Ext = TEXT(".struct.txt");
		}
		else if (UUserDefinedEnum* Enum = Cast<UUserDefinedEnum>(Obj))
		{
			Text = DumpEnum(Enum);
			Ext = TEXT(".enum.txt");
		}
		else if (UDataTable* Table = Cast<UDataTable>(Obj))
		{
			Text = DumpDataTable(Table);
			Ext = TEXT(".datatable.txt");
		}
		else if (UDataAsset* DataAsset = Cast<UDataAsset>(Obj))
		{
			Text = DumpDataAsset(DataAsset);
			Ext = TEXT(".dataasset.txt");
		}
		else
		{
			continue;
		}

		// Mirror the /Game tree under OutDir.
		FString Rel = AssetData.PackageName.ToString();
		Rel.RemoveFromStart(TEXT("/Game/"));
		const FString OutPath = FPaths::Combine(OutDir, Rel) + Ext;

		IFileManager::Get().MakeDirectory(*FPaths::GetPath(OutPath), /*Tree=*/true);
		if (FFileHelper::SaveStringToFile(Text, *OutPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
		{
			++NumDumped;
		}
		else
		{
			UE_LOG(LogContentDump, Warning, TEXT("Failed to write %s"), *OutPath);
		}
	}

	UE_LOG(LogContentDump, Display, TEXT("Done. Dumped %d asset(s) to %s"), NumDumped, *OutDir);
	return 0;
}
