#include "GpuParticleEffectEditor.h"
#include "GpuParticleEffectSerializer.h"
#include "../Runtime/GpuParticleEffectRuntime.h"

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

		void StopPreviewLoopIfNeeded(GpuParticleEffectRuntime::PlayHandle& previewLoopHandle)
		{
			if (!previewLoopHandle.IsValid()) return;
			GpuParticleEffectRuntime::GetInstance()->StopLoop(previewLoopHandle);
			previewLoopHandle = {};
		}

		bool RegisterPreviewEffect(
			GpuParticleEffectDesc& effect,
			GpuParticleEffectRuntime::PlayHandle& previewLoopHandle,
			std::string& statusMessage,
			bool& lastOperationSucceeded)
		{
			// In-memory Authoring値を同じGameplay Runtimeへ再登録し、保存前でも最終GPU経路をPreviewする。
			StopPreviewLoopIfNeeded(previewLoopHandle);
			GpuParticleEffectRuntime* runtime = GpuParticleEffectRuntime::GetInstance();
			lastOperationSucceeded = runtime->RegisterEffect(effect);
			statusMessage = runtime->GetLastStatus();
			return lastOperationSucceeded;
		}

		void DrawUserParameters(GpuParticleEffectDesc& effect)
		{
			if (!ImGui::CollapsingHeader("User Parameters", ImGuiTreeNodeFlags_DefaultOpen)) return;

			if (ImGui::Button("Add Float Parameter"))
			{
				GpuParticleUserParameterDesc parameter{};
				parameter.name = "Parameter_" + std::to_string(effect.userParameters.size());
				effect.userParameters.push_back(std::move(parameter));
			}

			for (size_t index = 0; index < effect.userParameters.size();)
			{
				ImGui::PushID(static_cast<int>(index));
				GpuParticleUserParameterDesc& parameter = effect.userParameters[index];
				DrawStringInput("Name", parameter.name);
				ImGui::DragFloat("Default", &parameter.defaultValue, 0.01f);
				ImGui::DragFloat("Min", &parameter.minValue, 0.01f);
				ImGui::DragFloat("Max", &parameter.maxValue, 0.01f);
				if (parameter.minValue > parameter.maxValue) std::swap(parameter.minValue, parameter.maxValue);
				parameter.defaultValue = std::clamp(parameter.defaultValue, parameter.minValue, parameter.maxValue);

				const bool remove = ImGui::Button("Remove Parameter");
				ImGui::Separator();
				ImGui::PopID();
				if (remove)
				{
					effect.userParameters.erase(effect.userParameters.begin() + static_cast<std::ptrdiff_t>(index));
					continue;
				}
				++index;
			}
		}

		void DrawParameterBindings(GpuParticleEmitterDesc& desc)
		{
			if (!ImGui::TreeNode("User Parameter Bindings")) return;

			if (ImGui::Button("Add Binding"))
			{
				desc.parameterBindings.push_back({});
			}

			const char* targetNames[] = { "SpawnRate", "BurstCount", "LifeTime", "Speed", "Size", "Alpha", "Force" };
			for (size_t index = 0; index < desc.parameterBindings.size();)
			{
				ImGui::PushID(static_cast<int>(index));
				GpuParticleParameterBindingDesc& binding = desc.parameterBindings[index];
				DrawStringInput("Parameter Name", binding.parameterName);
				int target = static_cast<int>(binding.target);
				if (ImGui::Combo("Target", &target, targetNames, IM_ARRAYSIZE(targetNames)))
				{
					binding.target = static_cast<GpuParticleParameterTarget>(target);
				}
				ImGui::DragFloat("Scale", &binding.scale, 0.01f);
				ImGui::DragFloat("Bias", &binding.bias, 0.01f);
				const bool remove = ImGui::Button("Remove Binding");
				ImGui::Separator();
				ImGui::PopID();
				if (remove)
				{
					desc.parameterBindings.erase(desc.parameterBindings.begin() + static_cast<std::ptrdiff_t>(index));
					continue;
				}
				++index;
			}
			ImGui::TreePop();
		}
	}
