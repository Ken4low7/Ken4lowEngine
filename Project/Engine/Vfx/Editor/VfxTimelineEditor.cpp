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
	if (!ImGui::Begin("VFX タイムライン", open))
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
		lastMessage_ = "VFXの読込に失敗しました: " + filePath_;
		return false;
	}
	StopPreview();
	editableCue_ = std::move(loaded);
	lastMessage_ = "読み込みました: " + filePath_;
	return RegisterPreviewCue();
}

bool VfxTimelineEditor::SaveToDisk()
{
	if (!VfxCueSerializer::Save(editableCue_, filePath_))
	{
		lastMessage_ = "VFXの保存に失敗しました: " + filePath_;
		return false;
	}
	lastMessage_ = "保存しました: " + filePath_;
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
	if (ImGui::InputText("キューファイル", pathBuffer, sizeof(pathBuffer))) filePath_ = pathBuffer;
	if (ImGui::Button("読込")) LoadFromDisk();
	ImGui::SameLine();
	if (ImGui::Button("保存して登録")) SaveToDisk();
	ImGui::SameLine();
	if (ImGui::Button("即時再読込"))
	{
		if (!editableCue_.cueName.empty() && VfxCueRuntime::GetInstance()->ReloadCue(editableCue_.cueName))
			lastMessage_ = "即時再読込しました: " + editableCue_.cueName;
		else lastMessage_ = VfxCueRuntime::GetInstance()->GetStats().lastStatus;
	}

	char nameBuffer[128]{};
	std::snprintf(nameBuffer, sizeof(nameBuffer), "%s", editableCue_.cueName.c_str());
	if (ImGui::InputText("キュー名", nameBuffer, sizeof(nameBuffer))) editableCue_.cueName = nameBuffer;
	ImGui::Checkbox("キューをループ", &editableCue_.loop);
	ImGui::DragFloat("設定時間", &editableCue_.duration, 0.01f, 0.0f, 120.0f);
	ImGui::DragFloat3("プレビュー位置", &previewPosition_.x, 0.05f);

	if (ImGui::Button("メモリ内へ登録")) RegisterPreviewCue();
	ImGui::SameLine();
	if (ImGui::Button("プレビュー"))
	{
		StopPreview();
		if (RegisterPreviewCue()) previewHandle_ = VfxCueRuntime::GetInstance()->Play(editableCue_.cueName, previewPosition_);
	}
	ImGui::SameLine();
	if (ImGui::Button("プレビュー停止")) StopPreview();
#endif
}

void VfxTimelineEditor::DrawUserParameters()
{
#ifdef USE_IMGUI
	if (!ImGui::CollapsingHeader("ユーザーパラメータ", ImGuiTreeNodeFlags_DefaultOpen)) return;
	int remove = -1;
	for (uint32_t i = 0; i < editableCue_.userParameters.size(); ++i)
	{
		VfxCueUserParameterDesc& parameterDesc = editableCue_.userParameters[i];
		ImGui::PushID(static_cast<int>(i));
		char name[96]{};
		std::snprintf(name, sizeof(name), "%s", parameterDesc.name.c_str());
		if (ImGui::InputText("名前", name, sizeof(name))) parameterDesc.name = name;
		ImGui::DragFloat("初期値", &parameterDesc.defaultValue, 0.01f);
		ImGui::DragFloat("最小値", &parameterDesc.minValue, 0.01f);
		ImGui::DragFloat("最大値", &parameterDesc.maxValue, 0.01f);
		if (ImGui::Button("パラメータを削除")) remove = static_cast<int>(i);
		ImGui::Separator();
		ImGui::PopID();
	}
	if (remove >= 0) editableCue_.userParameters.erase(editableCue_.userParameters.begin() + remove);
	if (editableCue_.userParameters.size() < VfxCueDesc::kMaxUserParameters && ImGui::Button("強度パラメータを追加"))
	{
		editableCue_.userParameters.push_back({ "Intensity", 1.0f, 0.0f, 3.0f });
	}
#endif
}

