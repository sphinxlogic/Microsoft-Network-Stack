// stdafx.h : include file for standard system include files,
//      or project specific include files that are used frequently,
//      but are changed infrequently

#if !defined(_MULTITRACK_STDAFX_DOT_H_)
#define _MULTITRACK_STDAFX_DOT_H_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#define STRICT
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0400
#endif

#include <atlbase.h>
//You may derive a class from CComModule and use it if you want to override
//something, but do not change the name of _Module
extern CComModule _Module;
#include <atlcom.h>
#include <mspbase.h>
#include "tm.h"


#endif // !defined(_MULTITRACK_STDAFX_DOT_H_)