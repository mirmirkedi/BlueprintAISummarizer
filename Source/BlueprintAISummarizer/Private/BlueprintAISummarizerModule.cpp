#include "Algo/Sort.h"
#include "BlueprintEditor.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "Editor.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "GraphEditorModule.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Modules/ModuleManager.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "UObject/Field.h"
#include "UObject/UnrealType.h"

#define LOCTEXT_NAMESPACE "FBlueprintAISummarizerModule"

namespace BlueprintAISummarizer
{
	constexpr int32 MaxNodesPerGraph = 120;
	constexpr int32 MaxPinsPerNode = 14;
	constexpr int32 MaxConnectionsPerPin = 8;
	constexpr int32 MaxDefaultValueLen = 80;

	static FString Shorten(const FString& Text, const int32 MaxLen)
	{
		FString Clean = Text;
		Clean.ReplaceInline(TEXT("\r"), TEXT(" "));
		Clean.ReplaceInline(TEXT("\n"), TEXT(" "));
		Clean.TrimStartAndEndInline();

		if (Clean.Len() <= MaxLen)
		{
			return Clean;
		}

		return Clean.Left(FMath::Max(0, MaxLen - 3)) + TEXT("...");
	}

	static FString PinDirectionToString(const EEdGraphPinDirection Direction)
	{
		switch (Direction)
		{
		case EGPD_Input:
			return TEXT("in");
		case EGPD_Output:
			return TEXT("out");
		default:
			return TEXT("io");
		}
	}

	static FString PinTypeToString(const FEdGraphPinType& PinType)
	{
		FString TypeName = PinType.PinCategory.ToString();
		if (!PinType.PinSubCategory.IsNone())
		{
			TypeName += TEXT(":") + PinType.PinSubCategory.ToString();
		}

		if (PinType.PinSubCategoryObject.IsValid())
		{
			TypeName += FString::Printf(TEXT("<%s>"), *PinType.PinSubCategoryObject->GetName());
		}

		if (PinType.ContainerType != EPinContainerType::None)
		{
			TypeName += FString::Printf(TEXT("[%s]"), *UEnum::GetValueAsString(PinType.ContainerType));
		}

		return TypeName;
	}

	static FString NodeTitle(const UEdGraphNode* Node)
	{
		if (!Node)
		{
			return TEXT("<null>");
		}

		FString Title = Node->GetNodeTitle(ENodeTitleType::ListView).ToString();
		if (Title.IsEmpty())
		{
			Title = Node->GetName();
		}

		return Shorten(Title, 96);
	}

