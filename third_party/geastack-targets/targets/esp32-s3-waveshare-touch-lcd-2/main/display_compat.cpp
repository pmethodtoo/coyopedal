// FORK PATCH (Waveshare LCD-2): the sticks3 display backend predates two
// framework seams the coyopedal app links against — the internal-DMA flush
// pool symbol (app boot verifies it is internal + DMA-capable) and the flush
// odometer diagnostics read. The backend uses its own staging ring, so these
// are inert bookkeeping surfaces; provided here to close the link contract.
#include "display.h"

#include <cstdint>

namespace gea::platform::esp32::display {

#ifndef GEA_EMBEDDED_DISPLAY_FLUSH_POOL_BYTES
#define GEA_EMBEDDED_DISPLAY_FLUSH_POOL_BYTES 5120
#endif

alignas(64) std::uint16_t g_flushPool[GEA_EMBEDDED_DISPLAY_FLUSH_POOL_BYTES / sizeof(std::uint16_t)];

}  // namespace gea::platform::esp32::display

namespace gea::platform::display {

void Display::flushOdometerRead(std::uint32_t &calls, std::uint64_t &pixels)
{
	calls = 0;
	pixels = 0;
}

}  // namespace gea::platform::display
