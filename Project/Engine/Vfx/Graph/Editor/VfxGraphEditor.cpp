#include "VfxGraphEditor.h"

#include "Engine/Vfx/Graph/Asset/VfxGraphSerializer.h"
#include "Engine/Vfx/Graph/Runtime/VfxGraphCompiler.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <unordered_map>
#include <utility>

#ifdef USE_IMGUI
#include <imgui.h>
#endif // USE_IMGUI

namespace Ken4lowEngine
{
namespace
{
	constexpr std::array<VfxGraphNodeType, 25> kNodeTypes = {
		VfxGraphNodeType::SpawnRate,
		VfxGraphNodeType::Burst,
		VfxGraphNodeType::SpawnPoint,
		VfxGraphNodeType::SpawnSphere,
		VfxGraphNodeType::SpawnBox,
		VfxGraphNodeType::Lifetime,
		VfxGraphNodeType::InitialVelocity,
		VfxGraphNodeType::InitialColor,
		VfxGraphNodeType::InitialSize,
		VfxGraphNodeType::Gravity,
		VfxGraphNodeType::Drag,
		VfxGraphNodeType::InitialRotation,
		VfxGraphNodeType::RotationRate,
		VfxGraphNodeType::SizeOverLife,
		VfxGraphNodeType::ColorOverLife,
		VfxGraphNodeType::Collision,
		VfxGraphNodeType::DeathEvent,
		VfxGraphNodeType::SubEmitter,
		VfxGraphNodeType::SpriteRenderer,
		VfxGraphNodeType::RibbonRenderer,
		VfxGraphNodeType::TrailRenderer,
		VfxGraphNodeType::MeshRenderer,
		VfxGraphNodeType::FluidOutput,
		VfxGraphNodeType::LightOutput,
		VfxGraphNodeType::PostEffectOutput,
	};

	const char* GetNodeTypeDisplayName(VfxGraphNodeType type)
	{
		switch (type)
		{
		case VfxGraphNodeType::SpawnRate: return "連続生成";
		case VfxGraphNodeType::Burst: return "一括生成";
		case VfxGraphNodeType::SpawnPoint: return "点から生成";
		case VfxGraphNodeType::SpawnSphere: return "球から生成";
		case VfxGraphNodeType::SpawnBox: return "箱から生成";
		case VfxGraphNodeType::Lifetime: return "寿命";
		case VfxGraphNodeType::InitialVelocity: return "初期速度";
		case VfxGraphNodeType::InitialColor: return "初期色";
		case VfxGraphNodeType::InitialSize: return "初期サイズ";
		case VfxGraphNodeType::Gravity: return "重力";
		case VfxGraphNodeType::Drag: return "減衰";
		case VfxGraphNodeType::InitialRotation: return "初期回転";
		case VfxGraphNodeType::RotationRate: return "回転速度";
		case VfxGraphNodeType::SizeOverLife: return "寿命によるサイズ変化";
		case VfxGraphNodeType::ColorOverLife: return "寿命による色変化";
		case VfxGraphNodeType::Collision: return "衝突";
		case VfxGraphNodeType::DeathEvent: return "消滅イベント";
		case VfxGraphNodeType::SubEmitter: return "サブエミッター";
		case VfxGraphNodeType::SpriteRenderer: return "スプライト描画";
		case VfxGraphNodeType::RibbonRenderer: return "リボン描画";
		case VfxGraphNodeType::TrailRenderer: return "トレイル描画";
		case VfxGraphNodeType::MeshRenderer: return "メッシュ描画";
		case VfxGraphNodeType::FluidOutput: return "流体出力";
		case VfxGraphNodeType::LightOutput: return "ライト出力";
		case VfxGraphNodeType::PostEffectOutput: return "ポストエフェクト出力";
		default: return "不明";
		}
	}

	const char* GetNodeStageDisplayName(VfxGraphNodeStage stage)
	{
		switch (stage)
		{
		case VfxGraphNodeStage::Spawn: return "生成";
		case VfxGraphNodeStage::Initialize: return "初期化";
		case VfxGraphNodeStage::Update: return "更新";
		case VfxGraphNodeStage::Render: return "描画";
		default: return "不明";
		}
	}

