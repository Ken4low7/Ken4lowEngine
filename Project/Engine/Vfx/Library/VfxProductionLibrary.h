#pragma once

#include "Engine/Vfx/Graph/Runtime/VfxGraphRuntime.h"

#include <array>
#include <cstdint>
#include <string_view>

namespace Ken4lowEngine
{

enum class VfxProductionCategory : uint32_t
{
	Combat = 0u,
	Elemental,
	Magic,
	Environment,
};

enum class VfxProductionCostClass : uint32_t
{
	Low = 0u,
	Medium,
	High,
};

struct VfxProductionLibraryEntry
{
	const char* id = "";
	const char* graphName = "";
	const char* assetPath = "";
	VfxProductionCategory category = VfxProductionCategory::Combat;
	VfxProductionCostClass costClass = VfxProductionCostClass::Low;
	bool loop = false;
	const char* tags = "";
};

struct VfxProductionLibraryLoadResult
{
	uint32_t requested = 0u;
	uint32_t loaded = 0u;
	uint32_t failed = 0u;
};

/// <summary>
/// Phase29 production catalog. Assets are loaded through the existing VfxGraphRuntime rather than a second runtime/backend.
/// </summary>
class VfxProductionLibrary
{
public:
	inline static constexpr std::array<VfxProductionLibraryEntry, 10> kEntries{
		VfxProductionLibraryEntry{ "combat.impact.spark", "ProdCombatImpactSpark", "Resources/VfxGraph/Production/Combat/ImpactSpark.vfxgraph.json", VfxProductionCategory::Combat, VfxProductionCostClass::Low, false, "impact,spark,hit,collision" },
		VfxProductionLibraryEntry{ "combat.muzzle.flash", "ProdCombatMuzzleFlash", "Resources/VfxGraph/Production/Combat/MuzzleFlash.vfxgraph.json", VfxProductionCategory::Combat, VfxProductionCostClass::Low, false, "weapon,muzzle,flash,light" },
		VfxProductionLibraryEntry{ "combat.explosion", "ProdCombatExplosion", "Resources/VfxGraph/Production/Combat/Explosion.vfxgraph.json", VfxProductionCategory::Combat, VfxProductionCostClass::High, false, "explosion,fluid,light,posteffect" },
		VfxProductionLibraryEntry{ "combat.debris", "ProdCombatDebris", "Resources/VfxGraph/Production/Combat/Debris.vfxgraph.json", VfxProductionCategory::Combat, VfxProductionCostClass::Medium, false, "debris,mesh,impact" },
		VfxProductionLibraryEntry{ "elemental.fire.burst", "ProdElementalFireBurst", "Resources/VfxGraph/Production/Elemental/FireBurst.vfxgraph.json", VfxProductionCategory::Elemental, VfxProductionCostClass::Medium, false, "fire,burst,light,fluid" },
		VfxProductionLibraryEntry{ "elemental.frost.burst", "ProdElementalFrostBurst", "Resources/VfxGraph/Production/Elemental/FrostBurst.vfxgraph.json", VfxProductionCategory::Elemental, VfxProductionCostClass::Low, false, "frost,ice,burst" },
		VfxProductionLibraryEntry{ "magic.arcane.ribbon", "ProdMagicArcaneRibbon", "Resources/VfxGraph/Production/Magic/ArcaneRibbon.vfxgraph.json", VfxProductionCategory::Magic, VfxProductionCostClass::Medium, true, "magic,arcane,ribbon,loop" },
		VfxProductionLibraryEntry{ "magic.energy.trail", "ProdMagicEnergyTrail", "Resources/VfxGraph/Production/Magic/EnergyTrail.vfxgraph.json", VfxProductionCategory::Magic, VfxProductionCostClass::Medium, true, "magic,energy,trail,loop" },
		VfxProductionLibraryEntry{ "environment.smoke.plume", "ProdEnvironmentSmokePlume", "Resources/VfxGraph/Production/Environment/SmokePlume.vfxgraph.json", VfxProductionCategory::Environment, VfxProductionCostClass::High, true, "smoke,environment,fluid,loop" },
		VfxProductionLibraryEntry{ "environment.ember.field", "ProdEnvironmentEmberField", "Resources/VfxGraph/Production/Environment/EmberField.vfxgraph.json", VfxProductionCategory::Environment, VfxProductionCostClass::Medium, true, "ember,ambient,environment,loop" },
	};

	static const std::array<VfxProductionLibraryEntry, 10>& GetEntries()
	{
		return kEntries;
	}

	static const VfxProductionLibraryEntry* Find(std::string_view id)
	{
		for (const VfxProductionLibraryEntry& entry : kEntries)
		{
			if (id == entry.id)
			{
				return &entry;
			}
		}
		return nullptr;
	}

	static bool Load(std::string_view id)
	{
		const VfxProductionLibraryEntry* entry = Find(id);
		if (entry == nullptr)
		{
			return false;
		}
		return VfxGraphRuntime::GetInstance()->LoadGraph(entry->assetPath); // Keep production assets on the existing compiler/runtime path.
	}

	static VfxProductionLibraryLoadResult LoadAll()
	{
		VfxProductionLibraryLoadResult result{};
		for (const VfxProductionLibraryEntry& entry : kEntries)
		{
			++result.requested;
			if (VfxGraphRuntime::GetInstance()->LoadGraph(entry.assetPath))
			{
				++result.loaded;
			}
			else
			{
				++result.failed;
			}
		}
		return result;
	}

	static VfxProductionLibraryLoadResult LoadCategory(VfxProductionCategory category)
	{
		VfxProductionLibraryLoadResult result{};
		for (const VfxProductionLibraryEntry& entry : kEntries)
		{
			if (entry.category != category)
			{
				continue;
			}

			++result.requested;
			if (VfxGraphRuntime::GetInstance()->LoadGraph(entry.assetPath))
			{
				++result.loaded;
			}
			else
			{
				++result.failed;
			}
		}
		return result;
	}
};

} // namespace Ken4lowEngine
