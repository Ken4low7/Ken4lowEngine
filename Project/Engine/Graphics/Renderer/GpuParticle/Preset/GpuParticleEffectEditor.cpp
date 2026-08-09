#include "GpuParticleEffectEditor.h"
#include "GpuParticleEffectSerializer.h"

#ifdef USE_IMGUI
#include <algorithm>
#include <array>
#include <cstring>

#include <imgui.h>
#endif

namespace Ken4lowEngine
{

#ifdef USE_IMGUI
	namespace
	{
		void DrawStringInput(const char* label, std::string& value)
		{
			std::array<char, 512> buffer{};
			const size_t copyLength = (std::min)(value.size(), buffer.size() - 1);
			std::memcpy(buffer.data(), value.data(), copyLength);
			if (ImGui::InputText(label, buffer.data(), buffer.size())) value = buffer.data();
		}

		// Emitter削除やJSON Load後に範囲外アクセスしないため、選択Indexを現在の配列へ合わせる。
		void ClampSelectedEmitterIndex(const GpuParticleEffectDesc& effect, int& selectedEmitterIndex)
		{
			if (effect.emitters.empty())
			{
				selectedEmitterIndex = -1;
				return;
			}

			selectedEmitterIndex = std::clamp(
				selectedEmitterIndex < 0 ? 0 : selectedEmitterIndex,
				0,
				static_cast<int>(effect.emitters.size()) - 1);
		}
	}
#endif

