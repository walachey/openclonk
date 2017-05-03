/*
 * OpenClonk, http://www.openclonk.org
 *
 * Copyright (c) 2006-2009, RedWolf Design GmbH, http://www.clonk.de/
 * Copyright (c) 2010-2017, The OpenClonk Team and contributors
 *
 * Distributed under the terms of the ISC license; see accompanying file
 * "COPYING" for details.
 *
 * "Clonk" is a registered trademark of Matthes Bender, used with permission.
 * See accompanying file "TRADEMARK" for details.
 *
 * To redistribute this file separately, substitute the full license texts
 * for the above references.
 */
// Screen for mod handling

#ifndef INC_C4StartupModsDlg
#define INC_C4StartupModsDlg

#include "gui/C4Startup.h"
#include "network/C4InteractiveThread.h"
#include "network/C4Network2Reference.h"

class TiXmlElement;

class C4StartupModsListEntry : public C4GUI::Window
{
public:
	C4StartupModsListEntry(C4GUI::ListBox *pForListBox, C4GUI::Element *pInsertBefore, class C4StartupModsDlg *pModsDlg);
	~C4StartupModsListEntry();

	
	enum { InfoLabelCount=5, MaxInfoIconCount=10 };

private:
	C4StartupModsDlg *pModsDlg;
	C4GUI::ListBox *pList;

	bool fError;                     // if set, the label was changed to an error message and no more updates are done
	StdStrBuf sError;

	C4GUI::Icon *pIcon;       // scenario icon
	C4GUI::Label *pInfoLbl[InfoLabelCount]; // info labels for reference or query; left side
	C4GUI::Label *pInfoLabelsRight[InfoLabelCount]; // info labels on the right side
	C4GUI::Icon *pInfoIcons[MaxInfoIconCount]; // right-aligned status icons at topright position
	int32_t iInfoIconCount;
	int32_t iSortOrder;
	bool fIsSmall;         // set if the item is in collapsed state
	bool fIsCollapsed;     // set if the item is forced to collapsed state
	bool fIsEnabled;       // if not set, the item is grayed out
	bool fIsImportant;     // if set, the item is presented in yellow (lower priority than fIsEnabled)
	C4Rect rctIconSmall;    // bounds for small icon
	C4Rect rctIconLarge;    // bounds for large icon

	StdStrBuf sInfoText[InfoLabelCount];
	StdStrBuf sInfoTextRight[InfoLabelCount];

	void SetError(const char *szErrorText);      // change secondary label to error label, mark error and set a removal timer
	//C4StartupModsListEntry *AddReference(C4Network2Reference *pAddRef, C4GUI::Element *pInsertBefore); // add a reference list item to the same list
	void InvalidateStatusIcons() { iInfoIconCount=0; } // schedule all current status icons for removal when UpdateText is called next
	void AddStatusIcon(C4GUI::Icons eIcon, const char *szToolTip); // add a status icon with the specified tooltip

	void UpdateSmallState();
	void UpdateEntrySize();
	void UpdateText(); // strings to labels

protected:
	virtual int32_t GetListItemTopSpacing() { return fIsCollapsed ? 5 : 10; }
	virtual void DrawElement(C4TargetFacet &cgo);

	C4GUI::Element* GetNextLower(int32_t sortOrder); // returns the element before which this element should be inserted

public:
	void FromXML(const TiXmlElement *xml);
	void ClearRef();    // del any ref/refclient/error data
	
	bool Execute(); // update stuff - if false is returned, the item is to be removed
	void UpdateCollapsed(bool fToCollapseValue);
	void SetVisibility(bool fToValue);

	// There is a special entry that conveys status information.
	void MakeInfoEntry();
	void OnNoResultsFound();
	void OnError(std::string message);

