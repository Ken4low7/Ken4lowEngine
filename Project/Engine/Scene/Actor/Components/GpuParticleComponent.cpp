#define NOMINMAX
#include "GpuParticleComponent.h"

#include "AssetPathSelector.h"
#include "GpuParticleEmitter.h"
#include "GpuParticleEmitterPreset.h"
#include "GpuParticleManager.h"
#include "GpuParticleType.h"
#include "BillboardMode.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

#ifdef USE_IMGUI
#include <imgui.h>
#endif // USE_IMGUI

namespace Ken4lowEngine
{
	namespace
	{
		constexpr float kEpsilon = 0.0001f;

		std::vector<ComponentPropertyChoice> MakeChoices(std::initializer_list<const char*> names)
		{
			std::vector<ComponentPropertyChoice> choices;
			for (const char* name : names)
			{
				choices.push_back({ name, name });
			}
			return choices;
		}

		GpuParticleType ParseGpuParticleType(const std::string& name)
		{
			for (uint32_t index = 0; index < GpuParticleEmitterPresetTable::GetSpritePresetCount(); ++index)
			{
				const GpuParticleType type = GpuParticleEmitterPresetTable::GetSpriteTypeByIndex(index);
				if (name == GpuParticleEmitterPresetTable::GetSpriteDisplayName(type))
				{
					return type;
				}
			}

			if (name == "HitSpark" || name == "Slash" || name == "Charge") return GpuParticleType::Spark;
			if (name == "Explosion" || name == "Fire") return GpuParticleType::DeathBurstCore;
			if (name == "MagicCircle") return GpuParticleType::Heal;
			if (name == "Default") return GpuParticleType::Default;
			return GpuParticleType::Default;
		}

		BillboardMode ParseBillboardMode(const std::string& name)
		{
			if (name == "None") return BillboardMode::None;
			if (name == "YAxis") return BillboardMode::YAxis;
			if (name == "Ribbon") return BillboardMode::Ribbon;
			return BillboardMode::Camera;
		}

		uint32_t ParseEmitterShape(const std::string& name)
		{
			if (name == "Sphere") return 1u;
			if (name == "Box") return 2u;
			if (name == "Cone") return 3u;
			if (name == "Circle") return 4u;
			if (name == "Ring") return 5u;
			if (name == "Hemisphere") return 6u;
			return 0u;
		}

		Vector3 NormalizeOr(const Vector3& value, const Vector3& fallback)
		{
			const float lengthSq = value.x * value.x + value.y * value.y + value.z * value.z;
			if (lengthSq <= kEpsilon)
			{
				return fallback;
			}

			const float invLength = 1.0f / std::sqrt(lengthSq);
			return { value.x * invLength, value.y * invLength, value.z * invLength };
		}

		float ClampMin(float value, float minValue)
		{
			return std::max(value, minValue);
		}

		int ClampMin(int value, int minValue)
		{
			return std::max(value, minValue);
		}

		const ComponentProperty* FindProperty(const std::vector<ComponentProperty>& properties, const char* name)
		{
			for (const ComponentProperty& property : properties)
			{
				if (property.name == name)
				{
					return &property;
				}
			}
			return nullptr;
		}

		void DrawNamedProperties(const std::vector<ComponentProperty>& properties, std::initializer_list<const char*> names)
		{
			std::vector<ComponentProperty> section;
			section.reserve(names.size());
			for (const char* name : names)
			{
				if (const ComponentProperty* property = FindProperty(properties, name))
				{
					section.push_back(*property);
				}
			}
			ComponentPropertyUtility::DrawImGui(section);
		}

		void DrawHelpText(const char* text)
		{
#ifdef USE_IMGUI
			ImGui::TextDisabled("%s", text);
#else
			(void)text;
#endif
		}

		const char* GetPresetDescription(const std::string& presetName)
		{
			if (presetName == "Smoke") return "用途: ゆっくり上昇して薄く消える煙\n主に調整する値: 発生レート、寿命、サイズ、速度、色、アルファ";
			if (presetName == "Explosion") return "用途: 一瞬で広がる爆発\n主に調整する値: バースト数、速度、外向き速度、寿命、色、サイズ";
			if (presetName == "HitSpark") return "用途: 攻撃や着弾時の火花\n主に調整する値: バースト数、速度、方向ランダム、寿命、ブレンド方式";
			if (presetName == "Slash") return "用途: 斬撃や残像\n主に調整する値: 軌跡、寿命、サイズ、方向、ブレンド方式";
			if (presetName == "Trail") return "用途: 魔法弾や弾道の軌跡\n主に調整する値: 寿命、サイズ、色、追従";
			if (presetName == "MagicCircle") return "用途: 魔法陣や範囲エフェクト\n主に調整する値: 発生形状、リング半径、回転、色、寿命";
			if (presetName == "Shockwave") return "用途: 衝撃波\n主に調整する値: リング半径、サイズ変化、寿命、アルファ";
			if (presetName == "MeshDebris" || presetName == "Debris") return "用途: 3Dモデルの破片を飛ばす爆散表現\n主に調整する値: メッシュ、バースト数、速度、重力、回転、スケール";
			if (presetName == "Fire") return "用途: 上へ揺らぐ炎\n主に調整する値: 発生レート、寿命、速度、色、ブレンド方式";
			if (presetName == "Heal") return "用途: 回復や祝福の粒子\n主に調整する値: 発生形状、色、寿命、ループ";
			if (presetName == "Charge") return "用途: 溜め演出や集中する光\n主に調整する値: バースト数、速度、方向ランダム、色";
			return "用途: 汎用GPUパーティクル\n主に調整する値: 発生量、寿命、速度、色、サイズ";
		}
	}

	void GpuParticleComponent::Initialize()
	{
		SceneComponent::Initialize();
		previousWorldPosition_ = GetWorldPosition();

		if (playOnStart_)
		{
			Play();
		}
	}

	void GpuParticleComponent::Update(float deltaTime)
	{
		SceneComponent::Update(deltaTime);

		const Vector3 currentPosition = GetWorldPosition();
		if (deltaTime > kEpsilon)
		{
			actorVelocity_ = {
				(currentPosition.x - previousWorldPosition_.x) / deltaTime,
				(currentPosition.y - previousWorldPosition_.y) / deltaTime,
				(currentPosition.z - previousWorldPosition_.z) / deltaTime
			};
		}
		previousWorldPosition_ = currentPosition;

		if (!visible_ || !IsActiveInHierarchy())
		{
			Stop();
			return;
		}

		if (!playing_)
		{
			return;
		}

		if (followOwner_)
		{
			SyncEmitterPosition();
		}

		if (burstEnabled_ && burstInterval_ > 0.0f && burstRepeatRemaining_ > 0)
		{
			burstTimer_ += deltaTime;
			while (burstTimer_ >= burstInterval_ && burstRepeatRemaining_ > 0)
			{
				burstTimer_ -= burstInterval_;
				if (GpuParticleEmitter* emitter = GpuParticleManager::GetInstance()->GetEmitter(activeEmitterName_))
				{
					emitter->RequestEmit(CalculateBurstCount());
				}
				--burstRepeatRemaining_;
			}
		}

		if (!loop_ && !IsPlaying())
		{
			playing_ = false;
			activeEmitterName_.clear();
		}
	}

