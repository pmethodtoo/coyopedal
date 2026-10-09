// CST816D capacitive touch (Waveshare ESP32-S3-Touch-LCD-2) on the primary
// I2C bus at 0x15, polled. Fork of the StickS3 stub: the board has no touch
// interrupt wired, so poll() reads the report registers at ~30 Hz and feeds
// transitions through injectEvent, reusing the debug-injection path so the
// observer/cache bookkeeping is identical for real and synthetic touches.
#include "touch.h"

#include "board.h"
#include "i2c.h"

#include <atomic>

#include "driver/i2c_master.h"
#include "esp_log.h"
#include "esp_timer.h"

namespace {

constexpr const char *kTag = "lcd2_touch";
constexpr std::uint8_t kAddr = 0x15;
constexpr std::uint8_t kRegGesture = 0x01;   // gesture id (unused here)
constexpr std::uint8_t kRegPoints = 0x02;    // finger count, then 7 bytes/point
constexpr int kStride = 7;
constexpr int kPollIntervalMs = 33;
constexpr int kI2cTimeoutMs = 50;

// Native panel geometry (portrait). CST816D reports native portrait
// coordinates; logical frames are landscape (see display.cpp rotation), so
// map per the panel-side rule for LandscapePrimary: panel x = logical y,
// panel y = (panelH-1) - logical x  =>  logical x = (panelH-1) - ty.
constexpr int kNativeH = 320;

struct TouchSample {
	bool touching = false;
	int x = 0;
	int y = 0;
};

gea::platform::touch::Touchscreen::Observer g_observer = nullptr;
TouchSample g_current{};
TouchSample g_latestMove{};
std::atomic<bool> g_latestMoveQueued{false};

i2c_master_bus_handle_t g_bus = nullptr;
i2c_master_dev_handle_t g_dev = nullptr;
std::int64_t g_lastPollUs = 0;
bool g_lastTouching = false;

bool hwInit()
{
	auto bus = gea::platform::i2c::Bus::primary();
	if (!bus.available()) {
		ESP_LOGE(kTag, "I2C bus unavailable; touch disabled");
		return false;
	}
	g_bus = static_cast<i2c_master_bus_handle_t>(bus.nativeHandle());
	i2c_device_config_t cfg = {};
	cfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
	cfg.device_address = kAddr;
	cfg.scl_speed_hz = 400000;
	const esp_err_t err = i2c_master_bus_add_device(g_bus, &cfg, &g_dev);
	if (err != ESP_OK) {
		ESP_LOGE(kTag, "add CST816D device failed: %s", esp_err_to_name(err));
		g_dev = nullptr;
		return false;
	}
	std::uint8_t reg = 0xA7;
	std::uint8_t id = 0;
	if (i2c_master_transmit_receive(g_dev, &reg, 1, &id, 1, kI2cTimeoutMs) == ESP_OK) {
		ESP_LOGI(kTag, "CST816D present, version=0x%02X", id);
	} else {
		ESP_LOGW(kTag, "CST816D version read failed; probing anyway");
	}
	return true;
}

void pollHardware()
{
	if (!g_dev) return;
	std::uint8_t reg = kRegPoints;
	std::uint8_t buf[1 + kStride] = {};
	if (i2c_master_transmit_receive(g_dev, &reg, 1, buf, sizeof(buf), kI2cTimeoutMs) != ESP_OK)
		return;

	const int count = buf[0] & 0x0F;
	if (count > 0) {
		const int tx = ((buf[1] & 0x0F) << 8) | buf[2];
		const int ty = ((buf[3] & 0x0F) << 8) | buf[4];
		// FORK (lcd-2): the framework canvas is native portrait 240x320
		// (panel census), and the CST816D already reports aligned portrait
		// coordinates — identity map is correct here.
		const int lx = tx;
		const int ly = ty;
		ESP_LOGI(kTag, "raw=(%d,%d) mapped=(%d,%d)", tx, ty, lx, ly);
		const auto phase = g_lastTouching ? gea::platform::touch::Phase::Move : gea::platform::touch::Phase::Down;
		g_lastTouching = true;
		gea::platform::touch::Touchscreen::injectEvent(phase, true, lx, ly);
	} else if (g_lastTouching) {
		g_lastTouching = false;
		gea::platform::touch::Touchscreen::injectEvent(gea::platform::touch::Phase::Up, false, g_current.x, g_current.y);
	}
}

}  // namespace

void gea::platform::touch::Touchscreen::setObserver(Observer observer)
{
	g_observer = observer;
}

bool gea::platform::touch::Touchscreen::init()
{
	hwInit();
	return true;
}

void gea::platform::touch::Touchscreen::poll(int /*nowMs*/)
{
	const std::int64_t now = esp_timer_get_time();
	if (now - g_lastPollUs < kPollIntervalMs * 1000) return;
	g_lastPollUs = now;
	pollHardware();
}

int gea::platform::touch::Touchscreen::read(int *x, int *y)
{
	if (x) *x = g_current.x;
	if (y) *y = g_current.y;
	return g_current.touching ? 1 : 0;
}

int gea::platform::touch::Touchscreen::readCached(int *x, int *y)
{
	return read(x, y);
}

void gea::platform::touch::Touchscreen::consumeLatestMove(int *x, int *y)
{
	if (g_latestMoveQueued.exchange(false, std::memory_order_acq_rel)) {
		if (x) *x = g_latestMove.x;
		if (y) *y = g_latestMove.y;
		return;
	}
	if (x) *x = -1;
	if (y) *y = -1;
}

void gea::platform::touch::Touchscreen::injectEvent(Phase phase, bool touching, int x, int y)
{
	g_current = {touching, x, y};
	if (phase == Phase::Move) {
		g_latestMove = {touching, x, y};
		g_latestMoveQueued.store(true, std::memory_order_release);
	}
	if (g_observer) g_observer(phase, touching, x, y);
}
