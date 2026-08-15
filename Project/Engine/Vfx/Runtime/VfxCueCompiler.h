#pragma once

#include "VfxCueProgram.h"

#include <string>

namespace Ken4lowEngine
{

class VfxCueCompiler
{
public:
	/// <summary>
	/// 編集用Descを検証し、開始時刻順のRuntime Programへ変換します。
	/// 失敗時はoutProgramを変更せずfalseを返します。
	/// </summary>
	static bool Compile(
		const VfxCueDesc& desc,
		VfxCueProgram& outProgram,
		std::string* outError = nullptr);

private:
	static bool ValidateTrack(
		const VfxCueTrackDesc& track,
		uint32_t trackIndex,
		std::string& outError);
};

} // namespace Ken4lowEngine