	VfxGraphNodePayload MakeDefaultPayload(VfxGraphNodeType type)
	{
		switch (type)
		{
		case VfxGraphNodeType::SpawnRate: return VfxGraphSpawnRateNode{};
		case VfxGraphNodeType::Burst: return VfxGraphBurstNode{};
		case VfxGraphNodeType::SpawnPoint: return VfxGraphSpawnPointNode{};
		case VfxGraphNodeType::SpawnSphere: return VfxGraphSpawnSphereNode{};
		case VfxGraphNodeType::SpawnBox: return VfxGraphSpawnBoxNode{};
		case VfxGraphNodeType::Lifetime: return VfxGraphLifetimeNode{};
		case VfxGraphNodeType::InitialVelocity: return VfxGraphInitialVelocityNode{};
		case VfxGraphNodeType::InitialColor: return VfxGraphInitialColorNode{};
		case VfxGraphNodeType::InitialSize: return VfxGraphInitialSizeNode{};
		case VfxGraphNodeType::Gravity: return VfxGraphGravityNode{};
		case VfxGraphNodeType::Drag: return VfxGraphDragNode{};
		case VfxGraphNodeType::InitialRotation: return VfxGraphInitialRotationNode{};
		case VfxGraphNodeType::RotationRate: return VfxGraphRotationRateNode{};
		case VfxGraphNodeType::SizeOverLife: return VfxGraphSizeOverLifeNode{};
		case VfxGraphNodeType::ColorOverLife: return VfxGraphColorOverLifeNode{};
		case VfxGraphNodeType::Collision: return VfxGraphCollisionNode{};
		case VfxGraphNodeType::DeathEvent: return VfxGraphDeathEventNode{};
		case VfxGraphNodeType::SubEmitter: return VfxGraphSubEmitterNode{};
		case VfxGraphNodeType::SpriteRenderer: return VfxGraphSpriteRendererNode{};
		case VfxGraphNodeType::RibbonRenderer: return VfxGraphRibbonRendererNode{};
		case VfxGraphNodeType::TrailRenderer: return VfxGraphTrailRendererNode{};
		case VfxGraphNodeType::MeshRenderer: return VfxGraphMeshRendererNode{};
		case VfxGraphNodeType::FluidOutput: return VfxGraphFluidOutputNode{};
		case VfxGraphNodeType::LightOutput: return VfxGraphLightOutputNode{};
		case VfxGraphNodeType::PostEffectOutput: return VfxGraphPostEffectOutputNode{};
		default: return VfxGraphSpawnRateNode{};
		}
	}

#ifdef USE_IMGUI
	bool DrawBlendMode(const char* label, GpuParticleBlendMode& mode)
	{
		static const char* kNames[] = { "アルファ", "加算", "乗算" };
		int value = static_cast<int>(mode);
		if (!ImGui::Combo(label, &value, kNames, IM_ARRAYSIZE(kNames)))
		{
			return false;
		}
		mode = static_cast<GpuParticleBlendMode>(value);
		return true;
	}
#endif // USE_IMGUI
}

VfxGraphEditor* VfxGraphEditor::GetInstance()
{
	static VfxGraphEditor instance;
	return &instance;
}

void VfxGraphEditor::Initialize()
{
	if (initialized_)
	{
		return;
	}

	initialized_ = true;
	if (!LoadFromDisk())
	{
		editableGraph_ = {};
		editableGraph_.graphName = "EditorPreviewGraph";
		editableGraph_.emitters.push_back(VfxGraphEmitterDesc{});
		lastMessage_ = "既定のVFXグラフを読み込めなかったため、空のグラフを作成しました。";
	}
	CompileEditableGraph(false);
}

void VfxGraphEditor::Finalize()
{
	StopPreview();
	editableGraph_ = {};
	compileResult_ = {};
	initialized_ = false;
	dirty_ = false;
}

bool VfxGraphEditor::LoadFromDisk()
{
	VfxGraphDesc loaded{};
	if (!VfxGraphSerializer::Load(loaded, filePath_))
	{
		lastMessage_ = "読み込み失敗: " + filePath_;
		return false;
	}

	StopPreview();
	editableGraph_ = std::move(loaded);
	selectedEmitterIndex_ = 0u;
	selectedNodeId_ = 0u;
	pendingEdgeFromNodeId_ = 0u;
	for (auto& emitter : editableGraph_.emitters)
	{
		EnsureEditorLayout(emitter);
	}
	CompileEditableGraph(false);
	dirty_ = false;
	lastMessage_ = "読み込み完了: " + filePath_;
	return true;
}

bool VfxGraphEditor::SaveToDisk()
{
	if (!VfxGraphSerializer::Save(editableGraph_, filePath_))
	{
		lastMessage_ = "保存失敗: " + filePath_;
		return false;
	}
	dirty_ = false;
	lastMessage_ = "保存完了: " + filePath_;
	return true;
}

bool VfxGraphEditor::CompileEditableGraph(bool restartPreview)
{
	compileResult_ = VfxGraphCompiler::Compile(editableGraph_);
	if (!compileResult_.success)
	{
		lastMessage_ = "コンパイル失敗: " + std::to_string(compileResult_.errors.size()) + " 件のエラーがあります。";
		return false;
	}

	lastMessage_ = "コンパイル成功。";
	if (restartPreview && previewHandle_.IsValid())
	{
		if (!VfxGraphRuntime::GetInstance()->RegisterGraph(editableGraph_))
		{
			lastMessage_ = VfxGraphRuntime::GetInstance()->GetLastStatus();
			return false;
		}

		VfxGraphRuntime::GetInstance()->StopLoop(previewHandle_);
		previewHandle_ = VfxGraphRuntime::GetInstance()->PlayLoop(editableGraph_.graphName, previewPosition_);
		if (!previewHandle_.IsValid())
		{
			lastMessage_ = VfxGraphRuntime::GetInstance()->GetLastStatus();
			return false;
		}
		lastMessage_ = "プレビューを再コンパイルしました。";
	}
	return true;
}

bool VfxGraphEditor::StartPreview()
{
	if (!CompileEditableGraph(false))
	{
		return false;
	}
	if (!VfxGraphRuntime::GetInstance()->RegisterGraph(editableGraph_))
	{
		lastMessage_ = VfxGraphRuntime::GetInstance()->GetLastStatus();
		return false;
	}

	StopPreview();
	previewHandle_ = VfxGraphRuntime::GetInstance()->PlayLoop(editableGraph_.graphName, previewPosition_);
	if (!previewHandle_.IsValid())
	{
		lastMessage_ = VfxGraphRuntime::GetInstance()->GetLastStatus();
		return false;
	}
	lastMessage_ = "プレビューを開始しました。";
	return true;
}

void VfxGraphEditor::StopPreview()
{
	if (previewHandle_.IsValid())
	{
		VfxGraphRuntime::GetInstance()->StopLoop(previewHandle_);
		previewHandle_ = {};
	}
}

void VfxGraphEditor::MarkGraphDirty()
{
	// 編集内容は共通のVFXグラフコンパイラーへ渡し、エディター専用の実行経路を増やさない。
	dirty_ = true;
}

VfxGraphEmitterDesc* VfxGraphEditor::GetSelectedEmitter()
{
	if (selectedEmitterIndex_ >= editableGraph_.emitters.size())
	{
		return nullptr;
	}
	return &editableGraph_.emitters[selectedEmitterIndex_];
}

VfxGraphNodeDesc* VfxGraphEditor::GetSelectedNode()
{
	VfxGraphEmitterDesc* emitter = GetSelectedEmitter();
	if (!emitter || selectedNodeId_ == 0u)
	{
		return nullptr;
	}
	for (auto& node : emitter->nodes)
	{
		if (node.id == selectedNodeId_)
		{
			return &node;
		}
	}
	return nullptr;
}

uint32_t VfxGraphEditor::AllocateNodeId(const VfxGraphEmitterDesc& emitter) const
{
	uint32_t result = 1u;
	for (const auto& node : emitter.nodes)
	{
		result = (std::max)(result, node.id + 1u);
	}
	return result;
}

void VfxGraphEditor::AddNode(VfxGraphNodeType type)
{
	VfxGraphEmitterDesc* emitter = GetSelectedEmitter();
	if (!emitter || emitter->nodes.size() >= VfxGraphDesc::kMaxNodesPerEmitter)
	{
		return;
	}

	VfxGraphNodeDesc node{};
	node.id = AllocateNodeId(*emitter);
	node.type = type;
	node.stage = GetExpectedVfxGraphNodeStage(type);
	node.name = ToString(type);
	node.payload = MakeDefaultPayload(type);
	node.editorPosition = { static_cast<float>(static_cast<uint32_t>(node.stage)) * 250.0f, 80.0f };
	emitter->nodes.push_back(std::move(node));
	selectedNodeId_ = emitter->nodes.back().id;
	MarkGraphDirty();
}

void VfxGraphEditor::RemoveSelectedNode()
{
	VfxGraphEmitterDesc* emitter = GetSelectedEmitter();
	if (!emitter || selectedNodeId_ == 0u)
	{
		return;
	}

	const uint32_t removedId = selectedNodeId_;
	emitter->nodes.erase(
		std::remove_if(emitter->nodes.begin(), emitter->nodes.end(), [removedId](const VfxGraphNodeDesc& node)
			{
				return node.id == removedId;
			}),
		emitter->nodes.end());
	emitter->edges.erase(
		std::remove_if(emitter->edges.begin(), emitter->edges.end(), [removedId](const VfxGraphEdgeDesc& edge)
			{
				return edge.fromNodeId == removedId || edge.toNodeId == removedId;
			}),
		emitter->edges.end());
	selectedNodeId_ = 0u;
	if (pendingEdgeFromNodeId_ == removedId)
	{
		pendingEdgeFromNodeId_ = 0u;
	}
	MarkGraphDirty();
}

void VfxGraphEditor::AddEdge(uint32_t fromNodeId, uint32_t toNodeId)
{
	VfxGraphEmitterDesc* emitter = GetSelectedEmitter();
	if (!emitter || fromNodeId == 0u || toNodeId == 0u || fromNodeId == toNodeId || emitter->edges.size() >= VfxGraphDesc::kMaxEdgesPerEmitter)
	{
		return;
	}
	const auto duplicate = std::find_if(emitter->edges.begin(), emitter->edges.end(), [fromNodeId, toNodeId](const VfxGraphEdgeDesc& edge)
		{
			return edge.fromNodeId == fromNodeId && edge.toNodeId == toNodeId;
		});
	if (duplicate != emitter->edges.end())
	{
		return;
	}
	emitter->edges.push_back({ fromNodeId, toNodeId });
	MarkGraphDirty();
}

void VfxGraphEditor::RemoveEdge(uint32_t fromNodeId, uint32_t toNodeId)
{
	VfxGraphEmitterDesc* emitter = GetSelectedEmitter();
	if (!emitter)
	{
		return;
	}
	const auto oldSize = emitter->edges.size();
	emitter->edges.erase(
		std::remove_if(emitter->edges.begin(), emitter->edges.end(), [fromNodeId, toNodeId](const VfxGraphEdgeDesc& edge)
			{
				return edge.fromNodeId == fromNodeId && edge.toNodeId == toNodeId;
			}),
		emitter->edges.end());
	if (oldSize != emitter->edges.size())
	{
		MarkGraphDirty();
	}
}

void VfxGraphEditor::EnsureEditorLayout(VfxGraphEmitterDesc& emitter)
{
	std::array<uint32_t, 4> stageRows{};
	for (auto& node : emitter.nodes)
	{
		const uint32_t stageIndex = static_cast<uint32_t>(node.stage);
		if (node.editorPosition.x == 0.0f && node.editorPosition.y == 0.0f)
		{
			node.editorPosition.x = static_cast<float>(stageIndex) * 250.0f;
			node.editorPosition.y = static_cast<float>(stageRows[stageIndex]) * 92.0f;
		}
		++stageRows[stageIndex];
	}
}

void VfxGraphEditor::Draw(bool* open)
{
#ifdef USE_IMGUI
	if (!initialized_)
	{
		Initialize();
	}
	if (!ImGui::Begin("VFXグラフエディター###VFX Graph Editor", open, ImGuiWindowFlags_MenuBar))
	{
		ImGui::End();
		return;
	}

	DrawToolbar();
	DrawGraphHeader();

	if (ImGui::BeginTable("##VfxGraphEditorLayout", 3, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV))
	{
		ImGui::TableSetupColumn("エミッター", ImGuiTableColumnFlags_WidthFixed, 190.0f);
		ImGui::TableSetupColumn("グラフ", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("インスペクター", ImGuiTableColumnFlags_WidthFixed, 340.0f);
		ImGui::TableNextColumn();
		DrawEmitterList();
		ImGui::TableNextColumn();
		DrawGraphCanvas();
		ImGui::TableNextColumn();
		DrawNodeInspector();
		ImGui::EndTable();
	}

	DrawPreviewPanel();
	DrawCompileDiagnostics();
	ImGui::TextDisabled("%s", lastMessage_.c_str());
	ImGui::End();

	if (dirty_ && liveCompile_)
	{
		dirty_ = false;
		CompileEditableGraph(livePreview_ && previewHandle_.IsValid());
	}
#else
	(void)open;
#endif // USE_IMGUI
}

void VfxGraphEditor::DrawToolbar()
{
#ifdef USE_IMGUI
	char pathBuffer[512]{};
	std::snprintf(pathBuffer, sizeof(pathBuffer), "%s", filePath_.c_str());
	ImGui::SetNextItemWidth(420.0f);
	if (ImGui::InputText("アセット", pathBuffer, sizeof(pathBuffer)))
	{
		filePath_ = pathBuffer;
	}
	ImGui::SameLine();
	if (ImGui::Button("読み込み"))
	{
		LoadFromDisk();
	}
	ImGui::SameLine();
	if (ImGui::Button("保存"))
	{
		SaveToDisk();
	}
	ImGui::SameLine();
	if (ImGui::Button("コンパイル"))
	{
		dirty_ = false;
		CompileEditableGraph(livePreview_ && previewHandle_.IsValid());
	}
	ImGui::SameLine();
	ImGui::Checkbox("自動コンパイル", &liveCompile_);
	ImGui::SameLine();
	ImGui::Checkbox("自動プレビュー", &livePreview_);
#endif // USE_IMGUI
}

void VfxGraphEditor::DrawGraphHeader()
{
#ifdef USE_IMGUI
	char graphName[160]{};
	std::snprintf(graphName, sizeof(graphName), "%s", editableGraph_.graphName.c_str());
	if (ImGui::InputText("グラフ名", graphName, sizeof(graphName)))
	{
		editableGraph_.graphName = graphName;
		MarkGraphDirty();
	}
	ImGui::SameLine();
	ImGui::Text("エミッター数: %d", static_cast<int>(editableGraph_.emitters.size()));
	if (ImGui::TreeNode("スケーラビリティ"))
	{
		auto& scalability = editableGraph_.scalability;
		const char* boundsModes[] = { "自動", "固定球" };
		int boundsMode = static_cast<int>(scalability.boundsMode);
		if (ImGui::Combo("境界方式", &boundsMode, boundsModes, IM_ARRAYSIZE(boundsModes))) { scalability.boundsMode = static_cast<VfxGraphBoundsMode>(boundsMode); MarkGraphDirty(); }
		if (scalability.boundsMode == VfxGraphBoundsMode::FixedSphere)
		{
			if (ImGui::DragFloat3("境界中心", &scalability.fixedBoundsCenter.x, 0.05f)) MarkGraphDirty();
			if (ImGui::DragFloat("境界半径", &scalability.fixedBoundsRadius, 0.05f, 0.1f, 10000.0f)) MarkGraphDirty();
		}
		if (ImGui::Checkbox("視錐台カリング", &scalability.frustumCulling)) MarkGraphDirty();
		if (ImGui::DragFloat("最大描画距離", &scalability.maxDrawDistance, 1.0f, 0.0f, 100000.0f)) MarkGraphDirty();
		if (ImGui::DragFloat("LOD近距離", &scalability.lodNearDistance, 0.5f, 0.0f, 100000.0f)) MarkGraphDirty();
		if (ImGui::DragFloat("LOD遠距離", &scalability.lodFarDistance, 0.5f, 0.0f, 100000.0f)) MarkGraphDirty();
		if (ImGui::SliderFloat("LOD中距離倍率", &scalability.lodMidScale, 0.05f, 1.0f)) MarkGraphDirty();
		if (ImGui::SliderFloat("LOD遠距離倍率", &scalability.lodFarScale, 0.01f, 1.0f)) MarkGraphDirty();
		int budgetCost = static_cast<int>(scalability.budgetCost);
		if (ImGui::DragInt("負荷コスト", &budgetCost, 1.0f, 1, 64)) { scalability.budgetCost = static_cast<uint32_t>((std::max)(budgetCost, 1)); MarkGraphDirty(); }
		ImGui::TreePop();
	}
	ImGui::Separator();
#endif // USE_IMGUI
}

void VfxGraphEditor::DrawEmitterList()
{
#ifdef USE_IMGUI
	ImGui::TextUnformatted("エミッター");
	for (uint32_t i = 0; i < editableGraph_.emitters.size(); ++i)
	{
		const bool selected = i == selectedEmitterIndex_;
		if (ImGui::Selectable(editableGraph_.emitters[i].name.c_str(), selected))
		{
			selectedEmitterIndex_ = i;
			selectedNodeId_ = 0u;
			pendingEdgeFromNodeId_ = 0u;
		}
	}
	if (ImGui::Button("＋ エミッター") && editableGraph_.emitters.size() < VfxGraphDesc::kMaxEmitters)
	{
		VfxGraphEmitterDesc emitter{};
		emitter.name = "Emitter" + std::to_string(editableGraph_.emitters.size() + 1u);
		editableGraph_.emitters.push_back(std::move(emitter));
		selectedEmitterIndex_ = static_cast<uint32_t>(editableGraph_.emitters.size() - 1u);
		selectedNodeId_ = 0u;
		MarkGraphDirty();
	}
	ImGui::SameLine();
	if (ImGui::Button("－ エミッター") && editableGraph_.emitters.size() > 1u && selectedEmitterIndex_ < editableGraph_.emitters.size())
	{
		editableGraph_.emitters.erase(editableGraph_.emitters.begin() + selectedEmitterIndex_);
		selectedEmitterIndex_ = std::min<uint32_t>(selectedEmitterIndex_, static_cast<uint32_t>(editableGraph_.emitters.size() - 1u));
		selectedNodeId_ = 0u;
		MarkGraphDirty();
	}

	VfxGraphEmitterDesc* emitter = GetSelectedEmitter();
	if (!emitter)
	{
		return;
	}
	ImGui::Separator();
	char nameBuffer[128]{};
	std::snprintf(nameBuffer, sizeof(nameBuffer), "%s", emitter->name.c_str());
	if (ImGui::InputText("名前", nameBuffer, sizeof(nameBuffer)))
	{
		emitter->name = nameBuffer;
		MarkGraphDirty();
	}
	int maxParticles = static_cast<int>(emitter->maxParticles);
	if (ImGui::DragInt("最大粒子数", &maxParticles, 16.0f, 1, 131072))
	{
		emitter->maxParticles = static_cast<uint32_t>((std::max)(maxParticles, 1));
		MarkGraphDirty();
	}
	if (ImGui::Checkbox("ループ", &emitter->loop))
	{
		MarkGraphDirty();
	}
	if (ImGui::DragFloat("継続時間", &emitter->duration, 0.05f, 0.01f, 60.0f))
	{
		MarkGraphDirty();
	}

	ImGui::SeparatorText("ノード追加");
	const VfxGraphNodeType previewType = kNodeTypes[static_cast<size_t>(std::clamp(addNodeType_, 0, static_cast<int>(kNodeTypes.size()) - 1))];
	if (ImGui::BeginCombo("種類", GetNodeTypeDisplayName(previewType)))
	{
		for (int i = 0; i < static_cast<int>(kNodeTypes.size()); ++i)
		{
			const bool selected = addNodeType_ == i;
			if (ImGui::Selectable(GetNodeTypeDisplayName(kNodeTypes[static_cast<size_t>(i)]), selected))
			{
				addNodeType_ = i;
			}
		}
		ImGui::EndCombo();
	}
	if (ImGui::Button("ノードを追加", ImVec2(-1.0f, 0.0f)))
	{
		AddNode(kNodeTypes[static_cast<size_t>(addNodeType_)]);
	}
#endif // USE_IMGUI
}

void VfxGraphEditor::DrawGraphCanvas()
{
#ifdef USE_IMGUI
	VfxGraphEmitterDesc* emitter = GetSelectedEmitter();
	if (!emitter)
	{
		ImGui::TextDisabled("エミッターが選択されていません。");
		return;
	}

	ImGui::Text("グラフキャンバス  |  拡大率 %.0f%%  |  接続数 %d", canvasZoom_ * 100.0f, static_cast<int>(emitter->edges.size()));
	ImGui::SameLine();
	if (ImGui::SmallButton("全体を表示"))
	{
		canvasPan_ = { 20.0f, 50.0f };
		canvasZoom_ = 1.0f;
	}

	ImGui::BeginChild("##VfxGraphCanvas", ImVec2(0.0f, 460.0f), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
	const ImVec2 canvasMin = ImGui::GetCursorScreenPos();
	const ImVec2 canvasSize = ImGui::GetContentRegionAvail();
	const ImVec2 canvasMax(canvasMin.x + canvasSize.x, canvasMin.y + canvasSize.y);
	ImDrawList* drawList = ImGui::GetWindowDrawList();
	drawList->AddRectFilled(canvasMin, canvasMax, ImGui::GetColorU32(ImGuiCol_WindowBg));

	const float gridStep = 32.0f * canvasZoom_;
	if (gridStep >= 10.0f)
	{
		for (float x = std::fmod(canvasPan_.x, gridStep); x < canvasSize.x; x += gridStep)
		{
			drawList->AddLine(ImVec2(canvasMin.x + x, canvasMin.y), ImVec2(canvasMin.x + x, canvasMax.y), ImGui::GetColorU32(ImGuiCol_Border, 0.25f));
		}
		for (float y = std::fmod(canvasPan_.y, gridStep); y < canvasSize.y; y += gridStep)
		{
			drawList->AddLine(ImVec2(canvasMin.x, canvasMin.y + y), ImVec2(canvasMax.x, canvasMin.y + y), ImGui::GetColorU32(ImGuiCol_Border, 0.25f));
		}
	}

	const bool canvasHovered = ImGui::IsWindowHovered();
	ImGuiIO& io = ImGui::GetIO();
	if (canvasHovered && ImGui::IsMouseDragging(ImGuiMouseButton_Middle))
	{
		canvasPan_.x += io.MouseDelta.x;
		canvasPan_.y += io.MouseDelta.y;
	}
	if (canvasHovered && io.KeyCtrl && io.MouseWheel != 0.0f)
	{
		canvasZoom_ = std::clamp(canvasZoom_ + io.MouseWheel * 0.1f, 0.35f, 2.0f);
	}

	constexpr float kNodeWidth = 178.0f;
	constexpr float kNodeHeight = 64.0f;
	std::unordered_map<uint32_t, ImVec2> nodeMinById;
	for (const auto& node : emitter->nodes)
	{
		nodeMinById[node.id] = ImVec2(
			canvasMin.x + canvasPan_.x + node.editorPosition.x * canvasZoom_,
			canvasMin.y + canvasPan_.y + node.editorPosition.y * canvasZoom_);
	}

	for (const auto& edge : emitter->edges)
	{
		const auto fromIt = nodeMinById.find(edge.fromNodeId);
		const auto toIt = nodeMinById.find(edge.toNodeId);
		if (fromIt == nodeMinById.end() || toIt == nodeMinById.end())
		{
			continue;
		}
		const ImVec2 from(fromIt->second.x + kNodeWidth * canvasZoom_, fromIt->second.y + kNodeHeight * 0.5f * canvasZoom_);
		const ImVec2 to(toIt->second.x, toIt->second.y + kNodeHeight * 0.5f * canvasZoom_);
		const float tangent = (std::max)(45.0f, std::abs(to.x - from.x) * 0.4f);
		drawList->AddBezierCubic(from, ImVec2(from.x + tangent, from.y), ImVec2(to.x - tangent, to.y), to, ImGui::GetColorU32(ImGuiCol_PlotLines), 2.0f);
	}

	for (auto& node : emitter->nodes)
	{
		ImGui::PushID(static_cast<int>(node.id));
		ImVec2 nodeMin = nodeMinById[node.id];
		const ImVec2 nodeSize(kNodeWidth * canvasZoom_, kNodeHeight * canvasZoom_);
		const ImVec2 nodeMax(nodeMin.x + nodeSize.x, nodeMin.y + nodeSize.y);
		ImGui::SetCursorScreenPos(nodeMin);
		ImGui::InvisibleButton("##node", nodeSize, ImGuiButtonFlags_MouseButtonLeft);
		if (ImGui::IsItemClicked())
		{
			selectedNodeId_ = node.id;
		}
		if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
		{
			node.editorPosition.x += io.MouseDelta.x / canvasZoom_;
			node.editorPosition.y += io.MouseDelta.y / canvasZoom_;
			MarkGraphDirty();
		}

		const bool selected = selectedNodeId_ == node.id;
		const ImU32 fill = ImGui::GetColorU32(selected ? ImGuiCol_HeaderActive : ImGuiCol_FrameBg);
		const ImU32 border = ImGui::GetColorU32(selected ? ImGuiCol_CheckMark : ImGuiCol_Border);
		drawList->AddRectFilled(nodeMin, nodeMax, fill, 5.0f);
		drawList->AddRect(nodeMin, nodeMax, border, 5.0f, 0, selected ? 2.0f : 1.0f);
		drawList->AddCircleFilled(ImVec2(nodeMin.x, nodeMin.y + nodeSize.y * 0.5f), 5.0f, ImGui::GetColorU32(ImGuiCol_PlotLines));
		drawList->AddCircleFilled(ImVec2(nodeMax.x, nodeMin.y + nodeSize.y * 0.5f), 5.0f, ImGui::GetColorU32(ImGuiCol_PlotLines));
		drawList->AddText(ImVec2(nodeMin.x + 10.0f, nodeMin.y + 8.0f), ImGui::GetColorU32(ImGuiCol_Text), node.name.c_str());
		const std::string subtitle = std::string(GetNodeStageDisplayName(node.stage)) + " / " + GetNodeTypeDisplayName(node.type) + (node.enabled ? "" : " [無効]");
		drawList->AddText(ImVec2(nodeMin.x + 10.0f, nodeMin.y + 31.0f), ImGui::GetColorU32(ImGuiCol_TextDisabled), subtitle.c_str());
		ImGui::PopID();
	}

	ImGui::EndChild();
#endif // USE_IMGUI
}

void VfxGraphEditor::DrawNodeInspector()
{
#ifdef USE_IMGUI
	VfxGraphNodeDesc* node = GetSelectedNode();
	if (!node)
	{
		ImGui::TextDisabled("編集するノードを選択してください。");
		return;
	}

	ImGui::SeparatorText("ノード設定");
	char nameBuffer[160]{};
	std::snprintf(nameBuffer, sizeof(nameBuffer), "%s", node->name.c_str());
	if (ImGui::InputText("名前", nameBuffer, sizeof(nameBuffer)))
	{
		node->name = nameBuffer;
		MarkGraphDirty();
	}
	ImGui::Text("ID: %u", node->id);
	ImGui::Text("段階: %s", GetNodeStageDisplayName(node->stage));
	ImGui::Text("種類: %s", GetNodeTypeDisplayName(node->type));
	if (ImGui::Checkbox("有効", &node->enabled))
	{
		MarkGraphDirty();
	}
	if (ImGui::DragFloat2("エディター上の位置", &node->editorPosition.x, 1.0f))
	{
		MarkGraphDirty();
	}

	if (DrawNodePayloadEditor(*node))
	{
		MarkGraphDirty();
	}

	ImGui::SeparatorText("接続");
	if (pendingEdgeFromNodeId_ == 0u)
	{
		if (ImGui::Button("このノードから接続開始"))
		{
			pendingEdgeFromNodeId_ = node->id;
		}
	}
	else
	{
		ImGui::Text("接続元ノード: %u", pendingEdgeFromNodeId_);
		if (pendingEdgeFromNodeId_ != node->id && ImGui::Button("このノードへ接続"))
		{
			AddEdge(pendingEdgeFromNodeId_, node->id);
			pendingEdgeFromNodeId_ = 0u;
		}
		if (ImGui::Button("接続をキャンセル"))
		{
			pendingEdgeFromNodeId_ = 0u;
		}
	}

	VfxGraphEmitterDesc* emitter = GetSelectedEmitter();
	if (emitter)
	{
		int edgeIndex = 0;
		for (const auto& edge : emitter->edges)
		{
			if (edge.fromNodeId != node->id && edge.toNodeId != node->id)
			{
				continue;
			}
			ImGui::PushID(edgeIndex++);
			ImGui::Text("%u → %u", edge.fromNodeId, edge.toNodeId);
			ImGui::SameLine();
			if (ImGui::SmallButton("削除"))
			{
				const uint32_t from = edge.fromNodeId;
				const uint32_t to = edge.toNodeId;
				ImGui::PopID();
				RemoveEdge(from, to);
				break;
			}
			ImGui::PopID();
		}
	}

	ImGui::Separator();
	if (ImGui::Button("ノードを削除", ImVec2(-1.0f, 0.0f)))
	{
		RemoveSelectedNode();
	}
#endif // USE_IMGUI
}

bool VfxGraphEditor::DrawNodePayloadEditor(VfxGraphNodeDesc& node)
{
#ifdef USE_IMGUI
	bool changed = false;
	auto editString = [&changed](const char* label, std::string& value)
		{
			char buffer[256]{};
			std::snprintf(buffer, sizeof(buffer), "%s", value.c_str());
			if (ImGui::InputText(label, buffer, sizeof(buffer)))
			{
				value = buffer;
				changed = true;
			}
		};

	if (auto* spawnRate = std::get_if<VfxGraphSpawnRateNode>(&node.payload))
	{
		changed = ImGui::DragFloat("生成レート", &spawnRate->rate, 0.1f, 0.0f, 100000.0f) || changed;
	}
	else if (auto* burst = std::get_if<VfxGraphBurstNode>(&node.payload))
	{
		int value = static_cast<int>(burst->count);
		if (ImGui::DragInt("生成数", &value, 1.0f, 0, 131072)) { burst->count = static_cast<uint32_t>((std::max)(value, 0)); changed = true; }
	}
	else if (std::holds_alternative<VfxGraphSpawnPointNode>(node.payload))
	{
		ImGui::TextDisabled("点生成には追加パラメーターがありません。");
	}
	else if (auto* spawnSphere = std::get_if<VfxGraphSpawnSphereNode>(&node.payload))
	{
		changed = ImGui::DragFloat("半径", &spawnSphere->radius, 0.01f, 0.0f, 10000.0f) || changed;
	}
	else if (auto* spawnBox = std::get_if<VfxGraphSpawnBoxNode>(&node.payload))
	{
		changed = ImGui::DragFloat3("サイズ", &spawnBox->size.x, 0.01f, 0.0f, 10000.0f) || changed;
	}
	else if (auto* lifetime = std::get_if<VfxGraphLifetimeNode>(&node.payload))
	{
		changed = ImGui::DragFloat("寿命", &lifetime->lifetime, 0.01f, 0.001f, 120.0f) || changed;
		changed = ImGui::DragFloat("寿命のランダム幅", &lifetime->random, 0.01f, 0.0f, 120.0f) || changed;
	}
	else if (auto* initialVelocity = std::get_if<VfxGraphInitialVelocityNode>(&node.payload))
	{
		changed = ImGui::DragFloat3("速度", &initialVelocity->velocity.x, 0.05f) || changed;
		changed = ImGui::DragFloat3("速度のランダム幅", &initialVelocity->random.x, 0.05f, 0.0f, 10000.0f) || changed;
		changed = ImGui::DragFloat("速さ", &initialVelocity->speed, 0.05f) || changed;
		changed = ImGui::DragFloat("速さのランダム幅", &initialVelocity->speedRandom, 0.05f, 0.0f, 10000.0f) || changed;
	}
	else if (auto* initialColor = std::get_if<VfxGraphInitialColorNode>(&node.payload))
	{
		changed = ImGui::ColorEdit4("開始色", &initialColor->start.x) || changed;
		changed = ImGui::ColorEdit4("終了色", &initialColor->end.x) || changed;
		changed = ImGui::Checkbox("アルファをフェード", &initialColor->alphaFade) || changed;
	}
	else if (auto* initialSize = std::get_if<VfxGraphInitialSizeNode>(&node.payload))
	{
		changed = ImGui::DragFloat2("開始サイズ", &initialSize->start.x, 0.01f, 0.0f, 10000.0f) || changed;
		changed = ImGui::DragFloat2("終了サイズ", &initialSize->end.x, 0.01f, 0.0f, 10000.0f) || changed;
		changed = ImGui::DragFloat("サイズのランダム幅", &initialSize->random, 0.01f, 0.0f, 10000.0f) || changed;
	}
	else if (auto* gravity = std::get_if<VfxGraphGravityNode>(&node.payload))
	{
		changed = ImGui::DragFloat3("加速度", &gravity->acceleration.x, 0.05f) || changed;
	}
	else if (auto* drag = std::get_if<VfxGraphDragNode>(&node.payload))
	{
		changed = ImGui::DragFloat("減衰率", &drag->damping, 0.01f, 0.0f, 1000.0f) || changed;
	}
	else if (auto* initialRotation = std::get_if<VfxGraphInitialRotationNode>(&node.payload))
	{
		changed = ImGui::DragFloat("回転", &initialRotation->rotation, 0.01f) || changed;
		changed = ImGui::DragFloat("回転のランダム幅", &initialRotation->random, 0.01f, 0.0f, 100.0f) || changed;
	}
	else if (auto* rotationRate = std::get_if<VfxGraphRotationRateNode>(&node.payload))
	{
		changed = ImGui::DragFloat("回転速度（rad/秒）", &rotationRate->radiansPerSecond, 0.01f) || changed;
	}
	else if (auto* sizeOverLife = std::get_if<VfxGraphSizeOverLifeNode>(&node.payload))
	{
		const size_t oldKeyCount = sizeOverLife->multiplier.keys.size();
		DrawFloatCurveEditor(sizeOverLife->multiplier);
		changed = oldKeyCount != sizeOverLife->multiplier.keys.size() || ImGui::IsItemEdited() || changed;
	}
	else if (auto* colorOverLife = std::get_if<VfxGraphColorOverLifeNode>(&node.payload))
	{
		const size_t oldKeyCount = colorOverLife->gradient.keys.size();
		DrawColorGradientEditor(colorOverLife->gradient);
		changed = oldKeyCount != colorOverLife->gradient.keys.size() || ImGui::IsItemEdited() || changed;
	}
	else if (auto* collision = std::get_if<VfxGraphCollisionNode>(&node.payload))
	{
		static const char* kShapes[] = { "平面", "球" };
		static const char* kResponses[] = { "反発", "滑る", "消滅" };
		int shape = static_cast<int>(collision->shape);
		int response = static_cast<int>(collision->response);
		if (ImGui::Combo("形状", &shape, kShapes, IM_ARRAYSIZE(kShapes))) { collision->shape = static_cast<VfxCollisionShape>(shape); changed = true; }
		if (ImGui::Combo("衝突時の動作", &response, kResponses, IM_ARRAYSIZE(kResponses))) { collision->response = static_cast<VfxCollisionResponse>(response); changed = true; }
		if (collision->shape == VfxCollisionShape::Plane)
		{
			changed = ImGui::DragFloat3("平面法線", &collision->planeNormal.x, 0.01f) || changed;
			changed = ImGui::DragFloat("平面距離", &collision->planeDistance, 0.01f) || changed;
		}
		else
		{
			changed = ImGui::DragFloat3("球の中心", &collision->sphereCenter.x, 0.01f) || changed;
			changed = ImGui::DragFloat("球の半径", &collision->sphereRadius, 0.01f, 0.001f, 10000.0f) || changed;
		}
		changed = ImGui::DragFloat("粒子半径", &collision->particleRadius, 0.001f, 0.0f, 1000.0f) || changed;
		changed = ImGui::SliderFloat("反発係数", &collision->restitution, 0.0f, 1.0f) || changed;
		changed = ImGui::SliderFloat("摩擦", &collision->friction, 0.0f, 1.0f) || changed;
		changed = ImGui::Checkbox("イベントを生成", &collision->generateEvent) || changed;
	}
	else if (std::holds_alternative<VfxGraphDeathEventNode>(node.payload))
	{
		ImGui::TextDisabled("消滅イベントには追加パラメーターがありません。");
	}
	else if (auto* subEmitter = std::get_if<VfxGraphSubEmitterNode>(&node.payload))
	{
		static const char* kEvents[] = { "衝突", "消滅" };
		int eventType = static_cast<int>(subEmitter->sourceEvent);
		if (ImGui::Combo("発生元イベント", &eventType, kEvents, IM_ARRAYSIZE(kEvents))) { subEmitter->sourceEvent = static_cast<VfxParticleEventType>(eventType); changed = true; }
		int count = static_cast<int>(subEmitter->count);
		if (ImGui::DragInt("子粒子数", &count, 1.0f, 1, static_cast<int>(VfxGraphDesc::kMaxSubEmitterSpawnCount))) { subEmitter->count = static_cast<uint32_t>(count); changed = true; }
		changed = ImGui::DragFloat("子粒子の寿命", &subEmitter->lifeTime, 0.01f, 0.001f, 120.0f) || changed;
		changed = ImGui::DragFloat("子粒子の速さ", &subEmitter->speed, 0.01f, 0.0f, 10000.0f) || changed;
		changed = ImGui::DragFloat("拡散範囲", &subEmitter->spread, 0.01f, 0.0f, 100.0f) || changed;
		changed = ImGui::DragFloat("速度継承率", &subEmitter->inheritVelocity, 0.01f) || changed;
		changed = ImGui::DragFloat2("子粒子の開始サイズ", &subEmitter->startSize.x, 0.01f, 0.0f, 10000.0f) || changed;
		changed = ImGui::DragFloat2("子粒子の終了サイズ", &subEmitter->endSize.x, 0.01f, 0.0f, 10000.0f) || changed;
		changed = ImGui::ColorEdit4("子粒子の開始色", &subEmitter->startColor.x) || changed;
		changed = ImGui::ColorEdit4("子粒子の終了色", &subEmitter->endColor.x) || changed;
		changed = ImGui::Checkbox("子粒子のアルファをフェード", &subEmitter->alphaFade) || changed;
	}
	else if (auto* spriteRenderer = std::get_if<VfxGraphSpriteRendererNode>(&node.payload))
	{
		editString("テクスチャ", spriteRenderer->texturePath);
		changed = DrawBlendMode("ブレンド方式", spriteRenderer->blendMode) || changed;
		changed = ImGui::Checkbox("ビルボード", &spriteRenderer->billboard) || changed;
	}
	else if (auto* ribbonRenderer = std::get_if<VfxGraphRibbonRendererNode>(&node.payload))
	{
		editString("テクスチャ", ribbonRenderer->texturePath);
		changed = DrawBlendMode("ブレンド方式", ribbonRenderer->blendMode) || changed;
		changed = ImGui::DragFloat("幅", &ribbonRenderer->width, 0.001f, 0.001f, 100.0f) || changed;
		changed = ImGui::DragFloat("長さ", &ribbonRenderer->length, 0.01f, 0.001f, 10000.0f) || changed;
	}
	else if (auto* trailRenderer = std::get_if<VfxGraphTrailRendererNode>(&node.payload))
	{
		editString("テクスチャ", trailRenderer->texturePath);
		changed = DrawBlendMode("ブレンド方式", trailRenderer->blendMode) || changed;
		changed = ImGui::DragFloat("幅", &trailRenderer->width, 0.001f, 0.001f, 100.0f) || changed;
		changed = ImGui::DragFloat("長さ", &trailRenderer->length, 0.01f, 0.001f, 10000.0f) || changed;
	}
	else if (auto* meshRenderer = std::get_if<VfxGraphMeshRendererNode>(&node.payload))
	{
		editString("メッシュ", meshRenderer->meshPath);
		int subMesh = static_cast<int>(meshRenderer->subMeshIndex);
		if (ImGui::DragInt("サブメッシュ", &subMesh, 1.0f, 0, 1024)) { meshRenderer->subMeshIndex = static_cast<uint32_t>(subMesh); changed = true; }
		changed = DrawBlendMode("ブレンド方式", meshRenderer->blendMode) || changed;
		changed = ImGui::DragFloat3("開始スケール", &meshRenderer->startScale.x, 0.01f, 0.0f, 1000.0f) || changed;
		changed = ImGui::DragFloat3("終了スケール", &meshRenderer->endScale.x, 0.01f, 0.0f, 1000.0f) || changed;
		changed = ImGui::DragFloat3("開始回転", &meshRenderer->startRotation.x, 0.01f) || changed;
		changed = ImGui::DragFloat3("角速度", &meshRenderer->angularVelocity.x, 0.01f) || changed;
	}
	else if (auto* fluidOutput = std::get_if<VfxGraphFluidOutputNode>(&node.payload))
	{
		static const char* kDomains[] = { "2D流体", "3Dボリューム流体" };
		int domain = static_cast<int>(fluidOutput->domain);
		if (ImGui::Combo("流体領域", &domain, kDomains, IM_ARRAYSIZE(kDomains))) { fluidOutput->domain = static_cast<VfxGraphFluidDomain>(domain); changed = true; }
		changed = ImGui::DragFloat3("ローカル位置補正", &fluidOutput->localOffset.x, 0.01f) || changed;
		changed = ImGui::DragFloat3("発生元速度", &fluidOutput->localVelocity.x, 0.01f) || changed;
		changed = ImGui::DragFloat("継続時間", &fluidOutput->duration, 0.01f, 0.001f, 120.0f) || changed;
		changed = ImGui::DragFloat("半径", &fluidOutput->radius, 0.01f, 0.001f, 1000.0f) || changed;
		changed = ImGui::DragFloat("速度の強さ", &fluidOutput->velocityStrength, 0.01f, 0.0f, 1000.0f) || changed;
		changed = ImGui::DragFloat("密度発生量", &fluidOutput->densityRate, 0.01f) || changed;
		changed = ImGui::DragFloat("温度発生量", &fluidOutput->temperatureRate, 0.01f) || changed;
		changed = ImGui::DragFloat("減衰指数", &fluidOutput->falloffExponent, 0.01f, 0.001f, 32.0f) || changed;
		editString("強度パラメーター", fluidOutput->intensityParameter);
		editString("半径パラメーター", fluidOutput->radiusParameter);
	}
	else if (auto* lightOutput = std::get_if<VfxGraphLightOutputNode>(&node.payload))
	{
		changed = ImGui::DragFloat3("ローカル位置補正", &lightOutput->localOffset.x, 0.01f) || changed;
		changed = ImGui::ColorEdit3("色", &lightOutput->color.x) || changed;
		changed = ImGui::DragFloat("継続時間", &lightOutput->duration, 0.01f, 0.001f, 120.0f) || changed;
		changed = ImGui::DragFloat("強度", &lightOutput->intensity, 0.01f, 0.0f, 100000.0f) || changed;
		changed = ImGui::DragFloat("範囲", &lightOutput->range, 0.01f, 0.001f, 10000.0f) || changed;
		editString("強度パラメーター", lightOutput->intensityParameter);
		editString("半径パラメーター", lightOutput->radiusParameter);
	}
	else if (auto* postEffectOutput = std::get_if<VfxGraphPostEffectOutputNode>(&node.payload))
	{
		editString("エフェクト名", postEffectOutput->effectName);
		changed = ImGui::DragFloat("継続時間", &postEffectOutput->duration, 0.01f, 0.001f, 120.0f) || changed;
		changed = ImGui::SliderFloat("適用率", &postEffectOutput->weight, 0.0f, 1.0f) || changed;
		editString("強度パラメーター", postEffectOutput->intensityParameter);
	}
	return changed;
#else
	(void)node;
	return false;
#endif // USE_IMGUI
}

void VfxGraphEditor::DrawFloatCurveEditor(VfxFloatCurve& curve)
{
#ifdef USE_IMGUI
	static const char* kInterpolation[] = { "線形", "段階", "滑らか" };
	int interpolation = static_cast<int>(curve.interpolation);
	if (ImGui::Combo("補間方式", &interpolation, kInterpolation, IM_ARRAYSIZE(kInterpolation)))
	{
		curve.interpolation = static_cast<VfxCurveInterpolation>(interpolation);
		MarkGraphDirty();
	}

	int removeIndex = -1;
	for (size_t i = 0; i < curve.keys.size(); ++i)
	{
		ImGui::PushID(static_cast<int>(i));
		bool changed = ImGui::DragFloat("時間", &curve.keys[i].time, 0.01f, 0.0f, 1.0f);
		changed = ImGui::DragFloat("値", &curve.keys[i].value, 0.01f) || changed;
		ImGui::SameLine();
		if (ImGui::SmallButton("削除"))
		{
			removeIndex = static_cast<int>(i);
		}
		if (changed)
		{
			MarkGraphDirty();
		}
		ImGui::PopID();
	}
	if (removeIndex >= 0 && curve.keys.size() > 1u)
	{
		curve.keys.erase(curve.keys.begin() + removeIndex);
		MarkGraphDirty();
	}
	if (curve.keys.size() < VfxGraphDesc::kMaxCurveKeys && ImGui::Button("カーブキーを追加"))
	{
		curve.keys.push_back({ 1.0f, 1.0f });
		MarkGraphDirty();
	}
	std::sort(curve.keys.begin(), curve.keys.end(), [](const VfxFloatCurveKey& a, const VfxFloatCurveKey& b) { return a.time < b.time; });
#endif // USE_IMGUI
}

void VfxGraphEditor::DrawColorGradientEditor(VfxColorGradient& gradient)
{
#ifdef USE_IMGUI
	static const char* kInterpolation[] = { "線形", "段階", "滑らか" };
	int interpolation = static_cast<int>(gradient.interpolation);
	if (ImGui::Combo("補間方式", &interpolation, kInterpolation, IM_ARRAYSIZE(kInterpolation)))
	{
		gradient.interpolation = static_cast<VfxCurveInterpolation>(interpolation);
		MarkGraphDirty();
	}

	int removeIndex = -1;
	for (size_t i = 0; i < gradient.keys.size(); ++i)
	{
		ImGui::PushID(static_cast<int>(i));
		bool changed = ImGui::DragFloat("時間", &gradient.keys[i].time, 0.01f, 0.0f, 1.0f);
		changed = ImGui::ColorEdit4("色", &gradient.keys[i].color.x) || changed;
		ImGui::SameLine();
		if (ImGui::SmallButton("削除"))
		{
			removeIndex = static_cast<int>(i);
		}
		if (changed)
		{
			MarkGraphDirty();
		}
		ImGui::PopID();
	}
	if (removeIndex >= 0 && gradient.keys.size() > 1u)
	{
		gradient.keys.erase(gradient.keys.begin() + removeIndex);
		MarkGraphDirty();
	}
	if (gradient.keys.size() < VfxGraphDesc::kMaxGradientKeys && ImGui::Button("グラデーションキーを追加"))
	{
		gradient.keys.push_back({ 1.0f, { 1.0f, 1.0f, 1.0f, 1.0f } });
		MarkGraphDirty();
	}
	std::sort(gradient.keys.begin(), gradient.keys.end(), [](const VfxColorGradientKey& a, const VfxColorGradientKey& b) { return a.time < b.time; });
#endif // USE_IMGUI
}

void VfxGraphEditor::DrawPreviewPanel()
{
#ifdef USE_IMGUI
	ImGui::SeparatorText("プレビュー");
	ImGui::DragFloat3("プレビュー位置", &previewPosition_.x, 0.05f);
	if (!previewHandle_.IsValid())
	{
		if (ImGui::Button("プレビュー再生"))
		{
			StartPreview();
		}
	}
	else
	{
		if (ImGui::Button("プレビュー再開始"))
		{
			StopPreview();
			StartPreview();
		}
		ImGui::SameLine();
		if (ImGui::Button("プレビュー停止"))
		{
			StopPreview();
			lastMessage_ = "プレビューを停止しました。";
		}
		VfxGraphRuntime::GetInstance()->SetLoopPosition(previewHandle_, previewPosition_);
	}
	ImGui::SameLine();
	ImGui::TextDisabled("GPUパーティクル、流体、ライト、ポストエフェクトを共通VFXグラフランタイム経由でプレビューします。");
#endif // USE_IMGUI
}

void VfxGraphEditor::DrawCompileDiagnostics()
{
#ifdef USE_IMGUI
	ImGui::SeparatorText("コンパイル診断");
	if (compileResult_.success)
	{
		ImGui::Text("コンパイル: 成功 | エミッター=%d | 連携トラック=%d | 警告=%d", static_cast<int>(compileResult_.program.emitters.size()), static_cast<int>(compileResult_.program.integrationOneShotCue.tracks.size()), static_cast<int>(compileResult_.warnings.size()));
	}
	else
	{
		ImGui::Text("コンパイル: 失敗 | エラー=%d", static_cast<int>(compileResult_.errors.size()));
	}
	for (const auto& warning : compileResult_.warnings)
	{
		ImGui::BulletText("警告: %s", warning.c_str());
	}
	for (const auto& error : compileResult_.errors)
	{
		ImGui::BulletText("エラー: %s", error.c_str());
	}

	if (VfxGraphEmitterDesc* emitter = GetSelectedEmitter())
	{
		for (const auto& compiledEmitter : compileResult_.program.emitters)
		{
			if (compiledEmitter.name != emitter->name)
			{
				continue;
			}
			ImGui::Text("実行順序:");
			for (uint32_t id : compiledEmitter.executionOrder)
			{
				ImGui::SameLine();
				ImGui::Text("%u", id);
			}
			break;
		}
	}
#endif // USE_IMGUI
}

} // namespace Ken4lowEngine
