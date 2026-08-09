#pragma once
#include <optional>
#include <string>
#include <vector>

#include "GpuParticleEmitterAsset.h"

namespace Ken4lowEngine
{

    class GpuParticleEmitterSerializer
    {
    public:
        static std::optional<GpuParticleEmitterAsset> LoadFromFile(const std::string& filePath);
        static bool SaveToFile(const GpuParticleEmitterAsset& asset, const std::string& filePath);
        static std::vector<std::string> FindJsonFiles(const std::string& directoryPath);

    private:
        static const char* ToString(GpuParticleKind value);
        static const char* ToString(GpuParticleType value);
        static const char* ToString(GpuRibbonType value);
        static const char* ToString(BillboardMode value);

        static bool TryParseGpuParticleKind(const std::string& text, GpuParticleKind& outValue);
        static bool TryParseGpuParticleType(const std::string& text, GpuParticleType& outValue);
        static bool TryParseGpuRibbonType(const std::string& text, GpuRibbonType& outValue);
        static bool TryParseBillboardMode(const std::string& text, BillboardMode& outValue);
    };

} // namespace Ken4lowEngine