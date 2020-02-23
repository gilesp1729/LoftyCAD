/*
 * (c) Copyright 1993, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED 
 * Permission to use, copy, modify, and distribute this software for 
 * any purpose and without fee is hereby granted, provided that the above
 * copyright notice appear in all copies and that both the copyright notice
 * and this permission notice appear in supporting documentation, and that 
 * the name of Silicon Graphics, Inc. not be used in advertising
 * or publicity pertaining to distribution of the software without specific,
 * written prior permission. 
 *
 * THE MATERIAL EMBODIED ON THIS SOFTWARE IS PROVIDED TO YOU "AS-IS"
 * AND WITHOUT WARRANTY OF ANY KIND, EXPRESS, IMPLIED OR OTHERWISE,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTY OF MERCHANTABILITY OR
 * FITNESS FOR A PARTICULAR PURPOSE.  IN NO EVENT SHALL SILICON
 * GRAPHICS, INC.  BE LIABLE TO YOU OR ANYONE ELSE FOR ANY DIRECT,
 * SPECIAL, INCIDENTAL, INDIRECT OR CONSEQUENTIAL DAMAGES OF ANY
 * KIND, OR ANY DAMAGES WHATSOEVER, INCLUDING WITHOUT LIMITATION,
 * LOSS OF PROFIT, LOSS OF USE, SAVINGS OR REVENUE, OR THE CLAIMS OF
 * THIRD PARTIES, WHETHER OR NOT SILICON GRAPHICS, INC.  HAS BEEN
 * ADVISED OF THE POSSIBILITY OF SUCH LOSS, HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, ARISING OUT OF OR IN CONNECTION WITH THE
 * POSSESSION, USE OR PERFORMANCE OF THIS SOFTWARE.
 * 
 * US Government Users Restricted Rights 
 * Use, duplication, or disclosure by the Government is subject to
 * restrictions set forth in FAR 52.227.19(c)(2) or subparagraph
 * (c)(1)(ii) of the Rights in Technical Data and Computer Software
 * clause at DFARS 252.227-7013 and/or in similar or successor
 * clauses in the FAR or the DOD or NASA FAR Supplement.
 * Unpublished-- rights reserved under the copyright laws of the
 * United States.  Contractor/manufacturer is Silicon Graphics,
 * Inc., 2011 N.  Shoreline Blvd., Mountain View, CA 94039-7311.
 *
 * OpenGL(TM) is a trademark of Silicon Graphics, Inc.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <windows.h>
#include <GL/gl.h>
#include "glaux.h"
#include "tk.h"

#define static

#if defined(__cplusplus) || defined(c_plusplus)
#define class c_class
#endif


static struct {
    int keyField;
    void (CALLBACK* KeyFunc)(int);
} keyTable[200];

static struct {
    int mouseField;
    void (CALLBACK* MouseFunc)(AUX_EVENTREC *);
} mouseDownTable[20], mouseUpTable[20], mouseLocTable[20], mouseWheelTable[20], mouseClickTable[20];

static int keyTableCount = 0;
static int mouseDownTableCount = 0;
static int mouseUpTableCount = 0;
static int mouseLocTableCount = 0;
static int mouseWheelTableCount = 0;
static int mouseClickTableCount = 0;
static int left_x, left_y;
static int right_x, right_y;
static int middle_x, middle_y;


static GLenum displayModeType = 0;
GLenum APIENTRY auxInitWindowAW(LPCSTR title, BOOL bUnicode, BOOL white_bgnd, HMENU menu, BOOL quit_app);

static void CALLBACK DefaultHandleReshape(GLsizei w, GLsizei h)
{
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho((GLdouble)0.0, (GLdouble)w, (GLdouble)0.0, (GLdouble)h, (GLdouble)-1.0, (GLdouble)1.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

static void CALLBACK DefaultHandleExpose(int w, int h)
{
}

static GLenum CALLBACK MouseWheel(int x, int y, int delta)
{
    AUX_EVENTREC info;
    GLenum flag;
    int i;

    flag = GL_FALSE;
    for (i = 0; i < mouseWheelTableCount; i++)
    {
        info.event = AUX_MOUSEWHEEL;
        info.data[AUX_MOUSEX] = x;
        info.data[AUX_MOUSEY] = y;
        info.data[AUX_MOUSESTATUS] = delta;
        (*mouseWheelTable[i].MouseFunc)(&info);
        flag |= GL_TRUE;
    }
    return flag;
}

static GLenum CALLBACK MouseLoc(int x, int y, GLenum button)
{
    AUX_EVENTREC info;
    GLenum flag;
    int i;

    flag = GL_FALSE;
    for (i = 0; i < mouseLocTableCount; i++)
    {
        if ((int)(button & AUX_LEFTBUTTON) == mouseLocTable[i].mouseField)
        {
            info.event = AUX_MOUSELOC;
            info.data[AUX_MOUSEX] = x;
            info.data[AUX_MOUSEY] = y;
            info.data[AUX_MOUSESTATUS] = button;
            (*mouseLocTable[i].MouseFunc)(&info);
            flag |= GL_TRUE;
        }
        if ((int)(button & AUX_RIGHTBUTTON) == mouseLocTable[i].mouseField)
        {
            info.event = AUX_MOUSELOC;
            info.data[AUX_MOUSEX] = x;
            info.data[AUX_MOUSEY] = y;
            info.data[AUX_MOUSESTATUS] = button;
            (*mouseLocTable[i].MouseFunc)(&info);
            flag |= GL_TRUE;
        }
        if ((int)(button & AUX_MIDDLEBUTTON) == mouseLocTable[i].mouseField)
        {
            info.event = AUX_MOUSELOC;
            info.data[AUX_MOUSEX] = x;
            info.data[AUX_MOUSEY] = y;
            info.data[AUX_MOUSESTATUS] = button;
            (*mouseLocTable[i].MouseFunc)(&info);
            flag |= GL_TRUE;
        }
    }
    return flag;
}

static GLenum CALLBACK MouseUp(int x, int y, GLenum button)
{
    AUX_EVENTREC info;
    GLenum flag;
    int i;

    flag = GL_FALSE;
    for (i = 0; i < mouseUpTableCount; i++) 
    {
        if ((int)(button & AUX_LEFTBUTTON) == mouseUpTable[i].mouseField) 
        {
	        info.event = AUX_MOUSEUP;
	        info.data[AUX_MOUSEX] = x;
	        info.data[AUX_MOUSEY] = y;
	        info.data[AUX_MOUSESTATUS] = button;
	        (*mouseUpTable[i].MouseFunc)(&info);
	        flag |= GL_TRUE;
	    }
        if ((int)(button & AUX_RIGHTBUTTON) == mouseUpTable[i].mouseField) 
        {
	        info.event = AUX_MOUSEUP;
	        info.data[AUX_MOUSEX] = x;
	        info.data[AUX_MOUSEY] = y;
	        info.data[AUX_MOUSESTATUS] = button;
	        (*mouseUpTable[i].MouseFunc)(&info);
	        flag |= GL_TRUE;
	    }
        if ((int)(button & AUX_MIDDLEBUTTON) == mouseUpTable[i].mouseField) 
        {
	        info.event = AUX_MOUSEUP;
	        info.data[AUX_MOUSEX] = x;
	        info.data[AUX_MOUSEY] = y;
            info.data[AUX_MOUSESTATUS] = button;
	        (*mouseUpTable[i].MouseFunc)(&info);
	        flag |= GL_TRUE;
	    }
    }

    for (i = 0; i < mouseClickTableCount; i++)
    {
        if ((int)(button & AUX_LEFTBUTTON) == mouseClickTable[i].mouseField && x == left_x && y == left_y)
        {
            info.event = AUX_MOUSECLICK;
            info.data[AUX_MOUSEX] = x;
            info.data[AUX_MOUSEY] = y;
            info.data[AUX_MOUSESTATUS] = button;
            (*mouseClickTable[i].MouseFunc)(&info);
        }
        if ((int)(button & AUX_RIGHTBUTTON) == mouseClickTable[i].mouseField && x == right_x && y == right_y)
        {
            info.event = AUX_MOUSECLICK;
            info.data[AUX_MOUSEX] = x;
            info.data[AUX_MOUSEY] = y;
            info.data[AUX_MOUSESTATUS] = button;
            (*mouseClickTable[i].MouseFunc)(&info);
        }
        if ((int)(button & AUX_MIDDLEBUTTON) == mouseClickTable[i].mouseField && x == middle_x && y == middle_y)
        {
            info.event = AUX_MOUSECLICK;
            info.data[AUX_MOUSEX] = x;
            info.data[AUX_MOUSEY] = y;
            info.data[AUX_MOUSESTATUS] = button;
            (*mouseClickTable[i].MouseFunc)(&info);
        }
    }

    return flag;
}

static GLenum CALLBACK MouseDown(int x, int y, GLenum button)
{
    AUX_EVENTREC info;
    GLenum flag;
    int i;

    flag = GL_FALSE;
    for (i = 0; i < mouseDownTableCount; i++) 
    {
        if ((int)(button & AUX_LEFTBUTTON) == mouseDownTable[i].mouseField) 
        {
	        info.event = AUX_MOUSEDOWN;
	        info.data[AUX_MOUSEX] = left_x = x;
	        info.data[AUX_MOUSEY] = left_y = y;
            info.data[AUX_MOUSESTATUS] = button;
	        (*mouseDownTable[i].MouseFunc)(&info);
	        flag |= GL_TRUE;
	    }
        if ((int)(button & AUX_RIGHTBUTTON) == mouseDownTable[i].mouseField) 
        {
	        info.event = AUX_MOUSEDOWN;
	        info.data[AUX_MOUSEX] = right_x = x;
	        info.data[AUX_MOUSEY] = right_y = y;
            info.data[AUX_MOUSESTATUS] = button;
	        (*mouseDownTable[i].MouseFunc)(&info);
	        flag |= GL_TRUE;
	    }
        if ((int)(button & AUX_MIDDLEBUTTON) == mouseDownTable[i].mouseField) 
        {
	        info.event = AUX_MOUSEDOWN;
	        info.data[AUX_MOUSEX] = middle_x = x;
	        info.data[AUX_MOUSEY] = middle_y = y;
            info.data[AUX_MOUSESTATUS] = button;
	        (*mouseDownTable[i].MouseFunc)(&info);
	        flag |= GL_TRUE;
	    }
    }

    return flag;
}

static GLenum CALLBACK KeyDown(int key, GLenum status)
{
GLenum flag;
int i;

   flag = GL_FALSE;
   if (keyTableCount) 
      {
	   for (i = 0; i < keyTableCount; i++) 
         {
	      if (key == keyTable[i].keyField) 
            {
		      (*keyTable[i].KeyFunc)(status);
		      flag |= GL_TRUE;
	         }
	      }
      }

   return flag;
}

void APIENTRY auxExposeFunc(AUXEXPOSEPROC Func)
{
    tkExposeFunc(Func);
}

void APIENTRY auxReshapeFunc(AUXRESHAPEPROC Func)
{
    tkExposeFunc((AUXEXPOSEPROC) Func);
    tkReshapeFunc(Func);
}

void APIENTRY auxIdleFunc(AUXIDLEPROC Func)
{
    tkIdleFunc(Func);
}

void APIENTRY auxCommandFunc(AUXCOMMANDPROC Func)
{
    tkCommandFunc(Func);
}

void APIENTRY auxDestroyFunc(AUXDESTROYPROC Func)
{
    tkDestroyFunc(Func);
}

void APIENTRY auxKeyFunc(int key, AUXKEYPROC Func)
{
    keyTable[keyTableCount].keyField = key;
    keyTable[keyTableCount++].KeyFunc = Func;
}

void APIENTRY auxMouseFunc(int mouse, int mode, AUXMOUSEPROC Func)
{
    if (mode == AUX_MOUSEDOWN) 
    {
	    mouseDownTable[mouseDownTableCount].mouseField = mouse;
	    mouseDownTable[mouseDownTableCount++].MouseFunc = Func;
    } 
    else if (mode == AUX_MOUSEUP) 
    {
     	mouseUpTable[mouseUpTableCount].mouseField = mouse;
	    mouseUpTable[mouseUpTableCount++].MouseFunc = Func;
    } 
    else if (mode == AUX_MOUSELOC) 
    {
        mouseLocTable[mouseLocTableCount].mouseField = mouse;
        mouseLocTable[mouseLocTableCount++].MouseFunc = Func;
    } 
    else if (mode == AUX_MOUSEWHEEL) 
    {
        mouseWheelTable[mouseWheelTableCount].mouseField = mouse;
        mouseWheelTable[mouseWheelTableCount++].MouseFunc = Func;
    }
    else if (mode == AUX_MOUSECLICK)
    {
        mouseClickTable[mouseClickTableCount].mouseField = mouse;
        mouseClickTable[mouseClickTableCount++].MouseFunc = Func;
    }
}

void APIENTRY auxMainLoop(AUXMAINPROC Func)
{
    tkDisplayFunc(Func);
    tkExec();
}

void APIENTRY auxInitPosition(int x, int y, int width, int height)
{
    tkInitPosition(x, y, width, height);
}

void APIENTRY auxInitDisplayMode(GLenum type)
{
    displayModeType = type;
    tkInitDisplayMode(type);
}

void APIENTRY auxInitDisplayModePolicy(GLenum type)
{
    tkInitDisplayModePolicy(type);
}

GLenum APIENTRY auxInitDisplayModeID(GLint id)
{
    return tkInitDisplayModeID(id);
}

GLenum APIENTRY auxInitWindowA(LPCSTR title, BOOL white_bgnd, HMENU menu, BOOL quit_app)
{
    return auxInitWindowAW(title, FALSE, white_bgnd, menu, quit_app);
}

GLenum APIENTRY auxInitWindowW(LPCWSTR title, BOOL white_bgnd, HMENU menu, BOOL quit_app)
{
    return auxInitWindowAW((LPCSTR)title, TRUE, white_bgnd, menu, quit_app);
}

GLenum APIENTRY auxInitWindowAW(LPCSTR title, BOOL bUnicode, BOOL white_bgnd, HMENU menu, BOOL quit_app)
{
int useDoubleAsSingle = 0;

    if (tkInitWindowAW((char *)title, bUnicode, white_bgnd, menu, quit_app) == GL_FALSE)
   	{
		if (AUX_WIND_IS_SINGLE(displayModeType))
      	{
	    	tkInitDisplayMode(displayModeType | AUX_DOUBLE);
            if (tkInitWindowAW((char *)title, bUnicode, white_bgnd, menu, quit_app) == GL_FALSE)
         	{
				return GL_FALSE;    /*  curses, foiled again	*/
            }
         MESSAGEBOX(GetFocus(), "Can't initialize a single buffer visual. "
                                 "Will use a double buffer visual instead, "
                                 "only drawing into the front buffer.",
                                 "Warning", MB_OK);
	    	displayModeType = displayModeType | AUX_DOUBLE;
	    	useDoubleAsSingle = 1;
		}
   }
   tkReshapeFunc(DefaultHandleReshape);
   tkExposeFunc(DefaultHandleExpose);
   tkMouseUpFunc(MouseUp);
   tkMouseDownFunc(MouseDown);
   tkMouseMoveFunc(MouseLoc);
   tkMouseWheelFunc(MouseWheel);
   tkKeyDownFunc(KeyDown);
   if (white_bgnd)
       glClearColor((GLclampf)1.0, (GLclampf)1.0, (GLclampf)1.0, (GLclampf)1.0);
   else
       glClearColor((GLclampf)0.0, (GLclampf)0.0, (GLclampf)0.0, (GLclampf)1.0);

   glClearIndex((GLfloat)0.0);
   glLoadIdentity();
   if (useDoubleAsSingle)
		glDrawBuffer(GL_FRONT);
   return GL_TRUE;
}

