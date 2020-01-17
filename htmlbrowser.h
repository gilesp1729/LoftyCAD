#pragma once

// From Jeff Glatt, 2002, "Display a Web Page in a Plain C Win32 Application"
// https://www.codeguru.com/cpp/i-n/ieprogram/article.php/c4379/Display-a-Web-Page-in-a-Plain-C-Win32-Application.htm
// with some minor rrearrangement.

#include <windows.h>
#include <exdisp.h>		// Defines of stuff like IWebBrowser2. This is an include file with Visual C 6 and above
#include <mshtml.h>		// Defines of stuff like IHTMLDocument2. This is an include file with Visual C 6 and above
#include <crtdbg.h>		// for _ASSERT()

extern unsigned char WindowCount;

long EmbedBrowserObject(HWND hwnd);
long DisplayHTMLPage(HWND hwnd, LPTSTR webPageName);
long DisplayHTMLStr(HWND hwnd, LPCTSTR string);
void SetBrowserSize(HWND hwnd, int width, int height);
void UnEmbedBrowserObject(HWND hwnd);
