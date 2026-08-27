#pragma once

#include "Engine/Vfx/Graph/Data/VfxGraphTypes.h"
#include "Engine/Vfx/Graph/Runtime/VfxGraphProgram.h"
#include "Engine/Vfx/Graph/Runtime/VfxGraphRuntime.h"

#include <cmath>
#include <cstdint>
#include <string>

namespace Ken4lowEngine
{

/// <summary>
/// VFX GraphアセットのAuthoringとPreviewを行うEditor Windowです。
/// 編集状態だけを保持し、Runtime SimulationはVfxGraphRuntime/GPU Particleへ委譲します。
/// </summary>
class VfxGraphEditor
{
public:
	static VfxGraphEditor* GetInstance();

	void Initialize();
	void Finalize();
	void Draw(bool* open = nullptr);

	[[nodiscard]] const VfxGraphDesc& GetEditableGraph() const { return editableGraph_; }
	[[nodiscard]] const VfxGraphCompileResult& GetLastCompileResult() const { return compileResult_; }
	[[nodiscard]] const std::string& GetLastMessage() const { return lastMessage_; }
	[[nodiscard]] bool IsPreviewPlaying() const { return previewHandle_.IsValid(); }

private:
	VfxGraphEditor() = default;
	~VfxGraphEditor() = default;
	VfxGraphEditor(const VfxGraphEditor&) = delete;
	VfxGraphEditor& operator=(const VfxGraphEditor&) = delete;

	bool LoadFromDisk();
	bool SaveToDisk();
	bool CompileEditableGraph(bool restartPreview);
	bool StartPreview();
	void StopPreview();
	void MarkGraphDirty();

	VfxGraphEmitterDesc* GetSelectedEmitter();
	VfxGraphNodeDesc* GetSelectedNode();
	uint32_t AllocateNodeId(const VfxGraphEmitterDesc& emitter) const;
	void AddNode(VfxGraphNodeType type);
	void RemoveSelectedNode();
	void AddEdge(uint32_t fromNodeId, uint32_t toNodeId);
	void RemoveEdge(uint32_t fromNodeId, uint32_t toNodeId);
	void EnsureEditorLayout(VfxGraphEmitterDesc& emitter);

	void DrawToolbar();
	void DrawGraphHeader();
	void DrawEmitterList();
	void DrawGraphCanvas();
	void DrawNodeInspector();
	void DrawCompileDiagnostics();
	void DrawPreviewPanel();
	void DrawFloatCurveEditor(VfxFloatCurve& curve);
	void DrawColorGradientEditor(VfxColorGradient& gradient);
	bool DrawNodePayloadEditor(VfxGraphNodeDesc& node);

private:
	VfxGraphDesc editableGraph_{};
	VfxGraphCompileResult compileResult_{};
	// 起動時は工程番号ではなく機能名で管理されたサンプル資産を開く。
	std::string filePath_ = "Resources/VfxGraph/Samples/EditorPreviewShowcase.vfxgraph.json";
	std::string lastMessage_ = "VFX Graph Editor ready.";
	VfxGraphPlayHandle previewHandle_{};
	Vector3 previewPosition_{ 0.0f, 1.0f, 0.0f };
	uint32_t selectedEmitterIndex_ = 0u;
	uint32_t selectedNodeId_ = 0u;
	uint32_t pendingEdgeFromNodeId_ = 0u;
	int addNodeType_ = 0;
	float canvasZoom_ = 1.0f;
	Vector2 canvasPan_{ 20.0f, 50.0f };
	bool initialized_ = false;
	bool dirty_ = false;
	bool liveCompile_ = true;
	bool livePreview_ = true;
};

} // namespace Ken4lowEngine
