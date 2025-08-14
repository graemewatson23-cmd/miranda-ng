#include "stdafx.h"

static INT_PTR Service_GetCaps(WPARAM wParam, LPARAM)
{
	switch (wParam) {
	case PFLAGNUM_4:
		return PF4_NOCUSTOMAUTH;
	case PFLAG_UNIQUEIDTEXT:
		return (INT_PTR)TranslateT("Nick");
	case PFLAG_MAXLENOFMESSAGE:
		return 400;
	case PFLAGNUM_2:
	case PFLAGNUM_5:
		return PF2_ONLINE | PF2_SHORTAWAY | PF2_LONGAWAY | PF2_LIGHTDND;
	}

	return 0;
}

static INT_PTR Service_LoadIcon(WPARAM wParam, LPARAM)
{
	if (LOWORD(wParam) == PLI_PROTOCOL)
		return (INT_PTR)CopyIcon(g_plugin.getIcon(IDI_CHECKMAIL)); // noone cares about other than PLI_PROTOCOL

	return (INT_PTR)(HICON)NULL;
}

static INT_PTR ContactApplication(WPARAM hContact, LPARAM)
{
	char *szProto = Proto_GetBaseAccountName(hContact);
	if (mir_strcmp(szProto, YAMN_DBMODULE))
		return 0;

	if (CAccount *ActualAccount = FindAccountByContact(POP3Plugin, hContact)) {
		SReadGuard sra(ActualAccount->AccountAccessSO);
		if (sra.Succeeded()) {
			if (ActualAccount->NewMailN.App != nullptr) {
				CMStringW wszCommand(L"\"");
				wszCommand.Append(ActualAccount->NewMailN.App);
				wszCommand.Append(L"\" ");
				if (ActualAccount->NewMailN.AppParam != nullptr)
					wszCommand.Append(ActualAccount->NewMailN.AppParam);

				PROCESS_INFORMATION pi;
				STARTUPINFOW si = {};
				si.cb = sizeof(si);
				CreateProcessW(nullptr, wszCommand.GetBuffer(), nullptr, nullptr, FALSE, NORMAL_PRIORITY_CLASS, nullptr, nullptr, &si, &pi);
			}
		}
	}
	return 0;
}

