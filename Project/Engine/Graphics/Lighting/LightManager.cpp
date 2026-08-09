#define NOMINMAX
#include "LightManager.h"
#include "DirectXCommon.h"
#include "LightEditorPanel.h"
#include "LightGpuBuffer.h"
#include "LightPresetService.h"
#include "ShadowSystem.h"
#include "ImGuiManager.h"
#include "Wireframe.h"

#include <numbers>    // 円周率（C++20）
#include <algorithm>  // std::clamp
#include <cmath>      // sin/cos/atan2/asin/acos

namespace Ken4lowEngine
{
	namespace
	{
		constexpr float kRadToDeg = 180.0f / std::numbers::pi_v<float>;
		constexpr float kDegToRad = std::numbers::pi_v<float> / 180.0f;

		Vector3 DirectionToEulerDeg(const Vector3& dir)
		{
			Vector3 n = Vector3::Normalize(dir);

			// pitch(X): 上下
			float pitch = std::asin(-n.y);

			// yaw(Y): 左右
			float yaw = std::atan2(n.x, n.z);

			// directional light の向き自体には roll は基本不要
			return {
				pitch * kRadToDeg,
				yaw * kRadToDeg,
				0.0f
			};
		}

		Vector3 EulerDegToDirection(const Vector3& eulerDeg)
		{
			float pitch = eulerDeg.x * kDegToRad;
			float yaw = eulerDeg.y * kDegToRad;

			float cp = std::cos(pitch);
			float sp = std::sin(pitch);
			float cy = std::cos(yaw);
			float sy = std::sin(yaw);

			// 左手系寄りの前方向ベース
			Vector3 dir;
			dir.x = sy * cp;
			dir.y = -sp;
			dir.z = cy * cp;

			return Vector3::Normalize(dir);
		}

		Vector3 TransformHomogeneousPoint(const Matrix4x4& m, const Vector3& p)
		{
			const float x = p.x * m.m[0][0] + p.y * m.m[1][0] + p.z * m.m[2][0] + m.m[3][0];
			const float y = p.x * m.m[0][1] + p.y * m.m[1][1] + p.z * m.m[2][1] + m.m[3][1];
			const float z = p.x * m.m[0][2] + p.y * m.m[1][2] + p.z * m.m[2][2] + m.m[3][2];
			const float w = p.x * m.m[0][3] + p.y * m.m[1][3] + p.z * m.m[2][3] + m.m[3][3];
			if (std::fabs(w) < 1e-6f) { return { x, y, z }; }
			return { x / w, y / w, z / w };
		}

		void DrawFrustumWireframeFromLightVP(Wireframe* wf, const Matrix4x4& lightViewProjection, const Vector4& color)
		{
			Matrix4x4 inv{};
			if (!Matrix4x4::TryInverse(lightViewProjection, inv))
			{
				return;
			}
			const Vector3 ndcCorners[8] = {
				{ -1.0f, -1.0f, 0.0f }, { 1.0f, -1.0f, 0.0f }, { 1.0f, 1.0f, 0.0f }, { -1.0f, 1.0f, 0.0f },
				{ -1.0f, -1.0f, 1.0f }, { 1.0f, -1.0f, 1.0f }, { 1.0f, 1.0f, 1.0f }, { -1.0f, 1.0f, 1.0f }
			};
			Vector3 wpos[8];
			for (int i = 0; i < 8; ++i)
			{
				wpos[i] = TransformHomogeneousPoint(inv, ndcCorners[i]);
				if (!std::isfinite(wpos[i].x) || !std::isfinite(wpos[i].y) || !std::isfinite(wpos[i].z))
				{
					return;
				}
			}
			const int e[12][2] = { { 0, 1 }, { 1, 2 }, { 2, 3 }, { 3, 0 }, { 4, 5 }, { 5, 6 }, { 6, 7 }, { 7, 4 }, { 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 } };
			for (auto& edge : e) { wf->DrawLine(wpos[edge[0]], wpos[edge[1]], color); }
		}
	}

	LightManager* LightManager::GetInstance()
	{
		static LightManager instance;
		return &instance;
	}

	LightManager::~LightManager() = default;