void APIENTRY auxCloseWindow(void)
{
    tkCloseWindow();
    keyTableCount = 0;
    mouseDownTableCount = 0;
    mouseUpTableCount = 0;
    mouseLocTableCount = 0;
    mouseWheelTableCount = 0;
    mouseClickTableCount = 0;
}

void APIENTRY auxQuit(void)
{
    tkQuit();
}

void APIENTRY auxSwapBuffers(void)
{
    tkSwapBuffers();
}

HWND APIENTRY auxGetHWND(void)
{
    return tkGetHWND();
}

HDC APIENTRY auxGetHDC(void)
{
    return tkGetHDC();
}

HGLRC APIENTRY auxGetHGLRC(void)
{
    return tkGetHRC();
}

GLenum APIENTRY auxGetDisplayModePolicy(void)
{
    return tkGetDisplayModePolicy();
}

GLint APIENTRY auxGetDisplayModeID(void)
{
    return tkGetDisplayModeID();
}

GLenum APIENTRY auxGetDisplayMode(void)
{
    return tkGetDisplayMode();
}

void APIENTRY auxSetOneColor(int index, float r, float g, float b)
{
    tkSetOneColor(index, r, g, b);
}

void APIENTRY auxSetFogRamp(int density, int startIndex)
{
    tkSetFogRamp(density, startIndex);
}

void APIENTRY auxSetGreyRamp(void)
{
    tkSetGreyRamp();
}

void APIENTRY auxSetRGBMap(int size, float *rgb)
{
    tkSetRGBMap(size, rgb);
}

int APIENTRY auxGetColorMapSize(void)
{
    return tkGetColorMapSize();
}

void APIENTRY auxGetMouseLoc(int *x, int *y)
{
    tkGetMouseLoc(x, y);
}