// This service will check/synchronize the account
static INT_PTR ContactMailCheck(WPARAM hContact, LPARAM)
{
	char *szProto = Proto_GetBaseAccountName(hContact);
	if (mir_strcmp(szProto, YAMN_DBMODULE))
		return 0;

	if (CAccount *ActualAccount = FindAccountByContact(POP3Plugin, hContact)) {
		// if we want to close miranda, we get event and do not run pop3 checking anymore
		if (WAIT_OBJECT_0 == WaitForSingleObject(ExitEV, 0))
			return 0;

		mir_cslock lck(PluginRegCS);
		SReadGuard sra(ActualAccount->AccountAccessSO);
		if (sra.Succeeded()) {
			// account cannot be forced to check
			if ((ActualAccount->Flags & YAMN_ACC_ENA) && (ActualAccount->StatusFlags & YAMN_ACC_FORCE))
				mir_forkThread<CheckParam>(ActualAccount->Plugin->Fcn->ForceCheckFcnPtr, new CheckParam(ActualAccount, g_plugin.CheckFlags()));
		}
	}
	return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////

static void ContactDoubleclicked(WPARAM hContact, LPARAM)
{
	char *szProto = Proto_GetBaseAccountName(hContact);
	if (mir_strcmp(szProto, YAMN_DBMODULE))
		return;

	if (CAccount *ActualAccount = FindAccountByContact(POP3Plugin, hContact)) {
		SReadGuard sra(ActualAccount->AccountAccessSO);
		if (sra.Succeeded()) {
			YAMN_MAILBROWSERPARAM Param = { ActualAccount, ActualAccount->NewMailN.Flags, ActualAccount->NoNewMailN.Flags, nullptr };

			Param.nnflags = Param.nnflags | YAMN_ACC_MSG;			// show mails in account even no new mail in account
			Param.nnflags = Param.nnflags & ~YAMN_ACC_POP;

			Param.nflags = Param.nflags | YAMN_ACC_MSG;			// show mails in account even no new mail in account
			Param.nflags = Param.nflags & ~YAMN_ACC_POP;

			RunMailBrowser(&Param);
		}
	}
}

INT_PTR ClistContactDoubleclicked(WPARAM, LPARAM lParam)
{
	ContactDoubleclicked(((CLISTEVENT *)lParam)->lParam, lParam);
	return 0;
}

static int Service_ContactDoubleclicked(WPARAM wParam, LPARAM lParam)
{
	ContactDoubleclicked(wParam, lParam);
	return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////

HBITMAP LoadBmpFromIcon(HICON hIcon)
{
	int IconSizeX = 16;
	int IconSizeY = 16;

	HBRUSH hBkgBrush = CreateSolidBrush(GetSysColor(COLOR_3DFACE));

	BITMAPINFOHEADER bih = {};
	bih.biSize = sizeof(bih);
	bih.biBitCount = 24;
	bih.biPlanes = 1;
	bih.biCompression = BI_RGB;
	bih.biHeight = IconSizeY;
	bih.biWidth = IconSizeX;

	RECT rc;
	rc.top = rc.left = 0;
	rc.right = bih.biWidth;
	rc.bottom = bih.biHeight;

	HDC hdc = GetDC(nullptr);
	HBITMAP hBmp = CreateCompatibleBitmap(hdc, bih.biWidth, bih.biHeight);
	HDC hdcMem = CreateCompatibleDC(hdc);
	HBITMAP hoBmp = (HBITMAP)SelectObject(hdcMem, hBmp);
	FillRect(hdcMem, &rc, hBkgBrush);
	DrawIconEx(hdcMem, 0, 0, hIcon, bih.biWidth, bih.biHeight, 0, nullptr, DI_NORMAL);
	SelectObject(hdcMem, hoBmp);
	return hBmp;
}

static int AddTopToolbarIcon(WPARAM, LPARAM)
{
	TTBButton btn = {};
	btn.pszService = MS_YAMN_FORCECHECK;
	btn.dwFlags = TTBBF_VISIBLE | TTBBF_SHOWTOOLTIP;
	btn.hIconHandleUp = btn.hIconHandleDn = g_plugin.getIconHandle(IDI_CHECKMAIL);
	btn.name = btn.pszTooltipUp = LPGEN("Check mail");
	g_plugin.addTTB(&btn);
	return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////

int Shutdown(WPARAM, LPARAM)
{
	g_plugin.setDword(YAMN_DBMSGPOSX, HeadPosX);
	g_plugin.setDword(YAMN_DBMSGPOSY, HeadPosY);
	g_plugin.setDword(YAMN_DBMSGSIZEX, HeadSizeX);
	g_plugin.setDword(YAMN_DBMSGSIZEY, HeadSizeY);
	g_plugin.setWord(YAMN_DBMSGPOSSPLIT, HeadSplitPos);
	g_bShutdown = true;
	KillTimer(nullptr, SecTimer);

	UnregisterProtoPlugins();
	return 0;
}

int SystemModulesLoaded(WPARAM, LPARAM); // in main.cpp

void HookEvents(void)
{
	HookTemporaryEvent(ME_TTB_MODULELOADED, AddTopToolbarIcon);

	HookEvent(ME_SYSTEM_MODULESLOADED, SystemModulesLoaded);
	HookEvent(ME_OPT_INITIALISE, YAMNOptInitSvc);
	HookEvent(ME_SYSTEM_PRESHUTDOWN, Shutdown);
	HookEvent(ME_CLIST_DOUBLECLICKED, Service_ContactDoubleclicked);
}

void CreateServiceFunctions(void)
{
	// Standard 'protocol' services
	CreateServiceFunction(YAMN_DBMODULE PS_GETCAPS, Service_GetCaps);
	CreateServiceFunction(YAMN_DBMODULE PS_LOADICON, Service_LoadIcon);

	// Checks mail
	CreateServiceFunction(MS_YAMN_FORCECHECK, ForceCheckSvc);

	// Function contact list double click
	CreateServiceFunction(MS_YAMN_CLISTDBLCLICK, ClistContactDoubleclicked);

	// Function contact list context menu click
	CreateServiceFunction(MS_YAMN_CLISTCONTEXT, ContactMailCheck);

	// Function contact list context menu click
	CreateServiceFunction(MS_YAMN_CLISTCONTEXTAPP, ContactApplication);
}
