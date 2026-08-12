#pragma once

#include <string_view>

namespace Ken4lowEngine
{
	 enum class BuildProfile
	 {
		 Debug,
		 Development,
		 Shipping,
	 };

	 // Build profile selection stays compile-time so crash reports and packages cannot disagree with the binary.
	 [[nodiscard]] constexpr BuildProfile GetBuildProfile()
	 {
#if defined(_DEBUG)
		 return BuildProfile::Debug;
#elif defined(NDEBUG)
		 return BuildProfile::Shipping;
#else
		 return BuildProfile::Development;
#endif
	 }

	 [[nodiscard]] constexpr std::string_view ToString(BuildProfile profile)
	 {
		 switch (profile)
		 {
		 case BuildProfile::Debug:
			 return "Debug";
		 case BuildProfile::Development:
			 return "Development";
		 case BuildProfile::Shipping:
			 return "Shipping";
		 }
		 return "Unknown";
	 }
} // namespace Ken4lowEngine
