#include "VfxGraphTypes.h"

#include <algorithm>

namespace Ken4lowEngine
{
namespace
{
	float ApplyCurveInterpolation(VfxCurveInterpolation interpolation, float t)
	{
		const float clamped = std::clamp(t, 0.0f, 1.0f);
		switch (interpolation)
		{
		case VfxCurveInterpolation::Step:
			return clamped >= 1.0f ? 1.0f : 0.0f;
		case VfxCurveInterpolation::SmoothStep:
			return clamped * clamped * (3.0f - 2.0f * clamped);
		case VfxCurveInterpolation::Linear:
		default:
			return clamped;
		}
	}

	Vector4 LerpColor(const Vector4& a, const Vector4& b, float t)
	{
		return {
			a.x + (b.x - a.x) * t,
			a.y + (b.y - a.y) * t,
			a.z + (b.z - a.z) * t,
			a.w + (b.w - a.w) * t,
		};
	}
}

float VfxFloatCurve::Evaluate(float normalizedTime) const
{
	if (keys.empty()) return 1.0f;
	if (keys.size() == 1u || normalizedTime <= keys.front().time) return keys.front().value;

	for (size_t i = 1u; i < keys.size(); ++i)
	{
		const VfxFloatCurveKey& previous = keys[i - 1u];
		const VfxFloatCurveKey& next = keys[i];
		if (normalizedTime > next.time) continue;

		const float span = next.time - previous.time;
		if (span <= 1.0e-6f) return next.value;
		const float localT = ApplyCurveInterpolation(interpolation, (normalizedTime - previous.time) / span);
		return previous.value + (next.value - previous.value) * localT;
	}
	return keys.back().value;
}

Vector4 VfxColorGradient::Evaluate(float normalizedTime) const
{
	if (keys.empty()) return { 1.0f, 1.0f, 1.0f, 1.0f };
	if (keys.size() == 1u || normalizedTime <= keys.front().time) return keys.front().color;

	for (size_t i = 1u; i < keys.size(); ++i)
	{
		const VfxColorGradientKey& previous = keys[i - 1u];
		const VfxColorGradientKey& next = keys[i];
		if (normalizedTime > next.time) continue;

		const float span = next.time - previous.time;
		if (span <= 1.0e-6f) return next.color;
		const float localT = ApplyCurveInterpolation(interpolation, (normalizedTime - previous.time) / span);
		return LerpColor(previous.color, next.color, localT);
	}
	return keys.back().color;
}

VfxGraphNodeStage GetExpectedVfxGraphNodeStage(VfxGraphNodeType type)
{
	switch (type)
	{
	case VfxGraphNodeType::SpawnRate:
	case VfxGraphNodeType::Burst:
		return VfxGraphNodeStage::Spawn;
	case VfxGraphNodeType::SpawnPoint:
	case VfxGraphNodeType::SpawnSphere:
	case VfxGraphNodeType::SpawnBox:
	case VfxGraphNodeType::Lifetime:
	case VfxGraphNodeType::InitialVelocity:
	case VfxGraphNodeType::InitialColor:
	case VfxGraphNodeType::InitialSize:
	case VfxGraphNodeType::InitialRotation:
		return VfxGraphNodeStage::Initialize;
	case VfxGraphNodeType::Gravity:
	case VfxGraphNodeType::Drag:
	case VfxGraphNodeType::RotationRate:
	case VfxGraphNodeType::SizeOverLife:
	case VfxGraphNodeType::ColorOverLife:
		return VfxGraphNodeStage::Update;
	case VfxGraphNodeType::SpriteRenderer:
		return VfxGraphNodeStage::Render;
	default:
		return VfxGraphNodeStage::Spawn;
	}
}

const char* ToString(VfxGraphNodeStage stage)
{
	switch (stage)
	{
	case VfxGraphNodeStage::Spawn: return "Spawn";
	case VfxGraphNodeStage::Initialize: return "Initialize";
	case VfxGraphNodeStage::Update: return "Update";
	case VfxGraphNodeStage::Render: return "Render";
	default: return "Spawn";
	}
}

const char* ToString(VfxGraphNodeType type)
{
	switch (type)
	{
	case VfxGraphNodeType::SpawnRate: return "SpawnRate";
	case VfxGraphNodeType::Burst: return "Burst";
	case VfxGraphNodeType::SpawnPoint: return "SpawnPoint";
	case VfxGraphNodeType::SpawnSphere: return "SpawnSphere";
	case VfxGraphNodeType::SpawnBox: return "SpawnBox";
	case VfxGraphNodeType::Lifetime: return "Lifetime";
	case VfxGraphNodeType::InitialVelocity: return "InitialVelocity";
	case VfxGraphNodeType::InitialColor: return "InitialColor";
	case VfxGraphNodeType::InitialSize: return "InitialSize";
	case VfxGraphNodeType::Gravity: return "Gravity";
	case VfxGraphNodeType::Drag: return "Drag";
	case VfxGraphNodeType::SpriteRenderer: return "SpriteRenderer";
	case VfxGraphNodeType::InitialRotation: return "InitialRotation";
	case VfxGraphNodeType::RotationRate: return "RotationRate";
	case VfxGraphNodeType::SizeOverLife: return "SizeOverLife";
	case VfxGraphNodeType::ColorOverLife: return "ColorOverLife";
	default: return "SpawnRate";
	}
}

bool TryParseVfxGraphNodeStage(const std::string& text, VfxGraphNodeStage& outStage)
{
	if (text == "Spawn") outStage = VfxGraphNodeStage::Spawn;
	else if (text == "Initialize") outStage = VfxGraphNodeStage::Initialize;
	else if (text == "Update") outStage = VfxGraphNodeStage::Update;
	else if (text == "Render") outStage = VfxGraphNodeStage::Render;
	else return false;
	return true;
}

bool TryParseVfxGraphNodeType(const std::string& text, VfxGraphNodeType& outType)
{
	if (text == "SpawnRate") outType = VfxGraphNodeType::SpawnRate;
	else if (text == "Burst") outType = VfxGraphNodeType::Burst;
	else if (text == "SpawnPoint") outType = VfxGraphNodeType::SpawnPoint;
	else if (text == "SpawnSphere") outType = VfxGraphNodeType::SpawnSphere;
	else if (text == "SpawnBox") outType = VfxGraphNodeType::SpawnBox;
	else if (text == "Lifetime") outType = VfxGraphNodeType::Lifetime;
	else if (text == "InitialVelocity") outType = VfxGraphNodeType::InitialVelocity;
	else if (text == "InitialColor") outType = VfxGraphNodeType::InitialColor;
	else if (text == "InitialSize") outType = VfxGraphNodeType::InitialSize;
	else if (text == "Gravity") outType = VfxGraphNodeType::Gravity;
	else if (text == "Drag") outType = VfxGraphNodeType::Drag;
	else if (text == "SpriteRenderer") outType = VfxGraphNodeType::SpriteRenderer;
	else if (text == "InitialRotation") outType = VfxGraphNodeType::InitialRotation;
	else if (text == "RotationRate") outType = VfxGraphNodeType::RotationRate;
	else if (text == "SizeOverLife") outType = VfxGraphNodeType::SizeOverLife;
	else if (text == "ColorOverLife") outType = VfxGraphNodeType::ColorOverLife;
	else return false;
	return true;
}

} // namespace Ken4lowEngine
