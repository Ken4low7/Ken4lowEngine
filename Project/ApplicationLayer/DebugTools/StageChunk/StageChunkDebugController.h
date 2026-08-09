#pragma once

namespace Ken4lowEngine { class Stage; }
namespace K4E = ::Ken4lowEngine;

/// <summary>
/// StageChunk Culling の ImGui 操作と統計表示を Scene から分離する Controller。
/// </summary>
class StageChunkDebugController
{
public:
	void DrawImGui(K4E::Stage* stage);
	void DrawImGuiContent(K4E::Stage* stage);
};