	static void AppendNode(const UEdGraphNode* Node, const TSet<const UEdGraphNode*>& SelectedNodeSet, FStringBuilderBase& Out)
	{
		if (!Node)
		{
			return;
		}

		Out.Appendf(TEXT("- [%s] %s"), *Node->GetClass()->GetName(), *NodeTitle(Node));
		Out.Appendf(TEXT(" pos=(%d,%d)"), Node->NodePosX, Node->NodePosY);

		const FString Comment = Shorten(Node->NodeComment, 160);
		if (!Comment.IsEmpty())
		{
			Out.Appendf(TEXT(" // %s"), *Comment);
		}
		Out.Append(TEXT("\n"));

		int32 PinCount = 0;
		for (const UEdGraphPin* Pin : Node->Pins)
		{
			if (!Pin || Pin->PinName.IsNone())
			{
				continue;
			}

			if (PinCount >= MaxPinsPerNode)
			{
				Out.Appendf(TEXT("  - ... %d more pins omitted\n"), Node->Pins.Num() - PinCount);
				break;
			}

			++PinCount;
			Out.Appendf(
				TEXT("  - %s %s: %s"),
				*PinDirectionToString(Pin->Direction),
				*Pin->PinName.ToString(),
				*PinTypeToString(Pin->PinType));

			const FString DefaultValue = Shorten(Pin->DefaultValue, MaxDefaultValueLen);
			if (!DefaultValue.IsEmpty())
			{
				Out.Appendf(TEXT(" = %s"), *DefaultValue);
			}

			if (!Pin->LinkedTo.IsEmpty())
			{
				TArray<const UEdGraphPin*> SelectedLinks;
				TArray<const UEdGraphPin*> ExternalLinks;

				for (const UEdGraphPin* LinkedPin : Pin->LinkedTo)
				{
					if (!LinkedPin || !LinkedPin->GetOwningNode())
					{
						continue;
					}

					(SelectedNodeSet.Contains(LinkedPin->GetOwningNode()) ? SelectedLinks : ExternalLinks).Add(LinkedPin);
				}

				auto AppendLinks = [&Out](const TCHAR* Prefix, const TArray<const UEdGraphPin*>& Links)
				{
					if (Links.IsEmpty())
					{
						return;
					}

					Out.Append(Prefix);
					for (int32 LinkIndex = 0; LinkIndex < Links.Num(); ++LinkIndex)
					{
						if (LinkIndex > 0)
						{
							Out.Append(TEXT(", "));
						}

						if (LinkIndex >= MaxConnectionsPerPin)
						{
							Out.Append(TEXT("..."));
							break;
						}

						const UEdGraphPin* LinkedPin = Links[LinkIndex];
						Out.Appendf(TEXT("%s.%s"), *NodeTitle(LinkedPin->GetOwningNode()), *LinkedPin->PinName.ToString());
					}
				};

				AppendLinks(TEXT(" -> selected: "), SelectedLinks);
				AppendLinks(TEXT(" -> external: "), ExternalLinks);
			}

			Out.Append(TEXT("\n"));
		}
	}

	static TArray<UEdGraphNode*> GetSelectedNodesForGraph(const UEdGraph* Graph, const UEdGraphNode* FallbackNode)
	{
		TArray<UEdGraphNode*> Nodes;

		UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(Graph);
		if (Blueprint && GEditor)
		{
			if (UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>())
			{
				if (IAssetEditorInstance* EditorInstance = AssetEditorSubsystem->FindEditorForAsset(Blueprint, false))
				{
					if (FBlueprintEditor* BlueprintEditor = static_cast<FBlueprintEditor*>(EditorInstance))
					{
						for (UObject* SelectedObject : BlueprintEditor->GetSelectedNodes())
						{
							UEdGraphNode* SelectedNode = Cast<UEdGraphNode>(SelectedObject);
							if (SelectedNode && SelectedNode->GetGraph() == Graph)
							{
								Nodes.AddUnique(SelectedNode);
							}
						}
					}
				}
			}
		}

		if (Nodes.IsEmpty() && FallbackNode)
		{
			Nodes.Add(const_cast<UEdGraphNode*>(FallbackNode));
		}

		Algo::Sort(Nodes, [](const UEdGraphNode* A, const UEdGraphNode* B)
		{
			if (A->NodePosY == B->NodePosY)
			{
				return A->NodePosX < B->NodePosX;
			}

			return A->NodePosY < B->NodePosY;
		});

		return Nodes;
	}

	static FText BuildSelectionLabel(const int32 NodeCount)
	{
		return NodeCount > 1
			? FText::Format(LOCTEXT("CopyManyNodeSummaries", "Copy AI Summary ({0} Nodes)"), NodeCount)
			: LOCTEXT("CopyNodeSummary", "Copy AI Summary");
	}

