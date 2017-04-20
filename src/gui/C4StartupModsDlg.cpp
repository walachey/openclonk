/*
 * OpenClonk, http://www.openclonk.org
 *
 * Copyright (c) 2006-2009, RedWolf Design GmbH, http://www.clonk.de/
 * Copyright (c) 2009-2017, The OpenClonk Team and contributors
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

#include "C4Include.h"
#include "gui/C4StartupModsDlg.h"

#include "game/C4Application.h"
#include "gui/C4UpdateDlg.h"
#include "game/C4Game.h"
#include "graphics/C4GraphicsResource.h"
#include "graphics/C4Draw.h"

#include <tinyxml.h>

// ----------- C4StartupNetListEntry -----------------------------------------------------------------------

C4StartupModsListEntry::C4StartupModsListEntry(C4GUI::ListBox *pForListBox, C4GUI::Element *pInsertBefore, C4StartupModsDlg *pModsDlg)
		: pModsDlg(pModsDlg), pList(pForListBox), fError(false), iInfoIconCount(0), iSortOrder(0), fIsSmall(false), fIsCollapsed(false), fIsEnabled(true), fIsImportant(false)
{
	// calc height
	int32_t iLineHgt = ::GraphicsResource.TextFont.GetLineHeight(), iHeight = iLineHgt * 2 + 4;
	// add icons - normal icons use small size, only animated netgetref uses full size
	rctIconLarge.Set(0, 0, iHeight, iHeight);
	int32_t iSmallIcon = iHeight * 2 / 3; rctIconSmall.Set((iHeight - iSmallIcon)/2, (iHeight - iSmallIcon)/2, iSmallIcon, iSmallIcon);
	pIcon = new C4GUI::Icon(rctIconSmall, C4GUI::Ico_Definition);
	AddElement(pIcon);
	SetBounds(pIcon->GetBounds());
	// add to listbox (will get resized horizontally and moved)
	pForListBox->InsertElement(this, pInsertBefore);
	// add status icons and text labels now that width is known
	CStdFont *pUseFont = &(::GraphicsResource.TextFont);
	int32_t iIconSize = pUseFont->GetLineHeight();
	C4Rect rcIconRect = GetContainedClientRect();
	int32_t iThisWdt = rcIconRect.Wdt;
	rcIconRect.x = iThisWdt - iIconSize * (iInfoIconCount + 1);
	rcIconRect.Wdt = rcIconRect.Hgt = iIconSize;
	for (int32_t iIcon = 0; iIcon<MaxInfoIconCount; ++iIcon)
	{
		AddElement(pInfoIcons[iIcon] = new C4GUI::Icon(rcIconRect, C4GUI::Ico_None));
		rcIconRect.x -= rcIconRect.Wdt;
	}
	C4Rect rcLabelBounds;
	rcLabelBounds.x = iHeight+3;
	rcLabelBounds.Hgt = iLineHgt;
	for (int i=0; i<InfoLabelCount; ++i)
	{
		C4GUI::Label *pLbl;
		rcLabelBounds.y = 1+i*(iLineHgt+2);
		rcLabelBounds.Wdt = iThisWdt - rcLabelBounds.x - 1;
		if (!i) rcLabelBounds.Wdt -= iLineHgt; // leave space for topright extra icon
		AddElement(pLbl = pInfoLbl[i] = new C4GUI::Label("", rcLabelBounds, ALeft, C4GUI_CaptionFontClr));
		// label will have collapsed due to no text: Repair it
		pLbl->SetAutosize(false);
		pLbl->SetBounds(rcLabelBounds);
	}
	// update small state, which will resize this to a small entry
	UpdateSmallState();
	// Set*-function will fill icon and text and calculate actual size
}

C4StartupModsListEntry::~C4StartupModsListEntry()
{
	ClearRef();
}

void C4StartupModsListEntry::FromXML(const TiXmlElement *xml)
{
	auto getSafeStringValue = [](const TiXmlElement *xml, const char *childName, std::string fallback="")
	{
		const TiXmlElement *child = xml->FirstChildElement(childName);
		if (child == nullptr) return fallback;
		const std::string value(child->GetText());
		if (!value.empty()) return value;
		return fallback;
	};

	sInfoText[0].Format(LoadResStr("IDS_MODS_TITLE"), getSafeStringValue(xml, "title", "???").c_str(), getSafeStringValue(xml, "author", "???").c_str());
	std::string description = getSafeStringValue(xml, "description");
	if (!description.empty())
	{
		if (description.size() > 42)
		{
			description.resize(42);
			description += "...";
		}
		sInfoText[1].Format("%s", description.c_str());
	}

	UpdateText();
}

void C4StartupModsListEntry::DrawElement(C4TargetFacet &cgo)
{
	typedef C4GUI::Window ParentClass;
	// background if important and not selected
	if (fIsImportant && !IsSelectedChild(this))
	{
		int32_t x1 = cgo.X+cgo.TargetX+rcBounds.x;
		int32_t y1 = cgo.Y+cgo.TargetY+rcBounds.y;
		pDraw->DrawBoxDw(cgo.Surface, x1,y1, x1+rcBounds.Wdt, y1+rcBounds.Hgt, C4GUI_ImportantBGColor);
	}
	// inherited
	ParentClass::DrawElement(cgo);
}

void C4StartupModsListEntry::ClearRef()
{
	fError = false;
	sError.Clear();
	int32_t i;
	for (i=0; i<InfoLabelCount; ++i) sInfoText[i].Clear();
	InvalidateStatusIcons();
	fIsEnabled = true;
	fIsImportant = false;
}

bool C4StartupModsListEntry::Execute()
{
	return true;
}

C4GUI::Element* C4StartupModsListEntry::GetNextLower(int32_t sortOrder)
{
	// search list for the next element of a lower sort order
	for (C4GUI::Element *pElem = pList->GetFirst(); pElem; pElem = pElem->GetNext())
	{
		C4StartupModsListEntry *pEntry = static_cast<C4StartupModsListEntry *>(pElem);
		if (pEntry->iSortOrder < sortOrder)
			return pElem;
	}
	// none found: insert at start
	return nullptr;
}

void C4StartupModsListEntry::UpdateCollapsed(bool fToCollapseValue)
{
	// if collapsed state changed, update the text
	if (fIsCollapsed == fToCollapseValue) return;
	fIsCollapsed = fToCollapseValue;
	UpdateSmallState();
}

void C4StartupModsListEntry::UpdateSmallState()
{
	// small view: Always collapsed if there is no extended text
	bool fNewIsSmall = !sInfoText[2].getLength() || fIsCollapsed;
	if (fNewIsSmall == fIsSmall) return;
	fIsSmall = fNewIsSmall;
	for (int i=2; i<InfoLabelCount; ++i) pInfoLbl[i]->SetVisibility(!fIsSmall);
	UpdateEntrySize();
}

void C4StartupModsListEntry::UpdateEntrySize()
{
	if(fVisible) {
		// restack all labels by their size
		int32_t iLblCnt = (fIsSmall ? 2 : InfoLabelCount), iY=1;
		for (int i=0; i<iLblCnt; ++i)
		{
			C4Rect rcBounds = pInfoLbl[i]->GetBounds();
			rcBounds.y = iY;
				iY += rcBounds.Hgt + 2;
		pInfoLbl[i]->SetBounds(rcBounds);
		}
		// resize this control
		GetBounds().Hgt = iY-1;
	} else GetBounds().Hgt = 0;
	UpdateSize();
}

void C4StartupModsListEntry::UpdateText()
{
	bool fRestackElements=false;
	CStdFont *pUseFont = &(::GraphicsResource.TextFont);
	// adjust icons
	int32_t sx=iInfoIconCount*pUseFont->GetLineHeight();
	int32_t i;
	for (i=iInfoIconCount; i<MaxInfoIconCount; ++i)
	{
		pInfoIcons[i]->SetIcon(C4GUI::Ico_None);
		pInfoIcons[i]->SetToolTip(nullptr);
	}
	// text to labels
	for (i=0; i<InfoLabelCount; ++i)
	{
		int iAvailableWdt = GetClientRect().Wdt - pInfoLbl[i]->GetBounds().x - 1;
		if (!i) iAvailableWdt -= sx;
		StdStrBuf BrokenText;
		pUseFont->BreakMessage(sInfoText[i].getData(), iAvailableWdt, &BrokenText, true);
		int32_t iHgt, iWdt;
		if (pUseFont->GetTextExtent(BrokenText.getData(), iWdt, iHgt, true))
		{
			if ((pInfoLbl[i]->GetBounds().Hgt != iHgt) || (pInfoLbl[i]->GetBounds().Wdt != iAvailableWdt))
			{
				C4Rect rcBounds = pInfoLbl[i]->GetBounds();
				rcBounds.Wdt = iAvailableWdt;
				rcBounds.Hgt = iHgt;
				pInfoLbl[i]->SetBounds(rcBounds);
				fRestackElements = true;
			}
		}
		pInfoLbl[i]->SetText(BrokenText.getData());
		pInfoLbl[i]->SetColor(fIsEnabled ? C4GUI_MessageFontClr : C4GUI_InactMessageFontClr);
	}
	if (fRestackElements) UpdateEntrySize();
}

void C4StartupModsListEntry::SetVisibility(bool fToValue) {
	bool fChange = fToValue != fVisible;
	C4GUI::Window::SetVisibility(fToValue);
	if(fChange) UpdateEntrySize();
}

void C4StartupModsListEntry::AddStatusIcon(C4GUI::Icons eIcon, const char *szToolTip)
{
	// safety
	if (iInfoIconCount==MaxInfoIconCount) return;
	// set icon to the left of the existing icons to the desired data
	pInfoIcons[iInfoIconCount]->SetIcon(eIcon);
	pInfoIcons[iInfoIconCount]->SetToolTip(szToolTip);
	++iInfoIconCount;
}

void C4StartupModsListEntry::SetError(const char *szErrorText)
{
	// set error message
	fError = true;
	sInfoText[1].Copy(szErrorText);
	for (int i=2; i<InfoLabelCount; ++i) sInfoText[i].Clear();
	InvalidateStatusIcons();
	UpdateSmallState(); UpdateText();
	pIcon->SetIcon(C4GUI::Ico_Close);
	pIcon->SetAnimated(false, 0);
	pIcon->SetBounds(rctIconSmall);
}

bool C4StartupModsListEntry::KeywordMatch(const char *szMatch)
{
	return false;
}

// ----------- C4StartupNetDlg ---------------------------------------------------------------------------------

C4StartupModsDlg::C4StartupModsDlg() : C4StartupDlg(LoadResStr("IDS_DLG_MODS")), pMasterserverClient(nullptr), fIsCollapsed(false), fUpdatingList(false)
{
	// ctor
	// key bindings
	C4CustomKey::CodeList keys;
	keys.push_back(C4KeyCodeEx(K_BACK)); keys.push_back(C4KeyCodeEx(K_LEFT));
	pKeyBack = new C4KeyBinding(keys, "StartupNetBack", KEYSCOPE_Gui,
	                            new C4GUI::DlgKeyCB<C4StartupModsDlg>(*this, &C4StartupModsDlg::KeyBack), C4CustomKey::PRIO_Dlg);
	pKeyRefresh = new C4KeyBinding(C4KeyCodeEx(K_F5), "StartupNetReload", KEYSCOPE_Gui,
	                               new C4GUI::DlgKeyCB<C4StartupModsDlg>(*this, &C4StartupModsDlg::KeyRefresh), C4CustomKey::PRIO_CtrlOverride);

	// screen calculations
	UpdateSize();
	int32_t iIconSize = C4GUI_IconExWdt;
	int32_t iButtonWidth,iCaptionFontHgt, iSideSize = std::max<int32_t>(GetBounds().Wdt/6, iIconSize);
	int32_t iButtonHeight = C4GUI_ButtonHgt, iButtonIndent = GetBounds().Wdt/40;
	::GraphicsResource.CaptionFont.GetTextExtent("<< BACK", iButtonWidth, iCaptionFontHgt, true);
	iButtonWidth *= 3;
	C4GUI::ComponentAligner caMain(GetClientRect(), 0,0, true);
	C4GUI::ComponentAligner caButtonArea(caMain.GetFromBottom(caMain.GetHeight()/7),0,0);
	int32_t iButtonAreaWdt = caButtonArea.GetWidth()*7/8;
	iButtonWidth = std::min<int32_t>(iButtonWidth, (iButtonAreaWdt - 8 * iButtonIndent)/4);
	iButtonIndent = (iButtonAreaWdt - 4 * iButtonWidth) / 8;
	C4GUI::ComponentAligner caButtons(caButtonArea.GetCentered(iButtonAreaWdt, iButtonHeight),iButtonIndent,0);
	C4GUI::ComponentAligner caLeftBtnArea(caMain.GetFromLeft(iSideSize), std::min<int32_t>(caMain.GetWidth()/20, (iSideSize-C4GUI_IconExWdt)/2), caMain.GetHeight()/40);
	C4GUI::ComponentAligner caConfigArea(caMain.GetFromRight(iSideSize), std::min<int32_t>(caMain.GetWidth()/20, (iSideSize-C4GUI_IconExWdt)/2), caMain.GetHeight()/40);

	// main area: Tabular to switch between game list and chat
	pMainTabular = new C4GUI::Tabular(caMain.GetAll(), C4GUI::Tabular::tbNone);
	pMainTabular->SetDrawDecoration(false);
	pMainTabular->SetSheetMargin(0);
	AddElement(pMainTabular);

	// main area: game selection sheet
	C4GUI::Tabular::Sheet *pSheetGameList = pMainTabular->AddSheet(nullptr);
	C4GUI::ComponentAligner caGameList(pSheetGameList->GetContainedClientRect(), 0,0, false);
	C4GUI::WoodenLabel *pGameListLbl; int32_t iCaptHgt = C4GUI::WoodenLabel::GetDefaultHeight(&::GraphicsResource.TextFont);
	pGameListLbl = new C4GUI::WoodenLabel(LoadResStr("IDS_MODS_MODSLIST"), caGameList.GetFromTop(iCaptHgt), C4GUI_Caption2FontClr, &::GraphicsResource.TextFont, ALeft);
	// search field
	C4GUI::WoodenLabel *pSearchLbl;
	const char *szSearchLblText = LoadResStr("IDS_NET_MSSEARCH"); // Text is the same as in the network view.
	int32_t iSearchWdt=100, iSearchHgt;
	::GraphicsResource.TextFont.GetTextExtent(szSearchLblText, iSearchWdt, iSearchHgt, true);
	C4GUI::ComponentAligner caSearch(caGameList.GetFromTop(iSearchHgt), 0,0);
	pSearchLbl = new C4GUI::WoodenLabel(szSearchLblText, caSearch.GetFromLeft(iSearchWdt+10), C4GUI_Caption2FontClr, &::GraphicsResource.TextFont);
	const char *szSearchTip = LoadResStr("IDS_NET_MSSEARCH_DESC");
	pSearchLbl->SetToolTip(szSearchTip);
	pSheetGameList->AddElement(pSearchLbl);
	pSearchFieldEdt = new C4GUI::CallbackEdit<C4StartupModsDlg>(caSearch.GetAll(), this, &C4StartupModsDlg::OnSearchFieldEnter);
	pSearchFieldEdt->SetToolTip(szSearchTip);
	pSheetGameList->AddElement(pSearchFieldEdt);
	pSheetGameList->AddElement(pGameListLbl);
	pGameSelList = new C4GUI::ListBox(caGameList.GetFromTop(caGameList.GetHeight() - iCaptHgt));
	pGameSelList->SetDecoration(true, nullptr, true, true);
	pGameSelList->UpdateElementPositions();
	pGameSelList->SetSelectionDblClickFn(new C4GUI::CallbackHandler<C4StartupModsDlg>(this, &C4StartupModsDlg::OnSelDblClick));
	pGameSelList->SetSelectionChangeCallbackFn(new C4GUI::CallbackHandler<C4StartupModsDlg>(this, &C4StartupModsDlg::OnSelChange));
	pSheetGameList->AddElement(pGameSelList);

	// button area
	C4GUI::CallbackButton<C4StartupModsDlg> *btn;
	AddElement(btn = new C4GUI::CallbackButton<C4StartupModsDlg>(LoadResStr("IDS_BTN_BACK"), caButtons.GetFromLeft(iButtonWidth), &C4StartupModsDlg::OnBackBtn));
	btn->SetToolTip(LoadResStr("IDS_DLGTIP_BACKMAIN"));
	AddElement(btn = new C4GUI::CallbackButton<C4StartupModsDlg>(LoadResStr("IDS_MODS_INSTALL"), caButtons.GetFromLeft(iButtonWidth), &C4StartupModsDlg::OnInstallModBtn));
	btn->SetToolTip(LoadResStr("IDS_MODS_INSTALL_DESC"));
	
	// initial focus
	SetFocus(GetDlgModeFocusControl(), false);
	
	// register timer
	Application.Add(this);

	// register as receiver of reference notifies
	Application.InteractiveThread.SetCallback(Ev_HTTP_Response, this);

}

C4StartupModsDlg::~C4StartupModsDlg()
{
	CancelRequest();
	// disable notifies
	Application.InteractiveThread.ClearCallback(Ev_HTTP_Response, this);

	Application.Remove(this);
	if (pMasterserverClient) delete pMasterserverClient;
	// dtor
	delete pKeyBack;
	delete pKeyRefresh;
}

void C4StartupModsDlg::DrawElement(C4TargetFacet &cgo)
{
	// draw background
	typedef C4GUI::FullscreenDialog Base;
	Base::DrawElement(cgo);
}

void C4StartupModsDlg::OnShown()
{
	// callback when shown: Start searching for games
	C4StartupDlg::OnShown();
	QueryModList();
	OnSec1Timer();
}

void C4StartupModsDlg::OnClosed(bool fOK)
{
	// dlg abort: return to main screen
	CancelRequest();
	if (pMasterserverClient) { delete pMasterserverClient; pMasterserverClient=nullptr; }
	if (!fOK) DoBack();
}

C4GUI::Control *C4StartupModsDlg::GetDefaultControl()
{
	return nullptr;
}

C4GUI::Control *C4StartupModsDlg::GetDlgModeFocusControl()
{
	return pGameSelList;
}

void C4StartupModsDlg::QueryModList()
{
	// Forward the filter-field to the server.
	std::string searchQueryPostfix("");
	if (pSearchFieldEdt->GetText())
	{
		const std::string searchText(pSearchFieldEdt->GetText());
		if (searchText.size() > 0)
		{
			searchQueryPostfix = "?where={%22title%22:%22" + searchText + "%22}";
		}
	}

	// Initialize connection.
	postClient = std::make_unique<C4Network2HTTPClient>();
	
	if (!postClient->Init() || !postClient->SetServer(("frustrum.pictor.uberspace.de/larry/items" + searchQueryPostfix).c_str()))
	{
		assert(false);
		return;
	}
	postClient->SetExpectedResponseType(C4Network2HTTPClient::ResponseType::XML);

	// Do the actual request.
	postClient->SetNotify(&Application.InteractiveThread);
	Application.InteractiveThread.AddProc(postClient.get());
	postClient->Query(nullptr, false); // Empty query.

	/*pMasterserverClient = new C4StartupModsListEntry(pGameSelList, nullptr, this);
	StdStrBuf strVersion; strVersion.Format("%d.%d", C4XVER1, C4XVER2);
	StdStrBuf strQuery; strQuery.Format("%s?version=%s&platform=%s", Config.Network.GetLeagueServerAddress(), strVersion.getData(), C4_OS);
	pMasterserverClient->SetRefQuery(strQuery.getData(), C4StartupNetListEntry::NRQT_Masterserver);*/
}