	void GpuParticleComponent::DrawImGui()
	{
		SceneComponent::DrawImGui();

#ifdef USE_IMGUI
		ImGui::SeparatorText("GPUパーティクルコンポーネント");

		const std::vector<ComponentProperty> properties = CreateProperties(false);
		const bool isSprite = particleType_ != "Mesh";

		ImGui::SeparatorText("プリセット");
		const std::array<const char*, 14> presetNames = {
			"Smoke", "Explosion", "HitSpark", "Slash", "Trail", "MagicCircle", "Shockwave",
			"Fire", "Debris", "MeshDebris", "Dust", "Heal", "Charge", "Default"
		};
		if (ImGui::BeginCombo("Preset", presetName_.empty() ? "None" : presetName_.c_str()))
		{
			for (const char* presetName : presetNames)
			{
				const bool selected = presetName_ == presetName;
				if (ImGui::Selectable(presetName, selected))
				{
					ApplyPreset(presetName);
				}
				if (selected)
				{
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}

		if (ImGui::Button("プリセット適用"))
		{
			ApplyPreset(presetName_);
		}
		ImGui::TextWrapped("%s", GetPresetDescription(presetName_));

		ImGui::SeparatorText("基本");
		DrawHelpText("再生方法、追従、位置、発生量を設定します。");
		DrawNamedProperties(properties, {
			"EffectName", "EmitterName", "PresetName", "ParticleType",
			"PlayOnStart", "Loop", "Visible", "FollowOwner",
			"LocalOffset", "Scale", "EmissionRate", "LifeTime"
		});

		if (isSprite)
		{
			ImGui::SeparatorText("Sprite");
			DrawHelpText("画像を使った軽量な粒子です。煙、火花、魔法弾に向いています。");
			ImGui::Text("テクスチャ: %s", texturePath_.empty() ? "未選択" : texturePath_.c_str());
			std::string selectedTexturePath = texturePath_;
			if (AssetPathSelector::DrawAssetSelector("一覧から選択##GpuParticleComponentTexturePath", selectedTexturePath, AssetType::Texture))
			{
				texturePath_ = selectedTexturePath;
			}
			DrawNamedProperties(properties, {
				"BillboardMode", "BlendMode", "SortMode",
				"StartRotation", "RotationSpeed", "RotationRandom", "UseSpriteSheet"
			});
			if (useSpriteSheet_)
			{
				DrawNamedProperties(properties, { "SpriteSheetRows", "SpriteSheetColumns", "SpriteSheetFrameRate" });
			}
		}
		else
		{
			ImGui::SeparatorText("Mesh");
			DrawHelpText("3Dモデルを粒子として飛ばします。破片や爆散表現に向いています。");
			ImGui::Text("メッシュ: %s", meshPath_.empty() ? "未選択" : meshPath_.c_str());
			std::string selectedMeshPath = meshPath_;
			if (AssetPathSelector::DrawAssetSelector("一覧から選択##GpuParticleComponentMeshPath", selectedMeshPath, AssetType::Model))
			{
				meshPath_ = selectedMeshPath;
			}
			DrawNamedProperties(properties, {
				"MeshId", "MeshAlignMode", "MeshTint"
			});
		}

		ImGui::SeparatorText("Emitter");
		DrawHelpText("粒子がどの形から発生するかを設定します。");
		DrawNamedProperties(properties, { "EmitterShape" });
		if (emitterShape_ == "Sphere" || emitterShape_ == "Circle" || emitterShape_ == "Hemisphere")
		{
			DrawNamedProperties(properties, { "SpawnRadius" });
		}
		else if (emitterShape_ == "Box")
		{
			DrawNamedProperties(properties, { "SpawnBoxSize" });
		}
		else if (emitterShape_ == "Cone")
		{
			DrawNamedProperties(properties, { "SpawnRadius", "ConeAngle", "ConeLength" });
		}
		else if (emitterShape_ == "Ring")
		{
			DrawNamedProperties(properties, { "RingRadius", "RingThickness" });
		}

		ImGui::SeparatorText("Velocity");
		DrawHelpText("粒子がどの方向へ、どれくらいの速さで飛ぶかを設定します。");
		DrawNamedProperties(properties, {
			"BaseVelocity", "VelocityRandom", "Direction", "Speed", "SpeedRandom",
			"RandomAngle", "OutwardVelocity", "TangentialVelocity", "InheritActorVelocity"
		});

		ImGui::SeparatorText("Burst / Lifetime");
		DrawHelpText("爆発やヒットのように一瞬で大量発生させる設定と、寿命中の動きを調整します。");
		DrawNamedProperties(properties, {
			"BurstEnabled", "BurstCount", "BurstRandom", "BurstRepeat", "BurstInterval",
			"LifeTimeRandom", "Acceleration", "Gravity", "Damping"
		});

		ImGui::SeparatorText("Color");
		DrawHelpText("生存時間に応じた色と透明度の変化を設定します。");
		DrawNamedProperties(properties, { "StartColor", "MidColor", "EndColor", "MidTime", "AlphaFade" });

		if (isSprite)
		{
			ImGui::SeparatorText("Size");
			DrawHelpText("生存時間に応じた大きさの変化を設定します。");
			DrawNamedProperties(properties, { "StartSize2D", "MidSize2D", "EndSize2D" });

			ImGui::SeparatorText("Trail");
			DrawHelpText("弾道や斬撃のような軌跡を設定します。");
			ImGui::TextDisabled("Trailは現在のGPUパーティクル描画には未対応です。値はJSONへ保持しますが、描画には反映されません。");
			ImGui::Text("軌跡テクスチャ: %s", trailTexturePath_.empty() ? "未選択" : trailTexturePath_.c_str());
			ImGui::Text("軌跡: %s / 長さ %.2f / 太さ %.2f / フェード %.2f",
				trailEnabled_ ? "有効" : "無効", trailLength_, trailWidth_, trailFadeTime_);
		}
		else
		{
			ImGui::SeparatorText("Mesh Scale / Rotation");
			DrawHelpText("破片の大きさと回転の変化を設定します。");
			DrawNamedProperties(properties, {
				"MeshStartScale", "MeshEndScale", "MeshScaleRandom",
				"MeshAngularVelocity", "MeshAngularVelocityRandom"
			});
		}

		ImGui::SeparatorText("Runtime");
		const GpuParticleEmitter* activeEmitter = activeEmitterName_.empty() ? nullptr : GpuParticleManager::GetInstance()->GetEmitter(activeEmitterName_);
		ImGui::Text("再生中: %s", IsPlaying() ? "はい" : "いいえ");
		ImGui::Text("ParticleCount: %u", activeEmitter ? activeEmitter->GetEstimatedActiveParticleCount() : 0u);
		ImGui::Text("Draw可能: %s", visible_ && IsActiveInHierarchy() ? "はい" : "いいえ");
		ImGui::Text("Texture読み込み状態: %s", texturePath_.empty() ? "未選択" : "選択済み");
		ImGui::Text("Mesh読み込み状態: %s", meshPath_.empty() ? "未選択" : (GpuParticleManager::GetInstance()->FindMeshAsset(static_cast<uint32_t>(std::max(meshId_, 0))) ? "登録済み" : "未登録"));
		ImGui::Text("現在のParticleType: %s", particleType_.c_str());
		ImGui::Text("現在のEmitterShape: %s", emitterShape_.c_str());
		ImGui::Text("現在のBlendMode: %s", blendMode_.c_str());

		if (ImGui::Button("再生")) Play();
		ImGui::SameLine();
		if (ImGui::Button("停止")) Stop();
		ImGui::SameLine();
		if (ImGui::Button("再開")) Restart();
#endif // USE_IMGUI
	}

	void GpuParticleComponent::Finalize()
	{
		Stop();
	}

	void GpuParticleComponent::ToJson(nlohmann::json& outJson) const
	{
		SceneComponent::ToJson(outJson);
		outJson["Class"] = GetClassTypeName();
		ComponentPropertyUtility::ToJson(const_cast<GpuParticleComponent*>(this)->CreateProperties(), outJson);
		outJson["SpawnRate"] = emissionRate_;
		outJson["FollowActor"] = followOwner_;
		outJson["StartSize"] = startSize_;
		outJson["EndSize"] = endSize_;
		outJson["VelocityScale"] = velocityScale_;
	}

	void GpuParticleComponent::FromJson(const nlohmann::json& inJson)
	{
		SceneComponent::FromJson(inJson);
		ComponentPropertyUtility::FromJson(CreateProperties(), inJson);

		if (inJson.contains("SpawnRate") && inJson["SpawnRate"].is_number())
		{
			emissionRate_ = inJson["SpawnRate"].get<float>();
		}
		if (inJson.contains("FollowActor") && inJson["FollowActor"].is_boolean())
		{
			followOwner_ = inJson["FollowActor"].get<bool>();
		}
		if (inJson.contains("StartSize") && inJson["StartSize"].is_number())
		{
			startSize_ = inJson["StartSize"].get<float>();
			startSize2D_ = { startSize_, startSize_ };
		}
		if (inJson.contains("EndSize") && inJson["EndSize"].is_number())
		{
			endSize_ = inJson["EndSize"].get<float>();
			endSize2D_ = { endSize_, endSize_ };
		}
		if (inJson.contains("VelocityScale") && inJson["VelocityScale"].is_number())
		{
			velocityScale_ = inJson["VelocityScale"].get<float>();
		}
		if (inJson.contains("EffectName") && inJson["EffectName"].is_string() && presetName_.empty())
		{
			presetName_ = inJson["EffectName"].get<std::string>();
		}
		if (texturePath_.empty())
		{
			texturePath_ = "Effects/white.dds";
		}
	}

	void GpuParticleComponent::Play()
	{
		if (!visible_ || !IsActiveInHierarchy() || effectName_.empty())
		{
			return;
		}

		Stop();
		lockedPosition_ = CalculateEmitterPosition();

		GpuParticleEmitter* emitter = EnsureEmitter();
		if (!emitter)
		{
			return;
		}

		emitter->SetPosition(lockedPosition_);
		const uint32_t initialCount = burstEnabled_ ? CalculateBurstCount() : std::max(1u, static_cast<uint32_t>(std::max(emissionRate_, 1.0f)));
		emitter->RequestEmit(initialCount);
		burstTimer_ = 0.0f;
		burstRepeatRemaining_ = std::max(burstRepeat_ - 1, 0);
		playing_ = true;
	}

	void GpuParticleComponent::Stop()
	{
		if (!activeEmitterName_.empty())
		{
			GpuParticleManager::GetInstance()->RemoveEmitter(activeEmitterName_);
		}

		activeEmitterName_.clear();
		playing_ = false;
		burstTimer_ = 0.0f;
		burstRepeatRemaining_ = 0;
	}

	void GpuParticleComponent::Restart()
	{
		Stop();
		Play();
	}

	bool GpuParticleComponent::IsPlaying() const
	{
		if (activeEmitterName_.empty())
		{
			return false;
		}

		GpuParticleEmitter* emitter = GpuParticleManager::GetInstance()->GetEmitter(activeEmitterName_);
		if (!emitter)
		{
			return false;
		}

		return loop_ || emitter->HasActiveParticles() || burstRepeatRemaining_ > 0;
	}

	void GpuParticleComponent::SetVisible(bool visible)
	{
		visible_ = visible;
		if (!visible_)
		{
			Stop();
		}
	}

	std::string GpuParticleComponent::ResolveEmitterName() const
	{
		if (!emitterName_.empty())
		{
			return emitterName_;
		}

		char buffer[128]{};
		std::snprintf(buffer, sizeof(buffer), "ActorGpuParticle_%p", static_cast<const void*>(this));
		return buffer;
	}

	Vector3 GpuParticleComponent::CalculateEmitterPosition() const
	{
		return GetWorldPosition() + localOffset_;
	}

	GpuParticleEmitter* GpuParticleComponent::EnsureEmitter()
	{
		const GpuParticleType type = ParseGpuParticleType(effectName_);
		GpuParticleEmitter::EmitterInfo info = GpuParticleEmitterPresetTable::MakeEmitterInfo(type);

		const bool isMesh = particleType_ == "Mesh";
		const float scaleAverage = std::max((scale_.x + scale_.y + scale_.z) / 3.0f, 0.0f);
		info.kind = isMesh ? GpuParticleKind::Mesh : GpuParticleKind::Sprite;
		info.spriteType = type;
		info.textureFilePath = texturePath_.empty() ? "Effects/white.dds" : texturePath_;
		info.billboardFlags = isMesh ? BillboardMode::None : ParseBillboardMode(billboardMode_);
		info.radius = std::max(spawnRadius_, 0.0f) * std::max(scaleAverage, 1.0f);
		info.speedScale = std::max(velocityScale_, 0.0f);
		info.useDescSpawnOverride = true;
		info.maxParticles = 4096;
		info.lifeTime = ClampMin(lifeTime_, 0.01f);
		info.lifeTimeRandom = ClampMin(lifeTimeRandom_, 0.0f);
		info.startColor = startColor_;
		info.endColor = endColor_;
		info.colorRandom = { 0.0f, 0.0f, 0.0f, 0.0f };
		info.alphaFade = alphaFade_;
		info.startSize = { std::max(startSize2D_.x, 0.0f), std::max(startSize2D_.y, 0.0f) };
		info.endSize = { std::max(endSize2D_.x, 0.0f), std::max(endSize2D_.y, 0.0f) };
		info.sizeRandom = 0.0f;
		info.positionRandom = spawnBoxSize_;
		info.spawnShape = ParseEmitterShape(emitterShape_);
		info.spawnRadius = std::max(spawnRadius_, std::max(ringRadius_, 0.0f));
		info.spawnBoxSize = spawnBoxSize_;

		const Vector3 normalizedDirection = NormalizeOr(direction_, { 0.0f, 1.0f, 0.0f });
		Vector3 velocity = baseVelocity_;
		if (speed_ > 0.0f)
		{
			velocity = { normalizedDirection.x * speed_, normalizedDirection.y * speed_, normalizedDirection.z * speed_ };
		}
		velocity.x += actorVelocity_.x * inheritActorVelocity_;
		velocity.y += actorVelocity_.y * inheritActorVelocity_;
		velocity.z += actorVelocity_.z * inheritActorVelocity_;
		info.velocity = velocity;
		info.velocityRandom = velocityRandom_;
		info.speed = std::max(speed_, 0.0f);
		info.speedRandom = std::max(speedRandom_, 0.0f);
		info.gravity = { gravity_.x + acceleration_.x, gravity_.y + acceleration_.y, gravity_.z + acceleration_.z };
		info.damping = std::max(damping_, 0.0f);
		info.startRotation = startRotation_;
		info.rotationSpeed = rotationSpeed_;
		info.rotationRandom = std::max(rotationRandom_, std::max(randomAngle_, 0.0f));
		info.startScale3D = isMesh ? meshStartScale_ : scale_;
		info.endScale3D = isMesh ? meshEndScale_ : scale_;
		info.useSpriteSheet = useSpriteSheet_;
		info.spriteSheetRows = static_cast<uint32_t>(ClampMin(spriteSheetRows_, 1));
		info.spriteSheetColumns = static_cast<uint32_t>(ClampMin(spriteSheetColumns_, 1));
		info.spriteSheetFrameRate = std::max(spriteSheetFrameRate_, 0.0f);

		if (isMesh)
		{
			if (!meshPath_.empty())
			{
				try
				{
					GpuParticleManager::GetInstance()->LoadMeshAssetsFromAssimp(static_cast<uint32_t>(std::max(meshId_, 0)), meshPath_, true);
				} catch (...)
				{
				}
			}
			info.textureFilePath = "Mesh:" + std::to_string(std::max(meshId_, 0));
		}

		if (loop_)
		{
			info.loopCount = std::max(1u, static_cast<uint32_t>(std::max(emissionRate_, 1.0f)));
			info.loopFrequency = emissionRate_ > 0.0f ? 1.0f : 0.0f;
		}
		else
		{
			info.loopCount = 0;
			info.loopFrequency = 0.0f;
		}

		activeEmitterName_ = ResolveEmitterName();
		return GpuParticleManager::GetInstance()->CreateRuntimeEmitter(activeEmitterName_, info);
	}

	void GpuParticleComponent::SyncEmitterPosition()
	{
		if (activeEmitterName_.empty())
		{
			return;
		}

		if (GpuParticleEmitter* emitter = GpuParticleManager::GetInstance()->GetEmitter(activeEmitterName_))
		{
			emitter->SetPosition(CalculateEmitterPosition());
		}
	}

	void GpuParticleComponent::ApplyPreset(const std::string& presetName)
	{
		presetName_ = presetName;
		effectName_ = presetName;
		particleType_ = "Sprite";
		texturePath_ = "Effects/white.dds";
		billboardMode_ = "Camera";
		blendMode_ = "Alpha";
		sortMode_ = "None";
		emitterShape_ = "Point";
		burstEnabled_ = true;
		burstRepeat_ = 1;
		burstInterval_ = 0.0f;
		gravity_ = { 0.0f, 0.0f, 0.0f };
		acceleration_ = { 0.0f, 0.0f, 0.0f };
		damping_ = 0.0f;
		trailEnabled_ = false;
		trailLength_ = 1.0f;
		trailWidth_ = 0.1f;
		trailColor_ = { 1.0f, 1.0f, 1.0f, 0.6f };
		trailFadeTime_ = 0.25f;
		trailLifeTime_ = 0.25f;

		if (presetName == "Explosion")
		{
			effectName_ = "DeathBurstCore"; loop_ = false; blendMode_ = "Additive"; emitterShape_ = "Sphere"; emissionRate_ = 0.0f; burstEnabled_ = true; burstCount_ = 60; burstRandom_ = 20; burstRepeat_ = 1; lifeTime_ = 0.8f; speed_ = 5.0f; speedRandom_ = 3.0f; outwardVelocity_ = 2.0f; gravity_ = { 0.0f, -1.0f, 0.0f }; damping_ = 1.0f; startSize2D_ = { 0.2f, 0.2f }; midSize2D_ = { 1.0f, 1.0f }; endSize2D_ = { 2.0f, 2.0f }; startColor_ = { 1.0f, 0.65f, 0.18f, 1.0f }; midColor_ = { 1.0f, 0.35f, 0.05f, 0.63f }; endColor_ = { 0.08f, 0.07f, 0.06f, 0.0f }; alphaFade_ = true;
		}
		else if (presetName == "HitSpark")
		{
			effectName_ = "Spark"; loop_ = false; blendMode_ = "Additive"; emitterShape_ = "Point"; emissionRate_ = 0.0f; burstEnabled_ = true; burstCount_ = 16; burstRandom_ = 8; burstRepeat_ = 1; lifeTime_ = 0.25f; speed_ = 8.0f; speedRandom_ = 4.0f; randomAngle_ = 60.0f; startSize2D_ = { 0.08f, 0.08f }; midSize2D_ = { 0.04f, 0.04f }; endSize2D_ = { 0.0f, 0.0f }; startColor_ = { 1.0f, 0.95f, 0.45f, 1.0f }; midColor_ = { 1.0f, 0.55f, 0.10f, 0.7f }; endColor_ = { 1.0f, 0.35f, 0.05f, 0.0f }; alphaFade_ = true;
		}
		else if (presetName == "Slash")
		{
			effectName_ = "Spark"; loop_ = false; blendMode_ = "Additive"; emitterShape_ = "Cone"; burstEnabled_ = true; burstCount_ = 24; burstRandom_ = 4; lifeTime_ = 0.3f; speed_ = 2.0f; randomAngle_ = 20.0f; coneAngle_ = 35.0f; coneLength_ = 1.2f; startSize2D_ = { 0.4f, 0.4f }; midSize2D_ = { 0.2f, 0.2f }; endSize2D_ = { 0.0f, 0.0f }; startColor_ = { 0.9f, 0.95f, 1.0f, 1.0f }; endColor_ = { 0.35f, 0.6f, 1.0f, 0.0f }; trailEnabled_ = true;
		}
		else if (presetName == "Charge")
		{
			effectName_ = "Spark"; loop_ = true; blendMode_ = "Additive"; emitterShape_ = "Sphere"; emissionRate_ = 24.0f; burstEnabled_ = false; lifeTime_ = 0.7f; speed_ = 1.2f; speedRandom_ = 0.8f; randomAngle_ = 45.0f; startSize2D_ = { 0.08f, 0.08f }; endSize2D_ = { 0.0f, 0.0f }; startColor_ = { 0.55f, 0.8f, 1.0f, 1.0f }; endColor_ = { 0.2f, 0.4f, 1.0f, 0.0f };
		}
		else if (presetName == "Trail")
		{
			effectName_ = "Trail"; loop_ = true; blendMode_ = "Additive"; emitterShape_ = "Point"; emissionRate_ = 40.0f; burstEnabled_ = false; lifeTime_ = 0.4f; speed_ = 0.0f; startSize2D_ = { 0.2f, 0.2f }; midSize2D_ = { 0.1f, 0.1f }; endSize2D_ = { 0.0f, 0.0f }; startColor_ = { 0.4f, 0.8f, 1.0f, 0.9f }; endColor_ = { 0.2f, 0.5f, 1.0f, 0.0f }; alphaFade_ = true; trailEnabled_ = true; trailLength_ = 1.6f; trailWidth_ = 0.18f; trailFadeTime_ = 0.35f; trailLifeTime_ = 0.35f;
		}
		else if (presetName == "MagicCircle" || presetName == "Heal")
		{
			effectName_ = "Heal"; loop_ = true; emissionRate_ = 20.0f; burstEnabled_ = false; burstCount_ = 24; lifeTime_ = 1.4f; emitterShape_ = "Ring"; ringRadius_ = 1.0f; ringThickness_ = 0.08f; speed_ = 1.2f; rotationSpeed_ = 1.0f; startColor_ = { 0.35f, 1.0f, 0.75f, 1.0f }; midColor_ = { 0.2f, 0.8f, 1.0f, 0.5f }; endColor_ = { 0.2f, 0.8f, 1.0f, 0.0f };
		}
		else if (presetName == "Shockwave")
		{
			effectName_ = "Shockwave"; loop_ = false; emissionRate_ = 0.0f; burstEnabled_ = true; burstCount_ = 1; lifeTime_ = 0.55f; emitterShape_ = "Circle"; ringRadius_ = 1.0f; startSize2D_ = { 0.2f, 0.2f }; midSize2D_ = { 2.0f, 2.0f }; endSize2D_ = { 4.0f, 4.0f }; startColor_ = { 0.6f, 0.9f, 1.0f, 0.8f }; endColor_ = { 0.6f, 0.9f, 1.0f, 0.0f };
		}
		else if (presetName == "Fire")
		{
			effectName_ = "DeathBurstCore"; loop_ = true; blendMode_ = "Additive"; emissionRate_ = 35.0f; burstEnabled_ = false; burstCount_ = 24; lifeTime_ = 1.0f; speed_ = 2.5f; velocityRandom_ = { 1.0f, 0.5f, 1.0f }; gravity_ = { 0.0f, 1.2f, 0.0f }; startColor_ = { 1.0f, 0.45f, 0.08f, 1.0f }; endColor_ = { 0.25f, 0.03f, 0.01f, 0.0f };
		}
		else if (presetName == "Debris" || presetName == "MeshDebris")
		{
			effectName_ = "Debris"; particleType_ = "Mesh"; loop_ = false; emitterShape_ = "Sphere"; emissionRate_ = 0.0f; burstEnabled_ = true; burstCount_ = 20; burstRandom_ = 0; lifeTime_ = 1.3f; speed_ = 4.0f; speedRandom_ = 2.0f; outwardVelocity_ = 2.0f; gravity_ = { 0.0f, -2.0f, 0.0f }; damping_ = 0.5f; meshStartScale_ = { 0.2f, 0.2f, 0.2f }; meshEndScale_ = { 0.1f, 0.1f, 0.1f }; meshScaleRandom_ = { 0.2f, 0.2f, 0.2f }; meshAngularVelocity_ = { 3.0f, 3.0f, 3.0f };
		}
		else if (presetName == "Dust" || presetName == "Smoke")
		{
			effectName_ = presetName; loop_ = true; blendMode_ = "Alpha"; emitterShape_ = "Sphere"; emissionRate_ = presetName == "Dust" ? 18.0f : 15.0f; burstEnabled_ = false; lifeTime_ = presetName == "Dust" ? 1.2f : 2.0f; speed_ = presetName == "Dust" ? 1.2f : 0.3f; speedRandom_ = 0.8f; velocityRandom_ = { 0.3f, 0.5f, 0.3f }; gravity_ = { 0.0f, 0.1f, 0.0f }; damping_ = 0.3f; startSize2D_ = { 0.3f, 0.3f }; midSize2D_ = { 1.0f, 1.0f }; endSize2D_ = { 2.0f, 2.0f }; startColor_ = { 1.0f, 1.0f, 1.0f, 0.63f }; midColor_ = { 0.45f, 0.45f, 0.45f, 0.39f }; endColor_ = { 0.35f, 0.35f, 0.35f, 0.0f }; alphaFade_ = true;
		}
		else
		{
			effectName_ = presetName.empty() ? "Default" : presetName;
			emissionRate_ = 10.0f;
			burstCount_ = 16;
			lifeTime_ = 1.0f;
			startSize2D_ = { startSize_, startSize_ };
			endSize2D_ = { endSize_, endSize_ };
		}

		startSize_ = startSize2D_.x;
		endSize_ = endSize2D_.x;
	}

	uint32_t GpuParticleComponent::CalculateBurstCount() const
	{
		const int randomRange = std::max(burstRandom_, 0);
		const int randomValue = randomRange > 0 ? (std::rand() % (randomRange + 1)) : 0;
		return static_cast<uint32_t>(std::max(burstCount_ + randomValue, 0));
	}

	std::vector<ComponentProperty> GpuParticleComponent::CreateProperties(bool includeAssetPaths)
	{
		std::vector<ComponentProperty> properties = {
			{ "EffectName", "EffectName", ComponentPropertyType::String, [this]() -> ComponentPropertyValue { return effectName_; }, [this](const ComponentPropertyValue& value) { if (const auto* v = std::get_if<std::string>(&value)) effectName_ = *v; } },
			{ "EmitterName", "EmitterName", ComponentPropertyType::String, [this]() -> ComponentPropertyValue { return emitterName_; }, [this](const ComponentPropertyValue& value) { if (const auto* v = std::get_if<std::string>(&value)) emitterName_ = *v; } },
			{ "PresetName", "PresetName", ComponentPropertyType::String, [this]() -> ComponentPropertyValue { return presetName_; }, [this](const ComponentPropertyValue& value) { if (const auto* v = std::get_if<std::string>(&value)) presetName_ = *v; }, 0.0f, 0.0f, 0.1f, false, MakeChoices({ "Smoke", "Explosion", "HitSpark", "Slash", "Trail", "MagicCircle", "Shockwave", "Fire", "Debris", "MeshDebris", "Dust", "Heal", "Charge", "Default" }) },
			{ "ParticleType", "ParticleType", ComponentPropertyType::String, [this]() -> ComponentPropertyValue { return particleType_; }, [this](const ComponentPropertyValue& value) { if (const auto* v = std::get_if<std::string>(&value)) particleType_ = *v; }, 0.0f, 0.0f, 0.1f, false, MakeChoices({ "Sprite", "Mesh" }) },
			{ "PlayOnStart", "開始時に再生", ComponentPropertyType::Bool, [this]() -> ComponentPropertyValue { return playOnStart_; }, [this](const ComponentPropertyValue& value) { if (const auto* v = std::get_if<bool>(&value)) playOnStart_ = *v; } },
			{ "Loop", "ループ", ComponentPropertyType::Bool, [this]() -> ComponentPropertyValue { return loop_; }, [this](const ComponentPropertyValue& value) { if (const auto* v = std::get_if<bool>(&value)) loop_ = *v; } },
			{ "Visible", "表示", ComponentPropertyType::Bool, [this]() -> ComponentPropertyValue { return visible_; }, [this](const ComponentPropertyValue& value) { if (const auto* v = std::get_if<bool>(&value)) SetVisible(*v); } },
			{ "FollowOwner", "Actorに追従", ComponentPropertyType::Bool, [this]() -> ComponentPropertyValue { return followOwner_; }, [this](const ComponentPropertyValue& value) { if (const auto* v = std::get_if<bool>(&value)) followOwner_ = *v; } },
			{ "LocalOffset", "ローカルオフセット", ComponentPropertyType::Vector3, [this]() -> ComponentPropertyValue { return localOffset_; }, [this](const ComponentPropertyValue& value) { if (const auto* v = std::get_if<Vector3>(&value)) localOffset_ = *v; }, 0.0f, 0.0f, 0.01f },
			{ "Scale", "スケール", ComponentPropertyType::Vector3, [this]() -> ComponentPropertyValue { return scale_; }, [this](const ComponentPropertyValue& value) { if (const auto* v = std::get_if<Vector3>(&value)) scale_ = *v; }, 0.0f, 100.0f, 0.01f, true },
			{ "EmissionRate", "SpawnRate", ComponentPropertyType::Float, [this]() -> ComponentPropertyValue { return emissionRate_; }, [this](const ComponentPropertyValue& value) { if (const auto* v = std::get_if<float>(&value)) emissionRate_ = std::max(*v, 0.0f); }, 0.0f, 10000.0f, 0.1f, true },
			{ "LifeTime", "LifeTime", ComponentPropertyType::Float, [this]() -> ComponentPropertyValue { return lifeTime_; }, [this](const ComponentPropertyValue& value) { if (const auto* v = std::get_if<float>(&value)) lifeTime_ = ClampMin(*v, 0.01f); }, 0.01f, 60.0f, 0.01f, true },

			{ "BillboardMode", "BillboardMode", ComponentPropertyType::String, [this]() -> ComponentPropertyValue { return billboardMode_; }, [this](const ComponentPropertyValue& value) { if (const auto* v = std::get_if<std::string>(&value)) billboardMode_ = *v; }, 0.0f, 0.0f, 0.1f, false, MakeChoices({ "None", "Camera", "YAxis", "Ribbon" }) },
			{ "BlendMode", "BlendMode", ComponentPropertyType::String, [this]() -> ComponentPropertyValue { return blendMode_; }, [this](const ComponentPropertyValue& value) { if (const auto* v = std::get_if<std::string>(&value)) blendMode_ = *v; }, 0.0f, 0.0f, 0.1f, false, MakeChoices({ "Alpha", "Additive", "Multiply" }) },
			{ "SortMode", "SortMode", ComponentPropertyType::String, [this]() -> ComponentPropertyValue { return sortMode_; }, [this](const ComponentPropertyValue& value) { if (const auto* v = std::get_if<std::string>(&value)) sortMode_ = *v; }, 0.0f, 0.0f, 0.1f, false, MakeChoices({ "None", "BackToFront", "FrontToBack", "Age" }) },
			{ "StartSize2D", "StartSize", ComponentPropertyType::Vector2, [this]() -> ComponentPropertyValue { return startSize2D_; }, [this](const ComponentPropertyValue& value) { if (const auto* v = std::get_if<Vector2>(&value)) { startSize2D_ = *v; startSize_ = v->x; } }, 0.0f, 100.0f, 0.01f, true },
			{ "MidSize2D", "MidSize", ComponentPropertyType::Vector2, [this]() -> ComponentPropertyValue { return midSize2D_; }, [this](const ComponentPropertyValue& value) { if (const auto* v = std::get_if<Vector2>(&value)) midSize2D_ = *v; }, 0.0f, 100.0f, 0.01f, true },
			{ "EndSize2D", "EndSize", ComponentPropertyType::Vector2, [this]() -> ComponentPropertyValue { return endSize2D_; }, [this](const ComponentPropertyValue& value) { if (const auto* v = std::get_if<Vector2>(&value)) { endSize2D_ = *v; endSize_ = v->x; } }, 0.0f, 100.0f, 0.01f, true },
			{ "StartRotation", "StartRotation", ComponentPropertyType::Float, [this]() -> ComponentPropertyValue { return startRotation_; }, [this](const ComponentPropertyValue& value) { if (const auto* v = std::get_if<float>(&value)) startRotation_ = *v; }, -1000.0f, 1000.0f, 0.01f, true },
			{ "RotationSpeed", "RotationSpeed", ComponentPropertyType::Float, [this]() -> ComponentPropertyValue { return rotationSpeed_; }, [this](const ComponentPropertyValue& value) { if (const auto* v = std::get_if<float>(&value)) rotationSpeed_ = *v; }, -1000.0f, 1000.0f, 0.01f, true },
			{ "RotationRandom", "RotationRandom", ComponentPropertyType::Float, [this]() -> ComponentPropertyValue { return rotationRandom_; }, [this](const ComponentPropertyValue& value) { if (const auto* v = std::get_if<float>(&value)) rotationRandom_ = std::max(*v, 0.0f); }, 0.0f, 1000.0f, 0.01f, true },
			{ "UseSpriteSheet", "UseSpriteSheet", ComponentPropertyType::Bool, [this]() -> ComponentPropertyValue { return useSpriteSheet_; }, [this](const ComponentPropertyValue& value) { if (const auto* v = std::get_if<bool>(&value)) useSpriteSheet_ = *v; } },
			{ "SpriteSheetRows", "SpriteSheetRows", ComponentPropertyType::Int, [this]() -> ComponentPropertyValue { return spriteSheetRows_; }, [this](const ComponentPropertyValue& value) { if (const auto* v = std::get_if<int>(&value)) spriteSheetRows_ = ClampMin(*v, 1); }, 1.0f, 64.0f, 1.0f, true },
			{ "SpriteSheetColumns", "SpriteSheetColumns", ComponentPropertyType::Int, [this]() -> ComponentPropertyValue { return spriteSheetColumns_; }, [this](const ComponentPropertyValue& value) { if (const auto* v = std::get_if<int>(&value)) spriteSheetColumns_ = ClampMin(*v, 1); }, 1.0f, 64.0f, 1.0f, true },

			{ "SpriteSheetFrameRate", "SpriteSheetFrameRate", ComponentPropertyType::Float, [this]() -> ComponentPropertyValue { return spriteSheetFrameRate_; }, [this](const ComponentPropertyValue& value) { if (const auto* v = std::get_if<float>(&value)) spriteSheetFrameRate_ = std::max(*v, 0.0f); }, 0.0f, 240.0f, 0.1f, true },
			{ "MeshId", "MeshId", ComponentPropertyType::Int, [this]() -> ComponentPropertyValue { return meshId_; }, [this](const ComponentPropertyValue& value) { if (const auto* v = std::get_if<int>(&value)) meshId_ = std::max(*v, 0); }, 0.0f, 1000000.0f, 1.0f, true },
			{ "MeshStartScale", "MeshStartScale", ComponentPropertyType::Vector3, [this]() -> ComponentPropertyValue { return meshStartScale_; }, [this](const ComponentPropertyValue& value) { if (const auto* v = std::get_if<Vector3>(&value)) meshStartScale_ = *v; }, 0.0f, 100.0f, 0.01f, true },
			{ "MeshEndScale", "MeshEndScale", ComponentPropertyType::Vector3, [this]() -> ComponentPropertyValue { return meshEndScale_; }, [this](const ComponentPropertyValue& value) { if (const auto* v = std::get_if<Vector3>(&value)) meshEndScale_ = *v; }, 0.0f, 100.0f, 0.01f, true },
			{ "MeshScaleRandom", "MeshScaleRandom", ComponentPropertyType::Vector3, [this]() -> ComponentPropertyValue { return meshScaleRandom_; }, [this](const ComponentPropertyValue& value) { if (const auto* v = std::get_if<Vector3>(&value)) meshScaleRandom_ = *v; }, 0.0f, 100.0f, 0.01f, true },
			{ "MeshAngularVelocity", "MeshAngularVelocity", ComponentPropertyType::Vector3, [this]() -> ComponentPropertyValue { return meshAngularVelocity_; }, [this](const ComponentPropertyValue& value) { if (const auto* v = std::get_if<Vector3>(&value)) meshAngularVelocity_ = *v; }, 0.0f, 0.0f, 0.01f },
			{ "MeshAngularVelocityRandom", "MeshAngularVelocityRandom", ComponentPropertyType::Vector3, [this]() -> ComponentPropertyValue { return meshAngularVelocityRandom_; }, [this](const ComponentPropertyValue& value) { if (const auto* v = std::get_if<Vector3>(&value)) meshAngularVelocityRandom_ = *v; }, 0.0f, 0.0f, 0.01f },
			{ "MeshAlignMode", "MeshAlignMode", ComponentPropertyType::String, [this]() -> ComponentPropertyValue { return meshAlignMode_; }, [this](const ComponentPropertyValue& value) { if (const auto* v = std::get_if<std::string>(&value)) meshAlignMode_ = *v; }, 0.0f, 0.0f, 0.1f, false, MakeChoices({ "None", "Velocity", "Emitter", "Camera" }) },

			{ "MeshTint", "MeshTint", ComponentPropertyType::Vector4, [this]() -> ComponentPropertyValue { return meshTint_; }, [this](const ComponentPropertyValue& value) { if (const auto* v = std::get_if<Vector4>(&value)) meshTint_ = *v; }, 0.0f, 1.0f, 0.01f, true, {}, ComponentPropertyDisplay::Color },
			{ "EmitterShape", "EmitterShape", ComponentPropertyType::String, [this]() -> ComponentPropertyValue { return emitterShape_; }, [this](const ComponentPropertyValue& value) { if (const auto* v = std::get_if<std::string>(&value)) emitterShape_ = *v; }, 0.0f, 0.0f, 0.1f, false, MakeChoices({ "Point", "Sphere", "Box", "Cone", "Circle", "Ring", "Hemisphere" }) },
			{ "SpawnRadius", "SpawnRadius", ComponentPropertyType::Float, [this]() -> ComponentPropertyValue { return spawnRadius_; }, [this](const ComponentPropertyValue& value) { if (const auto* v = std::get_if<float>(&value)) spawnRadius_ = std::max(*v, 0.0f); }, 0.0f, 1000.0f, 0.01f, true },
			{ "SpawnBoxSize", "SpawnBoxSize", ComponentPropertyType::Vector3, [this]() -> ComponentPropertyValue { return spawnBoxSize_; }, [this](const ComponentPropertyValue& value) { if (const auto* v = std::get_if<Vector3>(&value)) spawnBoxSize_ = *v; }, 0.0f, 1000.0f, 0.01f, true },
			{ "ConeAngle", "ConeAngle", ComponentPropertyType::Float, [this]() -> ComponentPropertyValue { return coneAngle_; }, [this](const ComponentPropertyValue& value) { if (const auto* v = std::get_if<float>(&value)) coneAngle_ = std::max(*v, 0.0f); }, 0.0f, 180.0f, 0.1f, true },
			{ "ConeLength", "ConeLength", ComponentPropertyType::Float, [this]() -> ComponentPropertyValue { return coneLength_; }, [this](const ComponentPropertyValue& value) { if (const auto* v = std::get_if<float>(&value)) coneLength_ = std::max(*v, 0.0f); }, 0.0f, 1000.0f, 0.01f, true },
			{ "RingRadius", "RingRadius", ComponentPropertyType::Float, [this]() -> ComponentPropertyValue { return ringRadius_; }, [this](const ComponentPropertyValue& value) { if (const auto* v = std::get_if<float>(&value)) ringRadius_ = std::max(*v, 0.0f); }, 0.0f, 1000.0f, 0.01f, true },

			{ "RingThickness", "RingThickness", ComponentPropertyType::Float, [this]() -> ComponentPropertyValue { return ringThickness_; }, [this](const ComponentPropertyValue& value) { if (const auto* v = std::get_if<float>(&value)) ringThickness_ = std::max(*v, 0.0f); }, 0.0f, 1000.0f, 0.01f, true },
			{ "BaseVelocity", "BaseVelocity", ComponentPropertyType::Vector3, [this]() -> ComponentPropertyValue { return baseVelocity_; }, [this](const ComponentPropertyValue& value) { if (const auto* v = std::get_if<Vector3>(&value)) baseVelocity_ = *v; }, 0.0f, 0.0f, 0.01f },
			{ "VelocityRandom", "VelocityRandom", ComponentPropertyType::Vector3, [this]() -> ComponentPropertyValue { return velocityRandom_; }, [this](const ComponentPropertyValue& value) { if (const auto* v = std::get_if<Vector3>(&value)) velocityRandom_ = *v; }, 0.0f, 0.0f, 0.01f },
			{ "Direction", "Direction", ComponentPropertyType::Vector3, [this]() -> ComponentPropertyValue { return direction_; }, [this](const ComponentPropertyValue& value) { if (const auto* v = std::get_if<Vector3>(&value)) direction_ = *v; }, 0.0f, 0.0f, 0.01f },
			{ "Speed", "Speed", ComponentPropertyType::Float, [this]() -> ComponentPropertyValue { return speed_; }, [this](const ComponentPropertyValue& value) { if (const auto* v = std::get_if<float>(&value)) speed_ = std::max(*v, 0.0f); }, 0.0f, 10000.0f, 0.01f, true },
			{ "SpeedRandom", "SpeedRandom", ComponentPropertyType::Float, [this]() -> ComponentPropertyValue { return speedRandom_; }, [this](const ComponentPropertyValue& value) { if (const auto* v = std::get_if<float>(&value)) speedRandom_ = std::max(*v, 0.0f); }, 0.0f, 10000.0f, 0.01f, true },
			{ "RandomAngle", "RandomAngle", ComponentPropertyType::Float, [this]() -> ComponentPropertyValue { return randomAngle_; }, [this](const ComponentPropertyValue& value) { if (const auto* v = std::get_if<float>(&value)) randomAngle_ = std::max(*v, 0.0f); }, 0.0f, 360.0f, 0.01f, true },
			{ "OutwardVelocity", "OutwardVelocity", ComponentPropertyType::Float, [this]() -> ComponentPropertyValue { return outwardVelocity_; }, [this](const ComponentPropertyValue& value) { if (const auto* v = std::get_if<float>(&value)) outwardVelocity_ = *v; }, -10000.0f, 10000.0f, 0.01f, true },
			{ "TangentialVelocity", "TangentialVelocity", ComponentPropertyType::Float, [this]() -> ComponentPropertyValue { return tangentialVelocity_; }, [this](const ComponentPropertyValue& value) { if (const auto* v = std::get_if<float>(&value)) tangentialVelocity_ = *v; }, -10000.0f, 10000.0f, 0.01f, true },
			{ "InheritActorVelocity", "InheritActorVelocity", ComponentPropertyType::Float, [this]() -> ComponentPropertyValue { return inheritActorVelocity_; }, [this](const ComponentPropertyValue& value) { if (const auto* v = std::get_if<float>(&value)) inheritActorVelocity_ = *v; }, 0.0f, 10.0f, 0.01f, true },

			{ "BurstEnabled", "BurstEnabled", ComponentPropertyType::Bool, [this]() -> ComponentPropertyValue { return burstEnabled_; }, [this](const ComponentPropertyValue& value) { if (const auto* v = std::get_if<bool>(&value)) burstEnabled_ = *v; } },
			{ "BurstCount", "BurstCount", ComponentPropertyType::Int, [this]() -> ComponentPropertyValue { return burstCount_; }, [this](const ComponentPropertyValue& value) { if (const auto* v = std::get_if<int>(&value)) burstCount_ = std::max(*v, 0); }, 0.0f, 100000.0f, 1.0f, true },
			{ "BurstRandom", "BurstRandom", ComponentPropertyType::Int, [this]() -> ComponentPropertyValue { return burstRandom_; }, [this](const ComponentPropertyValue& value) { if (const auto* v = std::get_if<int>(&value)) burstRandom_ = std::max(*v, 0); }, 0.0f, 100000.0f, 1.0f, true },
			{ "BurstRepeat", "BurstRepeat", ComponentPropertyType::Int, [this]() -> ComponentPropertyValue { return burstRepeat_; }, [this](const ComponentPropertyValue& value) { if (const auto* v = std::get_if<int>(&value)) burstRepeat_ = std::max(*v, 1); }, 1.0f, 1000.0f, 1.0f, true },
			{ "BurstInterval", "BurstInterval", ComponentPropertyType::Float, [this]() -> ComponentPropertyValue { return burstInterval_; }, [this](const ComponentPropertyValue& value) { if (const auto* v = std::get_if<float>(&value)) burstInterval_ = std::max(*v, 0.0f); }, 0.0f, 60.0f, 0.01f, true },
			{ "StartColor", "開始色", ComponentPropertyType::Vector4, [this]() -> ComponentPropertyValue { return startColor_; }, [this](const ComponentPropertyValue& value) { if (const auto* v = std::get_if<Vector4>(&value)) startColor_ = *v; }, 0.0f, 1.0f, 0.01f, true, {}, ComponentPropertyDisplay::Color },
			{ "MidColor", "MidColor", ComponentPropertyType::Vector4, [this]() -> ComponentPropertyValue { return midColor_; }, [this](const ComponentPropertyValue& value) { if (const auto* v = std::get_if<Vector4>(&value)) midColor_ = *v; }, 0.0f, 1.0f, 0.01f, true, {}, ComponentPropertyDisplay::Color },
			{ "EndColor", "終了色", ComponentPropertyType::Vector4, [this]() -> ComponentPropertyValue { return endColor_; }, [this](const ComponentPropertyValue& value) { if (const auto* v = std::get_if<Vector4>(&value)) endColor_ = *v; }, 0.0f, 1.0f, 0.01f, true, {}, ComponentPropertyDisplay::Color },
			{ "MidTime", "MidTime", ComponentPropertyType::Float, [this]() -> ComponentPropertyValue { return midTime_; }, [this](const ComponentPropertyValue& value) { if (const auto* v = std::get_if<float>(&value)) midTime_ = std::clamp(*v, 0.0f, 1.0f); }, 0.0f, 1.0f, 0.01f, true },
			{ "AlphaFade", "AlphaFade", ComponentPropertyType::Bool, [this]() -> ComponentPropertyValue { return alphaFade_; }, [this](const ComponentPropertyValue& value) { if (const auto* v = std::get_if<bool>(&value)) alphaFade_ = *v; } },
			{ "LifeTimeRandom", "LifeTimeRandom", ComponentPropertyType::Float, [this]() -> ComponentPropertyValue { return lifeTimeRandom_; }, [this](const ComponentPropertyValue& value) { if (const auto* v = std::get_if<float>(&value)) lifeTimeRandom_ = std::max(*v, 0.0f); }, 0.0f, 60.0f, 0.01f, true },

			{ "Acceleration", "Acceleration", ComponentPropertyType::Vector3, [this]() -> ComponentPropertyValue { return acceleration_; }, [this](const ComponentPropertyValue& value) { if (const auto* v = std::get_if<Vector3>(&value)) acceleration_ = *v; }, 0.0f, 0.0f, 0.01f },
			{ "Gravity", "Gravity", ComponentPropertyType::Vector3, [this]() -> ComponentPropertyValue { return gravity_; }, [this](const ComponentPropertyValue& value) { if (const auto* v = std::get_if<Vector3>(&value)) gravity_ = *v; }, 0.0f, 0.0f, 0.01f },
			{ "Damping", "Damping", ComponentPropertyType::Float, [this]() -> ComponentPropertyValue { return damping_; }, [this](const ComponentPropertyValue& value) { if (const auto* v = std::get_if<float>(&value)) damping_ = std::max(*v, 0.0f); }, 0.0f, 1000.0f, 0.01f, true },
			{ "TrailEnabled", "TrailEnabled", ComponentPropertyType::Bool, [this]() -> ComponentPropertyValue { return trailEnabled_; }, [this](const ComponentPropertyValue& value) { if (const auto* v = std::get_if<bool>(&value)) trailEnabled_ = *v; } },
			{ "TrailLength", "TrailLength", ComponentPropertyType::Float, [this]() -> ComponentPropertyValue { return trailLength_; }, [this](const ComponentPropertyValue& value) { if (const auto* v = std::get_if<float>(&value)) trailLength_ = std::max(*v, 0.0f); }, 0.0f, 100.0f, 0.01f, true },
			{ "TrailWidth", "TrailWidth", ComponentPropertyType::Float, [this]() -> ComponentPropertyValue { return trailWidth_; }, [this](const ComponentPropertyValue& value) { if (const auto* v = std::get_if<float>(&value)) trailWidth_ = std::max(*v, 0.0f); }, 0.0f, 100.0f, 0.01f, true },
			{ "TrailColor", "TrailColor", ComponentPropertyType::Vector4, [this]() -> ComponentPropertyValue { return trailColor_; }, [this](const ComponentPropertyValue& value) { if (const auto* v = std::get_if<Vector4>(&value)) trailColor_ = *v; }, 0.0f, 1.0f, 0.01f, true, {}, ComponentPropertyDisplay::Color },
			{ "TrailFadeTime", "TrailFadeTime", ComponentPropertyType::Float, [this]() -> ComponentPropertyValue { return trailFadeTime_; }, [this](const ComponentPropertyValue& value) { if (const auto* v = std::get_if<float>(&value)) trailFadeTime_ = std::max(*v, 0.0f); }, 0.0f, 60.0f, 0.01f, true },
			{ "TrailLifeTime", "TrailLifeTime", ComponentPropertyType::Float, [this]() -> ComponentPropertyValue { return trailLifeTime_; }, [this](const ComponentPropertyValue& value) { if (const auto* v = std::get_if<float>(&value)) trailLifeTime_ = std::max(*v, 0.0f); }, 0.0f, 60.0f, 0.01f, true },
			{ "TrailSegments", "TrailSegments", ComponentPropertyType::Int, [this]() -> ComponentPropertyValue { return trailSegments_; }, [this](const ComponentPropertyValue& value) { if (const auto* v = std::get_if<int>(&value)) trailSegments_ = std::max(*v, 1); }, 1.0f, 256.0f, 1.0f, true }
		};

		for (ComponentProperty& property : properties)
		{
			if (property.name == "EffectName") property.displayName = "エフェクト名";
			else if (property.name == "EmitterName") property.displayName = "エミッター名";
			else if (property.name == "PresetName") property.displayName = "プリセット名";
			else if (property.name == "ParticleType") property.displayName = "パーティクルタイプ";
			else if (property.name == "EmissionRate") property.displayName = "発生レート";
			else if (property.name == "LifeTime") property.displayName = "寿命";
			else if (property.name == "BillboardMode") property.displayName = "ビルボード方式";
			else if (property.name == "BlendMode") property.displayName = "ブレンド方式";
			else if (property.name == "SortMode") property.displayName = "ソート方式";
			else if (property.name == "StartSize2D") property.displayName = "開始サイズ";
			else if (property.name == "MidSize2D") property.displayName = "中間サイズ";
			else if (property.name == "EndSize2D") property.displayName = "終了サイズ";
			else if (property.name == "StartRotation") property.displayName = "開始回転";
			else if (property.name == "RotationSpeed") property.displayName = "回転速度";
			else if (property.name == "RotationRandom") property.displayName = "回転ランダム";
			else if (property.name == "UseSpriteSheet") property.displayName = "スプライトシート";
			else if (property.name == "SpriteSheetRows") property.displayName = "行数";
			else if (property.name == "SpriteSheetColumns") property.displayName = "列数";
			else if (property.name == "SpriteSheetFrameRate") property.displayName = "再生FPS";
			else if (property.name == "MeshId") property.displayName = "メッシュID";
			else if (property.name == "MeshStartScale") property.displayName = "開始スケール";
			else if (property.name == "MeshEndScale") property.displayName = "終了スケール";
			else if (property.name == "MeshScaleRandom") property.displayName = "スケールランダム";
			else if (property.name == "MeshAngularVelocity") property.displayName = "回転速度";
			else if (property.name == "MeshAngularVelocityRandom") property.displayName = "回転ランダム";
			else if (property.name == "MeshAlignMode") property.displayName = "向き合わせ";
			else if (property.name == "MeshTint") property.displayName = "色味";
			else if (property.name == "EmitterShape") property.displayName = "発生形状";
			else if (property.name == "SpawnRadius") property.displayName = "発生半径";
			else if (property.name == "SpawnBoxSize") property.displayName = "発生範囲";
			else if (property.name == "ConeAngle") property.displayName = "円錐角度";
			else if (property.name == "ConeLength") property.displayName = "円錐長さ";
			else if (property.name == "RingRadius") property.displayName = "リング半径";
			else if (property.name == "RingThickness") property.displayName = "リング太さ";
			else if (property.name == "BaseVelocity") property.displayName = "基本速度";
			else if (property.name == "VelocityRandom") property.displayName = "速度ランダム";
			else if (property.name == "Direction") property.displayName = "方向";
			else if (property.name == "Speed") property.displayName = "速度";
			else if (property.name == "SpeedRandom") property.displayName = "速度ランダム幅";
			else if (property.name == "RandomAngle") property.displayName = "方向ランダム角度";
			else if (property.name == "OutwardVelocity") property.displayName = "外向き速度";
			else if (property.name == "TangentialVelocity") property.displayName = "接線方向速度";
			else if (property.name == "InheritActorVelocity") property.displayName = "Actor速度を引き継ぐ";
			else if (property.name == "BurstEnabled") property.displayName = "バースト";
			else if (property.name == "BurstCount") property.displayName = "バースト数";
			else if (property.name == "BurstRandom") property.displayName = "バースト数ランダム";
			else if (property.name == "BurstRepeat") property.displayName = "バースト回数";
			else if (property.name == "BurstInterval") property.displayName = "バースト間隔";
			else if (property.name == "MidColor") property.displayName = "中間色";
			else if (property.name == "MidTime") property.displayName = "中間時間";
			else if (property.name == "AlphaFade") property.displayName = "アルファフェード";
			else if (property.name == "LifeTimeRandom") property.displayName = "寿命ランダム";
			else if (property.name == "Acceleration") property.displayName = "加速度";
			else if (property.name == "Gravity") property.displayName = "重力";
			else if (property.name == "Damping") property.displayName = "減衰";
			else if (property.name == "TrailEnabled") property.displayName = "軌跡";
			else if (property.name == "TrailLength") property.displayName = "軌跡の長さ";
			else if (property.name == "TrailWidth") property.displayName = "軌跡の太さ";
			else if (property.name == "TrailColor") property.displayName = "軌跡の色";
			else if (property.name == "TrailFadeTime") property.displayName = "軌跡フェード時間";
			else if (property.name == "TrailLifeTime") property.displayName = "軌跡寿命";
			else if (property.name == "TrailSegments") property.displayName = "軌跡分割数";
		}

		if (includeAssetPaths)
		{
			properties.push_back({ "TexturePath", "テクスチャ", ComponentPropertyType::String, [this]() -> ComponentPropertyValue { return texturePath_; }, [this](const ComponentPropertyValue& value) { if (const auto* v = std::get_if<std::string>(&value)) texturePath_ = *v; } });
			properties.push_back({ "MeshPath", "メッシュ", ComponentPropertyType::String, [this]() -> ComponentPropertyValue { return meshPath_; }, [this](const ComponentPropertyValue& value) { if (const auto* v = std::get_if<std::string>(&value)) meshPath_ = *v; } });
			properties.push_back({ "TrailTexturePath", "軌跡テクスチャ", ComponentPropertyType::String, [this]() -> ComponentPropertyValue { return trailTexturePath_; }, [this](const ComponentPropertyValue& value) { if (const auto* v = std::get_if<std::string>(&value)) trailTexturePath_ = *v; } });
		}

		return properties;
	}
}
