#pragma once

#include "../Data/VfxCueTypes.h"
#include "../Runtime/VfxRuntimeTypes.h"
#include "Vector3.h"

#include <cmath>
#include <cstdint>
#include <string>
#include <utility>

namespace Ken4lowEngine
{

class VfxTimelineEditor
{
public:
	static VfxTimelineEditor* GetInstance();

	void Initialize();
	void Finalize();
	void Draw(bool* open = nullptr);

private:
	VfxTimelineEditor() = default;
	~VfxTimelineEditor() = default;
	VfxTimelineEditor(const VfxTimelineEditor&) = delete;
	VfxTimelineEditor& operator=(const VfxTimelineEditor&) = delete;

	bool LoadFromDisk();
	bool SaveToDisk();
	bool RegisterPreviewCue();
	void StopPreview();
	void AddTrack(VfxCueTrackType type);
	void DrawCueHeader();
	void DrawUserParameters();
	void DrawTimeline();
	void DrawTrackEditor(VfxCueTrackDesc& track, uint32_t index, bool& removeRequested);
	void DrawRuntimeDiagnostics();
	const char* TrackTypeName(VfxCueTrackType type) const;

private:
	VfxCueDesc editableCue_{};
	std::string filePath_ = "Resources/Vfx/Phase18/Explosion.vfx.json";
	std::string lastMessage_;
	Vector3 previewPosition_{ 0.0f, 1.0f, 0.0f };
	VfxCueHandle previewHandle_{};
	int addTrackType_ = 0;
	int stressCount_ = 16;
	float timelinePixelsPerSecond_ = 120.0f;
	bool initialized_ = false;
};

} // namespace Ken4lowEngine
