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

#include <fstream>
#include <sstream>
#include <regex>
const std::string C4StartupModsDlg::baseServerURL = "frustrum.pictor.uberspace.de/larry/api/";

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
		const int alignments[] = { ALeft, ARight };
		for (int c = 0; c < 2; ++c)
		{
			const int alignment = alignments[c];
			rcLabelBounds.y = 1 + i*(iLineHgt + 2);
			rcLabelBounds.Wdt = iThisWdt - rcLabelBounds.x - 1;
			C4GUI::Label * const pLbl = new C4GUI::Label("", rcLabelBounds, alignment, C4GUI_CaptionFontClr);
			if (alignment == ALeft) pInfoLbl[i] = pLbl;
			else if (alignment == ARight) pInfoLabelsRight[i] = pLbl;
			else assert(false);
			AddElement(pLbl);
			// label will have collapsed due to no text: Repair it
			pLbl->SetAutosize(false);
			pLbl->SetBounds(rcLabelBounds);
		}
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
		const char *nodeText = child->GetText();
		if (nodeText == nullptr) return fallback;
		const std::string value(nodeText);
		if (!value.empty()) return value;
		return fallback;
	};

	id = getSafeStringValue(xml, "_id", "");
	bool installed = false;
	if (!id.empty() && pModsDlg->modsDiscovery.IsDiscoveryFinished() && pModsDlg->modsDiscovery.IsModInstalled(id))
	{
		installed = true;
		pIcon->SetIcon(C4GUI::Icons::Ico_Save);
	}

	title = getSafeStringValue(xml, "title", "???");
	sInfoTextRight[0].Format(LoadResStr("IDS_MODS_METAINFO"), getSafeStringValue(xml, "downloads", "0").c_str());
	sInfoText[0].Format(LoadResStr("IDS_MODS_TITLE"), title.c_str(), getSafeStringValue(xml, "author", "???").c_str());
	std::string description = installed ? std::string("<c 559955>") + LoadResStr("IDS_MODS_INSTALLED") + ".</c> " : "";
	description += getSafeStringValue(xml, "description");
	if (!description.empty())
	{
		if (description.size() > 200)
		{
			description.resize(200);
			description += "...";
		}
		sInfoText[1].Format("%s", description.c_str());
	}

	UpdateText();

	// Additional meta-information.
	for (const TiXmlElement *filenode = xml->FirstChildElement("file"); filenode != nullptr; filenode = filenode->NextSiblingElement("file"))
	{
		const std::string handle = getSafeStringValue(filenode, "file", "");
		const std::string name = getSafeStringValue(filenode, "name", "");
		const std::string lengthString = getSafeStringValue(filenode, "length", "");

		if (handle.empty() || name.empty() || lengthString.empty()) continue;
		size_t length{ 0 };

		try
		{
			length = std::stoi(lengthString);
		}
		catch (...)
		{
			continue;
		}

		files.emplace_back(FileInfo { handle, length, name });
	}
	
}

void C4StartupModsListEntry::MakeInfoEntry()
{
	const_cast<C4Facet &>(reinterpret_cast<const C4Facet &>(pIcon->GetFacet()))
		= (const C4Facet &)C4Startup::Get()->Graphics.fctNetGetRef;
	pIcon->SetAnimated(true, 1);
	pIcon->SetBounds(rctIconLarge);

	// set info
	sInfoText[0].Copy(LoadResStr("IDS_MODS_SEARCHING"));
	UpdateSmallState(); UpdateText();
}

void C4StartupModsListEntry::OnNoResultsFound()
{
	pIcon->SetAnimated(false, 1);
	sInfoText[0].Copy(LoadResStr("IDS_MODS_SEARCH_NORESULTS"));
	UpdateText();
}

