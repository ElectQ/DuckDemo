#include "v8_api_handle.h"



bool ApiV8Handler::Execute(const CefString& name,
    CefRefPtr<CefV8Value> object,
    const CefV8ValueList& arguments,
    CefRefPtr<CefV8Value>& retval,
    CefString& exception) {
    CefRefPtr<CefBrowser> browser =
        CefV8Context::GetCurrentContext()->GetBrowser();

    if (name == "NativeApi_MinimizeWindow") {
        browser->GetMainFrame()->SendProcessMessage(
            PID_BROWSER, CefProcessMessage::Create("MinimizeWindow"));
        return true;
    }

    if (name == "NativeApi_CloseWindow") {
        browser->GetMainFrame()->SendProcessMessage(
            PID_BROWSER, CefProcessMessage::Create("CloseWindow"));
        return true;
    }

    if (name == "NativeApi_ExitProcess") {
        browser->GetMainFrame()->SendProcessMessage(
            PID_BROWSER, CefProcessMessage::Create("ExitProcess"));
        return true;
    }

    if (name == "NativeApi_OnDragZoneMouseDown") {
        browser->GetMainFrame()->SendProcessMessage(
            PID_BROWSER, CefProcessMessage::Create("OnDragZoneMouseDown"));
        return true;
    }

    if (name == "NativeApi_OnDragZoneMouseUp") {
        browser->GetMainFrame()->SendProcessMessage(
            PID_BROWSER, CefProcessMessage::Create("OnDragZoneMouseUp"));
        return true;
    }

    if (name == "NativeApi_LogToConsole") {
        if (arguments.size() > 0 && arguments[0]->IsString()) {
            std::string message = arguments[0]->GetStringValue();
            printf("[UI_LOG] %s\n", message.c_str());
            return true;
        }
        return false;
    }

    if (name == "NativeApi_PopSelectAction") {
        printf("[V8Handler] ==========================================\n");
        printf("[V8Handler] === NativeApi_PopSelectAction CALLED ===\n");
        printf("[V8Handler] ==========================================\n");
        printf("[V8Handler] Current thread ID: %lu\n", GetCurrentThreadId());
        printf("[V8Handler] Browser ID: %d\n", browser->GetIdentifier());
        printf("[V8Handler] Frame URL: %s\n", browser->GetMainFrame()->GetURL().ToString().c_str());
        printf("[V8Handler] Arguments count: %zu\n", arguments.size());
        
        if (arguments.size() < 2) {
            printf("[V8Handler] ❌ ERROR: Not enough arguments! Expected 2, got %zu\n", arguments.size());
            return false;
        }
        
        auto intArgMsgId = arguments[0]->GetIntValue();
        auto boolArgMsgResult = arguments[1]->GetBoolValue();
        
        printf("[V8Handler] 📥 Parsed arguments: eventId=%d, isBlock=%s\n", 
            intArgMsgId, boolArgMsgResult ? "true (BLOCK)" : "false (ALLOW)");
        
        printf("[V8Handler] 📤 Creating process message...\n");
        auto theMsg = CefProcessMessage::Create("PopSelectAction");
        CefRefPtr<CefListValue> args = theMsg->GetArgumentList();
        args->SetInt(0, intArgMsgId);
        args->SetBool(1, boolArgMsgResult);

        printf("[V8Handler] 🚀 Sending message to browser process (PID_BROWSER)...\n");
        browser->GetMainFrame()->SendProcessMessage(PID_BROWSER, theMsg);
        /*printf("[V8Handler] ✅ SendProcessMessage completed, result: %s\n", sendResult ? "SUCCESS" : "FAILED");*/
        
        printf("[V8Handler] 🎯 Function execution completed successfully\n");
        printf("[V8Handler] ==========================================\n");
        
        return true;
    }

    return false;
}