/*

Miranda NG: the free IM client for Microsoft* Windows*

Copyright 2012-25 Miranda NG team,
all portions of this codebase are copyrighted to the people
listed in contributors.txt.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#pragma once

class MDatabaseCache : public MIDatabaseCache
{
	char* m_lastSetting;
	MIDatabase* m_db;
	DBCachedContact *m_lastVL;
	mir_cs m_csContact, m_csVal;

	LIST<DBCachedContact> m_lContacts;
	LIST<DBCachedGlobalValue> m_lGlobalSettings;
	LIST<char> m_lSettings;

	void FreeCachedVariant(DBVARIANT* V);

public:
	MDatabaseCache(MIDatabase*);
	~MDatabaseCache();

protected:
	STDMETHODIMP_(DBCachedContact*) AddContactToCache(MCONTACT contactID);
	STDMETHODIMP_(DBCachedContact*) GetCachedContact(MCONTACT contactID);
	STDMETHODIMP_(DBCachedContact*) GetFirstContact(void);
	STDMETHODIMP_(DBCachedContact*) GetNextContact(MCONTACT contactID);
	STDMETHODIMP_(void) FreeCachedContact(MCONTACT contactID);

	STDMETHODIMP_(char*) InsertCachedSetting(const char *szName, size_t size);
	STDMETHODIMP_(char*) GetCachedSetting(const char *szModuleName, const char *szSettingName, size_t, size_t);
	STDMETHODIMP_(void)  SetCachedVariant(DBVARIANT *s, DBVARIANT *d);
	STDMETHODIMP_(DBVARIANT*) GetCachedValuePtr(MCONTACT contactID, char *szSetting, int bAllocate);
};

extern HANDLE g_hevEventReaction;
