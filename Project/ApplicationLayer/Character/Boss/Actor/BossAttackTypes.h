#pragma once

#include <string_view>

namespace Ken4lowEngine
{
	enum class BossAttackProfile
	{
		Guardian,
		MineCrusher
	};

	enum class BossAttackId
	{
		None,
		Punch,
		HeavyPunch,
		Charge,
		Shockwave,
		GroundSlam,
		RapidPunch,
		FrenzyCharge,
		FastShockwave
	};

	enum class BossPhase
	{
		Phase1 = 1,
		Phase2 = 2,
		Phase3 = 3
	};

	enum class BossDistanceBand
	{
		Near,
		Middle,
		Far
	};

	constexpr int ToInt(BossPhase phase)
	{
		return static_cast<int>(phase);
	}

	constexpr BossPhase ToBossPhase(int phase)
	{
		if (phase <= ToInt(BossPhase::Phase1)) return BossPhase::Phase1;
		if (phase == ToInt(BossPhase::Phase2)) return BossPhase::Phase2;
		return BossPhase::Phase3;
	}

	constexpr std::string_view ToString(BossAttackProfile profile)
	{
		switch (profile)
		{
		case BossAttackProfile::MineCrusher:
			return "MineCrusher";
		case BossAttackProfile::Guardian:
		default:
			return "Guardian";
		}
	}

	constexpr BossAttackProfile BossAttackProfileFromString(std::string_view value)
	{
		// JSON境界だけで文字列を解釈し、実行時の攻撃選択は列挙型で統一する。
		return value == ToString(BossAttackProfile::MineCrusher)
			? BossAttackProfile::MineCrusher
			: BossAttackProfile::Guardian;
	}

	constexpr std::string_view ToString(BossAttackId attackId)
	{
		switch (attackId)
		{
		case BossAttackId::Punch:
			return "Punch";
		case BossAttackId::HeavyPunch:
			return "HeavyPunch";
		case BossAttackId::Charge:
			return "Charge";
		case BossAttackId::Shockwave:
			return "Shockwave";
		case BossAttackId::GroundSlam:
			return "GroundSlam";
		case BossAttackId::RapidPunch:
			return "RapidPunch";
		case BossAttackId::FrenzyCharge:
			return "FrenzyCharge";
		case BossAttackId::FastShockwave:
			return "FastShockwave";
		case BossAttackId::None:
		default:
			return "None";
		}
	}

	constexpr std::string_view ToString(BossDistanceBand distanceBand)
	{
		switch (distanceBand)
		{
		case BossDistanceBand::Middle:
			return "Middle";
		case BossDistanceBand::Far:
			return "Far";
		case BossDistanceBand::Near:
		default:
			return "Near";
		}
	}
} // namespace Ken4lowEngine
