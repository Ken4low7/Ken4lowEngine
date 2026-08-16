#include "BladeTrailComponent.h"

#include "Actor.h"
#include "Engine/Graphics/Renderer/BladeTrail/BladeTrailRenderer.h"
#include "Matrix4x4.h"
#include "SceneComponent.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace Ken4lowEngine
{
	namespace
	{
		float Saturate(float value)
		{
			return std::clamp(value, 0.0f, 1.0f);
		}

		Vector4 LerpColor(const Vector4& a, const Vector4& b, float t)
		{
			t = Saturate(t);
			return {
				a.x + (b.x - a.x) * t,
				a.y + (b.y - a.y) * t,
				a.z + (b.z - a.z) * t,
				a.w + (b.w - a.w) * t
			};
		}

		void ReadVector3(const nlohmann::json& json, const char* key, Vector3& value)
		{
			if (!json.contains(key) || !json[key].is_array() || json[key].size() != 3)
			{
				return;
			}
			value = { json[key][0].get<float>(), json[key][1].get<float>(), json[key][2].get<float>() };
		}

		void ReadVector4(const nlohmann::json& json, const char* key, Vector4& value)
		{
			if (!json.contains(key) || !json[key].is_array() || json[key].size() != 4)
			{
				return;
			}
			value = {
				json[key][0].get<float>(),
				json[key][1].get<float>(),
				json[key][2].get<float>(),
				json[key][3].get<float>()
			};
		}
	}

	BladeTrailComponent::BladeTrailComponent()
	{
		SetUpdateOrder(100); // Animation/Transform更新後の位置を取りやすい順序にする。
		SetDrawOrder(1000);  // Opaque modelより後ろで半透明の刀身軌跡を描きやすくする。
	}

	void BladeTrailComponent::Initialize()
	{
		BladeTrailRenderer::GetInstance()->Acquire();
		rendererAcquired_ = true;
		emitting_ = emitOnStart_;
	}

	void BladeTrailComponent::Finalize()
	{
		ClearTrail();
		if (rendererAcquired_)
		{
			BladeTrailRenderer::GetInstance()->Release();
			rendererAcquired_ = false;
		}
	}

	void BladeTrailComponent::Update(float deltaTime)
	{
		AgeHistory(deltaTime);
	}

	void BladeTrailComponent::UpdateEditor(float deltaTime)
	{
		AgeHistory(deltaTime);
		if (previewArcActive_)
		{
			previewArcElapsed_ += (std::max)(deltaTime, 0.0f);
			const float safeDuration = (std::max)(previewArcDuration_, 0.05f);
			const float normalizedTime = Saturate(previewArcElapsed_ / safeDuration);
			SamplePreviewArc(normalizedTime); // 実ゲームと同じように1Frameずつ軌跡を伸ばす。
			if (normalizedTime >= 1.0f)
			{
				previewArcActive_ = false;
			}
		}
		else if (emitting_)
		{
			SampleCurrentBlade();
		}
	}

	void BladeTrailComponent::PostPhysicsUpdate([[maybe_unused]] float deltaTime)
	{
		if (emitting_)
		{
			// Physics/Actor Transformが確定した後にRoot/Tipを記録し、1Frame前の位置を掴まないようにする。
			SampleCurrentBlade();
		}
	}

	void BladeTrailComponent::BeginTrail(bool clearHistory)
	{
		previewArcActive_ = false; // Gameplay記録開始時はEditor Previewを停止する。
		if (clearHistory)
		{
			ClearTrail();
		}
		emitting_ = true;
		SampleCurrentBlade();
	}

	void BladeTrailComponent::EndTrail()
	{
		emitting_ = false;
	}

	void BladeTrailComponent::ClearTrail()
	{
		previewArcActive_ = false;
		previewArcElapsed_ = 0.0f;
		samples_.clear();
		vertexScratch_.clear();
	}

	void BladeTrailComponent::SetBladeWorldEndpoints(const Vector3& root, const Vector3& tip)
	{
		overriddenWorldRoot_ = root;
		overriddenWorldTip_ = tip;
		useWorldEndpointOverride_ = true;
	}

	void BladeTrailComponent::ClearBladeWorldEndpointOverride()
	{
		useWorldEndpointOverride_ = false;
	}

	void BladeTrailComponent::AgeHistory(float deltaTime)
	{
		if (samples_.empty())
		{
			return;
		}

		const float safeDeltaTime = (std::max)(deltaTime, 0.0f);
		for (BladeTrailSample& sample : samples_)
		{
			sample.age += safeDeltaTime;
		}

		const float safeLifetime = (std::max)(historyLifetime_, 0.001f);
		while (!samples_.empty() && samples_.front().age >= safeLifetime)
		{
			samples_.pop_front();
		}
	}

	bool BladeTrailComponent::ResolveBladeEndpoints(Vector3& outRoot, Vector3& outTip) const
	{
		if (useWorldEndpointOverride_)
		{
			outRoot = overriddenWorldRoot_;
			outTip = overriddenWorldTip_;
			return true;
		}

		Actor* owner = GetOwner();
		SceneComponent* rootComponent = owner ? owner->GetRootComponent() : nullptr;
		if (!rootComponent)
		{
			return false;
		}

		const Matrix4x4 ownerWorld = Matrix4x4::MakeAffineMatrix(
			rootComponent->GetWorldScale(),
			rootComponent->GetWorldRotation(),
			rootComponent->GetWorldPosition());
		outRoot = Vector3::Transform(localRootOffset_, ownerWorld);
		outTip = Vector3::Transform(localTipOffset_, ownerWorld);
		return true;
	}

	void BladeTrailComponent::SampleCurrentBlade()
	{
		Vector3 root{};
		Vector3 tip{};
		if (!ResolveBladeEndpoints(root, tip))
		{
			return;
		}
		AppendSample(root, tip);
	}

	void BladeTrailComponent::AppendSample(const Vector3& root, const Vector3& tip)
	{
		if (!samples_.empty())
		{
			const BladeTrailSample& previous = samples_.back();
			const float rootDistance = Vector3::Length(root - previous.root);
			const float tipDistance = Vector3::Length(tip - previous.tip);
			if ((std::max)(rootDistance, tipDistance) < (std::max)(minSampleDistance_, 0.0f))
			{
				return;
			}
		}

		const uint32_t safeMaxSamples = std::clamp(maxSamples_, 2u, 128u);
		while (samples_.size() >= safeMaxSamples)
		{
			samples_.pop_front();
		}
		samples_.push_back({ root, tip, 0.0f });
	}

	std::vector<BladeTrailComponent::BladeTrailSample> BladeTrailComponent::BuildSmoothedSamples() const
	{
		std::vector<BladeTrailSample> result;
		if (samples_.size() < 2)
		{
			return result;
		}

		const uint32_t subdivisions = std::clamp(smoothingSubdivisions_, 1u, 4u);
		result.reserve((samples_.size() - 1u) * subdivisions + 1u);

		for (size_t i = 0; i + 1 < samples_.size(); ++i)
		{
			const BladeTrailSample& p0 = samples_[i == 0 ? 0 : i - 1];
			const BladeTrailSample& p1 = samples_[i];
			const BladeTrailSample& p2 = samples_[i + 1];
			const BladeTrailSample& p3 = samples_[(std::min)(i + 2, samples_.size() - 1)];

			for (uint32_t step = 0; step < subdivisions; ++step)
			{
				const float t = static_cast<float>(step) / static_cast<float>(subdivisions);
				BladeTrailSample sample{};
				sample.root = Vector3::CatmullRomSpline(p0.root, p1.root, p2.root, p3.root, t);
				sample.tip = Vector3::CatmullRomSpline(p0.tip, p1.tip, p2.tip, p3.tip, t);
				sample.age = p1.age + (p2.age - p1.age) * t;
				result.push_back(sample);
			}
		}
		result.push_back(samples_.back());
		return result;
	}

	void BladeTrailComponent::BuildVertices()
	{
		vertexScratch_.clear();
		const std::vector<BladeTrailSample> renderSamples = BuildSmoothedSamples();
		if (renderSamples.size() < 2)
		{
			return;
		}

		struct RibbonPoint
		{
			Vector3 root{};
			Vector3 tip{};
			Vector4 color{};
			float u = 0.0f;
		};

		std::vector<RibbonPoint> points;
		points.reserve(renderSamples.size());
		const float safeLifetime = (std::max)(historyLifetime_, 0.001f);
		for (size_t i = 0; i < renderSamples.size(); ++i)
		{
			const BladeTrailSample& sample = renderSamples[i];
			const Vector3 center = (sample.root + sample.tip) * 0.5f;
			const Vector3 halfBlade = (sample.tip - sample.root) * (0.5f * (std::max)(widthScale_, 0.0f));
			const float normalizedAge = Saturate(sample.age / safeLifetime);
			RibbonPoint point{};
			point.root = center - halfBlade;
			point.tip = center + halfBlade;
			point.color = LerpColor(headColor_, tailColor_, normalizedAge);
			point.u = static_cast<float>(i) / static_cast<float>(renderSamples.size() - 1u);
			points.push_back(point);
		}

		vertexScratch_.reserve((points.size() - 1u) * 6u);
		for (size_t i = 0; i + 1 < points.size(); ++i)
		{
			const RibbonPoint& a = points[i];
			const RibbonPoint& b = points[i + 1];

			const BladeTrailVertex rootA{ a.root, { a.u, 0.0f }, a.color };
			const BladeTrailVertex tipA{ a.tip, { a.u, 1.0f }, a.color };
			const BladeTrailVertex rootB{ b.root, { b.u, 0.0f }, b.color };
			const BladeTrailVertex tipB{ b.tip, { b.u, 1.0f }, b.color };

			vertexScratch_.push_back(rootA);
			vertexScratch_.push_back(tipA);
			vertexScratch_.push_back(rootB);
			vertexScratch_.push_back(tipA);
			vertexScratch_.push_back(tipB);
			vertexScratch_.push_back(rootB);
		}
	}

	void BladeTrailComponent::Draw()
	{
		if (!visible_ || !rendererAcquired_ || samples_.size() < 2)
		{
			return;
		}

		BuildVertices();
		if (vertexScratch_.empty())
		{
			return;
		}
		BladeTrailRenderer::GetInstance()->Draw(vertexScratch_, texturePath_, blendMode_);
	}

	void BladeTrailComponent::GeneratePreviewArc()
	{
		ClearTrail();
		emitting_ = false;
		previewArcElapsed_ = 0.0f;
		previewArcActive_ = true;
		SamplePreviewArc(0.0f); // 最初の1点だけ作り、以降はUpdateEditorで弧をなぞる。
	}

	void BladeTrailComponent::SamplePreviewArc(float normalizedTime)
	{
		Matrix4x4 ownerWorld = Matrix4x4::MakeIdentity();
		if (Actor* owner = GetOwner())
		{
			if (SceneComponent* rootComponent = owner->GetRootComponent())
			{
				ownerWorld = Matrix4x4::MakeAffineMatrix(
					rootComponent->GetWorldScale(),
					rootComponent->GetWorldRotation(),
					rootComponent->GetWorldPosition());
			}
		}

		const float t = Saturate(normalizedTime);
		const float angle = -1.15f + 2.30f * t;
		const Matrix4x4 localRotation = Matrix4x4::MakeRotateY(angle);
		const Vector3 rotatedRoot = Vector3::Transform(localRootOffset_, localRotation);
		const Vector3 rotatedTip = Vector3::Transform(localTipOffset_, localRotation);
		AppendSample(
			Vector3::Transform(rotatedRoot, ownerWorld),
			Vector3::Transform(rotatedTip, ownerWorld));
	}

	void BladeTrailComponent::DrawImGui()
	{
#ifdef USE_IMGUI
		ImGui::SeparatorText("Blade Trail");
		if (!emitting_)
		{
			if (ImGui::Button("Begin Trail")) BeginTrail(true);
		}
		else
		{
			if (ImGui::Button("End Trail")) EndTrail();
		}
		ImGui::SameLine();
		if (ImGui::Button("Clear")) ClearTrail();
		ImGui::SameLine();
		if (ImGui::Button("Preview Arc")) GeneratePreviewArc();

		ImGui::Checkbox("Visible", &visible_);
		ImGui::Checkbox("Emit On Start", &emitOnStart_);
		ImGui::Text("Samples: %d / %u", static_cast<int>(samples_.size()), maxSamples_);

		ImGui::SeparatorText("Blade Endpoints");
		ImGui::DragFloat3("Local Root", &localRootOffset_.x, 0.01f);
		ImGui::DragFloat3("Local Tip", &localTipOffset_.x, 0.01f);
		ImGui::TextDisabled("For skeletal weapons call SetBladeWorldEndpoints(root, tip) each frame.");
		ImGui::TextDisabled("Slash tip: set Local Root near the outer blade (for example Z=0.8) to avoid a filled fan.");

		ImGui::SeparatorText("History / Smoothing");
		ImGui::DragFloat("Lifetime", &historyLifetime_, 0.005f, 0.03f, 2.0f);
		ImGui::DragFloat("Min Sample Distance", &minSampleDistance_, 0.001f, 0.0f, 1.0f);
		ImGui::DragFloat("Width Scale", &widthScale_, 0.01f, 0.0f, 4.0f);
		int maxSamples = static_cast<int>(maxSamples_);
		if (ImGui::SliderInt("Max Samples", &maxSamples, 2, 128)) maxSamples_ = static_cast<uint32_t>(maxSamples);
		int subdivisions = static_cast<int>(smoothingSubdivisions_);
		if (ImGui::SliderInt("Smoothing Subdivisions", &subdivisions, 1, 4)) smoothingSubdivisions_ = static_cast<uint32_t>(subdivisions);

		ImGui::SeparatorText("Appearance");
		char textureBuffer[256]{};
		std::snprintf(textureBuffer, sizeof(textureBuffer), "%s", texturePath_.c_str());
		if (ImGui::InputText("Texture", textureBuffer, sizeof(textureBuffer))) texturePath_ = textureBuffer;
		static const char* kBlendNames[] = { "None", "Alpha", "Additive", "Subtract", "Multiply", "Screen" };
		int blendMode = static_cast<int>(blendMode_);
		if (ImGui::Combo("Blend Mode", &blendMode, kBlendNames, IM_ARRAYSIZE(kBlendNames))) blendMode_ = static_cast<BlendMode>(blendMode);
		const ImGuiColorEditFlags colorFlags = ImGuiColorEditFlags_Uint8 | ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreviewHalf;
		ImGui::ColorEdit4("Head Color (0-255)", &headColor_.x, colorFlags);
		ImGui::ColorEdit4("Tail Color (0-255)", &tailColor_.x, colorFlags);
#endif // USE_IMGUI
	}

	void BladeTrailComponent::ToJson(nlohmann::json& outJson) const
	{
		ActorComponent::ToJson(outJson);
		outJson["LocalRootOffset"] = { localRootOffset_.x, localRootOffset_.y, localRootOffset_.z };
		outJson["LocalTipOffset"] = { localTipOffset_.x, localTipOffset_.y, localTipOffset_.z };
		outJson["HistoryLifetime"] = historyLifetime_;
		outJson["MinSampleDistance"] = minSampleDistance_;
		outJson["WidthScale"] = widthScale_;
		outJson["MaxSamples"] = maxSamples_;
		outJson["SmoothingSubdivisions"] = smoothingSubdivisions_;
		outJson["HeadColor"] = { headColor_.x, headColor_.y, headColor_.z, headColor_.w };
		outJson["TailColor"] = { tailColor_.x, tailColor_.y, tailColor_.z, tailColor_.w };
		outJson["TexturePath"] = texturePath_;
		outJson["BlendMode"] = static_cast<int>(blendMode_);
		outJson["EmitOnStart"] = emitOnStart_;
		outJson["Visible"] = visible_;
	}

	void BladeTrailComponent::FromJson(const nlohmann::json& inJson)
	{
		ActorComponent::FromJson(inJson);
		ReadVector3(inJson, "LocalRootOffset", localRootOffset_);
		ReadVector3(inJson, "LocalTipOffset", localTipOffset_);
		ReadVector4(inJson, "HeadColor", headColor_);
		ReadVector4(inJson, "TailColor", tailColor_);

		if (inJson.contains("HistoryLifetime") && inJson["HistoryLifetime"].is_number()) historyLifetime_ = (std::max)(inJson["HistoryLifetime"].get<float>(), 0.03f);
		if (inJson.contains("MinSampleDistance") && inJson["MinSampleDistance"].is_number()) minSampleDistance_ = (std::max)(inJson["MinSampleDistance"].get<float>(), 0.0f);
		if (inJson.contains("WidthScale") && inJson["WidthScale"].is_number()) widthScale_ = (std::max)(inJson["WidthScale"].get<float>(), 0.0f);
		if (inJson.contains("MaxSamples") && inJson["MaxSamples"].is_number_unsigned()) maxSamples_ = std::clamp(inJson["MaxSamples"].get<uint32_t>(), 2u, 128u);
		if (inJson.contains("SmoothingSubdivisions") && inJson["SmoothingSubdivisions"].is_number_unsigned()) smoothingSubdivisions_ = std::clamp(inJson["SmoothingSubdivisions"].get<uint32_t>(), 1u, 4u);
		if (inJson.contains("TexturePath") && inJson["TexturePath"].is_string()) texturePath_ = inJson["TexturePath"].get<std::string>();
		if (inJson.contains("BlendMode") && inJson["BlendMode"].is_number_integer())
		{
			const int blendMode = std::clamp(inJson["BlendMode"].get<int>(), 0, static_cast<int>(BlendMode::kcountOfBlendMode) - 1);
			blendMode_ = static_cast<BlendMode>(blendMode);
		}
		if (inJson.contains("EmitOnStart") && inJson["EmitOnStart"].is_boolean()) emitOnStart_ = inJson["EmitOnStart"].get<bool>();
		if (inJson.contains("Visible") && inJson["Visible"].is_boolean()) visible_ = inJson["Visible"].get<bool>();
	}
}
