#pragma once

#include <atomic>

class BrowserManager;

// Manages input focus transitions between game and CEF browsers
// Handles cursor visibility, camera control, and chat input blocking
class FocusManager
{
public:
    explicit FocusManager(BrowserManager& mgr) : manager_(mgr) {}
    ~FocusManager() = default;

    FocusManager(const FocusManager&) = delete;
    FocusManager& operator=(const FocusManager&) = delete;

    void Update();
    void RestoreGameControls();
    void SetInputFocus(int browserId, bool has_focus);
    bool ShouldBlockChat() const;

private:
    BrowserManager& manager_;

    // Focus state to prevent redundant ShowCursor/HideCursor calls
    bool was_cef_focused_last_frame_ = false;

    // Browser ID currently holding input focus (-1 = none)
    std::atomic<int> input_focused_browser_id_{ -1 };
};