	static FString SummarizeSelectedNodes(const UEdGraph* Graph, const TArray<UEdGraphNode*>& Nodes)
	{
		TStringBuilder<65536> Out;
		Out.Append(TEXT("Blueprint Node AI Summary\n"));
		Out.Append(TEXT("=========================\n"));

		if (const UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(Graph))
		{
			Out.Appendf(TEXT("Blueprint: %s\n"), *Blueprint->GetPathName());
		}

		if (Graph)
		{
			Out.Appendf(TEXT("Graph: %s\n"), *Graph->GetName());
		}

		Out.Appendf(TEXT("SelectedNodes: %d\n\n"), Nodes.Num());

		TSet<const UEdGraphNode*> SelectedNodeSet;
		for (const UEdGraphNode* Node : Nodes)
		{
			SelectedNodeSet.Add(Node);
		}

		int32 NodeCount = 0;
		for (const UEdGraphNode* Node : Nodes)
		{
			if (NodeCount >= MaxNodesPerGraph)
			{
				Out.Appendf(TEXT("- ... %d more selected nodes omitted\n"), Nodes.Num() - NodeCount);
				break;
			}

			++NodeCount;
			AppendNode(Node, SelectedNodeSet, Out);
		}

		return FString(Out.ToString());
	}
}

class FBlueprintAISummarizerModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		FGraphEditorModule& GraphEditorModule = FModuleManager::LoadModuleChecked<FGraphEditorModule>(TEXT("GraphEditor"));
		GraphEditorModule.GetAllGraphEditorContextMenuExtender().Add(
			FGraphEditorModule::FGraphEditorMenuExtender_SelectedNode::CreateRaw(this, &FBlueprintAISummarizerModule::ExtendGraphContextMenu));
	}

	virtual void ShutdownModule() override
	{
		if (FModuleManager::Get().IsModuleLoaded(TEXT("GraphEditor")))
		{
			FGraphEditorModule& GraphEditorModule = FModuleManager::GetModuleChecked<FGraphEditorModule>(TEXT("GraphEditor"));
			GraphEditorModule.GetAllGraphEditorContextMenuExtender().RemoveAll(
				[this](const FGraphEditorModule::FGraphEditorMenuExtender_SelectedNode& Delegate)
				{
					return Delegate.IsBoundToObject(this);
				});
		}
	}

private:
	TSharedRef<FExtender> ExtendGraphContextMenu(
		const TSharedRef<FUICommandList> CommandList,
		const UEdGraph* Graph,
		const UEdGraphNode* Node,
		const UEdGraphPin* Pin,
		bool bIsReadOnly)
	{
		TSharedRef<FExtender> Extender = MakeShared<FExtender>();
		if (!Graph || !Node || Pin)
		{
			return Extender;
		}

		Extender->AddMenuExtension(
			TEXT("EdGraphSchemaNodeActions"),
			EExtensionHook::After,
			CommandList,
			FMenuExtensionDelegate::CreateRaw(this, &FBlueprintAISummarizerModule::AddGraphMenuEntry, Graph, Node));
		return Extender;
	}

	void AddGraphMenuEntry(FMenuBuilder& MenuBuilder, const UEdGraph* Graph, const UEdGraphNode* Node)
	{
		const TArray<UEdGraphNode*> SelectedNodes = BlueprintAISummarizer::GetSelectedNodesForGraph(Graph, Node);
		MenuBuilder.AddMenuEntry(
			BlueprintAISummarizer::BuildSelectionLabel(SelectedNodes.Num()),
			LOCTEXT("CopyNodeSummaryTooltip", "Copy a compact AI-readable text summary of the selected Blueprint nodes to the clipboard."),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateRaw(this, &FBlueprintAISummarizerModule::CopyNodeSummaries, Graph, Node)));
	}

	void CopyNodeSummaries(const UEdGraph* Graph, const UEdGraphNode* Node)
	{
		const TArray<UEdGraphNode*> SelectedNodes = BlueprintAISummarizer::GetSelectedNodesForGraph(Graph, Node);
		FPlatformApplicationMisc::ClipboardCopy(*BlueprintAISummarizer::SummarizeSelectedNodes(Graph, SelectedNodes));
	}
};

IMPLEMENT_MODULE(FBlueprintAISummarizerModule, BlueprintAISummarizer)

#undef LOCTEXT_NAMESPACE