	/// -------------------------------------------------------------
	///				　		初期化処理
	/// -------------------------------------------------------------
	void LightManager::Initialize(DirectXCommon* dxCommon)
	{
		dxCommon_ = dxCommon;

		CreatePunctualLight();

		if (punctualLights_.empty())
		{
			ResetToDefaultLighting();
		}
		lightParameterController_.Initialize(this);
		lightParameterController_.RegisterParameters();
		lightParameterController_.ApplyParameters();

		if (!shadowSystem_)
		{
			shadowSystem_ = std::make_unique<ShadowSystem>();
			shadowSystem_->Initialize(dxCommon_, shadowMapSize_);
		}
	}

	void LightManager::Finalize()
	{
		lightParameterController_.Finalize();
		if (shadowSystem_)
		{
			shadowSystem_->Finalize();
			shadowSystem_.reset();
		}

		if (lightGpuBuffer_)
		{
			// GPUリソース寿命管理はLightGpuBufferへ分離し、LightManagerはFacadeとして終了処理を委譲する。
			lightGpuBuffer_->Finalize();
			lightGpuBuffer_.reset();
		}

		lightingSettings_ = LightingSettingsGPU{};

		// CPU側データ
		punctualLights_.clear();
		lightComponentLights_.clear();
		punctualType_ = 1;

		// 借り物参照
		dxCommon_ = nullptr;
	}

	void LightManager::SetLightComponentLights(const std::vector<PunctualLightGPU>& lights)
	{
		lightComponentLights_ = lights; // Actor DetailsのLightComponent編集結果を描画用ライトとして反映する
	}

	void LightManager::SetLightComponentPointLights(const std::vector<PunctualLightGPU>& lights)
	{
		// 旧PointLight専用名の互換API。現在はLightComponent由来の全ライト種別を受け取る。
		SetLightComponentLights(lights);
	}

	void LightManager::EnsureDefaultLightForParameter()
	{
		if (punctualLights_.empty())
		{
			// ParameterManagerが保存対象にしているLight #0の反映先を必ず用意する。
			AddDefaultDirectionalLight();
		}
	}

	LightManager::ShadowSettings LightManager::GetShadowSettingsForParameter() const
	{
		ShadowSettings settings{};
		settings.enableShadow = enableShadow_;
		settings.shadowBias = shadowBias_;
		settings.normalBias = normalBias_;
		settings.shadowStrength = shadowStrength_;
		settings.shadowMapSize = shadowMapSize_;
		settings.showShadowMapDebug = showShadowMapDebug_;
		settings.showShadowFactorDebug = showShadowFactorDebug_;
		settings.shadowCasterLightIndex = shadowCasterLightIndex_;
		settings.shadowFocusMode = shadowFocusMode_;
		settings.manualShadowFocusPosition = manualShadowFocusPosition_;
		settings.directionalShadowDistance = directionalShadowDistance_;
		settings.directionalShadowWidth = directionalShadowWidth_;
		settings.directionalShadowHeight = directionalShadowHeight_;
		settings.directionalShadowNearZ = directionalShadowNearZ_;
		settings.directionalShadowFarZ = directionalShadowFarZ_;
		settings.directionalShadowFocusOffset = directionalShadowFocusOffset_;
		settings.spotShadowNearZ = spotShadowNearZ_;
		settings.pointShadowNearZ = pointShadowNearZ_;
		settings.enableCsm = enableCsm_;
		settings.csmMaxDistance = csmMaxDistance_;
		settings.csmSplitLambda = csmSplitLambda_;
		return settings;
	}

	void LightManager::SetShadowSettingsFromParameter(const ShadowSettings& settings)
	{
		enableShadow_ = settings.enableShadow;
		shadowBias_ = settings.shadowBias;
		normalBias_ = settings.normalBias;
		shadowStrength_ = settings.shadowStrength;
		ApplyShadowMapSizeFromParameter(settings.shadowMapSize);
		showShadowMapDebug_ = settings.showShadowMapDebug;
		showShadowFactorDebug_ = settings.showShadowFactorDebug;
		shadowCasterLightIndex_ = settings.shadowCasterLightIndex;
		shadowFocusMode_ = settings.shadowFocusMode;
		manualShadowFocusPosition_ = settings.manualShadowFocusPosition;
		directionalShadowDistance_ = settings.directionalShadowDistance;
		directionalShadowWidth_ = settings.directionalShadowWidth;
		directionalShadowHeight_ = settings.directionalShadowHeight;
		directionalShadowNearZ_ = settings.directionalShadowNearZ;
		directionalShadowFarZ_ = settings.directionalShadowFarZ;
		directionalShadowFocusOffset_ = settings.directionalShadowFocusOffset;
		spotShadowNearZ_ = settings.spotShadowNearZ;
		pointShadowNearZ_ = settings.pointShadowNearZ;
		enableCsm_ = settings.enableCsm;
		csmMaxDistance_ = settings.csmMaxDistance;
		csmSplitLambda_ = settings.csmSplitLambda;
	}

