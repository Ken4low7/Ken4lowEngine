#include "BossActorAttackComponent.h"

#include <Scene/Actor/Character/AttackBehaviors.h>

#include <array>
#include <sstream>
#include <span>
#include <string>
#include <string_view>
#include <utility>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace Ken4lowEngine
{
	namespace
	{
		using BossAttackIdList = std::span<const BossAttackId>;

		struct BossAttackSelectionRule
		{
			BossAttackProfile profile;
			BossPhase phase;
			float middleDistance;
			float farDistance;
			BossAttackIdList nearCandidates;
			BossAttackIdList middleCandidates;
			BossAttackIdList farCandidates;
		};

		constexpr std::array<BossAttackId, 2> kGuardianPhase1Near{
			BossAttackId::HeavyPunch,
			BossAttackId::Punch
		};
		constexpr std::array<BossAttackId, 1> kGuardianPhase1Far{
			BossAttackId::Charge
		};

		constexpr std::array<BossAttackId, 3> kGuardianPhase2Near{
			BossAttackId::GroundSlam,
			BossAttackId::HeavyPunch,
			BossAttackId::Punch
		};
		constexpr std::array<BossAttackId, 5> kGuardianPhase2Middle{
			BossAttackId::Shockwave,
			BossAttackId::GroundSlam,
			BossAttackId::HeavyPunch,
			BossAttackId::Punch,
			BossAttackId::Charge
		};
		constexpr std::array<BossAttackId, 3> kGuardianPhase2Far{
			BossAttackId::Charge,
			BossAttackId::Shockwave,
			BossAttackId::GroundSlam
		};

		constexpr std::array<BossAttackId, 4> kGuardianPhase3Near{
			BossAttackId::RapidPunch,
			BossAttackId::GroundSlam,
			BossAttackId::Punch,
			BossAttackId::HeavyPunch
		};
		constexpr std::array<BossAttackId, 6> kGuardianPhase3Middle{
			BossAttackId::FastShockwave,
			BossAttackId::RapidPunch,
			BossAttackId::GroundSlam,
			BossAttackId::FrenzyCharge,
			BossAttackId::HeavyPunch,
			BossAttackId::Punch
		};
		constexpr std::array<BossAttackId, 4> kGuardianPhase3Far{
			BossAttackId::FrenzyCharge,
			BossAttackId::FastShockwave,
			BossAttackId::Charge,
			BossAttackId::Shockwave
		};

		constexpr std::array<BossAttackId, 3> kMineCrusherPhase1Near{
			BossAttackId::HeavyPunch,
			BossAttackId::Punch,
			BossAttackId::GroundSlam
		};
		constexpr std::array<BossAttackId, 2> kMineCrusherPhase1Far{
			BossAttackId::Charge,
			BossAttackId::Shockwave
		};

		constexpr std::array<BossAttackId, 3> kMineCrusherPhase2Near{
			BossAttackId::GroundSlam,
			BossAttackId::HeavyPunch,
			BossAttackId::Punch
		};
		constexpr std::array<BossAttackId, 5> kMineCrusherPhase2Middle{
			BossAttackId::GroundSlam,
			BossAttackId::Shockwave,
			BossAttackId::HeavyPunch,
			BossAttackId::Charge,
			BossAttackId::Punch
		};
		constexpr std::array<BossAttackId, 3> kMineCrusherPhase2Far{
			BossAttackId::Shockwave,
			BossAttackId::Charge,
			BossAttackId::GroundSlam
		};

		constexpr std::array<BossAttackId, 4> kMineCrusherPhase3Near{
			BossAttackId::GroundSlam,
			BossAttackId::RapidPunch,
			BossAttackId::HeavyPunch,
			BossAttackId::Punch
		};
		constexpr std::array<BossAttackId, 5> kMineCrusherPhase3Middle{
			BossAttackId::FastShockwave,
			BossAttackId::GroundSlam,
			BossAttackId::Shockwave,
			BossAttackId::HeavyPunch,
			BossAttackId::FrenzyCharge
		};
		constexpr std::array<BossAttackId, 4> kMineCrusherPhase3Far{
			BossAttackId::FrenzyCharge,
			BossAttackId::FastShockwave,
			BossAttackId::Shockwave,
			BossAttackId::Charge
		};

		constexpr std::array<BossAttackSelectionRule, 6> kAttackSelectionRules{{
			{
				BossAttackProfile::Guardian,
				BossPhase::Phase1,
				3.4f,
				3.4f,
				BossAttackIdList{ kGuardianPhase1Near },
				BossAttackIdList{ kGuardianPhase1Near },
				BossAttackIdList{ kGuardianPhase1Far }
			},
			{
				BossAttackProfile::Guardian,
				BossPhase::Phase2,
				2.2f,
				4.5f,
				BossAttackIdList{ kGuardianPhase2Near },
				BossAttackIdList{ kGuardianPhase2Middle },
				BossAttackIdList{ kGuardianPhase2Far }
			},
			{
				BossAttackProfile::Guardian,
				BossPhase::Phase3,
				2.2f,
				4.5f,
				BossAttackIdList{ kGuardianPhase3Near },
				BossAttackIdList{ kGuardianPhase3Middle },
				BossAttackIdList{ kGuardianPhase3Far }
			},
			{
				BossAttackProfile::MineCrusher,
				BossPhase::Phase1,
				5.0f,
				5.0f,
				BossAttackIdList{ kMineCrusherPhase1Near },
				BossAttackIdList{ kMineCrusherPhase1Near },
				BossAttackIdList{ kMineCrusherPhase1Far }
			},
			{
				BossAttackProfile::MineCrusher,
				BossPhase::Phase2,
				3.0f,
				6.0f,
				BossAttackIdList{ kMineCrusherPhase2Near },
				BossAttackIdList{ kMineCrusherPhase2Middle },
				BossAttackIdList{ kMineCrusherPhase2Far }
			},
			{
				BossAttackProfile::MineCrusher,
				BossPhase::Phase3,
				3.0f,
				6.0f,
				BossAttackIdList{ kMineCrusherPhase3Near },
				BossAttackIdList{ kMineCrusherPhase3Middle },
				BossAttackIdList{ kMineCrusherPhase3Far }
			}
		}};

		constexpr const BossAttackSelectionRule* FindSelectionRule(BossAttackProfile profile, BossPhase phase)
		{
			for (const BossAttackSelectionRule& rule : kAttackSelectionRules)
			{
				if (rule.profile == profile && rule.phase == phase) return &rule;
			}

			return nullptr;
		}

		constexpr BossDistanceBand ClassifyDistance(const BossAttackSelectionRule& rule, float distanceToTarget)
		{
			if (distanceToTarget > rule.farDistance) return BossDistanceBand::Far;
			if (distanceToTarget > rule.middleDistance) return BossDistanceBand::Middle;
			return BossDistanceBand::Near;
		}

		constexpr BossAttackIdList GetCandidates(const BossAttackSelectionRule& rule, BossDistanceBand distanceBand)
		{
			switch (distanceBand)
			{
			case BossDistanceBand::Far:
				return rule.farCandidates;
			case BossDistanceBand::Middle:
				return rule.middleCandidates;
			case BossDistanceBand::Near:
			default:
				return rule.nearCandidates;
			}
		}

		std::string ToAttackKey(BossAttackId attackId)
		{
			return std::string(ToString(attackId));
		}
	}

	void BossAttackComponent::Initialize()
	{
		RegisterDefaultAttacks();
		AttackComponent::Initialize();
	}

	void BossAttackComponent::DrawImGui()
	{
		AttackComponent::DrawImGui();
#ifdef USE_IMGUI
		ImGui::Text("Bossプロファイル: %s", GetAttackProfileName().data());
		ImGui::Text("Boss選択: %s", GetLastSelectedAttackName().data());
#endif
	}

	void BossAttackComponent::ToJson(nlohmann::json& outJson) const
	{
		AttackComponent::ToJson(outJson);
		outJson["AttackProfile"] = ToString(attackProfile_);
	}

	void BossAttackComponent::FromJson(const nlohmann::json& inJson)
	{
		AttackComponent::FromJson(inJson);
		const std::string profileName = inJson.value("AttackProfile", std::string(ToString(attackProfile_)));
		attackProfile_ = BossAttackProfileFromString(profileName); // Prefab文字列は読込境界で列挙型へ変換する。
	}

	bool BossAttackComponent::TryStartBestAttack(float distanceToTarget, BossPhase bossPhase)
	{
		const BossAttackSelectionRule* rule = FindSelectionRule(attackProfile_, bossPhase);
		if (!rule) return false;

		const BossDistanceBand distanceBand = ClassifyDistance(*rule, distanceToTarget);
		const BossAttackIdList candidates = GetCandidates(*rule, distanceBand);

		for (const BossAttackId attackId : candidates)
		{
			if (!StartAttack(ToString(attackId))) continue;
			lastSelectedAttackId_ = attackId;
			return true;
		}

		return false;
	}

	bool BossAttackComponent::ValidateSelectionRules(std::string& outSummary) const
	{
		constexpr std::array<BossPhase, 3> phases{
			BossPhase::Phase1,
			BossPhase::Phase2,
			BossPhase::Phase3
		};
		constexpr std::array<BossDistanceBand, 3> distanceBands{
			BossDistanceBand::Near,
			BossDistanceBand::Middle,
			BossDistanceBand::Far
		};

		bool valid = true;
		std::ostringstream summary;
		summary << GetAttackProfileName() << " ";

		for (const BossPhase phase : phases)
		{
			const BossAttackSelectionRule* rule = FindSelectionRule(attackProfile_, phase);
			if (!rule)
			{
				valid = false;
				summary << "P" << ToInt(phase) << "=RuleMissing ";
				continue;
			}

			for (const BossDistanceBand distanceBand : distanceBands)
			{
				const BossAttackIdList candidates = GetCandidates(*rule, distanceBand);
				summary << "P" << ToInt(phase) << "/" << ToString(distanceBand) << "=";
				if (candidates.empty())
				{
					valid = false;
					summary << "Empty ";
					continue;
				}

				summary << ToString(candidates.front());
				for (const BossAttackId attackId : candidates)
				{
					if (FindAttackData(ToString(attackId))) continue;
					valid = false;
					summary << "(Missing:" << ToString(attackId) << ")";
				}
				summary << " ";
			}
		}

		outSummary = summary.str(); // DebugSceneで全組み合わせの先頭候補と登録漏れを一度に確認できる形へまとめる。
		return valid;
	}

	void BossAttackComponent::RegisterDefaultAttacks()
	{
		switch (attackProfile_)
		{
		case BossAttackProfile::MineCrusher:
			RegisterMineCrusherAttacks();
			return;
		case BossAttackProfile::Guardian:
		default:
			RegisterGuardianAttacks();
			return;
		}
	}

	void BossAttackComponent::RegisterGuardianAttacks()
	{
		auto upsertAttack = [this](AttackData data)
		{
			const std::string attackId = data.id;
			if (FindAttackData(attackId))
			{
				ConfigureAttack(attackId, data);
				return;
			}
			const std::string behaviorType = data.behaviorType;
			RegisterAttack(std::move(data), CreateAttackBehavior(behaviorType));
		};

		AttackData punch{ ToAttackKey(BossAttackId::Punch), "Melee", "Attack.Melee", 24.0f, 0.95f, 0.17f, 0.10f, 0.24f, 0.0f, 3.2f, 0.0f };
		punch.maxHeightDifference = 3.0f;
		upsertAttack(std::move(punch));

		AttackData heavyPunch{ ToAttackKey(BossAttackId::HeavyPunch), "Melee", "Attack.Melee", 42.0f, 1.65f, 0.32f, 0.13f, 0.36f, 0.0f, 4.0f, 0.0f };
		heavyPunch.maxHeightDifference = 3.5f;
		upsertAttack(std::move(heavyPunch));

		AttackData charge{ ToAttackKey(BossAttackId::Charge), "Charge", "Attack.Charge", 36.0f, 2.35f, 0.28f, 0.82f, 0.24f, 3.2f, 18.0f, 18.0f };
		charge.maxHeightDifference = 2.5f;
		upsertAttack(std::move(charge));

		AttackData shockwave{ ToAttackKey(BossAttackId::Shockwave), "Shockwave", "Attack.Shockwave", 44.0f, 3.15f, 0.54f, 0.10f, 0.38f, 2.0f, 10.0f, 0.0f };
		shockwave.maxHeightDifference = 2.0f;
		upsertAttack(std::move(shockwave));

		AttackData groundSlam{ ToAttackKey(BossAttackId::GroundSlam), "Shockwave", "Attack.Shockwave", 52.0f, 3.75f, 0.62f, 0.11f, 0.45f, 0.0f, 5.8f, 0.0f };
		groundSlam.maxHeightDifference = 2.3f;
		upsertAttack(std::move(groundSlam));

		AttackData rapidPunch{ ToAttackKey(BossAttackId::RapidPunch), "Melee", "Attack.Melee", 30.0f, 0.52f, 0.11f, 0.08f, 0.14f, 0.0f, 3.4f, 0.0f };
		rapidPunch.maxHeightDifference = 3.0f;
		upsertAttack(std::move(rapidPunch));

		AttackData frenzyCharge{ ToAttackKey(BossAttackId::FrenzyCharge), "Charge", "Attack.Charge", 42.0f, 1.55f, 0.16f, 0.78f, 0.13f, 3.0f, 22.0f, 24.0f };
		frenzyCharge.maxHeightDifference = 2.5f;
		upsertAttack(std::move(frenzyCharge));

		AttackData fastShockwave{ ToAttackKey(BossAttackId::FastShockwave), "Shockwave", "Attack.Shockwave", 48.0f, 1.90f, 0.29f, 0.08f, 0.21f, 1.5f, 10.5f, 0.0f };
		fastShockwave.maxHeightDifference = 2.0f;
		upsertAttack(std::move(fastShockwave));
	}

	void BossAttackComponent::RegisterMineCrusherAttacks()
	{
		auto upsertAttack = [this](AttackData data)
		{
			const std::string attackId = data.id;
			if (FindAttackData(attackId))
			{
				ConfigureAttack(attackId, data);
				return;
			}
			const std::string behaviorType = data.behaviorType;
			RegisterAttack(std::move(data), CreateAttackBehavior(behaviorType));
		};

		AttackData crusherClaw{ ToAttackKey(BossAttackId::Punch), "Melee", "Attack.Melee", 32.0f, 1.10f, 0.24f, 0.14f, 0.30f, 0.0f, 4.2f, 0.0f };
		crusherClaw.maxHeightDifference = 4.0f;
		upsertAttack(std::move(crusherClaw));

		AttackData drillHammer{ ToAttackKey(BossAttackId::HeavyPunch), "Melee", "Attack.Melee", 48.0f, 1.85f, 0.44f, 0.16f, 0.42f, 0.0f, 5.0f, 0.0f };
		drillHammer.maxHeightDifference = 4.5f;
		upsertAttack(std::move(drillHammer));

		AttackData tunnelCharge{ ToAttackKey(BossAttackId::Charge), "Charge", "Attack.Charge", 44.0f, 2.70f, 0.50f, 1.00f, 0.35f, 4.0f, 20.0f, 14.0f };
		tunnelCharge.maxHeightDifference = 4.0f;
		upsertAttack(std::move(tunnelCharge));

		AttackData oreBurst{ ToAttackKey(BossAttackId::Shockwave), "Shockwave", "Attack.Shockwave", 38.0f, 2.40f, 0.70f, 0.12f, 0.42f, 2.5f, 12.0f, 0.0f };
		oreBurst.maxHeightDifference = 5.0f;
		upsertAttack(std::move(oreBurst));

		AttackData caveIn{ ToAttackKey(BossAttackId::GroundSlam), "Shockwave", "Attack.Shockwave", 56.0f, 4.20f, 0.95f, 0.16f, 0.55f, 0.0f, 8.0f, 0.0f };
		caveIn.maxHeightDifference = 5.0f;
		upsertAttack(std::move(caveIn));

		AttackData crusherCombo{ ToAttackKey(BossAttackId::RapidPunch), "Melee", "Attack.Melee", 34.0f, 0.68f, 0.16f, 0.10f, 0.18f, 0.0f, 4.5f, 0.0f };
		crusherCombo.maxHeightDifference = 4.0f;
		upsertAttack(std::move(crusherCombo));

		AttackData tunnelRush{ ToAttackKey(BossAttackId::FrenzyCharge), "Charge", "Attack.Charge", 48.0f, 1.90f, 0.30f, 0.92f, 0.20f, 3.5f, 23.0f, 18.0f };
		tunnelRush.maxHeightDifference = 4.5f;
		upsertAttack(std::move(tunnelRush));

		AttackData rockBurst{ ToAttackKey(BossAttackId::FastShockwave), "Shockwave", "Attack.Shockwave", 42.0f, 1.55f, 0.38f, 0.10f, 0.24f, 1.5f, 12.5f, 0.0f };
		rockBurst.maxHeightDifference = 5.0f;
		upsertAttack(std::move(rockBurst)); // 坑道ボスは高低差を許容する範囲攻撃を多用し、立体Arenaでも待機状態になりにくくする。
	}
} // namespace Ken4lowEngine
