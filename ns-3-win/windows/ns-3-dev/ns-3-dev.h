
// ns-3-dev.h : main header file for the PROJECT_NAME application
//

#pragma once

#ifndef __AFXWIN_H__
	#error "include 'stdafx.h' before including this file for PCH"
#endif

#include "resource.h"		// main symbols


// Cns3devApp:
// See ns-3-dev.cpp for the implementation of this class
//

class Cns3devApp : public CWinApp
{
public:
	Cns3devApp();

// Overrides
public:
	virtual BOOL InitInstance();

// Implementation

	DECLARE_MESSAGE_MAP()
};

extern Cns3devApp theApp;