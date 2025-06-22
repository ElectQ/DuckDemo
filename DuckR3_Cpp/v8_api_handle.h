#pragma once
#include "main_app.h"

class ApiV8Handler : public CefV8Handler {
public:
	virtual bool Execute(const CefString& name, CefRefPtr<CefV8Value> object,
		const CefV8ValueList& arguments,
		CefRefPtr<CefV8Value>& retval,
		CefString& exception) override;
	

private:
	IMPLEMENT_REFCOUNTING(ApiV8Handler);
};