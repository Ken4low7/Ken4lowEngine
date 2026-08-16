#include "VfxGraphTypes.h"

namespace Ken4lowEngine
{

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
		return VfxGraphNodeStage::Initialize;
	case VfxGraphNodeType::Gravity:
	case VfxGraphNodeType::Drag:
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
	else return false;
	return true;
}

} // namespace Ken4lowEngine