void C4StartupModsDlg::CancelRequest()
{
	if (!postClient.get()) return;
	Application.InteractiveThread.RemoveProc(postClient.get());
	postClient.reset();
}

void C4StartupModsDlg::UpdateList(bool fGotReference)
{
	Log("Tick");
	// Already running a query?
	if (postClient.get() != nullptr)
	{
		// Check whether the data has arrived yet.
		if (!postClient->isBusy())
		{
			if (!postClient->isSuccess())
			{
				Log(postClient->GetError());
				// Destroy client and try again later.
				CancelRequest();
				Log("Failed :((");
				return;
			}
			Log("Received!");
			Log(postClient->getResultString());

			// Remove all existing items.
			C4GUI::Element *pElem, *pNextElem = pGameSelList->GetFirst();
			while ((pElem = pNextElem))
			{
				pNextElem = pElem->GetNext();
				C4StartupModsListEntry *pEntry = static_cast<C4StartupModsListEntry *>(pElem);
				delete pEntry;
			}

			TiXmlDocument xmlDocument;
			xmlDocument.Parse(postClient->getResultString());

			if (xmlDocument.Error())
			{
				Log(xmlDocument.ErrorDesc());
				return;
			}
			const char * resourceElementName = "resource";
			const TiXmlElement *root = xmlDocument.RootElement();
			assert(strcmp(root->Value(), resourceElementName) != 0);

			for (const TiXmlElement* e = root->FirstChildElement(resourceElementName); e != NULL; e = e->NextSiblingElement(resourceElementName))
			{
				C4StartupModsListEntry *pEntry = new C4StartupModsListEntry(pGameSelList, nullptr, this);
				pEntry->FromXML(e);
			}
		}
	}
	
	pGameSelList->FreezeScrolling();
	// Games display mask
	
	// Update all child entries
	bool fAnyRemoval = false;
	C4GUI::Element *pElem, *pNextElem = pGameSelList->GetFirst();
	while ((pElem=pNextElem))
	{
		pNextElem = pElem->GetNext(); // determine next exec element now - execution
		C4StartupModsListEntry *pEntry = static_cast<C4StartupModsListEntry *>(pElem);
		// do item updates
		//if(pEntry->GetReference()) pEntry->SetVisibility(pEntry->KeywordMatch(szGameMask));
		bool fKeepEntry = true;
		if (fKeepEntry)
			fKeepEntry = pEntry->Execute();
		// remove?
		if (!fKeepEntry)
		{
			// entry wishes to be removed
			// if the selected entry is being removed, the next entry should be selected (which might be the ref for a finished refquery)
			if (pGameSelList->GetSelectedItem() == pEntry)
				if (pEntry->GetNext())
				{
					pGameSelList->SelectEntry(pEntry->GetNext(), false);
				}
			delete pEntry;
			fAnyRemoval = true; // setting any removal will also update collapsed state of all entries; so no need to do updates because of selection change here
		}
	}

	// check whether view needs to be collapsed or uncollapsed
	if (fIsCollapsed && fAnyRemoval)
	{
		// try uncollapsing
		fIsCollapsed = false;
		UpdateCollapsed();
		// if scrolling is still necessary, the view will be collapsed again immediately
	}
	if (!fIsCollapsed && pGameSelList->IsScrollingNecessary())
	{
		fIsCollapsed = true;
		UpdateCollapsed();
	}

	fUpdatingList = false;
	// done; selection might have changed
	pGameSelList->UnFreezeScrolling();
	UpdateSelection(false);
}