	const char *GetError() { return fError ? sError.getData() : nullptr; } // return error message, if any is set
	//C4Network2Reference *GrabReference(); // grab the reference so it won't be deleted when this item is removed
	//C4Network2Reference *GetReference() const { return pRef; } // have a look at the reference
	//bool IsSameHost(const C4Network2Reference *pRef2); // check whether the reference was created by the same host as this one
	//bool IsSameAddress(const C4Network2Reference *pRef2); // check whether there is at least one matching address (address and port)
	bool KeywordMatch(const char *szMatch); // check whether any of the reference contents match a given keyword

};

// startup dialog: Network game selection
class C4StartupModsDlg : public C4StartupDlg, private C4InteractiveThread::Callback, private C4ApplicationSec1Timer
{
public:
	C4StartupModsDlg(); // ctor
	~C4StartupModsDlg(); // dtor

private:
	C4GUI::Tabular *pMainTabular;   // main tabular control: Contains game selection list and chat control
	C4GUI::ListBox *pGameSelList;        // game selection listbox
	C4KeyBinding *pKeyRefresh, *pKeyBack, *pKeyForward;
	//C4GUI::CallbackButton<C4StartupNetDlg, C4GUI::IconButton> *btnUpdate;
	C4GUI::Button *btnJoin, *btnRefresh;
	C4GUI::Edit *pSearchFieldEdt;
	C4StartupModsListEntry *pMasterserverClient; // set if masterserver query is enabled: Checks clonk.de for new games
	bool fIsCollapsed; // set if the number of games in the list requires them to be displayed in a condensed format
	// Whether the last query was successful. No re-fetching will be done.
	bool queryWasSuccessful = false;

protected:
	virtual bool HasBackground() { return false; }
	virtual void DrawElement(C4TargetFacet &cgo);

	virtual C4GUI::Control *GetDefaultControl(); // get Auto-Focus control
	C4GUI::Control *GetDlgModeFocusControl(); // get control to be focused when main tabular sheet changes

	virtual bool OnEnter() { DoOK(); return true; }
	virtual bool OnEscape() { DoBack(); return true; }
	bool KeyBack() { return DoBack(); }
	bool KeyRefresh() { DoRefresh(); return true; }
	bool KeyForward() { DoOK(); return true; }

	virtual void OnShown();             // callback when shown: Start searching for games
	virtual void OnClosed(bool fOK);    // callback when dlg got closed: Return to main screen
	void OnBackBtn(C4GUI::Control *btn) { DoBack(); }
	void OnRefreshBtn(C4GUI::Control *btn) { DoRefresh(); }
	void OnInstallModBtn(C4GUI::Control *btn) { DoOK(); }
	void OnSelChange(class C4GUI::Element *pEl) { UpdateSelection(true); }
	void OnSelDblClick(class C4GUI::Element *pEl) { DoOK(); }
	//void OnBtnUpdate(C4GUI::Control *btn);
	C4GUI::Edit::InputResult OnSearchFieldEnter(C4GUI::Edit *edt, bool fPasting, bool fPastingMore)
	{ DoOK(); return C4GUI::Edit::IR_Abort; }

private:
	// Deletes lingering updates etc.
	void CancelRequest();

	void QueryModList();
	void ClearList();
	void UpdateList(bool fGotReference = false);
	void UpdateCollapsed();
	void UpdateSelection(bool fUpdateCollapsed);

	//void AddReferenceQuery(const char *szAddress, C4StartupNetListEntry::QueryType eQueryType); // add a ref searcher entry and start searching

	// set during update information retrieval
	std::unique_ptr<C4Network2HTTPClient> postClient;

	// callback from C4Network2ReferenceClient
	virtual void OnThreadEvent(C4InteractiveEventType eEvent, void *pEventData);

public:
	bool DoOK(); // join currently selected item
	bool DoBack(); // abort dialog
	void DoRefresh(); // restart network search

	//void OnReferenceEntryAdd(C4StartupNetListEntry *pEntry);

	void OnSec1Timer(); // idle proc: update list
};


#endif // INC_C4StartupModsDlg
