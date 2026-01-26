#include "client.hpp"

#include <include/wrapper/cef_helpers.h>

#include "browser/audio.hpp"
#include "browser/focus.hpp"
#include "browser/manager.hpp"

CefRefPtr<BrowserClient> BrowserClient::Create(int id, BrowserManager& mgr, AudioManager& audio, FocusManager* focus)
{
    return new BrowserClient(id, mgr, audio, focus);
}

BrowserClient::BrowserClient(int id, BrowserManager& mgr, AudioManager& audio, FocusManager* focus)
    : browserId_(id), manager_(mgr), audio_(audio), focus_(focus)
{
}

void BrowserClient::OnAfterCreated(CefRefPtr<CefBrowser> browser)
{
    CEF_REQUIRE_UI_THREAD();
    manager_.OnBrowserCreated(browserId_, browser);

    // Inject focus tracking script
    const char* control_chat_js_script = R"JS(
        (function() {
            let isFocused = false;
            const checkFocus = (target) => {
                const isInput = target.tagName === 'INPUT' || 
                                target.tagName === 'TEXTAREA' || 
                                target.isContentEditable;
                if (isInput !== isFocused) {
                    isFocused = isInput;
                    if (window.cef && window.cef.emit) {
                        window.cef.emit('omp:cef:internal:inputFocusChanged', isFocused);
                    }
                }
            };
            document.addEventListener('focusin', (e) => checkFocus(e.target), true);
            document.addEventListener('focusout', (e) => {
                if (isFocused) {
                    isFocused = false;
                    if (window.cef && window.cef.emit) {
                        window.cef.emit('omp:cef:internal:inputFocusChanged', false);
                    }
                }
            }, true);
            document.addEventListener('mousedown', (e) => checkFocus(e.target), true);
        })();
    )JS";

    browser->GetMainFrame()->ExecuteJavaScript(control_chat_js_script, browser->GetMainFrame()->GetURL(), 0);
}

void BrowserClient::GetViewRect(CefRefPtr<CefBrowser> /*browser*/, CefRect& rect)
{
    rect = manager_.GetBrowserInstance(browserId_)->view.rect();
}

void BrowserClient::OnPaint(CefRefPtr<CefBrowser> /*browser*/,
                            PaintElementType type,
                            const RectList& /*dirtyRects*/,
                            const void* buffer,
                            int width,
                            int height)
{
    if (!buffer || type != PET_VIEW)
        return;
    manager_.OnPaint(browserId_, buffer, width, height);
}

bool BrowserClient::OnProcessMessageReceived(CefRefPtr<CefBrowser> /*browser*/,
                                             CefRefPtr<CefFrame> /*frame*/,
                                             CefProcessId /*source_process*/,
                                             CefRefPtr<CefProcessMessage> message)
{
    CEF_REQUIRE_UI_THREAD();

    const std::string msg_name = message->GetName();
    if (msg_name == "emit_event")
    {
        CefRefPtr<CefListValue> args = message->GetArgumentList();
        if (args->GetSize() < 1 || args->GetType(0) != VTYPE_STRING)
            return true;
        const std::string event_name = args->GetString(0);

        if (event_name == "omp:cef:internal:inputFocusChanged")
        {
            if (focus_ && args->GetSize() > 1 && args->GetType(1) == VTYPE_BOOL)
            {
                bool has_focus = args->GetBool(1);
                focus_->SetInputFocus(browserId_, has_focus);
            }
            return true;
        }
        return true;
    }
    return false;
}

bool BrowserClient::OnCursorChange(CefRefPtr<CefBrowser> /*browser*/,
                                   CefCursorHandle /*cursor*/,
                                   cef_cursor_type_t type,
                                   const CefCursorInfo& /*custom_cursor_info*/)
{
    manager_.SetCursorType(type);
    return true;
}

CefResourceRequestHandler::ReturnValue BrowserClient::OnBeforeResourceLoad(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefFrame> frame,
    CefRefPtr<CefRequest> request,
    CefRefPtr<CefCallback> callback)
{
    std::string url = request->GetURL();
    
    LOG_INFO("OnBeforeResourceLoad: {}", url);

    CefRequest::HeaderMap headers;
    request->GetHeaderMap(headers);
    
    if (url.find("youtube.com") != std::string::npos || 
        url.find("googlevideo.com") != std::string::npos)
    {
        //frame->LoadURL("about:blank");

        if (headers.find("Referer") == headers.end())
        {
            headers.emplace("Referer", "https://www.youtube.com/");
            LOG_INFO("Added YouTube Referer");
        }
    }
    
    if (url.find("twitch.tv") != std::string::npos)
    {
        if (headers.find("Referer") == headers.end())
        {
            headers.emplace("Referer", "https://player.twitch.tv/");
            LOG_INFO("Added Twitch Referer");
        }

        if (headers.find("Origin") == headers.end())
        {
            headers.emplace("Origin", "https://player.twitch.tv");
            LOG_INFO("Added Twitch Originf");
        }
    }
    
    request->SetHeaderMap(headers);
    return RV_CONTINUE;
}

void BrowserClient::OnAudioStreamStarted(CefRefPtr<CefBrowser> /*browser*/,
                                         const CefAudioParameters& params,
                                         int channels)
{
    LOG_DEBUG("[CEF] AudioStreamStarted | browserId={}, sample_rate={}, channels={}",
              browserId_,
              params.sample_rate,
              channels);
    channels_ = channels;
    sample_rate_ = params.sample_rate;
    audio_.EnsureStream(browserId_);
}

void BrowserClient::OnAudioStreamPacket(CefRefPtr<CefBrowser> /*browser*/,
                                        const float** data,
                                        int frames,
                                        int64_t /*pts*/)
{
    audio_.OnPcmPacket(browserId_, data, frames, channels_, sample_rate_);
}

void BrowserClient::OnAudioStreamStopped(CefRefPtr<CefBrowser> /*browser*/)
{
    LOG_DEBUG("[CEF] AudioStreamStopped | browserId={}", browserId_);
    audio_.RemoveStream(browserId_);
}

void BrowserClient::OnAudioStreamError(CefRefPtr<CefBrowser> /*browser*/, const CefString& message)
{
    LOG_ERROR("[CEF] AudioStreamError | browserId={} | message={}", browserId_, message.ToString().c_str());
    audio_.RemoveStream(browserId_);
}

bool BrowserClient::OnConsoleMessage(CefRefPtr<CefBrowser> browser,
                                     cef_log_severity_t level,
                                     const CefString& message,
                                     const CefString& source,
                                     int line)
{
    std::string level_str;
    switch(level) {
        case LOGSEVERITY_ERROR: level_str = "ERROR"; break;
        case LOGSEVERITY_WARNING: level_str = "WARN"; break;
        default: level_str = "INFO"; break;
    }
    
    LOG_INFO("[JS Console {}] {} ({}:{})", level_str, message.ToString(), source.ToString(), line);
    return false;
}