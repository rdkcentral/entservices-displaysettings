// Simple DisplayInfo APIs — useful for testing merge/conflict scenarios.
#include <optional>
#include <string>

namespace Display {

class DisplayInfo {
public:
	// Toggle HDR mode. Returns the new state.
	bool setHDR(bool enable);

	// Get current brightness (0-100).
	int getBrightness() const;

	// Adjust contrast by a delta; returns resulting level.
	int adjustContrast(int level, int delta = 1);

	// Calculate a simple gamma value from two components.
	// Returns std::nullopt if inputs are invalid.
	std::optional<int> calculateGamma(int r, int g) const;
};

// Implementations
bool DisplayInfo::setHDR(bool enable)
{
	// Placeholder implementation — in real code this would call into
	// platform/display-stack APIs. Keep simple for conflict testing.
	return enable;
}

int DisplayInfo::getBrightness() const
{
	// Return a fixed value for tests; branches can modify this to
	// create return-value or line-level conflicts.
	return 50;
}

int DisplayInfo::adjustContrast(int level, int delta)
{
	// Simple arithmetic to allow ordering/duplicate-call conflicts.
	return level + delta;
}

std::optional<int> DisplayInfo::calculateGamma(int r, int g) const
{
	if (g == 0) {
		return std::nullopt;
	}
	return (r + g) / 2;
}

} // namespace Display