	void LightManager::ApplyShadowMapSizeFromParameter(uint32_t size)
	{
		if (dxCommon_ && shadowMapSize_ != size)
		{
			// ShadowMapの再生成は実際にサイズが変わった時だけ行い、GPUリソース更新を最小化する。
			shadowMapSize_ = size;
			dxCommon_->SetShadowMapSize(shadowMapSize_, shadowMapSize_);
			if (shadowSystem_) { shadowSystem_->Resize(shadowMapSize_); }
			return;
		}
		shadowMapSize_ = size;
	}

	/// -------------------------------------------------------------
	///				　		パンクチュアルライトの生成
	/// -------------------------------------------------------------
	void LightManager::CreatePunctualLight()
	{
		if (!lightGpuBuffer_)
		{
			lightGpuBuffer_ = std::make_unique<LightGpuBuffer>();
		}
		// LightManagerはライト状態の所有に集中し、GPUバッファ生成はLightGpuBufferへ委譲する。
		lightGpuBuffer_->Initialize(dxCommon_, lightingSettings_);
	}


	/// -------------------------------------------------------------
	///				　		パンクチュアルライトの更新
	/// -------------------------------------------------------------
	void LightManager::UpdatePunctualLight()
	{
		// CPU側ライト配列はLightManagerが所有し、GPU転送だけをLightGpuBufferへ渡す。
		lightGpuBuffer_->UpdatePunctualLights(punctualLights_, lightComponentLights_);

		DebugDrawLightGizmos();
	}

