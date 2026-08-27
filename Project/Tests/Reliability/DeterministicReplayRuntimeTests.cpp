#include "DeterministicReplay.h"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

using namespace Ken4lowEngine;

int main()
{
	DeterministicReplay replay;
	replay.BeginRecording(42);

	std::string error;
	DeterministicReplayFrame frame0{};
	frame0.frameIndex = 0;
	frame0.fixedDeltaMicroseconds = 16667;
	frame0.rngState = 100;
	frame0.stateHash = 111;
	frame0.inputPayload = { 1, 2, 3 };
	assert(replay.RecordFrame(frame0, error));

	DeterministicReplayFrame frame1{};
	frame1.frameIndex = 1;
	frame1.fixedDeltaMicroseconds = 16667;
	frame1.rngState = 101;
	frame1.stateHash = 222;
	frame1.inputPayload = { 4, 5 };
	assert(replay.RecordFrame(frame1, error));

	DeterministicReplayFrame skipped{};
	skipped.frameIndex = 3;
	assert(!replay.RecordFrame(skipped, error));

	const std::filesystem::path replayPath = std::filesystem::temp_directory_path() / "ken4low_phase12_replay.k4r";
	assert(replay.Save(replayPath, error));

	DeterministicReplay playback;
	assert(playback.Load(replayPath, error));
	assert(playback.GetMode() == DeterministicReplay::Mode::Playback);
	assert(playback.GetSeed() == 42);
	assert(playback.GetFrameCount() == 2);

	DeterministicReplayFrame loaded{};
	assert(playback.TryReadFrame(0, loaded, error));
	assert(loaded.inputPayload == std::vector<uint8_t>({ 1, 2, 3 }));
	assert(loaded.stateHash == 111);
	assert(!playback.TryReadFrame(0, loaded, error));
	assert(playback.TryReadFrame(1, loaded, error));
	assert(loaded.rngState == 101);

	// Corrupting the magic must be rejected before any replay frame can influence simulation state.
	const std::filesystem::path corruptPath = std::filesystem::temp_directory_path() / "ken4low_phase12_replay_corrupt.k4r";
	{
		std::ofstream corrupt(corruptPath, std::ios::binary | std::ios::trunc);
		corrupt << "BADREPLY";
	}
	DeterministicReplay corruptReplay;
	assert(!corruptReplay.Load(corruptPath, error));

	const char bytes[] = "deterministic";
	assert(DeterministicReplay::HashBytes(bytes, sizeof(bytes)) == DeterministicReplay::HashBytes(bytes, sizeof(bytes)));

	std::error_code removeError;
	std::filesystem::remove(replayPath, removeError);
	std::filesystem::remove(corruptPath, removeError);
	std::cout << "Deterministic Replay runtime tests passed\n";
	return 0;
}
