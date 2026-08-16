#pragma once

#include "ActorComponent.h"
#include "BlendModeType.h"
#include "Vector3.h"
#include "Vector4.h"

#include <cstdint>
#include <deque>
#include <string>
#include <vector>

namespace Ken4lowEngine
{
	struct BladeTrailVertex;

	/// <summary>
	/// 剣のRoot/Tipを複数Frame保持し、刀身が通過した面そのものをN-point Ribbonとして描画するComponent。
	/// GPU Particleの1 previous-sample Trailとは独立し、武器軌跡専用の履歴を持つ。
	/// </summary>
	class BladeTrailComponent final : public ActorComponent
	{
	public:
		BladeTrailComponent();

		void Initialize() override;
		void Update(float deltaTime) override;
		void UpdateEditor(float deltaTime) override;
		void PostPhysicsUpdate(float deltaTime) override;
		void Draw() override;
		void DrawImGui() override;
		void Finalize() override;

		std::string GetClassTypeName() const override { return "BladeTrailComponent"; }
		void ToJson(nlohmann::json& outJson) const override;
		void FromJson(const nlohmann::json& inJson) override;

	public: /// ---------- Gameplay API ---------- ///
		void BeginTrail(bool clearHistory = true);
		void EndTrail();
		void ClearTrail();
		[[nodiscard]] bool IsEmitting() const { return emitting_; }

		/// Skeletal socket等からWorld座標を渡す場合に使用する。以降のsampleはこの2点を使用する。
		void SetBladeWorldEndpoints(const Vector3& root, const Vector3& tip);
		void ClearBladeWorldEndpointOverride();

		[[nodiscard]] size_t GetSampleCount() const { return samples_.size(); }
		[[nodiscard]] const Vector3& GetLocalRootOffset() const { return localRootOffset_; }
		[[nodiscard]] const Vector3& GetLocalTipOffset() const { return localTipOffset_; }
		void SetLocalRootOffset(const Vector3& value) { localRootOffset_ = value; }
		void SetLocalTipOffset(const Vector3& value) { localTipOffset_ = value; }

	private:
		struct BladeTrailSample
		{
			Vector3 root{};
			Vector3 tip{};
			float age = 0.0f;
		};

		void AgeHistory(float deltaTime);
		void SampleCurrentBlade();
		bool ResolveBladeEndpoints(Vector3& outRoot, Vector3& outTip) const;
		void AppendSample(const Vector3& root, const Vector3& tip);
		std::vector<BladeTrailSample> BuildSmoothedSamples() const;
		void BuildVertices();
		void GeneratePreviewArc();

	private:
		std::deque<BladeTrailSample> samples_;
		std::vector<BladeTrailVertex> vertexScratch_;

		Vector3 localRootOffset_{ 0.0f, 0.0f, 0.0f };
		Vector3 localTipOffset_{ 0.0f, 0.0f, 1.5f };
		float historyLifetime_ = 0.24f;
		float minSampleDistance_ = 0.01f;
		float widthScale_ = 1.0f;
		uint32_t maxSamples_ = 32;
		uint32_t smoothingSubdivisions_ = 2;

		Vector4 headColor_{ 1.0f, 1.0f, 1.0f, 1.0f };
		Vector4 tailColor_{ 0.08f, 0.35f, 1.0f, 0.0f };
		std::string texturePath_ = "Effects/white.dds";
		BlendMode blendMode_ = BlendMode::kBlendModeAdd;

		Vector3 overriddenWorldRoot_{};
		Vector3 overriddenWorldTip_{};
		bool useWorldEndpointOverride_ = false;
		bool emitOnStart_ = false;
		bool emitting_ = false;
		bool visible_ = true;
		bool rendererAcquired_ = false;
	};
}