	/// -------------------------------------------------------------
	///				　	ライト情報をシェーダーにバインド
	/// -------------------------------------------------------------
	void LightManager::DebugDrawLightGizmos()
	{
#ifdef _DEBUG

		auto* wf = Wireframe::GetInstance();

		// 可視化パラメータ（お好みで）
		const Vector4 colDir = { 0.0f, 1.0f, 1.0f, 1.0f };  // 平行光: シアン
		const Vector4 colPt = { 1.0f, 1.0f, 0.0f, 1.0f };  // 点光源: 黄
		const Vector4 colSpot = { 1.0f, 0.5f, 0.0f, 1.0f };  // スポット: オレンジ
		const float   rGizmo = 0.25f;                     // 球の半径(表示用)
		const float   dirLen = 1.5f;                      // 方向線の長さ

		for (const auto& L : punctualLights_)
		{
			if (L.enabled == 0u) { continue; }
			switch (L.lightType) {
			case 1:
				{ // Directional
					Vector3 base = { 0.0f, 3.0f, 0.0f };
					Vector3 d = Vector3::Normalize(L.direction);

					Vector3 tip = base - d * dirLen;
					wf->DrawLine(base, tip, colDir);

					// 矢印の羽
					Vector3 up = { 0.0f, 1.0f, 0.0f };
					if (std::fabs(Vector3::Dot(d, up)) > 0.95f)
					{
						up = { 1.0f, 0.0f, 0.0f };
					}

					Vector3 right = Vector3::Normalize(Vector3::Cross(up, d));
					Vector3 sideUp = Vector3::Normalize(Vector3::Cross(d, right));

					const float headLen = 0.35f;
					const float headWidth = 0.18f;

					Vector3 headBase = tip + d * headLen;

					wf->DrawLine(tip, headBase + right * headWidth, colDir);
					wf->DrawLine(tip, headBase - right * headWidth, colDir);
					wf->DrawLine(tip, headBase + sideUp * headWidth, colDir);
					wf->DrawLine(tip, headBase - sideUp * headWidth, colDir);

					// 方向が分かりやすいように始点にも小さい十字
					const float crossSize = 0.15f;
					wf->DrawLine(base - right * crossSize, base + right * crossSize, colDir);
					wf->DrawLine(base - sideUp * crossSize, base + sideUp * crossSize, colDir);
					DrawFrustumWireframeFromLightVP(wf, currentShadowLightViewProjection_, { 0.1f, 0.9f, 1.0f, 1.0f });
					break;
				}
			case 2:
				{ // Point
		   // 位置に小さな球。到達半径も併せて出したいなら2本目で可視化
					wf->DrawSphere(L.position, rGizmo, colPt);
					if (L.radius > 0.0f) {
						wf->DrawSphere(L.position, L.radius, { colPt.x, colPt.y, colPt.z, 0.5f });
					}
					break;
				}
			case 3:
				{ // Spot
		   // 位置に球＋方向線
					wf->DrawSphere(L.position, rGizmo, colSpot);
					Vector3 d = Vector3::Normalize(L.direction);
					wf->DrawLine(L.position, L.position + d * dirLen, colSpot);
					// 必要なら開き角をリングで表現する処理も追加可能
					break;
				}
			case 4:
				{ // RectArea (approx)
					const Vector4 colArea = { 0.4f, 1.0f, 0.4f, 1.0f };
					Vector3 d = Vector3::Normalize(L.direction);
					Vector3 up = (std::fabs(d.y) > 0.95f) ? Vector3{ 1.0f, 0.0f, 0.0f } : Vector3{ 0.0f, 1.0f, 0.0f };
					Vector3 right = Vector3::Normalize(Vector3::Cross(up, d));
					up = Vector3::Normalize(Vector3::Cross(d, right));
					float hw = std::max(L.areaSize.x * 0.5f, 0.05f);
					float hh = std::max(L.areaSize.y * 0.5f, 0.05f);
					Vector3 c = L.position;
					Vector3 p0 = c + right * hw + up * hh;
					Vector3 p1 = c - right * hw + up * hh;
					Vector3 p2 = c - right * hw - up * hh;
					Vector3 p3 = c + right * hw - up * hh;
					wf->DrawLine(p0, p1, colArea); wf->DrawLine(p1, p2, colArea);
					wf->DrawLine(p2, p3, colArea); wf->DrawLine(p3, p0, colArea);
					wf->DrawLine(c, c + d * dirLen, colArea);
					break;
				}
			case 5:
				{ // SphereArea (approx)
					const Vector4 colAreaSphere = { 0.6f, 1.0f, 0.4f, 1.0f };
					wf->DrawSphere(L.position, std::max(L.areaSize.z, 0.1f), colAreaSphere);
					if (L.distance > 0.0f) { wf->DrawSphere(L.position, L.distance, { colAreaSphere.x, colAreaSphere.y, colAreaSphere.z, 0.45f }); }
					break;
				}
			default:
				break;
			}
		}
		for (const auto& L : lightComponentLights_)
		{
			if (L.enabled == 0u) { continue; }
			switch (L.lightType) {
			case 2:
				wf->DrawSphere(L.position, rGizmo, colPt);
				if (L.radius > 0.0f)
				{
					wf->DrawSphere(L.position, L.radius, { colPt.x, colPt.y, colPt.z, 0.5f });
				}
				break;
			case 3:
				wf->DrawSphere(L.position, rGizmo, colSpot);
				wf->DrawLine(L.position, L.position + Vector3::Normalize(L.direction) * dirLen, colSpot);
				break;
			case 4:
			case 5:
				wf->DrawSphere(L.position, rGizmo, { 0.6f, 1.0f, 0.4f, 1.0f });
				break;
			default:
				break;
			}
		}
#endif // _DEBUG
	}


	void LightManager::DrawPunctualLightsInspector()
	{
		// 既存のDetails Inspector互換入口を維持しつつ、UI描画責務はLightEditorPanelへ分離する。
		static LightEditorPanel editorPanel;
		editorPanel.DrawPunctualLightsInspector(*this);
	}


