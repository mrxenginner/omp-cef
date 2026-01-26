#include "focus.hpp"
#include "browser/manager.hpp"
#include <samp/components/game.hpp>
#include <hooks/d3ddevice9.hpp>
#include <samp/common.hpp>

void FocusManager::Update()
{
    auto* game = GetComponent<GameComponent>();
    if (!game) return;

    const bool is_cef_focused_now = manager_.IsAnyBrowserFocused();

    if (is_cef_focused_now) {
        if (g_pD3DDeviceHook) g_pD3DDeviceHook->SetForceHideCursor(true);
        game->SetCursorMode(CMODE_LOCKCAMANDCONTROL, TRUE);

        // Set Windows cursor based on CEF request
        const cef_cursor_type_t type = manager_.GetCursorType();
        HCURSOR hCursor = nullptr;
        switch (type) {
            case CT_POINTER: hCursor = LoadCursor(nullptr, IDC_ARROW); break;
            case CT_IBEAM:   hCursor = LoadCursor(nullptr, IDC_IBEAM); break;
            case CT_HAND:    hCursor = LoadCursor(nullptr, IDC_HAND); break;
            default:         hCursor = LoadCursor(nullptr, IDC_ARROW); break;
        }
        SetCursor(hCursor);
        if (!was_cef_focused_last_frame_) { ShowCursor(TRUE); }
    }
    else if (was_cef_focused_last_frame_) {
        RestoreGameControls();
    }

    was_cef_focused_last_frame_ = is_cef_focused_now;
}

void FocusManager::RestoreGameControls()
{
    auto* game = GetComponent<GameComponent>();
    if (g_pD3DDeviceHook) g_pD3DDeviceHook->SetForceHideCursor(false);
    ShowCursor(FALSE); SetCursor(nullptr);
    if (game) game->SetCursorMode(CMODE_NONE, TRUE);
}

void FocusManager::SetInputFocus(int browserId, bool has_focus)
{
    if (has_focus) { input_focused_browser_id_.store(browserId); }
    else {
        int expected_id = browserId; input_focused_browser_id_.compare_exchange_strong(expected_id, -1);
    }
}

bool FocusManager::ShouldBlockChat() const
{
    const int focused_id = input_focused_browser_id_.load();
    if (focused_id == -1) return false;

    // Block chat if focused browser is configured to control chat input
    auto* instance = const_cast<BrowserManager&>(manager_).GetBrowserInstance(focused_id);
    if (instance) { return instance->controls_chat_input; }
    return false;
}
