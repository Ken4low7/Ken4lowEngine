#pragma once

#include "ModelSurfaceSampler.h"

#include <algorithm>
#include <cstdint>

#ifdef USE_IMGUI
#include <imgui.h>

namespace BlockPlacementImGui
{
	/// 崩壊と再構築で共有する配置方式の選択と、方式変更時の安全な初期値を適用する。
	template<class Parameters>
	void DrawPlacementMode(Parameters& parameters)
	{
		const char* labels[] = { "ランダム表面配置", "均一表面配置", "整列表面配置", "ボクセル敷き詰め配置" };
		int index = parameters.placementMode == DisintegrationPlacementMode::VoxelFill ? 3
			: parameters.placementMode == DisintegrationPlacementMode::AlignedSurfaceGrid ? 2
			: parameters.placementMode == DisintegrationPlacementMode::UniformSurface ? 1 : 0;
		if (!ImGui::Combo("配置モード", &index, labels, IM_ARRAYSIZE(labels))) { return; }

		parameters.placementMode = index == 3 ? DisintegrationPlacementMode::VoxelFill
			: index == 2 ? DisintegrationPlacementMode::AlignedSurfaceGrid
			: index == 1 ? DisintegrationPlacementMode::UniformSurface : DisintegrationPlacementMode::RandomSurface;
		if (parameters.placementMode == DisintegrationPlacementMode::VoxelFill)
		{
			parameters.useRandomScale = false;
			parameters.useRandomRotation = false;
			parameters.surfaceSampling = false;
			parameters.useSurfaceInset = false;
			parameters.voxelSpacing = parameters.blockSize;
			parameters.placementSpacing = parameters.voxelSpacing;
			parameters.voxelSurfaceThickness = parameters.blockSize * 1.5f;
			parameters.maxVoxelBlockCount = std::max(parameters.maxVoxelBlockCount, 10000);
		}
		else if (parameters.placementMode != DisintegrationPlacementMode::RandomSurface)
		{
			parameters.useRandomScale = false;
			parameters.useRandomRotation = false;
			parameters.useSurfaceInset = true;
		}
	}

	template<class Parameters>
	void DrawSeedAndVoxelSettings(Parameters& parameters)
	{
		int seed = static_cast<int>(parameters.placementSeed);
		if (ImGui::InputInt("配置シード", &seed))
		{
			parameters.placementSeed = static_cast<uint32_t>(std::max(seed, 0));
		}
		ImGui::SliderFloat("配置間隔", &parameters.placementSpacing, 0.0f, 0.5f);
		if (parameters.placementMode != DisintegrationPlacementMode::VoxelFill) { return; }
		ImGui::SliderFloat("ボクセル間隔", &parameters.voxelSpacing, 0.005f, 0.50f);
		ImGui::SliderInt("最大ブロック数", &parameters.maxVoxelBlockCount, 128, 30000);
		ImGui::SliderFloat("表面厚み", &parameters.voxelSurfaceThickness, 0.0f, 1.0f);
		ImGui::Checkbox("内外判定を使う", &parameters.useVoxelInsideTest);
		ImGui::Checkbox("表面近傍判定を使う", &parameters.useVoxelSurfaceNearTest);
		ImGui::Checkbox("グリッド原点を中央に揃える", &parameters.alignVoxelGridToCenter);
	}

	template<class Parameters>
	void DrawSurfaceSettings(Parameters& parameters)
	{
		ImGui::Checkbox("表面サンプリング", &parameters.surfaceSampling);
		ImGui::Checkbox("表面内側オフセットを使う", &parameters.useSurfaceInset);
		ImGui::Checkbox("ブロックサイズから自動計算", &parameters.autoSurfaceInsetFromBlockSize);
		if (!parameters.autoSurfaceInsetFromBlockSize)
		{
			ImGui::SliderFloat("表面内側オフセット量", &parameters.surfaceInset, 0.0f, 0.30f);
		}
		else
		{
			ImGui::Text("表面内側オフセット量: %.3f", parameters.blockSize * 0.5f);
		}
	}
}
#endif