	LightManager::ShadowCasterType LightManager::GetActiveShadowCasterType() const
	{
		int32_t activeIndex = -1;
		PunctualLightGPU activeLight{};
		ShadowCasterType activeType = ShadowCasterType::None;
		if (TryGetActiveShadowCasterLightInfo(activeIndex, activeLight, activeType))
		{
			return activeType;
		}
		return ShadowCasterType::None;
	}

	uint32_t LightManager::GetShadowReceiverMode() const
	{
		if (!enableShadow_) { return 0u; }
		switch (GetActiveShadowCasterType())
		{
		case ShadowCasterType::Directional: return enableCsm_ ? 4u : 1u;
		case ShadowCasterType::Spot: return 2u;
		case ShadowCasterType::Point: return 3u;
		default: return 0u;
		}
	}

	bool LightManager::TryGetActiveShadowCasterLightInfo(int32_t& outIndex, PunctualLightGPU& outLight, ShadowCasterType& outType) const
	{
		const auto isCandidate = [](const PunctualLightGPU& light)
			{
				return light.intensity > 0.0f && light.enabled != 0u && (light.lightType == 1 || light.lightType == 2 || light.lightType == 3);
			};
		if (shadowCasterLightIndex_ >= 0 && shadowCasterLightIndex_ < static_cast<int32_t>(punctualLights_.size()))
		{
			const auto& selected = punctualLights_[shadowCasterLightIndex_];
			if (isCandidate(selected))
			{
				outIndex = shadowCasterLightIndex_;
				outLight = selected;
				outType = (selected.lightType == 3) ? ShadowCasterType::Spot : (selected.lightType == 2 ? ShadowCasterType::Point : ShadowCasterType::Directional);
				return true;
			}
		}
		const int32_t componentIndex = shadowCasterLightIndex_ - static_cast<int32_t>(punctualLights_.size());
		if (componentIndex >= 0 && componentIndex < static_cast<int32_t>(lightComponentLights_.size()))
		{
			const auto& selected = lightComponentLights_[componentIndex];
			if (isCandidate(selected))
			{
				outIndex = shadowCasterLightIndex_;
				outLight = selected;
				outType = (selected.lightType == 3) ? ShadowCasterType::Spot : (selected.lightType == 2 ? ShadowCasterType::Point : ShadowCasterType::Directional);
				return true;
			}
		}
		for (int32_t i = 0; i < static_cast<int32_t>(punctualLights_.size()); ++i)
		{
			const auto& light = punctualLights_[i];
			// Shadowに使うライト選択条件を統一して、無効ライトが影行列に使われないようにする
			if (!isCandidate(light)) { continue; }
			outIndex = i;
			outLight = light;
			outType = (light.lightType == 3) ? ShadowCasterType::Spot : (light.lightType == 2 ? ShadowCasterType::Point : ShadowCasterType::Directional);
			return true;
		}
		for (int32_t i = 0; i < static_cast<int32_t>(lightComponentLights_.size()); ++i)
		{
			const auto& light = lightComponentLights_[i];
			if (!isCandidate(light)) { continue; }
			outIndex = static_cast<int32_t>(punctualLights_.size()) + i;
			outLight = light;
			outType = (light.lightType == 3) ? ShadowCasterType::Spot : (light.lightType == 2 ? ShadowCasterType::Point : ShadowCasterType::Directional);
			return true;
		}
		return false;
	}

