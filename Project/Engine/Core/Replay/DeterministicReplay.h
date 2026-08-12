#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace Ken4lowEngine
{
	struct DeterministicReplayFrame
	{
		uint64_t frameIndex = 0;
		uint32_t fixedDeltaMicroseconds = 0;
		uint64_t rngState = 0;
		uint64_t stateHash = 0;
		std::vector<uint8_t> inputPayload;
	};

	class DeterministicReplay
	{
	public:
		enum class Mode
		{
			Disabled,
			Recording,
			Playback,
		};

		static constexpr uint32_t kFormatVersion = 1;
		static constexpr uint32_t kMaxInputPayloadBytes = 1024u * 1024u;

		void BeginRecording(uint64_t seed)
		{
			Reset();
			mode_ = Mode::Recording;
			seed_ = seed;
		}

		bool RecordFrame(DeterministicReplayFrame frame, std::string& outError)
		{
			outError.clear();
			if (mode_ != Mode::Recording)
			{
				outError = "Replay is not recording.";
				return false;
			}
			if (!frames_.empty() && frame.frameIndex != frames_.back().frameIndex + 1)
			{
				outError = "Replay frame indices must be contiguous.";
				return false;
			}
			if (frame.inputPayload.size() > kMaxInputPayloadBytes)
			{
				outError = "Replay input payload exceeds the per-frame safety limit.";
				return false;
			}

			// A replay frame owns its input bytes so later input-buffer reuse cannot mutate recorded history.
			frames_.push_back(std::move(frame));
			return true;
		}

		bool Save(const std::filesystem::path& path, std::string& outError) const
		{
			outError.clear();
			if (mode_ != Mode::Recording)
			{
				outError = "Only a recording replay can be saved.";
				return false;
			}

			std::ofstream stream(path, std::ios::binary | std::ios::trunc);
			if (!stream.is_open())
			{
				outError = "Failed to open replay file for writing.";
				return false;
			}

			constexpr char magic[8] = { 'K', '4', 'R', 'E', 'P', 'L', 'A', 'Y' };
			stream.write(magic, sizeof(magic));
			WriteValue(stream, kFormatVersion);
			WriteValue(stream, seed_);
			WriteValue(stream, static_cast<uint64_t>(frames_.size()));
			for (const DeterministicReplayFrame& frame : frames_)
			{
				WriteValue(stream, frame.frameIndex);
				WriteValue(stream, frame.fixedDeltaMicroseconds);
				WriteValue(stream, frame.rngState);
				WriteValue(stream, frame.stateHash);
				WriteValue(stream, static_cast<uint32_t>(frame.inputPayload.size()));
				if (!frame.inputPayload.empty())
				{
					stream.write(reinterpret_cast<const char*>(frame.inputPayload.data()),
						static_cast<std::streamsize>(frame.inputPayload.size()));
				}
			}

			if (!stream.good())
			{
				outError = "Failed while writing replay data.";
				return false;
			}
			return true;
		}

		bool Load(const std::filesystem::path& path, std::string& outError)
		{
			outError.clear();
			std::ifstream stream(path, std::ios::binary);
			if (!stream.is_open())
			{
				outError = "Failed to open replay file for reading.";
				return false;
			}

			char magic[8]{};
			stream.read(magic, sizeof(magic));
			constexpr char expectedMagic[8] = { 'K', '4', 'R', 'E', 'P', 'L', 'A', 'Y' };
			for (std::size_t index = 0; index < sizeof(magic); ++index)
			{
				if (magic[index] != expectedMagic[index])
				{
					outError = "Replay magic is invalid.";
					return false;
				}
			}

			uint32_t version = 0;
			uint64_t seed = 0;
			uint64_t frameCount = 0;
			if (!ReadValue(stream, version) || !ReadValue(stream, seed) || !ReadValue(stream, frameCount))
			{
				outError = "Replay header is truncated.";
				return false;
			}
			if (version != kFormatVersion)
			{
				outError = "Replay format version is unsupported.";
				return false;
			}
			if (frameCount > static_cast<uint64_t>(std::numeric_limits<std::size_t>::max()))
			{
				outError = "Replay frame count is too large.";
				return false;
			}

			std::vector<DeterministicReplayFrame> loadedFrames;
			loadedFrames.reserve(static_cast<std::size_t>(frameCount));
			for (uint64_t index = 0; index < frameCount; ++index)
			{
				DeterministicReplayFrame frame{};
				uint32_t payloadSize = 0;
				if (!ReadValue(stream, frame.frameIndex) ||
					!ReadValue(stream, frame.fixedDeltaMicroseconds) ||
					!ReadValue(stream, frame.rngState) ||
					!ReadValue(stream, frame.stateHash) ||
					!ReadValue(stream, payloadSize))
				{
					outError = "Replay frame data is truncated.";
					return false;
				}
				if (payloadSize > kMaxInputPayloadBytes)
				{
					outError = "Replay input payload exceeds the per-frame safety limit.";
					return false;
				}
				if (index > 0 && frame.frameIndex != loadedFrames.back().frameIndex + 1)
				{
					outError = "Replay frame indices are not contiguous.";
					return false;
				}
				frame.inputPayload.resize(payloadSize);
				if (payloadSize > 0)
				{
					stream.read(reinterpret_cast<char*>(frame.inputPayload.data()), static_cast<std::streamsize>(payloadSize));
					if (!stream.good())
					{
						outError = "Replay input payload is truncated.";
						return false;
					}
				}
				loadedFrames.push_back(std::move(frame));
			}

			Reset();
			mode_ = Mode::Playback;
			seed_ = seed;
			frames_ = std::move(loadedFrames);
			return true;
		}

		bool TryReadFrame(uint64_t expectedFrameIndex, DeterministicReplayFrame& outFrame, std::string& outError)
		{
			outError.clear();
			if (mode_ != Mode::Playback)
			{
				outError = "Replay is not in playback mode.";
				return false;
			}
			if (playbackCursor_ >= frames_.size())
			{
				outError = "Replay playback reached the end of the recording.";
				return false;
			}

			const DeterministicReplayFrame& frame = frames_[playbackCursor_];
			if (frame.frameIndex != expectedFrameIndex)
			{
				outError = "Replay frame index does not match the simulation frame.";
				return false;
			}
			outFrame = frame;
			++playbackCursor_;
			return true;
		}

		void ResetPlaybackCursor() { playbackCursor_ = 0; }
		void Reset()
		{
			mode_ = Mode::Disabled;
			seed_ = 0;
			playbackCursor_ = 0;
			frames_.clear();
		}

		[[nodiscard]] Mode GetMode() const { return mode_; }
		[[nodiscard]] uint64_t GetSeed() const { return seed_; }
		[[nodiscard]] std::size_t GetFrameCount() const { return frames_.size(); }
		[[nodiscard]] std::size_t GetPlaybackCursor() const { return playbackCursor_; }

		[[nodiscard]] static uint64_t HashBytes(const void* data, std::size_t size)
		{
			constexpr uint64_t offsetBasis = 14695981039346656037ull;
			constexpr uint64_t prime = 1099511628211ull;
			uint64_t hash = offsetBasis;
			const auto* bytes = static_cast<const uint8_t*>(data);
			for (std::size_t index = 0; index < size; ++index)
			{
				hash ^= bytes[index];
				hash *= prime;
			}
			return hash;
		}

	private:
		template<class T>
		static void WriteValue(std::ofstream& stream, const T& value)
		{
			stream.write(reinterpret_cast<const char*>(&value), sizeof(T));
		}

		template<class T>
		static bool ReadValue(std::ifstream& stream, T& value)
		{
			stream.read(reinterpret_cast<char*>(&value), sizeof(T));
			return stream.good();
		}

		Mode mode_ = Mode::Disabled;
		uint64_t seed_ = 0;
		std::size_t playbackCursor_ = 0;
		std::vector<DeterministicReplayFrame> frames_;
	};
} // namespace Ken4lowEngine