	void DrawEmitterDescImGui(GpuParticleEmitterDesc& desc)
	{
#ifdef USE_IMGUI
		// 基本情報とEmitterの生成期間・寿命を調整するセクション。
		if (ImGui::CollapsingHeader("Basic", ImGuiTreeNodeFlags_DefaultOpen))
		{
			DrawStringInput("Name", desc.name);
			int renderType = static_cast<int>(desc.renderType);
			const char* renderTypeNames[] = { "Sprite", "Mesh" };
			if (ImGui::Combo("Render Type", &renderType, renderTypeNames, IM_ARRAYSIZE(renderTypeNames)))
				desc.renderType = static_cast<GpuParticleRenderType>(renderType);
			DrawStringInput("Texture Path", desc.texturePath);
			DrawStringInput("Mesh Path", desc.meshPath);
			int maxParticles = static_cast<int>((std::min)(desc.maxParticles, 1000000u));
			if (ImGui::DragInt("Max Particles", &maxParticles, 1.0f, 0, 1000000))
				desc.maxParticles = static_cast<uint32_t>((std::max)(maxParticles, 0));
			ImGui::Checkbox("Loop", &desc.loop);
			ImGui::DragFloat("Duration", &desc.duration, 0.01f, 0.0f, 3600.0f);
			ImGui::DragFloat("Spawn Rate", &desc.spawnRate, 0.1f, 0.0f, 100000.0f);
			int burstCount = static_cast<int>((std::min)(desc.burstCount, 100000u));
			if (ImGui::DragInt("Burst Count", &burstCount, 1.0f, 0, 100000))
				desc.burstCount = static_cast<uint32_t>((std::max)(burstCount, 0));
			ImGui::DragFloat("Life Time", &desc.lifeTime, 0.01f, 0.01f, 3600.0f);
			ImGui::DragFloat("Life Time Random", &desc.lifeTimeRandom, 0.01f, 0.0f, 3600.0f);
		}

		// 発生位置とPoint/Sphere/Box分布を調整するセクション。
		if (ImGui::CollapsingHeader("Spawn", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::DragFloat3("Position", &desc.position.x, 0.01f);
			ImGui::DragFloat3("Position Random", &desc.positionRandom.x, 0.01f);
			int spawnShape = static_cast<int>(desc.spawnShape);
			const char* spawnShapeNames[] = { "Point", "Sphere", "Box" };
			if (ImGui::Combo("Spawn Shape", &spawnShape, spawnShapeNames, IM_ARRAYSIZE(spawnShapeNames)))
				desc.spawnShape = static_cast<GpuParticleSpawnShape>(spawnShape);
			ImGui::DragFloat("Spawn Radius", &desc.spawnRadius, 0.01f, 0.0f, 10000.0f);
			ImGui::DragFloat3("Spawn Box Size", &desc.spawnBoxSize.x, 0.01f, 0.0f, 10000.0f);
		}

		// 初速度、速さ、重力、減衰を調整するセクション。
		if (ImGui::CollapsingHeader("Velocity / Physics", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::DragFloat3("Velocity", &desc.velocity.x, 0.01f);
			ImGui::DragFloat3("Velocity Random", &desc.velocityRandom.x, 0.01f);
			ImGui::DragFloat("Speed", &desc.speed, 0.01f, 0.0f, 10000.0f);
			ImGui::DragFloat("Speed Random", &desc.speedRandom, 0.01f, 0.0f, 10000.0f);
			ImGui::DragFloat3("Gravity", &desc.gravity.x, 0.01f);
			ImGui::DragFloat("Damping", &desc.damping, 0.01f, 0.0f, 1000.0f);
		}

		// Spriteの2DサイズとMeshの3Dスケールを調整するセクション。
		if (ImGui::CollapsingHeader("Size", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::DragFloat2("Start Size", &desc.startSize.x, 0.01f, 0.0f, 10000.0f);
			ImGui::DragFloat2("End Size", &desc.endSize.x, 0.01f, 0.0f, 10000.0f);
			ImGui::DragFloat("Size Random", &desc.sizeRandom, 0.01f, 0.0f, 1.0f);
			ImGui::DragFloat3("Start Scale 3D", &desc.startScale3D.x, 0.01f, 0.0f, 10000.0f);
			ImGui::DragFloat3("End Scale 3D", &desc.endScale3D.x, 0.01f, 0.0f, 10000.0f);
		}

		// RGBAは内部値と同じ0.0～1.0のColorEdit4で編集する。
		if (ImGui::CollapsingHeader("Color", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::ColorEdit4("Start Color", &desc.startColor.x);
			ImGui::ColorEdit4("End Color", &desc.endColor.x);
			ImGui::ColorEdit4("Color Random", &desc.colorRandom.x);
			ImGui::Checkbox("Alpha Fade", &desc.alphaFade);
		}

		// Sprite Z回転と将来のMesh 3軸回転を調整するセクション。
		if (ImGui::CollapsingHeader("Rotation"))
		{
			ImGui::DragFloat("Start Rotation", &desc.startRotation, 0.01f);
			ImGui::DragFloat("Rotation Speed", &desc.rotationSpeed, 0.01f);
			ImGui::DragFloat("Rotation Random", &desc.rotationRandom, 0.01f, 0.0f, 1000.0f);
			ImGui::DragFloat3("Angular Velocity", &desc.angularVelocity.x, 0.01f);
			ImGui::DragFloat3("Angular Velocity Random", &desc.angularVelocityRandom.x, 0.01f);
		}

		// Sprite専用の向き、合成、SpriteSheet設定。
		if (ImGui::CollapsingHeader("Sprite"))
		{
			ImGui::Checkbox("Billboard", &desc.billboard);
			int blendMode = static_cast<int>(desc.blendMode);
			const char* blendModeNames[] = { "Alpha", "Additive", "Multiply" };
			if (ImGui::Combo("Blend Mode", &blendMode, blendModeNames, IM_ARRAYSIZE(blendModeNames)))
				desc.blendMode = static_cast<GpuParticleBlendMode>(blendMode);
			ImGui::Checkbox("Use Sprite Sheet", &desc.useSpriteSheet);
			if (desc.useSpriteSheet)
			{
				ImGui::DragInt("Sprite Sheet Rows", &desc.spriteSheetRows, 1.0f, 1, 64);
				ImGui::DragInt("Sprite Sheet Columns", &desc.spriteSheetColumns, 1.0f, 1, 64);
				ImGui::DragFloat("Sprite Sheet Frame Rate", &desc.spriteSheetFrameRate, 0.1f, 0.0f, 1000.0f);
			}
			else ImGui::TextDisabled("SpriteSheet parameters are disabled.");
		}

		// Mesh専用のモデル、テクスチャ、3Dスケール・回転設定。
		if (ImGui::CollapsingHeader("Mesh"))
		{
			DrawStringInput("Mesh Path##MeshSection", desc.meshPath);
			DrawStringInput("Texture Path##MeshSection", desc.texturePath);
			ImGui::DragFloat3("Start Scale 3D##MeshSection", &desc.startScale3D.x, 0.01f, 0.0f, 10000.0f);
			ImGui::DragFloat3("End Scale 3D##MeshSection", &desc.endScale3D.x, 0.01f, 0.0f, 10000.0f);
			ImGui::DragFloat3("Angular Velocity##MeshSection", &desc.angularVelocity.x, 0.01f);
			ImGui::DragFloat3("Angular Velocity Random##MeshSection", &desc.angularVelocityRandom.x, 0.01f);
		}
#else
		(void)desc;
#endif
	}

	void DrawGpuParticleEffectEditor(
		GpuParticleEffectDesc& effect,
		int& selectedEmitterIndex,
		std::string& jsonPath,
		std::string& statusMessage,
		bool& lastOperationSucceeded)
	{
#ifdef USE_IMGUI
		DrawStringInput("Effect Name", effect.effectName);
		DrawStringInput("JSON Path", jsonPath);

		// ImGuiで編集したEffect設定をJSON化し、次回の編集時に復元できるようにする。
		if (ImGui::Button("Save JSON"))
		{
			lastOperationSucceeded = GpuParticleEffectSerializer::Save(effect, jsonPath);
			statusMessage = lastOperationSucceeded
				? "Effect JSONを保存しました: " + jsonPath
				: "Effect JSONの保存に失敗しました: " + jsonPath;
		}
		ImGui::SameLine();
		if (ImGui::Button("Load JSON"))
		{
			lastOperationSucceeded = GpuParticleEffectSerializer::Load(effect, jsonPath);
			statusMessage = lastOperationSucceeded
				? "Effect JSONを読み込みました: " + jsonPath
				: "Effect JSONの読み込みに失敗しました: " + jsonPath;

			// LoadでEmitter数が変わっても範囲外アクセスしないよう、選択Indexを補正する。
			if (lastOperationSucceeded) ClampSelectedEmitterIndex(effect, selectedEmitterIndex);
		}

		const ImVec4 statusColor = lastOperationSucceeded
			? ImVec4(0.35f, 0.9f, 0.45f, 1.0f)
			: ImVec4(1.0f, 0.35f, 0.3f, 1.0f);
		ImGui::TextColored(statusColor, "%s", statusMessage.c_str());
		ImGui::Separator();

		// Sprite用EmitterとMesh用Emitterを明確に分け、用途に合った既定値で追加する。
		if (ImGui::Button("Add Sprite Emitter"))
		{
			auto emitter = CreateDefaultSpriteEmitterDesc();
			emitter.name += "_" + std::to_string(effect.emitters.size());
			effect.emitters.push_back(std::move(emitter));
			selectedEmitterIndex = static_cast<int>(effect.emitters.size()) - 1;
		}
		ImGui::SameLine();
		if (ImGui::Button("Add Mesh Emitter"))
		{
			auto emitter = CreateDefaultMeshEmitterDesc();
			emitter.name += "_" + std::to_string(effect.emitters.size());
			effect.emitters.push_back(std::move(emitter));
			selectedEmitterIndex = static_cast<int>(effect.emitters.size()) - 1;
		}
		ImGui::SameLine();
		if (ImGui::Button("Clear Emitters"))
		{
			effect.emitters.clear();
			selectedEmitterIndex = -1;
		}

		ImGui::Text("Emitter Count: %zu", effect.emitters.size());
		ClampSelectedEmitterIndex(effect, selectedEmitterIndex);

		ImGui::BeginChild("Emitter List", ImVec2(220.0f, 360.0f), true);
		ImGui::SeparatorText("Emitters");
		for (size_t index = 0; index < effect.emitters.size(); ++index)
		{
			const auto& emitter = effect.emitters[index];
			ImGui::PushID(static_cast<int>(index));
			const char* renderTypeName = emitter.renderType == GpuParticleRenderType::Mesh ? "Mesh" : "Sprite";
			const std::string label = emitter.name + " [" + renderTypeName + "]";
			if (ImGui::Selectable(label.c_str(), selectedEmitterIndex == static_cast<int>(index)))
			{
				selectedEmitterIndex = static_cast<int>(index);
			}
			ImGui::PopID();
		}
		ImGui::EndChild();

		ImGui::SameLine();
		ImGui::BeginChild("Selected Emitter", ImVec2(0.0f, 360.0f), true);
		if (selectedEmitterIndex >= 0 && selectedEmitterIndex < static_cast<int>(effect.emitters.size()))
		{
			ImGui::SeparatorText("Selected Emitter");
			ImGui::Text("Index: %d", selectedEmitterIndex);
			DrawEmitterDescImGui(effect.emitters[static_cast<size_t>(selectedEmitterIndex)]);

			if (ImGui::Button("Remove Selected Emitter"))
			{
				effect.emitters.erase(effect.emitters.begin() + selectedEmitterIndex);
				// 削除後に範囲外アクセスしないよう、次に選択できるIndexへ補正する。
				ClampSelectedEmitterIndex(effect, selectedEmitterIndex);
			}
		}
		else
		{
			ImGui::TextDisabled("Emitterを追加または選択してください。");
		}
		ImGui::EndChild();
#else
		(void)effect;
		(void)selectedEmitterIndex;
		(void)jsonPath;
		(void)statusMessage;
		(void)lastOperationSucceeded;
#endif
	}

} // namespace Ken4lowEngine