	Matrix4x4 LightManager::BuildShadowLightViewProjection(const Vector3& focusPosition) const
	{
		Vector3 lightDir = { 0.3f, -1.0f, 0.2f };
		int32_t activeIndex = -1;
		PunctualLightGPU activeLight{};
		ShadowCasterType casterType = ShadowCasterType::None;
		if (TryGetActiveShadowCasterLightInfo(activeIndex, activeLight, casterType))
		{
			if (casterType == ShadowCasterType::Directional)
			{
				lightDir = Vector3::Normalize(activeLight.direction);
			}
		}

		if (casterType == ShadowCasterType::Spot)
		{
			const auto& light = activeLight;
			const float spotDistance = std::max(light.distance, 5.0f);
			const float spotOuterCos = std::clamp(light.cosAngle, 0.01f, 0.999f);
			const float outerAngle = std::acos(spotOuterCos) * 2.0f;
			const float fovY = std::clamp(outerAngle, 0.15f, 3.0f);
			const float spotNear = std::clamp(spotShadowNearZ_, 0.001f, spotDistance - 0.001f);
			const Vector3 spotDirection = Vector3::NormalizeSafe(light.direction, { 0.0f, -1.0f, 0.0f });
			const Vector3 spotUp = std::fabs(Vector3::Dot(spotDirection, { 0.0f, 1.0f, 0.0f })) > 0.98f ? Vector3{ 1.0f, 0.0f, 0.0f } : Vector3{ 0.0f, 1.0f, 0.0f };
			const Matrix4x4 view = Matrix4x4::MakeLookAtMatrix(light.position, light.position + spotDirection, spotUp);
			const Matrix4x4 proj = Matrix4x4::MakePerspectiveFovMatrix(fovY, 1.0f, spotNear, spotDistance);
			currentShadowFocusPosition_ = light.position + spotDirection * (spotDistance * 0.5f);
			currentShadowDirection_ = spotDirection;
			currentShadowLightViewProjection_ = Matrix4x4::Multiply(view, proj);
			currentShadowFrustumWidth_ = fovY;
			currentShadowFrustumHeight_ = fovY;
			currentShadowFrustumNearZ_ = spotNear;
			currentShadowFrustumFarZ_ = spotDistance;
			return currentShadowLightViewProjection_;
		}

		Vector3 directionalFocusPosition = focusPosition;
		if (shadowFocusMode_ == ShadowFocusMode::Manual || shadowFocusMode_ == ShadowFocusMode::StageCenter)
		{
			// Shadow Frustumの中心を調整できるようにして、ステージ全体を影範囲に収めやすくする
			directionalFocusPosition = manualShadowFocusPosition_;
		}
		directionalFocusPosition += Vector3{ 0.0f, directionalShadowFocusOffset_, 0.0f };
		const float shadowWidth = std::max(std::fabs(directionalShadowWidth_), 0.01f);
		const float shadowHeight = std::max(std::fabs(directionalShadowHeight_), 0.01f);
		const float shadowNear = std::clamp(directionalShadowNearZ_, 0.01f, 500.0f);
		const float shadowFar = std::max(directionalShadowFarZ_, shadowNear + 1.0f);
		// カメラの微小移動でShadow境界が泳がないよう、Light空間のXYをShadow texel単位へ固定する。
		const Vector3 normalizedDirection = Vector3::NormalizeSafe(lightDir, { 0.3f, -1.0f, 0.2f });
		const Vector3 referenceUp = std::fabs(Vector3::Dot(normalizedDirection, { 0.0f, 1.0f, 0.0f })) > 0.98f ? Vector3{ 1.0f, 0.0f, 0.0f } : Vector3{ 0.0f, 1.0f, 0.0f };
		const Vector3 shadowRight = Vector3::NormalizeSafe(Vector3::Cross(referenceUp, normalizedDirection), { 1.0f, 0.0f, 0.0f });
		const Vector3 shadowUp = Vector3::NormalizeSafe(Vector3::Cross(normalizedDirection, shadowRight), { 0.0f, 1.0f, 0.0f });
		const float texelX = (shadowWidth * 2.0f) / static_cast<float>(std::max(shadowMapSize_, 1u));
		const float texelY = (shadowHeight * 2.0f) / static_cast<float>(std::max(shadowMapSize_, 1u));
		const float projectedX = Vector3::Dot(directionalFocusPosition, shadowRight);
		const float projectedY = Vector3::Dot(directionalFocusPosition, shadowUp);
		directionalFocusPosition += shadowRight * (std::round(projectedX / texelX) * texelX - projectedX);
		directionalFocusPosition += shadowUp * (std::round(projectedY / texelY) * texelY - projectedY);

		// Shadow Frustum debugは実際のLightViewProjectionから復元し、サイズに関係なく描画する
		const Matrix4x4 lightViewProjection = Matrix4x4::MakeLightViewProjection(
			lightDir, directionalFocusPosition, directionalShadowDistance_, shadowWidth, shadowHeight, shadowNear, shadowFar);
		currentShadowFocusPosition_ = directionalFocusPosition;
		currentShadowDirection_ = lightDir;
		currentShadowLightViewProjection_ = lightViewProjection;
		currentShadowFrustumWidth_ = shadowWidth;
		currentShadowFrustumHeight_ = shadowHeight;
		currentShadowFrustumNearZ_ = shadowNear;
		currentShadowFrustumFarZ_ = shadowFar;
		return lightViewProjection;
	}

