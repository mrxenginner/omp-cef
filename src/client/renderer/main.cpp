#include "include/cef_app.h"
#include "include/cef_render_process_handler.h"
#include "include/wrapper/cef_helpers.h"
#include <map>

// Global map to store event names and their callback functions
std::map<std::string, std::vector<CefRefPtr<CefV8Value>>> events_;

class V8HandlerImpl : public CefV8Handler 
{
public:
    bool Execute(const CefString& name,
        CefRefPtr<CefV8Value> object,
        const CefV8ValueList& arguments,
        CefRefPtr<CefV8Value>& retval,
        CefString& exception) override {

        if (name == "emit") {
            if (arguments.size() < 1 || !arguments[0]->IsString()) {
                exception = "Invalid arguments to cef.emit(eventName, ...args)";
                return true;
            }

            CefRefPtr<CefProcessMessage> msg = CefProcessMessage::Create("emit_event");
            CefRefPtr<CefListValue> list = msg->GetArgumentList();
            list->SetString(0, arguments[0]->GetStringValue());

            for (size_t i = 1; i < arguments.size(); ++i) {
                auto arg = arguments[i];

                if (arg->IsBool()) 
                    list->SetBool(i, arg->GetBoolValue());
                else if (arg->IsInt()) 
                    list->SetInt(i, arg->GetIntValue());
                else if (arg->IsDouble()) 
                    list->SetDouble(i, arg->GetDoubleValue());
                else if (arg->IsString()) 
                    list->SetString(i, arg->GetStringValue());
                else 
                    list->SetNull(i);
            }

            CefV8Context::GetCurrentContext()->GetFrame()->SendProcessMessage(PID_BROWSER, msg);
            return true;
        }
        else if (name == "on") {
            if (arguments.size() != 2 || !arguments[0]->IsString() || !arguments[1]->IsFunction()) {
                exception = "Invalid arguments to cef.on(eventName, callback)";
                return true;
            }

            const std::string eventName = arguments[0]->GetStringValue();
            CefRefPtr<CefV8Value> callback = arguments[1];

            // Store the callback function to be called later
            events_[eventName].push_back(callback);
            return true;
        }
        else if (name == "off") {
            // cef.off() - clear everything
            if (arguments.size() == 0) {
                events_.clear();
                return true;
            }

            // cef.off(eventName)
            if (arguments.size() == 1) {
                if (!arguments[0]->IsString()) {
                    exception = "Invalid arguments to cef.off(eventName[, callback])";
                    return true;
                }

                const std::string eventName = arguments[0]->GetStringValue();
                events_.erase(eventName);
                return true;
            }

            // cef.off(eventName, callback)
            if (arguments.size() == 2) {
                if (!arguments[0]->IsString() || !arguments[1]->IsFunction()) {
                    exception = "Invalid arguments to cef.off(eventName, callback)";
                    return true;
                }

                const std::string eventName = arguments[0]->GetStringValue();
                CefRefPtr<CefV8Value> callback = arguments[1];

                auto it = events_.find(eventName);
                if (it == events_.end())
                    return true;

                auto& vec = it->second;
                vec.erase(
                    std::remove_if(vec.begin(), vec.end(),
                        [&](const CefRefPtr<CefV8Value>& cb) {
                            return cb && cb->IsSame(callback);
                        }),
                    vec.end()
                );

                if (vec.empty())
                    events_.erase(it);

                return true;
            }

            exception = "Invalid arguments to cef.off(eventName[, callback])";
            return true;
        }

        return false;
    }

    IMPLEMENT_REFCOUNTING(V8HandlerImpl);
};

class RenderProcessHandler : public CefRenderProcessHandler {
public:
    void OnContextCreated(CefRefPtr<CefBrowser> browser,
        CefRefPtr<CefFrame> frame,
        CefRefPtr<CefV8Context> context) override {
        CEF_REQUIRE_RENDERER_THREAD();

        // Create the 'cef' object in JavaScript
        CefRefPtr<CefV8Value> global = context->GetGlobal();
        CefRefPtr<CefV8Value> cefObj = CefV8Value::CreateObject(nullptr, nullptr);
        CefRefPtr<CefV8Handler> handler = new V8HandlerImpl();

        // Create the 'cef.' functions
        cefObj->SetValue("emit", CefV8Value::CreateFunction("emit", handler), V8_PROPERTY_ATTRIBUTE_NONE);
        cefObj->SetValue("on", CefV8Value::CreateFunction("on", handler), V8_PROPERTY_ATTRIBUTE_NONE);
        cefObj->SetValue("off", CefV8Value::CreateFunction("off", handler), V8_PROPERTY_ATTRIBUTE_NONE);
        global->SetValue("cef", cefObj, V8_PROPERTY_ATTRIBUTE_NONE);
    }

    void OnContextReleased(CefRefPtr<CefBrowser> browser,
        CefRefPtr<CefFrame> frame,
        CefRefPtr<CefV8Context> context) override {

        CEF_REQUIRE_RENDERER_THREAD();
        events_.clear();
    }

    bool OnProcessMessageReceived(CefRefPtr<CefBrowser> browser,
        CefRefPtr<CefFrame> frame,
        CefProcessId source_process,
        CefRefPtr<CefProcessMessage> message) override {
        CEF_REQUIRE_RENDERER_THREAD();

        if (message->GetName() == "emit_event") {
            CefRefPtr<CefListValue> args = message->GetArgumentList();

            if (args->GetSize() < 1) {
                return true; // Not enough arguments
            }

            // The event name is always at index 0
            std::string eventName = args->GetString(0);

            CefV8ValueList jsArgs;
            for (size_t i = 1; i < args->GetSize(); ++i) {
                switch (args->GetType(i)) {
                    case VTYPE_INT:
                        jsArgs.push_back(CefV8Value::CreateInt(args->GetInt(i)));
                        break;
                    case VTYPE_DOUBLE:
                        jsArgs.push_back(CefV8Value::CreateDouble(args->GetDouble(i)));
                        break;
                    case VTYPE_BOOL:
                        jsArgs.push_back(CefV8Value::CreateBool(args->GetBool(i)));
                        break;
                    case VTYPE_STRING:
                        jsArgs.push_back(CefV8Value::CreateString(args->GetString(i)));
                        break;
                    default:
                        jsArgs.push_back(CefV8Value::CreateNull());
                        break;
                }
            }

            // Find and execute the stored JavaScript callbacks
            CefRefPtr<CefV8Context> context = frame->GetV8Context();
            if (context->Enter()) {
                auto it = events_.find(eventName);
                if (it != events_.end()) {
                    for (const auto& cb : it->second) {
                        cb->ExecuteFunction(nullptr, jsArgs);
                    }
                }

                context->Exit();
            }

            return true;
        }

        return false;
    }

    IMPLEMENT_REFCOUNTING(RenderProcessHandler);
};

class RendererApp : public CefApp, public CefRenderProcessHandler {
public:
    CefRefPtr<CefRenderProcessHandler> GetRenderProcessHandler() override {
        return new RenderProcessHandler();
    }

    IMPLEMENT_REFCOUNTING(RendererApp);
};

// Entry point for the renderer process
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int) 
{
    CefMainArgs args(hInstance);
    CefRefPtr<RendererApp> app = new RendererApp();

    return CefExecuteProcess(args, app, nullptr);
}