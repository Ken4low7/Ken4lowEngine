#include "VfxTimelineEditor.h"

#include "../Asset/VfxCueSerializer.h"
#include "../Runtime/VfxCueRuntime.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace Ken4lowEngine
{

VfxTimelineEditor* VfxTimelineEditor::GetInstance()
{
	static VfxTimelineEditor instance;
	return &instance;
}

void VfxTimelineEditor::Initialize()
{
	if (initialized_) return;
	initialized_ = true;
	LoadFromDisk();
}

void VfxTimelineEditor::Finalize()
{
	StopPreview();
	initialized_ = false;
}

void VfxTimelineEditor::Draw(bool* open)
{
#ifdef USE_IMGUI
	if (!initialized_) Initialize();
	if (!ImGui::Begin("VFX Timeline", open))
	{
		ImGui::End();
		return;
	}

	DrawCueHeader();
	ImGui::Separator();
	DrawUserParameters();
	ImGui::Separator();
	DrawTimeline();
	ImGui::Separator();
	DrawRuntimeDiagnostics();

	if (!lastMessage_.empty())
	{
		ImGui::Separator();
		ImGui::TextWrapped("%s", lastMessage_.c_str());
	}
	ImGui::End();
#else
	(void)open;
#endif
}

bool VfxTimelineEditor::LoadFromDisk()
{
	VfxCueDesc loaded{};
	if (!VfxCueSerializer::Load(loaded, filePath_))
	{
		lastMessage_ = "VFX load failed: " + filePath_;
		return false;
	}
	StopPreview();
	editableCue_ = std::move(loaded);
	lastMessage_ = "Loaded: " + filePath_;
	return RegisterPreviewCue();
}

bool VfxTimelineEditor::SaveToDisk()
{
	if (!VfxCueSerializer::Save(editableCue_, filePath_))
	{
		lastMessage_ = "VFX save failed: " + filePath_;
		return false;
	}
	lastMessage_ = "Saved: " + filePath_;
	return RegisterPreviewCue();
}

bool VfxTimelineEditor::RegisterPreviewCue()
{
	if (!VfxCueRuntime::GetInstance()->RegisterCue(editableCue_, filePath_))
	{
		lastMessage_ = VfxCueRuntime::GetInstance()->GetStats().lastStatus;
		return false;
	}
	return true;
}

void VfxTimelineEditor::StopPreview()
{
	if (previewHandle_.IsValid())
	{
		VfxCueRuntime::GetInstance()->Stop(previewHandle_);
		previewHandle_ = {};
	}
}

void VfxTimelineEditor::AddTrack(VfxCueTrackType type)
{
	if (editableCue_.tracks.size() >= VfxCueDesc::kMaxTracks) return;
	VfxCueTrackDesc track = CreateDefaultVfxCueTrack(type);
	track.name = std::string(TrackTypeName(type)) + std::to_string(editableCue_.tracks.size());
	track.duration = type == VfxCueTrackType::Particle ? 0.0f : 0.25f;
	if (type == VfxCueTrackType::Particle)
	{
		auto* particlePayload = std::get_if<VfxParticleTrackPayload>(&track.payload);
		particlePayload->effectName = "Phase13Explosion";
		particlePayload->effectAssetPath = "Resources/Effects/Phase13/Explosion.effect.json";
	}
	else if (type == VfxCueTrackType::PostEffect)
	{
		std::get<VfxPostEffectTrackPayload>(track.payload).effectName = "BloomEffect";
	}
	editableCue_.tracks.push_back(std::move(track));
}

void VfxTimelineEditor::DrawCueHeader()
{
#ifdef USE_IMGUI
	char pathBuffer[512]{};
	std::snprintf(pathBuffer, sizeof(pathBuffer), "%s", filePath_.c_str());
	if (ImGui::InputText("Cue File", pathBuffer, sizeof(pathBuffer))) filePath_ = pathBuffer;
	if (ImGui::Button("Load")) LoadFromDisk();
	ImGui::SameLine();
	if (ImGui::Button("Save + Register")) SaveToDisk();
	ImGui::SameLine();
	if (ImGui::Button("Hot Reload"))
	{
		if (!editableCue_.cueName.empty() && VfxCueRuntime::GetInstance()->ReloadCue(editableCue_.cueName))
			lastMessage_ = "Hot reloaded: " + editableCue_.cueName;
		else lastMessage_ = VfxCueRuntime::GetInstance()->GetStats().lastStatus;
	}

	char nameBuffer[128]{};
	std::snprintf(nameBuffer, sizeof(nameBuffer), "%s", editableCue_.cueName.c_str());
	if (ImGui::InputText("Cue Name", nameBuffer, sizeof(nameBuffer))) editableCue_.cueName = nameBuffer;
	ImGui::Checkbox("Loop Cue", &editableCue_.loop);
	ImGui::DragFloat("Authored Duration", &editableCue_.duration, 0.01f, 0.0f, 120.0f);
	ImGui::DragFloat3("Preview Position", &previewPosition_.x, 0.05f);

	if (ImGui::Button("Register In-Memory")) RegisterPreviewCue();
	ImGui::SameLine();
	if (ImGui::Button("Preview"))
	{
		StopPreview();
		if (RegisterPreviewCue()) previewHandle_ = VfxCueRuntime::GetInstance()->Play(editableCue_.cueName, previewPosition_);
	}
	ImGui::SameLine();
	if (ImGui::Button("Stop Preview")) StopPreview();
#endif
}

void VfxTimelineEditor::DrawUserParameters()
{
#ifdef USE_IMGUI
	if (!ImGui::CollapsingHeader("User Parameters", ImGuiTreeNodeFlags_DefaultOpen)) return;
	int remove = -1;
	for (uint32_t i = 0; i < editableCue_.userParameters.size(); ++i)
	{
		VfxCueUserParameterDesc& parameterDesc = editableCue_.userParameters[i];
		ImGui::PushID(static_cast<int>(i));
		char name[96]{};
		std::snprintf(name, sizeof(name), "%s", parameterDesc.name.c_str());
		if (ImGui::InputText("Name", name, sizeof(name))) parameterDesc.name = name;
		ImGui::DragFloat("Default", &parameterDesc.defaultValue, 0.01f);
		ImGui::DragFloat("Min", &parameterDesc.minValue, 0.01f);
		ImGui::DragFloat("Max", &parameterDesc.maxValue, 0.01f);
		if (ImGui::Button("Remove Parameter")) remove = static_cast<int>(i);
		ImGui::Separator();
		ImGui::PopID();
	}
	if (remove >= 0) editableCue_.userParameters.erase(editableCue_.userParameters.begin() + remove);
	if (editableCue_.userParameters.size() < VfxCueDesc::kMaxUserParameters && ImGui::Button("Add Intensity Parameter"))
	{
		editableCue_.userParameters.push_back({ "Intensity", 1.0f, 0.0f, 3.0f });
	}
#endif
}

void VfxTimelineEditor::DrawTimeline()
{
#ifdef USE_IMGUI
	if (!ImGui::CollapsingHeader("Timeline", ImGuiTreeNodeFlags_DefaultOpen)) return;

	const char* types[] = { "Particle", "Fluid2D", "VolumetricFluid", "Light", "PostEffect", "CameraShake" };
	ImGui::Combo("New Track Type", &addTrackType_, types, IM_ARRAYSIZE(types));
	ImGui::SameLine();
	if (ImGui::Button("Add Track")) AddTrack(static_cast<VfxCueTrackType>(std::clamp(addTrackType_, 0, 5)));
	ImGui::SliderFloat("Timeline Zoom", &timelinePixelsPerSecond_, 40.0f, 400.0f, "%.0f px/s");

	float maxEnd = editableCue_.duration;
	for (const VfxCueTrackDesc& track : editableCue_.tracks) maxEnd = (std::max)(maxEnd, track.startTime + track.duration);
	const float timelineWidth = (std::max)(ImGui::GetContentRegionAvail().x, maxEnd * timelinePixelsPerSecond_ + 80.0f);
	ImGui::BeginChild("VfxTimelineTracks", ImVec2(0.0f, 440.0f), true, ImGuiWindowFlags_HorizontalScrollbar);
	ImDrawList* drawList = ImGui::GetWindowDrawList();
	const ImVec2 origin = ImGui::GetCursorScreenPos();
	for (int second = 0; second <= static_cast<int>(std::ceil(maxEnd)); ++second)
	{
		const float x = origin.x + 180.0f + second * timelinePixelsPerSecond_;
		drawList->AddLine(ImVec2(x, origin.y), ImVec2(x, origin.y + 420.0f), ImGui::GetColorU32(ImGuiCol_Border));
		drawList->AddText(ImVec2(x + 2.0f, origin.y), ImGui::GetColorU32(ImGuiCol_TextDisabled), std::to_string(second).c_str());
	}

	int removeIndex = -1;
	for (uint32_t i = 0; i < editableCue_.tracks.size(); ++i)
	{
		bool remove = false;
		DrawTrackEditor(editableCue_.tracks[i], i, remove);
		if (remove) removeIndex = static_cast<int>(i);
	}
	if (removeIndex >= 0) editableCue_.tracks.erase(editableCue_.tracks.begin() + removeIndex);
	ImGui::Dummy(ImVec2(timelineWidth, 4.0f));
	ImGui::EndChild();
#endif
}

void VfxTimelineEditor::DrawTrackEditor(VfxCueTrackDesc& track, uint32_t index, bool& removeRequested)
{
#ifdef USE_IMGUI
	ImGui::PushID(static_cast<int>(index));
	const float rowY = ImGui::GetCursorScreenPos().y;
	ImGui::SetNextItemWidth(155.0f);
	char name[96]{};
	std::snprintf(name, sizeof(name), "%s", track.name.c_str());
	if (ImGui::InputText("##TrackName", name, sizeof(name))) track.name = name;
	ImGui::SameLine(162.0f);
	const float barX = ImGui::GetCursorScreenPos().x + track.startTime * timelinePixelsPerSecond_;
	const float barWidth = (std::max)(4.0f, track.duration * timelinePixelsPerSecond_);
	ImGui::GetWindowDrawList()->AddRectFilled(
		ImVec2(barX, rowY + 3.0f), ImVec2(barX + barWidth, rowY + 19.0f), ImGui::GetColorU32(ImGuiCol_Button));
	ImGui::Dummy(ImVec2(160.0f + (track.startTime + track.duration) * timelinePixelsPerSecond_, 22.0f));

	if (ImGui::TreeNode("Edit %s [%s]", track.name.c_str(), TrackTypeName(track.type)))
	{
		ImGui::Checkbox("Enabled", &track.enabled);
		ImGui::DragFloat("Start", &track.startTime, 0.01f, 0.0f, 120.0f);
		ImGui::DragFloat("Duration", &track.duration, 0.01f, 0.0f, 120.0f);
		ImGui::DragFloat3("Local Offset", &track.localOffset.x, 0.05f);

		if (auto* particlePayload = std::get_if<VfxParticleTrackPayload>(&track.payload))
		{
			char asset[256]{};
			std::snprintf(asset, sizeof(asset), "%s", particlePayload->effectAssetPath.c_str());
			if (ImGui::InputText("Effect Asset", asset, sizeof(asset))) particlePayload->effectAssetPath = asset;
			char effect[128]{};
			std::snprintf(effect, sizeof(effect), "%s", particlePayload->effectName.c_str());
			if (ImGui::InputText("Effect Name", effect, sizeof(effect))) particlePayload->effectName = effect;
			ImGui::Checkbox("Particle Loop", &particlePayload->loop);
		}
		else if (auto* fluidPayload = std::get_if<VfxFluidTrackPayload>(&track.payload))
		{
			ImGui::DragFloat3("Velocity", &fluidPayload->localVelocity.x, 0.05f);
			ImGui::DragFloat("Radius", &fluidPayload->radius, 0.02f, 0.001f, 100.0f);
			ImGui::DragFloat("Velocity Strength", &fluidPayload->velocityStrength, 0.02f, 0.0f, 100.0f);
			ImGui::DragFloat("Density Rate", &fluidPayload->densityRate, 0.02f);
			ImGui::DragFloat("Temperature Rate", &fluidPayload->temperatureRate, 0.02f);
			ImGui::DragFloat("Falloff", &fluidPayload->falloffExponent, 0.02f, 0.01f, 16.0f);
		}
		else if (auto* lightPayload = std::get_if<VfxLightTrackPayload>(&track.payload))
		{
			ImGui::ColorEdit3("Color", &lightPayload->color.x);
			ImGui::DragFloat("Intensity", &lightPayload->intensity, 0.05f, 0.0f, 1000.0f);
			ImGui::DragFloat("Range", &lightPayload->range, 0.05f, 0.01f, 1000.0f);
		}
		else if (auto* postEffectPayload = std::get_if<VfxPostEffectTrackPayload>(&track.payload))
		{
			char effect[128]{};
			std::snprintf(effect, sizeof(effect), "%s", postEffectPayload->effectName.c_str());
			if (ImGui::InputText("Post Effect", effect, sizeof(effect))) postEffectPayload->effectName = effect;
			ImGui::SliderFloat("Weight", &postEffectPayload->weight, 0.0f, 1.0f);
		}
		else if (auto* cameraShakePayload = std::get_if<VfxCameraShakeTrackPayload>(&track.payload))
		{
			ImGui::DragFloat3("Translate Amp", &cameraShakePayload->translationAmplitude.x, 0.005f);
			ImGui::DragFloat3("Rotate Amp Deg", &cameraShakePayload->rotationAmplitudeDegrees.x, 0.05f);
			ImGui::DragFloat("Frequency", &cameraShakePayload->frequency, 0.1f, 0.0f, 120.0f);
			ImGui::DragFloat("FOV Amp Deg", &cameraShakePayload->fovAmplitudeDegrees, 0.05f);
		}

		if (ImGui::Button("Add Intensity Binding"))
		{
			track.bindings.push_back({ "Intensity", VfxCueBindingTarget::IntensityScale, "", 1.0f, 0.0f });
		}
		for (uint32_t bindingIndex = 0; bindingIndex < track.bindings.size(); ++bindingIndex)
		{
			ImGui::PushID(static_cast<int>(bindingIndex) + 10000);
			VfxCueTrackBindingDesc& binding = track.bindings[bindingIndex];
			char parameter[96]{};
			std::snprintf(parameter, sizeof(parameter), "%s", binding.parameterName.c_str());
			if (ImGui::InputText("Parameter", parameter, sizeof(parameter))) binding.parameterName = parameter;
			int target = static_cast<int>(binding.target);
			const char* targets[] = { "IntensityScale", "RadiusScale", "ParticleFloat" };
			if (ImGui::Combo("Target", &target, targets, IM_ARRAYSIZE(targets)))
			{
				binding.target = static_cast<VfxCueBindingTarget>(std::clamp(target, 0, 2));
			}
			if (binding.target == VfxCueBindingTarget::ParticleFloat)
			{
				char targetName[96]{};
				std::snprintf(targetName, sizeof(targetName), "%s", binding.targetName.c_str());
				if (ImGui::InputText("Target Name", targetName, sizeof(targetName))) binding.targetName = targetName;
			}
			ImGui::DragFloat("Scale", &binding.scale, 0.01f);
			ImGui::DragFloat("Bias", &binding.bias, 0.01f);
			if (ImGui::Button("Remove Binding"))
			{
				track.bindings.erase(track.bindings.begin() + bindingIndex);
				ImGui::PopID();
				break;
			}
			ImGui::PopID();
		}

		if (ImGui::Button("Delete Track")) removeRequested = true;
		ImGui::TreePop();
	}
	ImGui::Separator();
	ImGui::PopID();
#else
	(void)track;
	(void)index;
	(void)removeRequested;
#endif
}

void VfxTimelineEditor::DrawRuntimeDiagnostics()
{
#ifdef USE_IMGUI
	if (!ImGui::CollapsingHeader("Runtime / Budget / Stress", ImGuiTreeNodeFlags_DefaultOpen)) return;
	VfxCueRuntime* runtime = VfxCueRuntime::GetInstance();
	const VfxRuntimeStats& stats = runtime->GetStats();
	VfxRuntimeBudget& budget = runtime->GetEditableBudget();

	ImGui::Text("Registered Cues: %u | Active Instances: %u (Peak %u)", stats.registeredCueCount, stats.activeInstanceCount, stats.peakActiveInstanceCount);
	ImGui::Text("Active Tracks: %u (Peak %u)", stats.activeTrackCount, stats.peakActiveTrackCount);
	ImGui::Text("Particle %u | Fluid2D %u | Volume %u | Light %u | Post %u | Shake %u",
		stats.activeParticleTrackCount, stats.activeFluid2DTrackCount, stats.activeVolumetricTrackCount,
		stats.activeLightTrackCount, stats.activePostEffectTrackCount, stats.activeCameraShakeTrackCount);
	ImGui::Text("Starts/Stops: %llu / %llu | Adapter Failures: %llu",
		static_cast<unsigned long long>(stats.totalTrackStarts),
		static_cast<unsigned long long>(stats.totalTrackStops),
		static_cast<unsigned long long>(stats.adapterFailures));
	ImGui::Text("Budget Rejected: %llu | Delayed Starts: %llu | Hot Reloads: %llu",
		static_cast<unsigned long long>(stats.budgetRejectedInstances),
		static_cast<unsigned long long>(stats.budgetDelayedTrackStarts),
		static_cast<unsigned long long>(stats.hotReloadCount));

	int maxInstances = static_cast<int>(budget.maxActiveInstances);
	int maxStarts = static_cast<int>(budget.maxTrackStartsPerFrame);
	int maxTracks = static_cast<int>(budget.maxActiveTracks);
	int maxLights = static_cast<int>(budget.maxTransientLights);
	int maxFluid = static_cast<int>(budget.maxFluidTracks);
	int maxShake = static_cast<int>(budget.maxCameraShakes);
	if (ImGui::InputInt("Max Instances", &maxInstances)) budget.maxActiveInstances = static_cast<uint32_t>(std::clamp(maxInstances, 1, 4096));
	if (ImGui::InputInt("Max Starts / Frame", &maxStarts)) budget.maxTrackStartsPerFrame = static_cast<uint32_t>(std::clamp(maxStarts, 1, 1024));
	if (ImGui::InputInt("Max Active Tracks", &maxTracks)) budget.maxActiveTracks = static_cast<uint32_t>(std::clamp(maxTracks, 1, 8192));
	if (ImGui::InputInt("Max VFX Lights", &maxLights)) budget.maxTransientLights = static_cast<uint32_t>(std::clamp(maxLights, 0, 512));
	if (ImGui::InputInt("Max Fluid Tracks", &maxFluid)) budget.maxFluidTracks = static_cast<uint32_t>(std::clamp(maxFluid, 0, 512));
	if (ImGui::InputInt("Max Camera Shakes", &maxShake)) budget.maxCameraShakes = static_cast<uint32_t>(std::clamp(maxShake, 0, 128));

	ImGui::InputInt("Stress Count", &stressCount_);
	stressCount_ = std::clamp(stressCount_, 1, 512);
	if (ImGui::Button("Stress Burst Current Cue"))
	{
		RegisterPreviewCue();
		const uint32_t played = runtime->RunStressBurst(editableCue_.cueName, static_cast<uint32_t>(stressCount_), previewPosition_);
		lastMessage_ = "Stress burst played " + std::to_string(played) + " cue instances.";
	}
	ImGui::SameLine();
	if (ImGui::Button("Stop All VFX")) runtime->StopAll();
#endif
}

const char* VfxTimelineEditor::TrackTypeName(VfxCueTrackType type) const
{
	switch (type)
	{
	case VfxCueTrackType::Particle: return "Particle";
	case VfxCueTrackType::Fluid2D: return "Fluid2D";
	case VfxCueTrackType::VolumetricFluid: return "VolumetricFluid";
	case VfxCueTrackType::Light: return "Light";
	case VfxCueTrackType::PostEffect: return "PostEffect";
	case VfxCueTrackType::CameraShake: return "CameraShake";
	default: return "Unknown";
	}
}

} // namespace Ken4lowEngine