void VfxTimelineEditor::DrawTimeline()
{
#ifdef USE_IMGUI
	if (!ImGui::CollapsingHeader("タイムライン", ImGuiTreeNodeFlags_DefaultOpen)) return;

	const char* types[] = { "パーティクル", "2D流体", "3Dボリューム流体", "ライト", "ポストエフェクト", "カメラシェイク" };
	ImGui::Combo("新規トラック種類", &addTrackType_, types, IM_ARRAYSIZE(types));
	ImGui::SameLine();
	if (ImGui::Button("トラックを追加")) AddTrack(static_cast<VfxCueTrackType>(std::clamp(addTrackType_, 0, 5)));
	ImGui::SliderFloat("タイムライン拡大率", &timelinePixelsPerSecond_, 40.0f, 400.0f, "%.0f px/s");

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

	if (ImGui::TreeNode("編集 %s [%s]", track.name.c_str(), TrackTypeName(track.type)))
	{
		ImGui::Checkbox("有効", &track.enabled);
		ImGui::DragFloat("開始時間", &track.startTime, 0.01f, 0.0f, 120.0f);
		ImGui::DragFloat("継続時間", &track.duration, 0.01f, 0.0f, 120.0f);
		ImGui::DragFloat3("ローカルオフセット", &track.localOffset.x, 0.05f);

		if (auto* particlePayload = std::get_if<VfxParticleTrackPayload>(&track.payload))
		{
			char asset[256]{};
			std::snprintf(asset, sizeof(asset), "%s", particlePayload->effectAssetPath.c_str());
			if (ImGui::InputText("エフェクトアセット", asset, sizeof(asset))) particlePayload->effectAssetPath = asset;
			char effect[128]{};
			std::snprintf(effect, sizeof(effect), "%s", particlePayload->effectName.c_str());
			if (ImGui::InputText("エフェクト名", effect, sizeof(effect))) particlePayload->effectName = effect;
			ImGui::Checkbox("パーティクルをループ", &particlePayload->loop);
		}
		else if (auto* fluidPayload = std::get_if<VfxFluidTrackPayload>(&track.payload))
		{
			ImGui::DragFloat3("速度", &fluidPayload->localVelocity.x, 0.05f);
			ImGui::DragFloat("半径", &fluidPayload->radius, 0.02f, 0.001f, 100.0f);
			ImGui::DragFloat("速度強度", &fluidPayload->velocityStrength, 0.02f, 0.0f, 100.0f);
			ImGui::DragFloat("密度変化率", &fluidPayload->densityRate, 0.02f);
			ImGui::DragFloat("温度変化率", &fluidPayload->temperatureRate, 0.02f);
			ImGui::DragFloat("減衰指数", &fluidPayload->falloffExponent, 0.02f, 0.01f, 16.0f);
		}
		else if (auto* lightPayload = std::get_if<VfxLightTrackPayload>(&track.payload))
		{
			ImGui::ColorEdit3("色", &lightPayload->color.x);
			ImGui::DragFloat("強度", &lightPayload->intensity, 0.05f, 0.0f, 1000.0f);
			ImGui::DragFloat("範囲", &lightPayload->range, 0.05f, 0.01f, 1000.0f);
		}
		else if (auto* postEffectPayload = std::get_if<VfxPostEffectTrackPayload>(&track.payload))
		{
			char effect[128]{};
			std::snprintf(effect, sizeof(effect), "%s", postEffectPayload->effectName.c_str());
			if (ImGui::InputText("ポストエフェクト", effect, sizeof(effect))) postEffectPayload->effectName = effect;
			ImGui::SliderFloat("重み", &postEffectPayload->weight, 0.0f, 1.0f);
		}
		else if (auto* cameraShakePayload = std::get_if<VfxCameraShakeTrackPayload>(&track.payload))
		{
			ImGui::DragFloat3("移動振幅", &cameraShakePayload->translationAmplitude.x, 0.005f);
			ImGui::DragFloat3("回転振幅（度）", &cameraShakePayload->rotationAmplitudeDegrees.x, 0.05f);
			ImGui::DragFloat("周波数", &cameraShakePayload->frequency, 0.1f, 0.0f, 120.0f);
			ImGui::DragFloat("視野角振幅（度）", &cameraShakePayload->fovAmplitudeDegrees, 0.05f);
		}

		if (ImGui::Button("強度バインドを追加"))
		{
			track.bindings.push_back({ "Intensity", VfxCueBindingTarget::IntensityScale, "", 1.0f, 0.0f });
		}
		for (uint32_t bindingIndex = 0; bindingIndex < track.bindings.size(); ++bindingIndex)
		{
			ImGui::PushID(static_cast<int>(bindingIndex) + 10000);
			VfxCueTrackBindingDesc& binding = track.bindings[bindingIndex];
			char parameter[96]{};
			std::snprintf(parameter, sizeof(parameter), "%s", binding.parameterName.c_str());
			if (ImGui::InputText("パラメータ", parameter, sizeof(parameter))) binding.parameterName = parameter;
			int target = static_cast<int>(binding.target);
			const char* targets[] = { "強度倍率", "半径倍率", "パーティクル浮動小数" };
			if (ImGui::Combo("対象", &target, targets, IM_ARRAYSIZE(targets)))
			{
				binding.target = static_cast<VfxCueBindingTarget>(std::clamp(target, 0, 2));
			}
			if (binding.target == VfxCueBindingTarget::ParticleFloat)
			{
				char targetName[96]{};
				std::snprintf(targetName, sizeof(targetName), "%s", binding.targetName.c_str());
				if (ImGui::InputText("対象名", targetName, sizeof(targetName))) binding.targetName = targetName;
			}
			ImGui::DragFloat("倍率", &binding.scale, 0.01f);
			ImGui::DragFloat("加算値", &binding.bias, 0.01f);
			if (ImGui::Button("バインドを削除"))
			{
				track.bindings.erase(track.bindings.begin() + bindingIndex);
				ImGui::PopID();
				break;
			}
			ImGui::PopID();
		}

		if (ImGui::Button("トラックを削除")) removeRequested = true;
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
	if (!ImGui::CollapsingHeader("実行状況 / 実行予算 / 負荷確認", ImGuiTreeNodeFlags_DefaultOpen)) return;
	VfxCueRuntime* runtime = VfxCueRuntime::GetInstance();
	const VfxRuntimeStats& stats = runtime->GetStats();
	VfxRuntimeBudget& budget = runtime->GetEditableBudget();

	ImGui::Text("登録キュー: %u | 使用中実体: %u (最大 %u)", stats.registeredCueCount, stats.activeInstanceCount, stats.peakActiveInstanceCount);
	ImGui::Text("使用中トラック: %u (最大 %u)", stats.activeTrackCount, stats.peakActiveTrackCount);
	ImGui::Text("パーティクル %u | 2D流体 %u | 3D流体 %u | ライト %u | ポスト %u | シェイク %u",
		stats.activeParticleTrackCount, stats.activeFluid2DTrackCount, stats.activeVolumetricTrackCount,
		stats.activeLightTrackCount, stats.activePostEffectTrackCount, stats.activeCameraShakeTrackCount);
	ImGui::Text("開始/停止: %llu / %llu | アダプター失敗: %llu",
		static_cast<unsigned long long>(stats.totalTrackStarts),
		static_cast<unsigned long long>(stats.totalTrackStops),
		static_cast<unsigned long long>(stats.adapterFailures));
	ImGui::Text("実行予算拒否: %llu | 遅延開始: %llu | 即時再読込: %llu",
		static_cast<unsigned long long>(stats.budgetRejectedInstances),
		static_cast<unsigned long long>(stats.budgetDelayedTrackStarts),
		static_cast<unsigned long long>(stats.hotReloadCount));

	int maxInstances = static_cast<int>(budget.maxActiveInstances);
	int maxStarts = static_cast<int>(budget.maxTrackStartsPerFrame);
	int maxTracks = static_cast<int>(budget.maxActiveTracks);
	int maxLights = static_cast<int>(budget.maxTransientLights);
	int maxFluid = static_cast<int>(budget.maxFluidTracks);
	int maxShake = static_cast<int>(budget.maxCameraShakes);
	if (ImGui::InputInt("最大実体数", &maxInstances)) budget.maxActiveInstances = static_cast<uint32_t>(std::clamp(maxInstances, 1, 4096));
	if (ImGui::InputInt("1フレームの最大開始数", &maxStarts)) budget.maxTrackStartsPerFrame = static_cast<uint32_t>(std::clamp(maxStarts, 1, 1024));
	if (ImGui::InputInt("最大使用中トラック数", &maxTracks)) budget.maxActiveTracks = static_cast<uint32_t>(std::clamp(maxTracks, 1, 8192));
	if (ImGui::InputInt("最大VFXライト数", &maxLights)) budget.maxTransientLights = static_cast<uint32_t>(std::clamp(maxLights, 0, 512));
	if (ImGui::InputInt("最大流体トラック数", &maxFluid)) budget.maxFluidTracks = static_cast<uint32_t>(std::clamp(maxFluid, 0, 512));
	if (ImGui::InputInt("最大カメラシェイク数", &maxShake)) budget.maxCameraShakes = static_cast<uint32_t>(std::clamp(maxShake, 0, 128));

	ImGui::InputInt("負荷確認数", &stressCount_);
	stressCount_ = std::clamp(stressCount_, 1, 512);
	if (ImGui::Button("現在のキューを一括負荷確認"))
	{
		RegisterPreviewCue();
		const uint32_t played = runtime->RunStressBurst(editableCue_.cueName, static_cast<uint32_t>(stressCount_), previewPosition_);
		lastMessage_ = "負荷確認で " + std::to_string(played) + " 個のキュー実体を再生しました。";
	}
	ImGui::SameLine();
	if (ImGui::Button("全VFXを停止")) runtime->StopAll(); // 表示文字列のみ日本語化し、内部識別子と保存値は維持する。
#endif
}

const char* VfxTimelineEditor::TrackTypeName(VfxCueTrackType type) const
{
	switch (type)
	{
	case VfxCueTrackType::Particle: return "パーティクル";
	case VfxCueTrackType::Fluid2D: return "2D流体";
	case VfxCueTrackType::VolumetricFluid: return "3Dボリューム流体";
	case VfxCueTrackType::Light: return "ライト";
	case VfxCueTrackType::PostEffect: return "ポストエフェクト";
	case VfxCueTrackType::CameraShake: return "カメラシェイク";
	default: return "不明";
	}
}

} // namespace Ken4lowEngine