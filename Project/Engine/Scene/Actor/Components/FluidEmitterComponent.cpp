#include "FluidEmitterComponent.h"

#include "../../../Graphics/Renderer/GpuFluid/Data/GpuFluidEmitterTypes.h"
#include "../../../Graphics/Renderer/GpuFluid/Volumetric/Data/GpuVolumetricFluidEmitterTypes.h"
#include "../Core/Actor.h"
#include "../Serialization/ComponentFactory.h"

#include <algorithm>
#include <utility>

#ifdef USE_IMGUI
#include <imgui.h>
#endif // USE_IMGUI

namespace Ken4lowEngine
{
namespace
{
	constexpr const char* ToTargetDomainKey(FluidEmitterTargetDomain target)
	{
		switch (target)
		{
		case FluidEmitterTargetDomain::Volumetric3D: return "Volumetric3D";
		case FluidEmitterTargetDomain::Both: return "Both";
		case FluidEmitterTargetDomain::Fluid2D:
		default: return "Fluid2D";
		}
	}

	FluidEmitterTargetDomain ParseTargetDomain(const std::string& value)
	{
		if (value == "Volumetric3D") return FluidEmitterTargetDomain::Volumetric3D;
		if (value == "Both") return FluidEmitterTargetDomain::Both;
		return FluidEmitterTargetDomain::Fluid2D;
	}

	bool RegisterFluidEmitterComponentType()
	{
		ComponentFactory::ComponentTypeInfo typeInfo{};
		typeInfo.className = "FluidEmitterComponent";
		typeInfo.displayName = "Fluidエミッター";
		typeInfo.category = "演出";
		typeInfo.description = "Target Domainを明示して2Dまたは3D GPU Fluidへ速度・密度・温度を注入するSource Componentです。";
		typeInfo.allowMultiple = true;
		typeInfo.canBeRoot = true;
		typeInfo.createFunc = [](Actor* owner) -> ActorComponent*
		{
			return owner ? &owner->AddComponent<FluidEmitterComponent>() : nullptr;
		};
		typeInfo.createRootFunc = [](Actor* owner) -> SceneComponent*
		{
			return owner ? &owner->CreateRootComponent<FluidEmitterComponent>() : nullptr;
		};
		ComponentFactory::RegisterComponentType(std::move(typeInfo));
		return true;
	}