	void LightManager::DrawImGui(bool* pOpen)
	{
		// LightManagerは外部互換のFacadeとして残し、ImGui編集UIの構築だけをLightEditorPanelへ委譲する。
		static LightEditorPanel editorPanel;
		editorPanel.Draw(*this, pOpen);
	}

	/// -------------------------------------------------------------
	///					ライト情報をシェーダーにバインド
	/// -------------------------------------------------------------
	void LightManager::BindPunctualLights(uint32_t rootIndexCB_b2, uint32_t rootIndexSRV_t2)
	{
		UpdatePunctualLight();

		// 既存のroot indexを維持したまま、CBV/SRVの実バインドだけをLightGpuBufferへ委譲する。
		lightGpuBuffer_->BindPunctualLights(rootIndexCB_b2, rootIndexSRV_t2);
	}

	void LightManager::BindLightingSettings(uint32_t rootIndexCB_b5)
	{
		// LightingSettingsの所有はLightManagerに残し、HLSL定数バッファ反映だけをLightGpuBufferへ委譲する。
		lightGpuBuffer_->BindLightingSettings(rootIndexCB_b5, lightingSettings_);
	}

	void LightManager::ExecuteShadowPasses(const std::function<void()>& drawShadowObjects)
	{
		if (shadowSystem_)
		{
			shadowSystem_->Execute(*this, drawShadowObjects);
			return;
		}
		// 初期化途中などShadowSystemがまだ無い場合も旧単一Passへ安全に戻す。
		dxCommon_->BeginShadowMapPass();
		if (drawShadowObjects) { drawShadowObjects(); }
		dxCommon_->EndShadowMapPass();
	}

	void LightManager::BindExtendedShadowResources(uint32_t extendedShadowCbvRootIndex, uint32_t csmSrvRootIndex, uint32_t pointSrvRootIndex) const
	{
		if (shadowSystem_) { shadowSystem_->Bind(extendedShadowCbvRootIndex, csmSrvRootIndex, pointSrvRootIndex); }
	}

	const Matrix4x4& LightManager::GetActiveShadowPassLightViewProjection() const
	{
		return shadowSystem_ ? shadowSystem_->GetActivePassLightViewProjection() : currentShadowLightViewProjection_;
	}

	void LightManager::AddDefaultDirectionalLight()
	{
		PunctualLightGPU light{};
		light.lightType = 1; // Directional
		light.color = { 1.0f, 0.97f, 0.92f, 1.0f };
		light.intensity = 0.90f;
		light.direction = Vector3::Normalize({ 0.45f, -1.0f, 0.35f });
		light.enabled = 1u;

		punctualLights_.clear();
		punctualLights_.push_back(light);
		lightingSettings_ = LightingSettingsGPU{};
		lightingSettings_.ambientColor = { 0.10f, 0.11f, 0.13f, 0.18f };
		lightingSettings_.diffuseStrength = 0.95f;
		lightingSettings_.specularStrength = 0.10f;
		lightingSettings_.rimLightStrength = 0.55f;
	}

	void LightManager::ResetToDefaultLighting()
	{
		punctualLights_.clear();
		// 保存済み設定を優先し、ファイルがない場合だけ確認用の初期ライトを生成する。
		if (!ApplyLightPresetByPath("Resources/DataAssets/LightPresets/default_light.json"))
		{
			AddDefaultDirectionalLight();
		}
	}

	bool LightManager::SaveLightPreset(const std::string& assetId)
	{
		// 既存API互換の入口を維持し、LightPresetのJSON保存責務だけをServiceへ委譲する。
		return LightPresetService::Save(*this, assetId);
	}

	bool LightManager::ApplyLightPresetByPath(const std::string& filePath)
	{
		// 既存API互換の入口を維持し、LightPresetのJSON読み込み責務だけをServiceへ委譲する。
		return LightPresetService::ApplyByPath(*this, filePath);
	}
} // namespace Ken4lowEngine