void C4StartupModsDlg::UpdateCollapsed()
{
	// update collapsed state for all child entries
	for (C4GUI::Element *pElem = pGameSelList->GetFirst(); pElem; pElem = pElem->GetNext())
	{
		C4StartupModsListEntry *pEntry = static_cast<C4StartupModsListEntry *>(pElem);
		pEntry->UpdateCollapsed(fIsCollapsed && pElem != pGameSelList->GetSelectedItem());
	}
}

void C4StartupModsDlg::UpdateSelection(bool fUpdateCollapsed)
{
	// not during list updates - list update call will do this
	if (fUpdatingList) return;
	// in collapsed view, updating the selection may uncollapse something
	if (fIsCollapsed && fUpdateCollapsed) UpdateCollapsed();
}

void C4StartupModsDlg::OnThreadEvent(C4InteractiveEventType eEvent, void *pEventData)
{
	UpdateList(true);
}

bool C4StartupModsDlg::DoOK()
{
	if (GetFocus() == pSearchFieldEdt)
	{
		UpdateList();
		return true;
	}
	// get currently selected item
	C4GUI::Element *pSelection = pGameSelList->GetSelectedItem();
	StdCopyStrBuf strNoJoin(LoadResStr("IDS_MODS_NOINSTALL"));
	if (!pSelection)
	{
		// no ref selected: Oh noes!
		::pGUI->ShowMessageModal(
		  LoadResStr("IDS_MODS_NOINSTALL_NOMOD"),
		  strNoJoin.getData(),
		  C4GUI::MessageDialog::btnOK,
		  C4GUI::Ico_Error);
		return true;
	}
	return true;
}

bool C4StartupModsDlg::DoBack()
{
	// abort dialog: Back to main
	C4Startup::Get()->SwitchDialog(C4Startup::SDID_Back);
	return true;
}

void C4StartupModsDlg::DoRefresh()
{
	// restart masterserver query
	QueryModList();
	// done; update stuff
	fUpdatingList = false;
	UpdateList();
}

void C4StartupModsDlg::OnSec1Timer()
{
	// no updates if dialog is inactive (e.g., because a join password dlg is shown!)
	if (!IsActive(true))
		return;

	UpdateList(false);
}


