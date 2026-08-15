#pragma once

#include "SceneComponent.h"
#include "ComponentProperty.h"
#include "Vector3.h"

#include <string>
#include <vector>

namespace Ken4lowEngine
{

struct GpuFluidEmitterSource;

/// -------------------------------------------------------------
/// ActorからGPU FluidへVelocity/Density/Temperature Sourceを供給するComponent。
/// -------------------------------------------------------------
class FluidEmitterComponent : public SceneComponent
{
public:
	void DrawImGui() override;

	std::string GetClassTypeName() const override
	{
		return "FluidEmitterComponent";
	}

	void ToJson(nlohmann::json& outJson) const override;
	void FromJson(const nlohmann::json& inJson) override;

	/// Renderer側へScene依存を漏らさず、現在のWorld位置とSource設定だけを値として渡す。
	[[nodiscard]] GpuFluidEmitterSource BuildEmitterSource() const;

	[[nodiscard]] bool IsEmissionEnabled() const { return emissionEnabled_; }
	void SetEmissionEnabled(bool enabled) { emissionEnabled_ = enabled; }

	[[nodiscard]] float GetRadius() const { return radius_; }
	void SetRadius(float radius);

	[[nodiscard]] const Vector3& GetSourceVelocity() const { return sourceVelocity_; }
	void SetSourceVelocity(const Vector3& velocity) { sourceVelocity_ = velocity; }

	[[nodiscard]] float GetVelocityStrength() const { return velocityStrength_; }
	void SetVelocityStrength(float strength);

	[[nodiscard]] float GetDensityRate() const { return densityRate_; }
	void SetDensityRate(float rate) { densityRate_ = rate; }

	[[nodiscard]] float GetTemperatureRate() const { return temperatureRate_; }
	void SetTemperatureRate(float rate) { temperatureRate_ = rate; }

	[[nodiscard]] float GetFalloffExponent() const { return falloffExponent_; }
	void SetFalloffExponent(float exponent);

	std::vector<ComponentProperty> CreateProperties();

private:
	void Sanitize();

private:
	bool emissionEnabled_ = true;
	float radius_ = 0.5f;
	Vector3 sourceVelocity_{ 0.0f, 1.0f, 0.0f };
	float velocityStrength_ = 1.0f;
	float densityRate_ = 1.0f;
	float temperatureRate_ = 1.0f;
	float falloffExponent_ = 2.0f;
};

} // namespace Ken4lowEngine
