# Blueprint AI Summarizer

Blueprint AI Summarizer is an editor-only Unreal Engine plugin that copies a compact, AI-readable summary of the Blueprint graph nodes you select.

It is meant for the common workflow where copying raw Blueprint text into an AI chat is too large, noisy, or awkward. Select only the nodes you want to discuss, right-click one of them, and choose **Copy AI Summary**.

## Features

- Adds **Copy AI Summary** to the Blueprint graph node right-click menu.
- Summarizes only the currently selected nodes, not the entire Blueprint asset.
- Includes node class, readable node title, graph position, comments, pin names, pin types, default values, and links.
- Separates links to selected nodes from links to external nodes.
- Supports multi-node selections.
- Editor-only module; it is not packaged into your game runtime.
- Uses Unreal's cross-platform clipboard API, so there is no platform-specific Windows or macOS code.

## Compatibility

This plugin was built and tested with Unreal Engine 5.7 on macOS. The code uses public Unreal Editor APIs and should build on Windows with Unreal Engine 5.x as long as the `GraphEditor`, `Kismet`, and `UnrealEd` modules are available.

For Windows, use a normal C++ Unreal project setup with Visual Studio and the workload required by your Unreal Engine version.

## Installation

1. Copy this folder into your project:

   ```text
   YourProject/
     Plugins/
       BlueprintAISummarizer/
   ```

2. Open the `.uproject` file or regenerate project files.
3. Build your editor target.
4. Open Unreal Editor.
5. Enable the plugin if Unreal asks.

You can also add it to your `.uproject` plugin list:

```json
{
  "Name": "BlueprintAISummarizer",
  "Enabled": true,
  "TargetAllowList": [
    "Editor"
  ]
}
```

## Usage

1. Open a Blueprint.
2. In the graph editor, select one or more nodes.
3. Right-click one selected node.
4. Choose **Copy AI Summary**.
5. Paste the clipboard contents into your AI chat.

If no multi-selection can be found, the command falls back to the node you right-clicked.

## Example Output

```text
Blueprint Node AI Summary
=========================
Blueprint: /Game/BP_MyActor.BP_MyActor
Graph: EventGraph
SelectedNodes: 2

- [K2Node_Event] Event BeginPlay id=K2Node_Event_0
  - out then: exec -> selected: Print String [K2Node_CallFunction_1].execute
- [K2Node_CallFunction] Print String id=K2Node_CallFunction_1
  - in execute: exec -> selected: Event BeginPlay [K2Node_Event_0].then
  - in In String: string = Hello
```

## Notes

- This is a summarizer, not a lossless Blueprint exporter.
- It intentionally omits most unselected graph context.
- Large selections are capped to keep clipboard output usable.

## License

MIT. See [LICENSE](LICENSE).
