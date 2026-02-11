// stdafx.h : 标准系统包含文件的包含文件，
// 或是经常使用但不常更改的
// 特定于项目的包含文件

#pragma once

#ifndef VC_EXTRALEAN
#define VC_EXTRALEAN            // 从 Windows 头中排除极少使用的资料
#endif


#define _ATL_CSTRING_EXPLICIT_CONSTRUCTORS      // 某些 CString 构造函数将是显式的

// 关闭 MFC 对某些常见但经常可放心忽略的警告消息的隐藏
#define _AFX_ALL_WARNINGS

#ifdef SHELLPROTECT_CLI
#include <Windows.h>
#include <stdio.h>
#include <string>
#include <cwchar>
#include <cstdarg>

class CString {
public:
	CString() = default;
	CString(const wchar_t* value) : data_(value ? value : L"") {}
	CString(const char* value) { assign_from_ansi(value); }
	CString(const std::wstring& value) : data_(value) {}

	CString& operator=(const wchar_t* value) {
		data_ = value ? value : L"";
		return *this;
	}

	CString& operator=(const char* value) {
		assign_from_ansi(value);
		return *this;
	}

	CString& operator=(const CString&) = default;
	CString(const CString&) = default;

	bool IsEmpty() const { return data_.empty(); }
	void Empty() { data_.clear(); }
	int GetLength() const { return static_cast<int>(data_.size()); }

	int ReverseFind(wchar_t ch) const {
		const size_t pos = data_.find_last_of(ch);
		return pos == std::wstring::npos ? -1 : static_cast<int>(pos);
	}

	CString Left(int count) const {
		if (count <= 0) {
			return CString();
		}
		if (count >= GetLength()) {
			return *this;
		}
		return CString(data_.substr(0, static_cast<size_t>(count)));
	}

	CString Right(int count) const {
		if (count <= 0) {
			return CString();
		}
		const int len = GetLength();
		if (count >= len) {
			return *this;
		}
		return CString(data_.substr(static_cast<size_t>(len - count)));
	}

	CString Mid(int start) const {
		if (start <= 0) {
			return *this;
		}
		const int len = GetLength();
		if (start >= len) {
			return CString();
		}
		return CString(data_.substr(static_cast<size_t>(start)));
	}

	void Format(const wchar_t* fmt, ...) {
		wchar_t buffer[512] = { 0 };
		va_list args;
		va_start(args, fmt);
		_vsnwprintf_s(buffer, _countof(buffer), _TRUNCATE, fmt, args);
		va_end(args);
		data_ = buffer;
	}

	const wchar_t* GetString() const { return data_.c_str(); }
	const wchar_t* GetBSTR() const { return data_.c_str(); }
	operator const wchar_t*() const { return data_.c_str(); }

	CString operator+(const CString& rhs) const {
		return CString(data_ + rhs.data_);
	}

	CString& operator+=(const CString& rhs) {
		data_ += rhs.data_;
		return *this;
	}

private:
	void assign_from_ansi(const char* value) {
		if (value == nullptr) {
			data_.clear();
			return;
		}
		const int size = MultiByteToWideChar(CP_ACP, 0, value, -1, nullptr, 0);
		if (size <= 0) {
			data_.clear();
			return;
		}
		std::wstring wide(static_cast<size_t>(size), L'\0');
		MultiByteToWideChar(CP_ACP, 0, value, -1, &wide[0], size);
		if (!wide.empty() && wide.back() == L'\0') {
			wide.pop_back();
		}
		data_ = wide;
	}

	std::wstring data_;
};

inline int ShellProtectCliMessageBox(const wchar_t* msg) {
	if (msg != nullptr) {
		fwprintf(stderr, L"[ShellProtect] %ls\n", msg);
	}
	return 0;
}

#define AfxMessageBox(msg) ShellProtectCliMessageBox(msg)
#else
#include <afxwin.h>         // MFC 核心组件和标准组件
#include <afxext.h>         // MFC 扩展
#include <afxdisp.h>        // MFC 自动化类

#ifndef _AFX_NO_OLE_SUPPORT
#include <afxdtctl.h>           // MFC 对 Internet Explorer 4 公共控件的支持
#endif
#ifndef _AFX_NO_AFXCMN_SUPPORT
#include <afxcmn.h>             // MFC 对 Windows 公共控件的支持
#endif // _AFX_NO_AFXCMN_SUPPORT
#include <afxcontrolbars.h>     // 功能区和控件条的 MFC 支持
#endif

#include "CodeTool.h"
#include <list>
#include <vector>
#include <string>
using namespace std;

#include "SingletonHandler.h"

#ifdef _UNICODE
#if defined _M_IX86
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='x86' publicKeyToken='6595b64144ccf1df' language='*'\"")
#elif defined _M_X64
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='amd64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#else
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#endif
#endif