void C4StartupModsListEntry::OnError(std::string message)
{
	pIcon->SetAnimated(false, 1);
	sInfoText[0].Copy(LoadResStr("IDS_MODS_SEARCH_ERROR"));
	sInfoText[1].Copy(message.c_str());
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
	for (i = 0; i < InfoLabelCount; ++i)
	{
		sInfoText[i].Clear();
		sInfoTextRight[i].Clear();
	}
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
	for (int i = 2; i < InfoLabelCount; ++i)
	{
		pInfoLbl[i]->SetVisibility(!fIsSmall);
		pInfoLabelsRight[i]->SetVisibility(!fIsSmall);
	}
	UpdateEntrySize();
}

void C4StartupModsListEntry::UpdateEntrySize()
{
	if(fVisible) {
		// restack all labels by their size
		const int32_t iLblCnt = (fIsSmall ? 2 : InfoLabelCount);
		for (int c = 0; c < 2; ++c)
		{
			int iY = 1;
			C4GUI::Label **labelList = (c == 0) ? pInfoLbl : pInfoLabelsRight;
			for (int i = 0; i < iLblCnt; ++i)
			{
				C4Rect rcBounds = labelList[i]->GetBounds();
				rcBounds.y = iY;
				iY += rcBounds.Hgt + 2;
				pInfoLbl[i]->SetBounds(rcBounds);
			}
			// Resize this control according to the labels on the left.
			if (c == 0)
				GetBounds().Hgt = iY - 1;
		}
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
	for (int c = 0; c < 2; ++c)
	{
		C4GUI::Label **infoLabels = (c == 0) ? pInfoLbl : pInfoLabelsRight;
		StdStrBuf *infoTexts = (c == 0) ? sInfoText : sInfoTextRight;
		for (i = 0; i < InfoLabelCount; ++i)
		{
			C4GUI::Label *infoLabel = infoLabels[i];
			int iAvailableWdt = GetClientRect().Wdt - infoLabel->GetBounds().x - 1;
			if (!i) iAvailableWdt -= sx;
			StdStrBuf BrokenText;
			pUseFont->BreakMessage(infoTexts[i].getData(), iAvailableWdt, &BrokenText, true);
			int32_t iHgt, iWdt;
			if (pUseFont->GetTextExtent(BrokenText.getData(), iWdt, iHgt, true))
			{
				if ((infoLabel->GetBounds().Hgt != iHgt) || (infoLabel->GetBounds().Wdt != iAvailableWdt))
				{
					C4Rect rcBounds = infoLabel->GetBounds();
					rcBounds.Wdt = iAvailableWdt;
					rcBounds.Hgt = iHgt;
					infoLabel->SetBounds(rcBounds);
					fRestackElements = true;
				}
			}
			infoLabel->SetText(BrokenText.getData());
			infoLabel->SetColor(fIsEnabled ? C4GUI_MessageFontClr : C4GUI_InactMessageFontClr);
		}
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

void C4StartupModsLocalModDiscovery::Execute()
{
	ExecuteDiscovery();

	StdThread::Stop();
}

void C4StartupModsLocalModDiscovery::ExecuteDiscovery()
{
	assert(!discoveryFinished);
	// Check the mods directory for existing files.
	const std::string path = std::string(Config.General.UserDataPath) + "mods";
	for (DirectoryIterator iter(path.c_str()); *iter; ++iter)
	{
		const std::string filename(*iter);
		
		// No folder?
		if (!DirectoryExists(filename.c_str())) continue;

		const size_t lastSeparaterPosition = filename.find_last_of(DirectorySeparator);
		if (lastSeparaterPosition == std::string::npos) continue;
		const std::string leaf = filename.substr(lastSeparaterPosition + 1);
		// The leaf is prefixed with "<item ID>_" if it's a mod directory.
		const size_t idSeparatorPosition = leaf.find_first_of("_");
		if (idSeparatorPosition == std::string::npos) continue;
		const std::string id = leaf.substr(0, idSeparatorPosition);
		if (id.empty()) continue;

		ModsInfo mod;
		mod.id = id;
		mod.path = filename;

		modsInformation[id] = mod;
	}

	discoveryFinished = true;
}

bool C4StartupModsListEntry::KeywordMatch(const char *szMatch)
{
	return false;
}

C4StartupModsDownloader::C4StartupModsDownloader(C4StartupModsDlg *parent, const C4StartupModsListEntry *entry) : CStdTimerProc(30)
{
	this->parent = parent;

	ModInfo mod{ entry->GetID(), entry->GetTitle() };
	for (auto & fileInfo : entry->GetFileInfos())
	{
		mod.files.emplace_back(ModInfo::FileInfo{ fileInfo.handle, fileInfo.name, fileInfo.size });
	}
	items.emplace_back(std::move(mod));

	// Register timer.
	Application.Add(this);
}

C4StartupModsDownloader::~C4StartupModsDownloader()
{
	Application.Remove(this);
	CancelRequest();
}

void C4StartupModsDownloader::CancelRequest()
{
	for (auto & mod : items)
		mod.CancelRequest();
	items.resize(0);

	delete progressDialog;
	progressDialog = nullptr;
}

void C4StartupModsDownloader::OnConfirmInstallation(C4GUI::Element *element)
{
	assert(!items.empty());
	assert(!items[0].files.empty());

	std::string message = std::string("Downloading and installing ") + items[0].name + "...";
	progressDialog = new C4GUI::ProgressDialog(message.c_str(), "Downloading...", 100, 0, C4GUI::Icons::Ico_Save);
	parent->GetScreen()->ShowRemoveDlg(progressDialog);
	
	CheckProgress();
}

void C4StartupModsDownloader::ModInfo::CancelRequest()
{
	if (!postClient.get()) return;
	Application.InteractiveThread.RemoveProc(postClient.get());
	postClient.reset();
}

void C4StartupModsDownloader::ModInfo::CheckProgress()
{
	// Determining success or starting a new download.
	if (!errorMessage.empty()) return;
	if (successful) return;

	if (postClient.get() == nullptr) // Start new file?
	{
		postClient = std::make_unique<C4Network2HTTPClient>();

		if (!postClient->Init() || !postClient->SetServer((C4StartupModsDlg::baseServerURL + files.back().handle).c_str()))
		{
			assert(false);
			return;
		}
		postClient->SetExpectedResponseType(C4Network2HTTPClient::ResponseType::XML);

		// Do the actual request.
		postClient->SetNotify(&Application.InteractiveThread);
		Application.InteractiveThread.AddProc(postClient.get());
		postClient->Query(nullptr, true); // Empty query for binary data.
	}

	// Update progress bar.
	downloadedBytes = postClient->getDownloadedSize();
	totalBytes = postClient->getTotalSize();

	if (!postClient->isBusy())
	{
		if (!postClient->isSuccess())
		{
			CancelRequest();
			return;
		}
		else
		{
			const std::string path = std::string(Config.General.UserDataPath) + "mods" + DirectorySeparator + \
				modID + "_" + name;

			if (!CreatePath(path))
			{
				errorMessage = LoadResStr("IDS_MODS_NOINSTALL_CREATEDIR");
				CancelRequest();
				return;
			}

			std::ofstream os(path + DirectorySeparator + files.back().name, std::iostream::out | std::iostream::binary);
			if (!os.good())
			{
				errorMessage = LoadResStr("IDS_MODS_NOINSTALL_CREATEFILE");
				CancelRequest();
				return;
			}

			os.write(static_cast<const char*>(postClient->getResultBin().getData()), postClient->getDownloadedSize());
			os.close();

			CancelRequest();
			
			files.pop_back();
			if (files.empty())
				successful = true;
			return;
		}
	}
}

void C4StartupModsDownloader::CheckProgress()
{
	assert(progressDialog);

	// Let mods check their progress.
	size_t downloadedBytes{ 0 }, totalBytes{ 0 };

	bool anyNotFinished = false;

	for (auto & mod : items)
	{
		mod.CheckProgress();
		size_t downloaded, total;
		std::tie(downloaded, total) = mod.GetProgress();
		
		if (mod.IsBusy())
		{
			downloadedBytes += downloaded;
			totalBytes += total;
			anyNotFinished = true;
		}
	}

	if (totalBytes)
		progressDialog->SetProgress(100 * downloadedBytes / totalBytes);

	// All done?
	if (!anyNotFinished)
	{
		// Report errors (all in one).
		std::string errorMessage;
		for (auto & mod : items)
		{
			const std::string modError = mod.GetErrorMessage();
			if (!modError.empty())
				errorMessage += "|" + modError;
		}

		if (!errorMessage.empty())
		{
			::pGUI->ShowMessageModal(errorMessage.c_str(), LoadResStr("IDS_MODS_NOINSTALL"), C4GUI::MessageDialog::btnOK, C4GUI::Ico_Error);
		}

		CancelRequest();
	}

	if (!progressDialog->Execute() || progressDialog->IsAborted())
	{
		CancelRequest();
	}
}

void C4StartupModsDownloader::RequestConfirmation()
{
	// Calculate total filesize to be downloaded.
	size_t totalSize{ 0 };
	for (auto &mod : items)
	{
		for (auto &file : mod.files)
		{
			totalSize += file.size;
		}
	}

	if (totalSize == 0)
	{
		::pGUI->ShowMessageModal(LoadResStr("IDS_MODS_NOINSTALL_NODATA"), LoadResStr("IDS_MODS_NOINSTALL"), C4GUI::MessageDialog::btnOK, C4GUI::Ico_Error);
		return;
	}

	std::string filesizeString;
	const size_t totalSizeMB = (totalSize / 1000 + 500) / 1000;
	if (totalSizeMB == 0)
	{
		filesizeString = "<1MB";
	}
	else
	{
		filesizeString = std::string("~") + std::to_string(totalSizeMB) + "MB";
	}

	StdStrBuf confirmationMessage;
	confirmationMessage.Format(LoadResStr("IDS_MODS_INSTALL_CONFIRM"), items[0].name.c_str(), filesizeString.c_str());
	auto *callbackHandler = new C4GUI::CallbackHandler<C4StartupModsDownloader>(this, &C4StartupModsDownloader::OnConfirmInstallation);
	auto *dialog = new C4GUI::ConfirmationDialog(confirmationMessage.getData(), "Confirm installation", callbackHandler, C4GUI::MessageDialog::btnYesNo, false, C4GUI::Icons::Ico_Save);
	parent->GetScreen()->ShowRemoveDlg(dialog);
}

// ----------- C4StartupNetDlg ---------------------------------------------------------------------------------

C4StartupModsDlg::C4StartupModsDlg() : C4StartupDlg(LoadResStr("IDS_DLG_MODS")), pMasterserverClient(nullptr), fIsCollapsed(false)
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
	pSheetGameList->AddElement(pGameListLbl);

	// precalculate space needed for sorting labels
	int32_t maxSortLabelWidth = 0;
	
	sortingOptions =
	{
		{ "version", "IDS_MODS_SORT_RATING_UP", "IDS_MODS_SORT_RATING_DOWN" },
		{ "title", "IDS_MODS_SORT_NAME_UP", "IDS_MODS_SORT_NAME_DOWN" },
		{ "_created", "IDS_MODS_SORT_DATE_UP", "IDS_MODS_SORT_DATE_DOWN" },
	};
	// Translate all labels.
	for (auto &option : sortingOptions)
	{
		int32_t iSortWdt = 100, iSortHgt;
		for (auto label : { &SortingOption::titleAsc, &SortingOption::titleDesc })
		{
			option.*label = LoadResStr(option.*label);
			// Get width of label and remember if it's the longest yet.
			::GraphicsResource.TextFont.GetTextExtent(option.*label, iSortWdt, iSortHgt, true);
			if (iSortWdt > maxSortLabelWidth)
				maxSortLabelWidth = iSortWdt;
		}
	}

	// search field
	C4GUI::WoodenLabel *pSearchLbl;
	const char *szSearchLblText = LoadResStr("IDS_NET_MSSEARCH"); // Text is the same as in the network view.
	int32_t iSearchWdt=100, iSearchHgt;
	::GraphicsResource.TextFont.GetTextExtent(szSearchLblText, iSearchWdt, iSearchHgt, true);
	C4GUI::ComponentAligner caSearch(caGameList.GetFromTop(iSearchHgt), 0,0);
	pSearchLbl = new C4GUI::WoodenLabel(szSearchLblText, caSearch.GetFromLeft(iSearchWdt+10), C4GUI_Caption2FontClr, &::GraphicsResource.TextFont);
	const char *szSearchTip = LoadResStr("IDS_MODS_SEARCH_DESC");
	pSearchLbl->SetToolTip(szSearchTip);
	pSheetGameList->AddElement(pSearchLbl);
	pSearchFieldEdt = new C4GUI::CallbackEdit<C4StartupModsDlg>(caSearch.GetFromLeft(caSearch.GetWidth() - maxSortLabelWidth - 40), this, &C4StartupModsDlg::OnSearchFieldEnter);
	pSearchFieldEdt->SetToolTip(szSearchTip);
	pSheetGameList->AddElement(pSearchFieldEdt);

	// Sorting options
	C4GUI::ComponentAligner caSorting(caSearch.GetAll(), 0, 0);
	auto pSortComboBox = new C4GUI::ComboBox(caSearch.GetAll());
	pSortComboBox->SetComboCB(new C4GUI::ComboBox_FillCallback<C4StartupModsDlg>(this, &C4StartupModsDlg::OnSortComboFill, &C4StartupModsDlg::OnSortComboSelChange));
	pSortComboBox->SetText(LoadResStr("IDS_MODS_SORT"));
	pSheetGameList->AddElement(pSortComboBox);

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
	// Clear the list and add an info entry.
	ClearList();
	C4StartupModsListEntry *infoEntry = new C4StartupModsListEntry(pGameSelList, nullptr, this);
	infoEntry->MakeInfoEntry();

	// Forward the filter-field to the server.
	std::string searchQueryPostfix("");
	if (pSearchFieldEdt->GetText())
	{
		std::string searchText(pSearchFieldEdt->GetText());
		if (searchText.size() > 0)
		{
			// Sanity, escape quotes etc.
			searchText = std::regex_replace(searchText, std::regex("\""), "\\\"");
			searchText = std::regex_replace(searchText, std::regex("[ ]+"), "%20");
			searchQueryPostfix = "?where={%22$text%22:{%22$search%22:%22" + searchText + "%22}}";
		}
	}

	// Forward the sorting criterion to the server.
	if (!sortKeySuffix.empty())
	{
		searchQueryPostfix += std::string(searchQueryPostfix.empty() ? "?" : "&") + "sort=" + sortKeySuffix;
	}

	// Initialize connection.
	queryWasSuccessful = false;
	postClient = std::make_unique<C4Network2HTTPClient>();
	
	if (!postClient->Init() || !postClient->SetServer((C4StartupModsDlg::baseServerURL + "items" + searchQueryPostfix).c_str()))
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
	StdStrBuf strQuery; strQuery.format("%s?version=%s&platform=%s", Config.Network.GetLeagueServerAddress(), strVersion.getData(), C4_OS);
	pMasterserverClient->SetRefQuery(strQuery.getData(), C4StartupNetListEntry::NRQT_Masterserver);*/
}

void C4StartupModsDlg::CancelRequest()
{
	if (!postClient.get()) return;
	Application.InteractiveThread.RemoveProc(postClient.get());
	postClient.reset();
}

void C4StartupModsDlg::ClearList()
{
	C4GUI::Element *pElem, *pNextElem = pGameSelList->GetFirst();
	while ((pElem = pNextElem))
	{
		pNextElem = pElem->GetNext();
		C4StartupModsListEntry *pEntry = static_cast<C4StartupModsListEntry *>(pElem);
		delete pEntry;
	}
}

void C4StartupModsDlg::UpdateList(bool fGotReference)
{
	// Already running a query?
	if (postClient.get() != nullptr)
	{
		// Check whether the data has arrived yet.
		if (!postClient->isBusy())
		{
			// At this point we can assert that the list contains only one child - the info field.
			C4StartupModsListEntry *infoEntry = static_cast<C4StartupModsListEntry*> (pGameSelList->GetFirst());
			assert(infoEntry != nullptr);

			if (!postClient->isSuccess())
			{
				Log(postClient->GetError());
				infoEntry->OnError(postClient->GetError());
				// Destroy client and try again later.
				CancelRequest();
				return;
			}
			Log("Received!");
			Log(postClient->getResultString());
			queryWasSuccessful = true;

			TiXmlDocument xmlDocument;
			xmlDocument.Parse(postClient->getResultString());

			if (xmlDocument.Error())
			{
				Log(xmlDocument.ErrorDesc());
				CancelRequest();
				infoEntry->OnError(xmlDocument.ErrorDesc());
				return;
			}
			const char * resourceElementName = "resource";
			const TiXmlElement *root = xmlDocument.RootElement();
			assert(strcmp(root->Value(), resourceElementName) == 0);

			int newElementCount = 0;
			for (const TiXmlElement* e = root->FirstChildElement(resourceElementName); e != NULL; e = e->NextSiblingElement(resourceElementName))
			{
				C4StartupModsListEntry *pEntry = new C4StartupModsListEntry(pGameSelList, nullptr, this);
				pEntry->FromXML(e);
				++newElementCount;
			}

			// Nothing found? Notify!
			if (newElementCount == 0)
				infoEntry->OnNoResultsFound();
			else
				delete infoEntry;

			CancelRequest();
		}
	}
	else // Not running a query.
	{
		if (!queryWasSuccessful) // Last query failed?
		{
			QueryModList();
			return;
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
		QueryModList();
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
	else // Show confirmation dialogue.
	{
		auto *elem = static_cast<C4StartupModsListEntry*> (pSelection);

		if (downloader.get() != nullptr)
			downloader.reset();
		downloader = std::make_unique<C4StartupModsDownloader>(this, elem);
		downloader->RequestConfirmation();
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
	UpdateList();
}

void C4StartupModsDlg::OnSec1Timer()
{
	// no updates if dialog is inactive (e.g., because a join password dlg is shown!)
	if (!IsActive(true))
		return;

	UpdateList(false);
}


void C4StartupModsDlg::OnSortComboFill(C4GUI::ComboBox_FillCB *pFiller)
{
	int32_t counter = 0;
	for (auto & option : sortingOptions)
	{
		// The labels were already translated earlier.
		pFiller->AddEntry(option.titleAsc, counter++);
		pFiller->AddEntry(option.titleDesc, counter++);
	}
}

bool C4StartupModsDlg::OnSortComboSelChange(C4GUI::ComboBox *pForCombo, int32_t idNewSelection)
{
	const size_t selected = idNewSelection / 2;
	const bool descending = idNewSelection % 2 == 1;
	const std::string newSortKeySuffix = std::string(descending ? "-" : "") + sortingOptions[selected].key;
	if (newSortKeySuffix == sortKeySuffix) return true;
	sortKeySuffix = newSortKeySuffix;
	// Update label.
	const char *sortLabel = descending ? sortingOptions[selected].titleDesc : sortingOptions[selected].titleAsc;
	pForCombo->SetText(sortLabel);
	// Refresh view.
	QueryModList();
	return true;
}