	// Factory本体の巨大な組み込み一覧を増やさず、Translation Unitの読込時にEditor/JSON生成へ自己登録する。
	[[maybe_unused]] const bool kFluidEmitterComponentRegistered = RegisterFluidEmitterComponentType();
}

void FluidEmitterComponent::DrawImGui()
{
	SceneComponent::DrawImGui();

#ifdef USE_IMGUI
	ImGui::SeparatorText("GPU Fluid Emitter");
	ImGui::TextDisabled("Target Domain decides whether this source feeds 2D Fluid, 3D Volumetric Fluid, or both.");
	ComponentPropertyUtility::DrawImGui(CreateProperties());
#endif // USE_IMGUI
}

void FluidEmitterComponent::ToJson(nlohmann::json& outJson) const
{
	SceneComponent::ToJson(outJson);
	outJson["Class"] = GetClassTypeName();
	ComponentPropertyUtility::ToJson(const_cast<FluidEmitterComponent*>(this)->CreateProperties(), outJson);
}

void FluidEmitterComponent::FromJson(const nlohmann::json& inJson)
{
	SceneComponent::FromJson(inJson);
	ComponentPropertyUtility::FromJson(CreateProperties(), inJson);
	Sanitize();
}

GpuFluidEmitterSource FluidEmitterComponent::BuildEmitterSource() const
{
	GpuFluidEmitterSource source{};
	source.worldPosition = GetWorldPosition();
	source.worldVelocity = sourceVelocity_;
	source.radius = radius_;
	source.velocityStrength = velocityStrength_;
	source.densityRate = densityRate_;
	source.temperatureRate = temperatureRate_;
	source.falloffExponent = falloffExponent_;
	source.enabled = emissionEnabled_ && IsActiveInHierarchy() && Targets2D();
	return source;
}

GpuVolumetricFluidEmitterSource FluidEmitterComponent::BuildVolumetricEmitterSource() const
{
	GpuVolumetricFluidEmitterSource source{};
	source.worldPosition = GetWorldPosition();
	source.worldVelocity = sourceVelocity_;
	source.radius = radius_;
	source.velocityStrength = velocityStrength_;
	source.densityRate = densityRate_;
	source.temperatureRate = temperatureRate_;
	source.falloffExponent = falloffExponent_;
	source.enabled = emissionEnabled_ && IsActiveInHierarchy() && TargetsVolumetric3D();
	return source;
}

void FluidEmitterComponent::SetRadius(float radius)
{
	radius_ = radius;
	Sanitize();
}

void FluidEmitterComponent::SetVelocityStrength(float strength)
{
	velocityStrength_ = strength;
	Sanitize();
}

void FluidEmitterComponent::SetFalloffExponent(float exponent)
{
	falloffExponent_ = exponent;
	Sanitize();
}

std::vector<ComponentProperty> FluidEmitterComponent::CreateProperties()
{
	return {
		ComponentProperty{
			"TargetDomain",
			"Target Domain",
			ComponentPropertyType::String,
			[this]() -> ComponentPropertyValue { return std::string(ToTargetDomainKey(targetDomain_)); },
			[this](const ComponentPropertyValue& value)
			{
				if (const std::string* typed = std::get_if<std::string>(&value))
				{
					targetDomain_ = ParseTargetDomain(*typed);
				}
			},
			0.0f, 0.0f, 0.1f, false,
			{
				{ "Fluid2D", "2D Fluid" },
				{ "Volumetric3D", "3D Volumetric" },
				{ "Both", "Both (2D + 3D)" },
			}
		},
		ComponentProperty{
			"EmissionEnabled",
			"Emission Enabled",
			ComponentPropertyType::Bool,
			[this]() -> ComponentPropertyValue { return emissionEnabled_; },
			[this](const ComponentPropertyValue& value)
			{
				if (const bool* typed = std::get_if<bool>(&value)) emissionEnabled_ = *typed;
			}
		},
		ComponentProperty{
			"Radius",
			"Radius",
			ComponentPropertyType::Float,
			[this]() -> ComponentPropertyValue { return radius_; },
			[this](const ComponentPropertyValue& value)
			{
				if (const float* typed = std::get_if<float>(&value))
				{
					radius_ = *typed;
					Sanitize();
				}
			},
			0.01f, 100.0f, 0.05f, true
		},
		ComponentProperty{
			"SourceVelocity",
			"Source Velocity (World)",
			ComponentPropertyType::Vector3,
			[this]() -> ComponentPropertyValue { return sourceVelocity_; },
			[this](const ComponentPropertyValue& value)
			{
				if (const Vector3* typed = std::get_if<Vector3>(&value))
				{
					sourceVelocity_ = *typed; // Domain軸への射影はRenderer側で行い、ComponentはWorld速度を保持する。
				}
			},
			-100.0f, 100.0f, 0.05f, true
		},
		ComponentProperty{
			"VelocityStrength",
			"Velocity Strength",
			ComponentPropertyType::Float,
			[this]() -> ComponentPropertyValue { return velocityStrength_; },
			[this](const ComponentPropertyValue& value)
			{
				if (const float* typed = std::get_if<float>(&value))
				{
					velocityStrength_ = *typed;
					Sanitize();
				}
			},
			0.0f, 100.0f, 0.05f, true
		},
		ComponentProperty{
			"DensityRate",
			"Density / sec",
			ComponentPropertyType::Float,
			[this]() -> ComponentPropertyValue { return densityRate_; },
			[this](const ComponentPropertyValue& value)
			{
				if (const float* typed = std::get_if<float>(&value)) densityRate_ = *typed;
			},
			-100.0f, 100.0f, 0.05f, true
		},
		ComponentProperty{
			"TemperatureRate",
			"Temperature / sec",
			ComponentPropertyType::Float,
			[this]() -> ComponentPropertyValue { return temperatureRate_; },
			[this](const ComponentPropertyValue& value)
			{
				if (const float* typed = std::get_if<float>(&value)) temperatureRate_ = *typed;
			},
			-100.0f, 100.0f, 0.05f, true
		},
		ComponentProperty{
			"FalloffExponent",
			"Falloff Exponent",
			ComponentPropertyType::Float,
			[this]() -> ComponentPropertyValue { return falloffExponent_; },
			[this](const ComponentPropertyValue& value)
			{
				if (const float* typed = std::get_if<float>(&value))
				{
					falloffExponent_ = *typed;
					Sanitize();
				}
			},
			0.1f, 16.0f, 0.05f, true
		}
	};
}

void FluidEmitterComponent::Sanitize()
{
	radius_ = std::max(radius_, 0.01f);
	velocityStrength_ = std::max(velocityStrength_, 0.0f);
	falloffExponent_ = std::max(falloffExponent_, 0.1f);
}

} // namespace Ken4lowEngine
