#include "emote_display.h"

// Standard C++ headers
#include <atomic>
#include <cmath>
#include <cstring>
#include <memory>
#include <unordered_map>
#include <tuple>
#include <algorithm>
#include <cinttypes>

// Standard C headers
#include <sys/time.h>
#include <time.h>

// ESP-IDF headers
#include <esp_log.h>
#include <esp_lcd_panel_io.h>
#include <esp_timer.h>
#include <lvgl.h>

// FreeRTOS headers
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// Project headers
#include "assets/lang_config.h"
#include "assets.h"
#include "board.h"
#include "gfx.h"
#include "expression_emote.h"


namespace emote {

// ============================================================================
// Constants and Type Definitions
// ============================================================================

static const char* TAG = "EmoteDisplay";

// ============================================================================
// Forward Declarations
// ============================================================================

class EmoteDisplay;

// ============================================================================
// Helper Functions
// ============================================================================

static bool OnFlushIoReady(const esp_lcd_panel_io_handle_t panel_io,
    esp_lcd_panel_io_event_data_t* const edata, void* user_ctx)
{
    emote_handle_t handle = static_cast<emote_handle_t>(user_ctx);
    if (handle) {
        emote_notify_flush_finished(handle);
    }
    return true;
}

// Accent ring state. The emote engine owns every pixel of the frame, so the
// only place an overlay survives all animations is the stripe it is about to
// flush. Stored pre-byte-swapped because the engine renders with swap=true.
// White (0xFFFF) is the resting color until the server names the active mode.
static std::atomic<uint16_t> s_accent_ring_color{0xFFFF};
static int s_display_width = 0;
static int s_display_height = 0;
constexpr float kAccentRingThickness = 8.0f;

static void OverlayAccentRing(int x_start, int y_start, int x_end, int y_end, uint16_t* pixels)
{
    if (s_display_width == 0 || s_display_height == 0) {
        return;
    }
    const uint16_t color = s_accent_ring_color.load(std::memory_order_relaxed);
    const float cx = (s_display_width - 1) / 2.0f;
    const float cy = (s_display_height - 1) / 2.0f;
    const float outer_r = std::min(s_display_width, s_display_height) / 2.0f;
    const float inner_r = outer_r - kAccentRingThickness;
    const int stride = x_end - x_start;

    for (int y = y_start; y < y_end; ++y) {
        const float dy = y - cy;
        const float outer_sq = outer_r * outer_r - dy * dy;
        if (outer_sq <= 0.0f) {
            continue;
        }
        const float outer_half = sqrtf(outer_sq);
        const float inner_sq = inner_r * inner_r - dy * dy;
        const float inner_half = inner_sq > 0.0f ? sqrtf(inner_sq) : 0.0f;
        uint16_t* row = pixels + (size_t)(y - y_start) * stride;
        // Two arcs per row: [cx-outer, cx-inner] and [cx+inner, cx+outer].
        // Near the vertical extremes inner_half is 0 and they merge into one.
        const int spans[2][2] = {
            {(int)ceilf(cx - outer_half), (int)floorf(cx - inner_half)},
            {(int)ceilf(cx + inner_half), (int)floorf(cx + outer_half)},
        };
        for (const auto& span : spans) {
            const int from = std::max(span[0], x_start);
            const int to = std::min(span[1], x_end - 1);
            for (int x = from; x <= to; ++x) {
                row[x - x_start] = color;
            }
        }
    }
}

// Flush callback for emote
static void OnFlushCallback(int x_start, int y_start, int x_end, int y_end, const void* data, emote_handle_t handle)
{
    esp_lcd_panel_handle_t panel = (esp_lcd_panel_handle_t)emote_get_user_data(handle);
    if (panel != nullptr) {
        OverlayAccentRing(x_start, y_start, x_end, y_end,
                          const_cast<uint16_t*>(static_cast<const uint16_t*>(data)));
        esp_lcd_panel_draw_bitmap(panel, x_start, y_start, x_end, y_end, data);
    }
}

// ============================================================================
// Graphics Initialization Functions
// ============================================================================

static emote_handle_t InitializeEmote(const esp_lcd_panel_handle_t panel, const int width, const int height)
{
    if (!panel) {
        ESP_LOGE(TAG, "Invalid panel");
        return nullptr;
    }

    emote_config_t emote_cfg = {
        .flags = {
            .swap = true,
            .double_buffer = true,
            .buff_dma = false,
        },
        .gfx_emote = {
            .h_res = width,
            .v_res = height,
            .fps = 30,
        },
        .buffers = {
            .buf_pixels = static_cast<size_t>(width * 16),
        },
        .task = {
            .task_priority = 5,
            .task_stack = 6 * 1024,
            .task_affinity = 0,
            .task_stack_in_ext = false,
        },
        .flush_cb = OnFlushCallback,
        .user_data = (void*)panel,
    };

    emote_handle_t emote_handle = emote_init(&emote_cfg);
    if (!emote_handle) {
        ESP_LOGE(TAG, "Failed to initialize emote");
        return nullptr;
    }

    return emote_handle;
}

// ============================================================================
// EmoteDisplay Class Implementation
// ============================================================================

EmoteDisplay::EmoteDisplay(const esp_lcd_panel_handle_t panel, const esp_lcd_panel_io_handle_t panel_io,
                           const int width, const int height)
{
    s_display_width = width;
    s_display_height = height;
    emote_handle_ = InitializeEmote(panel, width, height);

    const esp_lcd_panel_io_callbacks_t cbs = {
        .on_color_trans_done = OnFlushIoReady,
    };
    esp_lcd_panel_io_register_event_callbacks(panel_io, &cbs, emote_handle_);
}

EmoteDisplay::~EmoteDisplay()
{
    if (emote_handle_) {
        emote_deinit(emote_handle_);
        emote_handle_ = nullptr;
    }
}

void EmoteDisplay::SetEmotion(const char* const emotion)
{
    ESP_LOGI(TAG, "SetEmotion: %s", emotion);
    if (emote_handle_ && emotion && strlen(emotion) > 0) {
        emote_set_anim_emoji(emote_handle_, emotion);
    }
}

void EmoteDisplay::SetChatMessage(const char* const role, const char* const content)
{
    ESP_LOGI(TAG, "SetChatMessage: %s, %s", role, content);
    if (emote_handle_ && content && strlen(content) > 0) {
        if ((std::strcmp(role, "system") == 0) && std::strstr(content, "xiaozhi.me")) {
            size_t len = strlen(content);
            char* new_content = new char[len + 1];
            strcpy(new_content, content);
            std::replace(new_content, new_content + len, static_cast<char>(0x0A), static_cast<char>(0x20));
            emote_set_event_msg(emote_handle_, EMOTE_MGR_EVT_SYS, new_content);
            delete[] new_content;
        } else {
            emote_set_event_msg(emote_handle_, EMOTE_MGR_EVT_SPEAK, content);
        }
    }
}

void EmoteDisplay::SetStatus(const char* const status)
{
    ESP_LOGI(TAG, "SetStatus: %s", status);
    if (emote_handle_ && status && strlen(status) > 0) {
        if (std::strcmp(status, Lang::Strings::LISTENING) == 0) {
            emote_set_event_msg(emote_handle_, EMOTE_MGR_EVT_LISTEN, NULL);
        } else if (std::strcmp(status, Lang::Strings::STANDBY) == 0) {
            emote_set_event_msg(emote_handle_, EMOTE_MGR_EVT_IDLE, NULL);
        } else if (std::strcmp(status, Lang::Strings::SPEAKING) == 0) {
            emote_set_event_msg(emote_handle_, EMOTE_MGR_EVT_SPEAK, NULL);
        } else if (std::strcmp(status, Lang::Strings::ERROR) == 0) {
            emote_set_event_msg(emote_handle_, EMOTE_MGR_EVT_SET, NULL);
        }
    }
}

void EmoteDisplay::ShowNotification(const char* notification, int duration_ms)
{
    ESP_LOGI(TAG, "ShowNotification: %s", notification);
    if (emote_handle_ && notification && strlen(notification) > 0) {
        emote_set_event_msg(emote_handle_, EMOTE_MGR_EVT_SYS, notification);
    }
}

void EmoteDisplay::UpdateStatusBar(bool update_all)
{
    ESP_LOGD(TAG, "UpdateStatusBar: %s", update_all ? "true" : "false");
    if (!emote_handle_) {
        return;
    }
}

void EmoteDisplay::SetPowerSaveMode(bool on)
{
    ESP_LOGI(TAG, "SetPowerSaveMode: %s", on ? "ON" : "OFF");
    if (!emote_handle_) {
        return;
    }
    // Was a stub. With the backlight off the animation is invisible but still
    // decodes frames at 20 fps, so stop it: that is the part that costs cpu.
    emote_set_anim_visible(emote_handle_, !on);
}

void EmoteDisplay::SetAccentColor(const char* const color)
{
    ESP_LOGI(TAG, "SetAccentColor: %s", color ? color : "(null)");
    const char* hex = color;
    if (hex == nullptr) {
        return;
    }
    if (*hex == '#') {
        ++hex;
    }
    if (strlen(hex) != 6) {
        ESP_LOGW(TAG, "Accent color must be #RRGGBB, got '%s'", color);
        return;
    }
    const uint32_t rgb = strtoul(hex, nullptr, 16);
    const uint16_t rgb565 = (uint16_t)((((rgb >> 19) & 0x1F) << 11) |
                                       (((rgb >> 10) & 0x3F) << 5) |
                                       ((rgb >> 3) & 0x1F));
    // The engine renders byte-swapped (swap=true), so the overlay color has to
    // match what is already in the flush buffer.
    const uint16_t swapped = (uint16_t)((rgb565 >> 8) | (rgb565 << 8));
    if (s_accent_ring_color.exchange(swapped, std::memory_order_relaxed) == swapped) {
        // Same color as before: every ui_state repeats the accent, and only an
        // actual change is worth a full redraw.
        return;
    }
    // The engine only re-flushes dirty areas and the screen border belongs to
    // none of them, so a color change has to invalidate everything or the ring
    // keeps its old color until the next full redraw.
    RefreshAll();
}

void EmoteDisplay::SetPreviewImage(const void* image)
{
    if (image) {
        ESP_LOGI(TAG, "SetPreviewImage: Preview image not supported, using default icon");
    }
}

void EmoteDisplay::SetTheme(Theme* const theme)
{
    ESP_LOGI(TAG, "SetTheme: %p", theme);
}

bool EmoteDisplay::Lock(const int timeout_ms)
{
    (void)timeout_ms;
    return true;
}

void EmoteDisplay::Unlock()
{
}

bool EmoteDisplay::SetObjectVisible(const char* name, bool visible)
{
    ESP_LOGI(TAG, "SetObjectVisible: %s -> %d", name, visible);
    if (emote_handle_) {
        return emote_set_obj_visible(emote_handle_, name, visible) == ESP_OK;
    }
    return false;
}

bool EmoteDisplay::StopAnimDialog()
{
    ESP_LOGI(TAG, "StopAnimDialog");
    if (emote_handle_) {
        return emote_stop_anim_dialog(emote_handle_);
    }
    return false;
}

bool EmoteDisplay::InsertAnimDialog(const char* emoji_name, uint32_t duration_ms)
{
    ESP_LOGI(TAG, "InsertAnimDialog: %s, %" PRIu32, emoji_name, duration_ms);
    if (emote_handle_ && emoji_name) {
        return emote_insert_anim_dialog(emote_handle_, emoji_name, duration_ms);
    }
    return false;
}

void EmoteDisplay::RefreshAll()
{
    if (emote_handle_) {
        emote_notify_all_refresh(emote_handle_);
        return;
    }
}

} // namespace emote