#endif

	void DrawEmitterDescImGui(GpuParticleEmitterDesc& desc)
	{
#ifdef USE_IMGUI
		if (ImGui::CollapsingHeader("Emission Module", ImGuiTreeNodeFlags_DefaultOpen))
		{
			DrawStringInput("Name", desc.name);
			int maxParticles = static_cast<int>((std::min)(desc.maxParticles, 1000000u));
			if (ImGui::DragInt("Max Particles", &maxParticles, 1.0f, 1, 1000000))
				desc.maxParticles = static_cast<uint32_t>((std::max)(maxParticles, 1));
			ImGui::Checkbox("Loop", &desc.loop);
			ImGui::DragFloat("Duration", &desc.duration, 0.01f, 0.0f, 3600.0f);
			ImGui::DragFloat("Spawn Rate", &desc.spawnRate, 0.1f, 0.0f, 100000.0f);
			int burstCount = static_cast<int>((std::min)(desc.burstCount, 100000u));
			if (ImGui::DragInt("Burst Count", &burstCount, 1.0f, 0, 100000))
				desc.burstCount = static_cast<uint32_t>((std::max)(burstCount, 0));
		}

		if (ImGui::CollapsingHeader("Spawn Module", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::DragFloat3("Position", &desc.position.x, 0.01f);
			ImGui::DragFloat3("Position Random", &desc.positionRandom.x, 0.01f);
			int spawnShape = static_cast<int>(desc.spawnShape);
			const char* spawnShapeNames[] = { "Point", "Sphere", "Box", "Cone", "Circle", "Ring", "Hemisphere" };
			if (ImGui::Combo("Spawn Shape", &spawnShape, spawnShapeNames, IM_ARRAYSIZE(spawnShapeNames)))
				desc.spawnShape = static_cast<GpuParticleSpawnShape>(spawnShape);
			ImGui::DragFloat("Spawn Radius", &desc.spawnRadius, 0.01f, 0.0f, 10000.0f);
			ImGui::DragFloat3("Spawn Box Size / Cone Height", &desc.spawnBoxSize.x, 0.01f, 0.0f, 10000.0f);
			ImGui::DragFloat("Life Time", &desc.lifeTime, 0.01f, 0.01f, 3600.0f);
			ImGui::DragFloat("Life Time Random", &desc.lifeTimeRandom, 0.01f, 0.0f, 3600.0f);
			ImGui::DragFloat3("Velocity", &desc.velocity.x, 0.01f);
			ImGui::DragFloat3("Velocity Random", &desc.velocityRandom.x, 0.01f);
			ImGui::DragFloat("Speed", &desc.speed, 0.01f, 0.0f, 10000.0f);
			ImGui::DragFloat("Speed Random", &desc.speedRandom, 0.01f, 0.0f, 10000.0f);
		}

		if (ImGui::CollapsingHeader("Update Module", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::SeparatorText("Forces");
			ImGui::DragFloat3("Gravity", &desc.gravity.x, 0.01f);
			ImGui::DragFloat("Damping", &desc.damping, 0.01f, 0.0f, 1000.0f);
			ImGui::DragFloat("Noise Strength", &desc.noiseStrength, 0.01f);
			ImGui::DragFloat("Noise Frequency", &desc.noiseFrequency, 0.01f, 0.0f, 1000.0f);
			ImGui::DragFloat3("Vortex Axis", &desc.vortexAxis.x, 0.01f);
			ImGui::DragFloat("Vortex Strength", &desc.vortexStrength, 0.01f);
			ImGui::DragFloat3("Attractor Position", &desc.attractorPosition.x, 0.01f);
			ImGui::DragFloat("Attractor Strength", &desc.attractorStrength, 0.01f);
			ImGui::DragFloat("Attractor Radius", &desc.attractorRadius, 0.01f, 0.0f, 10000.0f);

			ImGui::SeparatorText("Size / Scale");
			ImGui::DragFloat2("Start Size", &desc.startSize.x, 0.01f, 0.0f, 10000.0f);
			ImGui::DragFloat2("End Size", &desc.endSize.x, 0.01f, 0.0f, 10000.0f);
			ImGui::DragFloat("Size Random", &desc.sizeRandom, 0.01f, 0.0f, 1.0f);
			ImGui::Checkbox("Use Size Curve", &desc.useSizeCurve);
			if (desc.useSizeCurve)
			{
				ImGui::DragFloat4("Size Curve LUT [0,1/3,2/3,1]", &desc.sizeCurveLut.x, 0.01f, 0.0f, 100.0f);
			}
			ImGui::DragFloat3("Start Scale 3D", &desc.startScale3D.x, 0.01f, 0.0f, 10000.0f);
			ImGui::DragFloat3("End Scale 3D", &desc.endScale3D.x, 0.01f, 0.0f, 10000.0f);

			ImGui::SeparatorText("Color");
			ImGui::ColorEdit4("Start Color", &desc.startColor.x);
			ImGui::ColorEdit4("End Color", &desc.endColor.x);
			ImGui::ColorEdit4("Color Random", &desc.colorRandom.x);
			ImGui::Checkbox("Alpha Fade", &desc.alphaFade);
			ImGui::Checkbox("Use Color Gradient", &desc.useColorGradient);
			if (desc.useColorGradient)
			{
				for (size_t index = 0; index < desc.colorGradientLut.size(); ++index)
				{
					ImGui::PushID(static_cast<int>(index));
					ImGui::ColorEdit4("Gradient Key", &desc.colorGradientLut[index].x);
					ImGui::PopID();
				}
			}

			ImGui::SeparatorText("Rotation");
			ImGui::DragFloat("Start Rotation", &desc.startRotation, 0.01f);
			ImGui::DragFloat("Rotation Speed", &desc.rotationSpeed, 0.01f);
			ImGui::DragFloat("Rotation Random", &desc.rotationRandom, 0.01f, 0.0f, 1000.0f);
			ImGui::DragFloat3("Start Rotation 3D", &desc.startRotation3D.x, 0.01f);
			ImGui::DragFloat3("Rotation Random 3D", &desc.rotationRandom3D.x, 0.01f, 0.0f, 1000.0f);
			ImGui::DragFloat3("Angular Velocity", &desc.angularVelocity.x, 0.01f);
			ImGui::DragFloat3("Angular Velocity Random", &desc.angularVelocityRandom.x, 0.01f, 0.0f, 1000.0f);

			DrawParameterBindings(desc);
		}

		if (ImGui::CollapsingHeader("Render Module", ImGuiTreeNodeFlags_DefaultOpen))
		{
			int renderType = static_cast<int>(desc.renderType);
			const char* renderTypeNames[] = { "Sprite", "Mesh" };
			if (ImGui::Combo("Render Type", &renderType, renderTypeNames, IM_ARRAYSIZE(renderTypeNames)))
				desc.renderType = static_cast<GpuParticleRenderType>(renderType);
			DrawStringInput("Texture Path", desc.texturePath);
			DrawStringInput("Mesh Path", desc.meshPath);
			if (desc.renderType == GpuParticleRenderType::Mesh)
			{
				int subMeshIndex = static_cast<int>((std::min)(desc.meshSubMeshIndex, 65535u));
				if (ImGui::DragInt("Mesh SubMesh Index", &subMeshIndex, 1.0f, 0, 65535))
					desc.meshSubMeshIndex = static_cast<uint32_t>((std::max)(subMeshIndex, 0));
			}
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
		static Vector3 previewPosition{ 0.0f, 0.0f, 0.0f };
		static GpuParticleEffectRuntime::PlayHandle previewLoopHandle{};

		DrawStringInput("Effect Name", effect.effectName);
		DrawStringInput("JSON Path", jsonPath);

		if (ImGui::Button("Save JSON"))
		{
			lastOperationSucceeded = GpuParticleEffectSerializer::Save(effect, jsonPath);
			statusMessage = lastOperationSucceeded ? "Effect JSONを保存しました: " + jsonPath : "Effect JSONの保存に失敗しました: " + jsonPath;
		}
		ImGui::SameLine();
		if (ImGui::Button("Load JSON"))
		{
			lastOperationSucceeded = GpuParticleEffectSerializer::Load(effect, jsonPath);
			statusMessage = lastOperationSucceeded ? "Effect JSONを読み込みました: " + jsonPath : "Effect JSONの読み込みに失敗しました: " + jsonPath;
			if (lastOperationSucceeded) ClampSelectedEmitterIndex(effect, selectedEmitterIndex);
		}

		const ImVec4 statusColor = lastOperationSucceeded
			? ImVec4(0.35f, 0.9f, 0.45f, 1.0f)
			: ImVec4(1.0f, 0.35f, 0.3f, 1.0f);
		ImGui::TextColored(statusColor, "%s", statusMessage.c_str());
		ImGui::Separator();

		DrawUserParameters(effect);

		if (ImGui::CollapsingHeader("Runtime Preview", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::DragFloat3("Preview Position", &previewPosition.x, 0.05f);
			if (ImGui::Button("Preview Burst"))
			{
				if (RegisterPreviewEffect(effect, previewLoopHandle, statusMessage, lastOperationSucceeded))
				{
					GpuParticleEffectRuntime* runtime = GpuParticleEffectRuntime::GetInstance();
					lastOperationSucceeded = runtime->Play(effect.effectName, previewPosition);
					statusMessage = runtime->GetLastStatus();
				}
			}
			ImGui::SameLine();
			if (ImGui::Button("Start Loop Preview"))
			{
				if (RegisterPreviewEffect(effect, previewLoopHandle, statusMessage, lastOperationSucceeded))
				{
					GpuParticleEffectRuntime* runtime = GpuParticleEffectRuntime::GetInstance();
					previewLoopHandle = runtime->PlayLoop(effect.effectName, previewPosition);
					lastOperationSucceeded = previewLoopHandle.IsValid();
					statusMessage = runtime->GetLastStatus();
				}
			}
			ImGui::SameLine();
			if (ImGui::Button("Stop Loop Preview"))
			{
				GpuParticleEffectRuntime* runtime = GpuParticleEffectRuntime::GetInstance();
				lastOperationSucceeded = previewLoopHandle.IsValid() && runtime->StopLoop(previewLoopHandle);
				previewLoopHandle = {};
				statusMessage = runtime->GetLastStatus();
			}

			if (previewLoopHandle.IsValid())
			{
				GpuParticleEffectRuntime::GetInstance()->SetLoopPosition(previewLoopHandle, previewPosition);
				ImGui::TextDisabled("Loop Preview Handle: %u", previewLoopHandle.id);
			}
			ImGui::TextDisabled("Runtime: Sprite/Mesh + Alpha/Additive/Multiply + all authored Spawn Shapes + Curve/Force modules.");
		}

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

		ImGui::BeginChild("Emitter List", ImVec2(220.0f, 540.0f), true);
		ImGui::SeparatorText("Emitters");
		for (size_t index = 0; index < effect.emitters.size(); ++index)
		{
			const auto& emitter = effect.emitters[index];
			ImGui::PushID(static_cast<int>(index));
			const char* renderTypeName = emitter.renderType == GpuParticleRenderType::Mesh ? "Mesh" : "Sprite";
			const std::string label = emitter.name + " [" + renderTypeName + "]";
			if (ImGui::Selectable(label.c_str(), selectedEmitterIndex == static_cast<int>(index))) selectedEmitterIndex = static_cast<int>(index);
			ImGui::PopID();
		}
		ImGui::EndChild();

		ImGui::SameLine();
		ImGui::BeginChild("Selected Emitter", ImVec2(0.0f, 540.0f), true);
		if (selectedEmitterIndex >= 0 && selectedEmitterIndex < static_cast<int>(effect.emitters.size()))
		{
			ImGui::SeparatorText("Selected Emitter");
			ImGui::Text("Index: %d", selectedEmitterIndex);
			DrawEmitterDescImGui(effect.emitters[static_cast<size_t>(selectedEmitterIndex)]);
			if (ImGui::Button("Remove Selected Emitter"))
			{
				effect.emitters.erase(effect.emitters.begin() + selectedEmitterIndex);
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
