#include "v8_api_handle.h"
//和JS交互的部分
//bool ApiV8Handler::Execute(const CefString& name, CefRefPtr<CefV8Value> object,
//	const CefV8ValueList& arguments,
//	CefRefPtr<CefV8Value>& retval,
//	CefString& exception) {
//	CefRefPtr<CefBrowser> browser =
//		CefV8Context::GetCurrentContext()->GetBrowser();
//
//	if (name == "NativeApi_MinimumWindow") {
//		browser->GetMainFrame()->SendProcessMessage(
//			PID_BROWSER, CefProcessMessage::Create("MinimumWindow"));
//		return true;
//	}
//	if (name == "NativeApi_CloseEventWindow") {
//		browser->GetMainFrame()->SendProcessMessage(
//			PID_BROWSER, CefProcessMessage::Create("CloseEventWindow"));
//		return true;
//	}
//
//	if (name == "NativeApi_ExitProcess") {
//		browser->GetMainFrame()->SendProcessMessage(
//			PID_BROWSER, CefProcessMessage::Create("ExitProcess"));
//		return true;
//	}
//
//	if (name == "NativeApi_OnDragZoneMouseDown") {
//		browser->GetMainFrame()->SendProcessMessage(
//			PID_BROWSER, CefProcessMessage::Create("OnDragZoneMouseDown"));
//		return true;
//	}
//
//	if (name == "NativeApi_OnDragZoneMouseUp") {
//		browser->GetMainFrame()->SendProcessMessage(
//			PID_BROWSER, CefProcessMessage::Create("OnDragZoneMouseUp"));
//		return true;
//	}
//	if (name == "NativeApi_PopSelectAction") {
//		auto intArgMsgId = arguments[0]->GetIntValue();
//		auto boolArgMsgResult = arguments[1]->GetBoolValue();
//		auto theMsg = CefProcessMessage::Create("PopSelectAction");
//		CefRefPtr<CefListValue> args = theMsg->GetArgumentList();
//		args->SetInt(0, intArgMsgId);
//		args->SetBool(1, boolArgMsgResult);
//
//		browser->GetMainFrame()->SendProcessMessage(
//			PID_BROWSER, theMsg);
//		return true;
//	}
//
//	return false;
//}



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

    if (name == "NativeApi_PopSelectAction") {
        auto intArgMsgId = arguments[0]->GetIntValue();
        auto boolArgMsgResult = arguments[1]->GetBoolValue();
        auto theMsg = CefProcessMessage::Create("PopSelectAction");
        CefRefPtr<CefListValue> args = theMsg->GetArgumentList();
        args->SetInt(0, intArgMsgId);
        args->SetBool(1, boolArgMsgResult);

        browser->GetMainFrame()->SendProcessMessage(
            PID_BROWSER, theMsg);
        return true;
    }

    return false;
}