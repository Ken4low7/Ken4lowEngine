#pragma once

#include "../Data/VfxCueTypes.h"

#include <cstdint>
#include <string>
#include <vector>

namespace Ken4lowEngine
{

/// <summary>
/// Authoring順序とは独立して、Runtimeが開始時刻順に消費できる1命令です。
/// </summary>
struct VfxCueInstruction
{
	uint32_t sourceTrackIndex = 0;
	VfxCueTrackType type = VfxCueTrackType::Particle;
	float startTime = 0.0f;
	float endTime = 0.0f;
	Vector3 localOffset{};
	VfxCueTrackPayload payload = VfxParticleTrackPayload{};
};

/// <summary>
/// .vfx.jsonをGameplay実行向けへ正規化したCompiled Cueです。
/// 18.1では実行せず、18.2 SchedulerがこのProgramだけを見る契約を固定します。
/// </summary>
struct VfxCueProgram
{
	std::string cueName;
	bool loop = false;
	float duration = 0.0f;
	std::vector<VfxCueInstruction> instructions;
};

} // namespace Ken4lowEngine
