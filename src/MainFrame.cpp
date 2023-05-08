#include "MainFrame.h"
#include "Dialogs/BaseSelection.h"
#include "Dialogs/PrcSelection.h"
#include "Dialogs/InkSelection.h"
#include <filesystem>
#include <fstream>
#include <wx/wx.h>
#include <wx/spinctrl.h>
namespace fs = std::filesystem;
using std::ofstream, std::string;

MainFrame::MainFrame(const wxString& title) :
	wxFrame
	(
		nullptr,
		wxID_ANY,
		title,
		wxDefaultPosition,
		wxDefaultSize,
		wxSYSTEM_MENU | wxCAPTION | wxCLOSE_BOX | wxMINIMIZE_BOX & ~wxRESIZE_BORDER
	),
	panel(new wxPanel(this, wxID_ANY, wxDefaultPosition, wxDefaultSize)),
	logWindow(new wxTextCtrl(panel, wxID_ANY, "Log Window:\n", wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE)),
	log(new wxLogTextCtrl(logWindow)),
	mHandler(log)
{
	// Set Initial Path
	iPath = fs::current_path().string();
	replace(iPath.begin(), iPath.end(), '\\', '/');

	// Set log window to be greyed out.
	logWindow->SetEditable(false);
	logWindow->Show(false);

	readSettings();
	updateSettings();

	// Setup Menu
	wxMenuBar* menuBar = new wxMenuBar();

	wxMenu* fileMenu = new wxMenu();
	this->Bind(wxEVT_MENU, &MainFrame::onBrowse, this, fileMenu->Append(wxID_ANY, "&Open Folder\tCtrl-O")->GetId());
	this->Bind(wxEVT_MENU, &MainFrame::onMenuClose, this, fileMenu->Append(wxID_ANY, "Close\tAlt-F4")->GetId());

	wxMenu* toolsMenu = new wxMenu();
	inkMenu = new wxMenuItem(fileMenu, wxID_ANY, "Edit Inkling Colors", "Open a directory to enable this feature.");
	deskMenu = new wxMenuItem(fileMenu, wxID_ANY, "Delete Desktop.ini Files", "Open a directory to enable this feature.");
	this->Bind(wxEVT_MENU, &MainFrame::onInkPressed, this, toolsMenu->Append(inkMenu)->GetId());
	this->Bind(wxEVT_MENU, &MainFrame::onDeskPressed, this, toolsMenu->Append(deskMenu)->GetId());
	inkMenu->Enable(false);
	deskMenu->Enable(false);

	this->Bind(wxEVT_MENU, &MainFrame::onTestPressed, this, toolsMenu->Append(wxID_ANY, "Test Function")->GetId());

	wxMenu* optionsMenu = new wxMenu();

	wxMenu* loadFromMod = new wxMenu();
	optionsMenu->AppendSubMenu(loadFromMod, "Load from mod");
	auto readBaseID = loadFromMod->AppendCheckItem(wxID_ANY, "Base Slots", "Enables reading information from config.json")->GetId();
	auto readNameID = loadFromMod->AppendCheckItem(wxID_ANY, "Custom Names", "Enables reading information from msg_name.xmsbt")->GetId();
	auto readInkID = loadFromMod->AppendCheckItem(wxID_ANY, "Inkling Colors", "Enables reading information from effect.prcxml")->GetId();
	this->Bind(wxEVT_MENU, &MainFrame::toggleBaseReading, this, readBaseID);
	this->Bind(wxEVT_MENU, &MainFrame::toggleNameReading, this, readNameID);
	this->Bind(wxEVT_MENU, &MainFrame::toggleInkReading, this, readInkID);
	loadFromMod->Check(readBaseID, settings.readBase);
	loadFromMod->Check(readNameID, settings.readNames);
	loadFromMod->Check(readInkID, settings.readInk);

	wxMenu* selectionType = new wxMenu();
	optionsMenu->AppendSubMenu(selectionType, "Selection type");
	auto selectUnionID = selectionType->AppendRadioItem(wxID_ANY, "Union", "Mario [c00 & c02] + Luigi [c00 + c03]-> [c01, c02, c03]")->GetId();
	auto selectIntersectID = selectionType->AppendRadioItem(wxID_ANY, "Intersect", "Mario [c00 & c02] + Luigi [c00 + c03] -> [c00]")->GetId();
	this->Bind(wxEVT_MENU, &MainFrame::toggleSelectionType, this, selectUnionID);
	this->Bind(wxEVT_MENU, &MainFrame::toggleSelectionType, this, selectIntersectID);
	selectionType->Check(selectUnionID, !settings.selectionType);
	selectionType->Check(selectIntersectID, settings.selectionType);

	menuBar->Append(fileMenu, "&File");
	menuBar->Append(toolsMenu, "&Tools");
	menuBar->Append(optionsMenu, "&Options");

	SetMenuBar(menuBar);

	// Create browse button and text field
	browse.text = new wxTextCtrl(panel, wxID_ANY, "", wxDefaultPosition, wxDefaultSize);
	browse.button = new wxButton(panel, wxID_ANY, "Browse...", wxDefaultPosition, wxDefaultSize);
	browse.button->SetToolTip("Open folder containing fighter/ui/effect/sound folder(s)");
	browse.button->Bind(wxEVT_BUTTON, &MainFrame::onBrowse, this);

	// Create characters List
	charsList = new wxListBox(panel, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxArrayString(), wxLB_MULTIPLE);
	charsList->Bind(wxEVT_LISTBOX, &MainFrame::onCharSelect, this);

	// Create file type checkboxes
	auto fTypes = mHandler.wxGetFileTypes();
	for (int i = 0; i < fTypes.size(); i++)
	{
		fileTypeBoxes.push_back(new wxCheckBox(panel, wxID_ANY, fTypes[i], wxDefaultPosition, wxDefaultSize));
		fileTypeBoxes[i]->Bind(wxEVT_CHECKBOX, &MainFrame::onFileTypeSelect, this);
		fileTypeBoxes[i]->Disable();
	}

	// Create mod slot list
	initSlots.text = new wxStaticText(panel, wxID_ANY, "Initial Slot: ", wxDefaultPosition, wxSize(55, -1));
	initSlots.list = new wxChoice(panel, wxID_ANY, wxDefaultPosition, wxSize(50, -1));
	initSlots.list->Bind(wxEVT_CHOICE, &MainFrame::onModSlotSelect, this);
	initSlots.list->SetToolTip("Final Slot's source slot");

	// Create user slot List
	finalSlots.text = new wxStaticText(panel, wxID_ANY, "Final Slot: ", wxDefaultPosition, wxSize(55, -1));
	finalSlots.list = new wxSpinCtrl(panel, wxID_ANY, "", wxDefaultPosition, wxSize(50, -1), wxSP_WRAP, 0, 255, 0);
	finalSlots.list->Bind(wxEVT_SPINCTRL, &MainFrame::onUserSlotSelect, this);
	finalSlots.list->SetToolTip("Initial Slot's target slot");

	// Create Move Button
	buttons.mov = new wxButton(panel, wxID_ANY, "Move", wxDefaultPosition, wxDefaultSize);
	buttons.mov->Bind(wxEVT_BUTTON, &MainFrame::onMovePressed, this);
	buttons.mov->SetToolTip("Initial Slot is moved to Final Slot");
	buttons.mov->Disable();

	// Create Duplicate Button
	buttons.dup = new wxButton(panel, wxID_ANY, "Duplicate", wxDefaultPosition, wxDefaultSize);
	buttons.dup->Bind(wxEVT_BUTTON, &MainFrame::onDuplicatePressed, this);
	buttons.dup->SetToolTip("Initial Slot is duplicated to Final Slot");
	buttons.dup->Disable();

	// Create Delete Button
	buttons.del = new wxButton(panel, wxID_ANY, "Delete", wxDefaultPosition, wxDefaultSize);
	buttons.del->Bind(wxEVT_BUTTON, &MainFrame::onDeletePressed, this);
	buttons.del->SetToolTip("Initial Slot is deleted");
	buttons.del->Disable();

	// Create Log Button
	buttons.log = new wxButton(panel, wxID_ANY, "Show Log", wxDefaultPosition, wxDefaultSize);
	buttons.log->Bind(wxEVT_BUTTON, &MainFrame::onLogPressed, this);
	buttons.log->SetToolTip("Log Window can help debug issues.");

	// Create Base Slots Button
	buttons.base = new wxButton(panel, wxID_ANY, "Select Base Slots", wxDefaultPosition, wxDefaultSize);
	buttons.base->Bind(wxEVT_BUTTON, &MainFrame::onBasePressed, this);
	buttons.base->SetToolTip("Choose source slot(s) for each additional slot");
	buttons.base->Hide();

	// Create Config Button
	buttons.config = new wxButton(panel, wxID_ANY, "Create Config", wxDefaultPosition, wxDefaultSize);
	buttons.config->Bind(wxEVT_BUTTON, &MainFrame::onConfigPressed, this);
	buttons.config->SetToolTip("Creata a config for any additionals costumes, extra textures, and/or effects");
	buttons.config->Hide();

	// Create prcx Buttons
	buttons.prcxml = new wxButton(panel, wxID_ANY, "Create PRCXML", wxDefaultPosition, wxDefaultSize);
	buttons.prcxml->Bind(wxEVT_BUTTON, &MainFrame::onPrcPressed, this);
	buttons.prcxml->SetToolTip("Edit slot names or modify max slots for each character");
	buttons.prcxml->Hide();

	// Misc.
	this->Bind(wxEVT_CLOSE_WINDOW, &MainFrame::onClose, this);
	CreateStatusBar();

	// Setup Sizer
	wxBoxSizer* sizerM = new wxBoxSizer(wxVERTICAL);
	wxBoxSizer* sizerA = new wxBoxSizer(wxHORIZONTAL);
	wxBoxSizer* sizerB = new wxBoxSizer(wxHORIZONTAL);
	wxBoxSizer* sizerB1 = new wxBoxSizer(wxVERTICAL);
	wxBoxSizer* sizerB2 = new wxBoxSizer(wxVERTICAL);
	wxBoxSizer* sizerB3 = new wxBoxSizer(wxVERTICAL);
	wxBoxSizer* sizerB3A = new wxBoxSizer(wxHORIZONTAL);
	wxBoxSizer* sizerB3B = new wxBoxSizer(wxHORIZONTAL);
	wxBoxSizer* sizerC = new wxBoxSizer(wxHORIZONTAL);
	wxBoxSizer* sizerD = new wxBoxSizer(wxHORIZONTAL);

	// Main Sizer
	sizerM->Add(sizerA, 0, wxEXPAND | wxALL, 20);
	sizerM->Add(sizerB, 0, wxALIGN_CENTER_HORIZONTAL | wxLEFT | wxBOTTOM | wxRIGHT, 20);
	sizerM->Add(sizerC, 0, wxLEFT | wxBOTTOM | wxRIGHT, 20);
	sizerM->Add(sizerD, 0, wxEXPAND | wxLEFT | wxBOTTOM | wxRIGHT, 20);

	// A
	sizerA->Add(browse.text, 1, wxRIGHT, 10);
	sizerA->Add(browse.button, 0);

	// B1
	sizerB->Add(sizerB1, 3, wxEXPAND | wxRIGHT, 20);
	sizerB1->Add(charsList, 1, wxEXPAND);

	// B2
	sizerB->Add(sizerB2, 1, wxALIGN_CENTER_VERTICAL | wxRIGHT, 20);
	for (int i = 0; i < fileTypeBoxes.size() - 1; i++)
	{
		sizerB2->Add(fileTypeBoxes[i], 1);
		sizerB2->AddSpacer(20);
	}
	sizerB2->Add(fileTypeBoxes[fTypes.size() - 1], 1);

	// B3
	sizerB->Add(sizerB3, 1, wxALIGN_CENTER_VERTICAL);

	// B3A
	sizerB3->Add(sizerB3A, 1, wxBOTTOM, 10);
	sizerB3A->Add(initSlots.text, 0, wxALIGN_CENTER_VERTICAL);
	sizerB3A->Add(initSlots.list, 1, wxALIGN_CENTER_VERTICAL);

	// B3B
	sizerB3->Add(sizerB3B, 1, wxBOTTOM, 10);
	sizerB3B->Add(finalSlots.text, 0, wxALIGN_CENTER_VERTICAL);
	sizerB3B->Add(finalSlots.list, 1, wxALIGN_CENTER_VERTICAL);

	// B3(C)
	sizerB3->Add(buttons.mov, 1, wxEXPAND);
	sizerB3->Add(buttons.dup, 1, wxEXPAND);
	sizerB3->Add(buttons.del, 1, wxEXPAND);

	// C
	sizerC->Add(buttons.log, 1, wxALIGN_CENTER_VERTICAL);

	sizerC->AddStretchSpacer();
	sizerC->AddStretchSpacer();

	sizerC->Add(buttons.config, 1, wxALIGN_CENTER_VERTICAL);
	sizerC->Add(buttons.prcxml, 1, wxALIGN_CENTER_VERTICAL);
	sizerC->Add(buttons.base, 1, wxALIGN_CENTER_VERTICAL);

	// D
	sizerD->Add(logWindow, 1, wxEXPAND);

	panel->SetSizerAndFit(sizerM);
}

void MainFrame::resetFileTypeBoxes()
{
	for (auto& box : fileTypeBoxes)
	{
		box->Disable();
		box->SetValue(false);
	}
}

void MainFrame::resetButtons()
{
	buttons.mov->Disable();
	buttons.dup->Disable();
	buttons.del->Disable();
}

wxArrayString MainFrame::getSelectedCharCodes()
{
	auto selections = charsList->GetStrings();

	wxArrayString codes;
	if (!selections.empty())
	{
		for (auto& selection : selections)
		{
			codes.Add(mHandler.getCode(selection.ToStdString()));
		}
	}
	return codes;
}

wxArrayString MainFrame::getSelectedFileTypes()
{
	wxArrayString result;
	for (auto& box : fileTypeBoxes)
	{
		if (box->IsChecked())
		{
			result.Add(box->GetLabel());
		}
	}
	return result;
}

bool MainFrame::isFileTypeSelected()
{
	for (auto& box : fileTypeBoxes)
	{
		if (box->IsChecked())
		{
			return true;
		}
	}
	return false;
}

void MainFrame::readSettings()
{
	ifstream settingsFile(iPath + "/Files/settings.data");
	if (settingsFile.is_open())
	{
		int bit;

		settingsFile >> bit;
		settings.selectionType = (bit == 1) ? true : false;
		settingsFile >> bit;
		settings.readBase = (bit == 1) ? true : false;
		settingsFile >> bit;
		settings.readNames = (bit == 1) ? true : false;
		settingsFile >> bit;
		settings.readInk = (bit == 1) ? true : false;

		settingsFile.close();
	}
	else
	{
		log->LogText("Files/settings.data could not be loaded, using default values.");
	}
}

void MainFrame::updateSettings()
{
	ofstream settingsFile(iPath + "/Files/settings.data");
	if (settingsFile.is_open())
	{
		settingsFile << settings.selectionType << ' ';
		settingsFile << settings.readBase << ' ';
		settingsFile << settings.readNames << ' ';
		settingsFile << settings.readInk;

		settingsFile.close();
	}
	else
	{
		log->LogText("Files/settings.data could not be loaded, settings will not be saved.");
	}
}

void MainFrame::updateFileTypeBoxes()
{
	auto codes = getSelectedCharCodes();
	if (!codes.empty())
	{
		wxArrayString fileTypes = mHandler.wxGetFileTypes(codes, settings.selectionType);

		// Enable or disable file type checkbox based on whether or not it exists in character's map
		auto fTypes = mHandler.wxGetFileTypes();
		for (int i = 0, j = 0; i < fTypes.size(); i++)
		{
			if (j < fileTypes.size() && fileTypes[j] == fTypes[i])
			{
				fileTypeBoxes[i]->Enable();
				j++;
			}
			else
			{
				fileTypeBoxes[i]->Disable();
				fileTypeBoxes[i]->SetValue(false);
			}
		}
	}
	else
	{
		resetFileTypeBoxes();
	}
}

void MainFrame::updateButtons()
{
	if (initSlots.list->GetSelection() != wxNOT_FOUND)
	{
		if (mHandler.wxHasSlot(getSelectedCharCodes(), Slot(finalSlots.list->GetValue()), getSelectedFileTypes(), false))
		{
			buttons.mov->Disable();
			buttons.dup->Disable();
			buttons.del->Enable();
		}
		else
		{
			buttons.mov->Enable();
			buttons.dup->Enable();
			buttons.del->Enable();
		}
	}
	else
	{
		resetButtons();
	}

	panel->SendSizeEvent();
}

void MainFrame::updateInkMenu()
{
	// Update Inkling Menu
	if (mHandler.hasAddSlot("inkling"))
	{
		inkMenu->Enable(false);
		inkMenu->SetHelp("Select base slots to enable this feature.");
	}
	else
	{
		inkMenu->Enable();
		inkMenu->SetHelp("Add or modify colors. Required for additional slots.");
	}
}

void MainFrame::toggleBaseReading(wxCommandEvent& evt)
{
	settings.readBase = !settings.readBase;
	if (settings.readBase)
	{
		log->LogText("> Base slots will now be read from mods.");
	}
	else
	{
		log->LogText("> Base slots will NOT be read from mods.");
	}
	updateSettings();
}

void MainFrame::toggleNameReading(wxCommandEvent& evt)
{
	settings.readNames = !settings.readNames;
	if (settings.readNames)
	{
		log->LogText("> Custom names will now be read from mods.");
	}
	else
	{
		log->LogText("> Custom names will NOT be read from mods.");
	}
	updateSettings();
}

void MainFrame::toggleInkReading(wxCommandEvent& evt)
{
	settings.readInk = !settings.readInk;
	if (settings.readInk)
	{
		log->LogText("> Inkling colors will now be read from mods.");
	}
	else
	{
		log->LogText("> Inkling colors will NOT be read from mods.");
	}
	updateSettings();
}

void MainFrame::toggleSelectionType(wxCommandEvent& evt)
{
	settings.selectionType = !settings.selectionType;
	if (settings.selectionType)
	{
		log->LogText("> Selection type is now set to intersect.");
	}
	else
	{
		log->LogText("> Selection type is now set to union.");
	}
	updateSettings();
	updateFileTypeBoxes();

	auto selectedFileTypes = getSelectedFileTypes();
	if (!selectedFileTypes.empty())
	{
		initSlots.list->Set(mHandler.wxGetSlots(getSelectedCharCodes(), selectedFileTypes, settings.selectionType));

		if (!initSlots.list->IsEmpty())
		{
			initSlots.list->Select(0);
		}
	}
	else
	{
		initSlots.list->Clear();
	}

	updateButtons();
}

void MainFrame::onBrowse(wxCommandEvent& evt)
{
	wxDirDialog dialog(this, "Choose the root directory of your mod...", "", wxDD_DEFAULT_STYLE | wxDD_DIR_MUST_EXIST);

	if (dialog.ShowModal() != wxID_CANCEL)
	{
		// Reset previous information
		charsList->Clear();
		initSlots.list->Clear();
		this->resetFileTypeBoxes();
		this->resetButtons();

		// Update path field
		browse.text->SetValue(dialog.GetPath());

		// Update data
		mHandler.readFiles(dialog.GetPath().ToStdString());
		charsList->Set(mHandler.wxGetCharacterNames());

		// Update buttons
		if (mHandler.hasAddSlot())
		{
			buttons.base->Show();
			buttons.config->Hide();
			buttons.prcxml->Hide();
		}
		else
		{
			buttons.base->Hide();
			buttons.config->Show();
			buttons.prcxml->Show();
		}

		updateInkMenu();
		deskMenu->Enable();
		deskMenu->SetHelp("desktop.ini files are unnecessary and end up causing file-conflict issues.");

		panel->SendSizeEvent();
	}
}

void MainFrame::onCharSelect(wxCommandEvent& evt)
{
	updateFileTypeBoxes();
	auto selectedFileTypes = getSelectedFileTypes();
	if (!selectedFileTypes.empty())
	{
		initSlots.list->Set(mHandler.wxGetSlots(getSelectedCharCodes(), selectedFileTypes, settings.selectionType));

		if (!initSlots.list->IsEmpty())
		{
			initSlots.list->Select(0);
		}
	}
	else
	{
		initSlots.list->Clear();
	}
	updateButtons();
}

void MainFrame::onFileTypeSelect(wxCommandEvent& evt)
{
	initSlots.list->Set(mHandler.wxGetSlots(getSelectedCharCodes(), getSelectedFileTypes(), settings.selectionType));

	if (!initSlots.list->IsEmpty())
	{
		initSlots.list->Select(0);
	}

	updateButtons();
}

void MainFrame::onModSlotSelect(wxCommandEvent& evt)
{
	this->updateButtons();
}

void MainFrame::onUserSlotSelect(wxCommandEvent& evt)
{
	this->updateButtons();
}

void MainFrame::onMovePressed(wxCommandEvent& evt)
{
	wxArrayString fileTypes = getSelectedFileTypes();

	Slot initSlot = Slot(initSlots.list->GetStringSelection().ToStdString());
	Slot finalSlot = Slot(finalSlots.list->GetValue());

	mHandler.adjustFiles("move", getSelectedCharCodes(), fileTypes, initSlot, finalSlot);
	initSlots.list->Set(mHandler.wxGetSlots(getSelectedCharCodes(), getSelectedFileTypes(), settings.selectionType));
	initSlots.list->SetStringSelection("c" + finalSlot.getString());

	this->updateButtons();

	// Update buttons
	if (mHandler.hasAddSlot())
	{
		buttons.base->Show();
		buttons.config->Hide();
		buttons.prcxml->Hide();
	}
	else
	{
		buttons.base->Hide();
		buttons.config->Show();
		buttons.prcxml->Show();
	}

	updateInkMenu();

	panel->SendSizeEvent();
}

void MainFrame::onDuplicatePressed(wxCommandEvent& evt)
{
	wxArrayString fileTypes = getSelectedFileTypes();

	Slot initSlot = Slot(initSlots.list->GetStringSelection().ToStdString());
	Slot finalSlot = Slot(finalSlots.list->GetValue());

	mHandler.adjustFiles("duplicate", getSelectedCharCodes(), fileTypes, initSlot, finalSlot);
	initSlots.list->Set(mHandler.wxGetSlots(getSelectedCharCodes(), getSelectedFileTypes(), settings.selectionType));
	initSlots.list->SetStringSelection("c" + initSlot.getString());

	this->updateButtons();

	// Update buttons
	if (mHandler.hasAddSlot())
	{
		buttons.base->Show();
		buttons.config->Hide();
		buttons.prcxml->Hide();
	}

	updateInkMenu();

	panel->SendSizeEvent();
}

void MainFrame::onDeletePressed(wxCommandEvent& evt)
{
	int numChar = mHandler.getNumCharacters();
	
	auto selections = charsList->GetStrings();
	set<wxString> selectionSet(selections.begin(), selections.end());

	mHandler.adjustFiles("delete", getSelectedCharCodes(), getSelectedFileTypes(), Slot(initSlots.list->GetStringSelection().ToStdString()), Slot(-1));

	if (mHandler.getNumCharacters() != numChar)
	{
		charsList->Set(mHandler.wxGetCharacterNames());

		selections = charsList->GetStrings();

		for (int i = 0; i != selections.size(); i++)
		{
			if (selectionSet.find(selections[i]) != selectionSet.end())
			{
				charsList->Select(i);
			}
		}
	}

	updateFileTypeBoxes();
	initSlots.list->Set(mHandler.wxGetSlots(getSelectedCharCodes(), getSelectedFileTypes(), settings.selectionType));

	if (!initSlots.list->IsEmpty())
	{
		initSlots.list->SetSelection(0);
	}
	updateButtons();

	// Update buttons
	if (!mHandler.hasAddSlot())
	{
		buttons.base->Hide();
		buttons.config->Show();
		buttons.prcxml->Show();
	}

	updateInkMenu();

	panel->SendSizeEvent();
}

void MainFrame::onLogPressed(wxCommandEvent& evt)
{
	if (logWindow->IsShown())
	{
		logWindow->Show(false);
		this->SetSize(wxSize(this->GetSize().x, this->GetSize().y - 200));
		buttons.log->SetLabel("Show Log");
	}
	else
	{
		logWindow->Show(true);
		this->SetSize(wxSize(this->GetSize().x, this->GetSize().y + 200));
		buttons.log->SetLabel("Hide Log");
	}

	panel->SendSizeEvent();
}

void MainFrame::onBasePressed(wxCommandEvent& evt)
{
	BaseSelection dlg(this, wxID_ANY, "Choose Base Slots", mHandler, settings.readBase);

	if (dlg.ShowModal() == wxID_OK)
	{
		mHandler.setBaseSlots(dlg.getBaseSlots());

		// Update Buttons
		buttons.base->Hide();
		buttons.config->Show();
		buttons.prcxml->Show();

		inkMenu->Enable();
		inkMenu->SetHelp("Add or modify colors. Required for additional slots.");

		panel->SendSizeEvent();
	}
}

void MainFrame::onConfigPressed(wxCommandEvent& evt)
{
	mHandler.create_config();
}

void MainFrame::onInkPressed(wxCommandEvent& evt)
{
	InkSelection dlg(this, wxID_ANY, "Choose Inkling Colors", mHandler, settings.readInk);

	if (dlg.ShowModal() == wxID_OK)
	{
		auto finalColors = dlg.getFinalColors();

		if (!finalColors.empty())
		{
			// Change directory to parcel and param-xml's location
			// TODO: Make function not rely on directory change
			wxSetWorkingDirectory("Files/prc/");

			mHandler.create_ink_prcxml(finalColors);

			fs::create_directories(mHandler.getPath() + "/fighter/common/param/");
			fs::rename(fs::current_path() / "effect_Edit.prcxml", mHandler.getPath() + "/fighter/common/param/effect.prcxml");

			if (fs::exists(mHandler.getPath() + "/fighter/common/param/effect.prcx"))
			{
				fs::remove(mHandler.getPath() + "/fighter/common/param/effect.prcx");
			}

			// Restore working directory
			wxSetWorkingDirectory("../../");
		}
		else
		{
			log->LogText("> N/A: No changes were made.");
		}
	}
}

void MainFrame::onPrcPressed(wxCommandEvent& evt)
{
	bool hasAddSlots = mHandler.hasAddSlot();

	if (hasAddSlots || mHandler.hasChar())
	{
		// Make character-slots map

		PrcSelection dlg(this, wxID_ANY, "Make Selection", mHandler, settings.readNames);
		if (dlg.ShowModal() == wxID_OK)
		{
			auto finalSlots = dlg.getMaxSlots(&mHandler);
			auto finalNames = dlg.getNames();
			auto finalAnnouncers = dlg.getAnnouncers();

			wxArrayString exeLog;

			if (hasAddSlots || !finalNames.empty() || !finalAnnouncers.empty())
			{
				// Change working directory
				wxSetWorkingDirectory("Files/prc/");

				// Create XML
				wxExecute("param-xml disasm vanilla.prc -o ui_chara_db.xml -l ParamLabels.csv", exeLog, exeLog, wxEXEC_SYNC | wxEXEC_NODISABLE);

				mHandler.create_db_prcxml(finalNames, finalAnnouncers, finalSlots);
				if (!finalNames.empty())
				{
					mHandler.create_message_xmsbt(finalNames);
				}

				if (exeLog.size() == 1 && exeLog[0].substr(0, 9) == "Completed")
				{
					log->LogText("> Success! ui_chara_db.prcxml was created!");

					if (!finalNames.empty())
					{
						log->LogText("> Success! msg_name.xmsbt was created!");
					}

					fs::create_directories(mHandler.getPath() + "/ui/param/database/");
					fs::rename(fs::current_path() / "ui_chara_db.prcxml", mHandler.getPath() + "/ui/param/database/ui_chara_db.prcxml");

					if (fs::exists(mHandler.getPath() + "/ui/param/database/ui_chara_db.prcx"))
					{
						fs::remove(mHandler.getPath() + "/ui/param/database/ui_chara_db.prcx");
					}
				}
				else
				{
					log->LogText("> Error: Something went wrong, possible issue below:");

					for (int i = 0; i < exeLog.size(); i++)
					{
						log->LogText(">" + exeLog[i]);
					}
				}

				// Restore working directory
				wxSetWorkingDirectory("../../");
			}
			else if (fs::exists(mHandler.getPath() + "/ui/param/database/ui_chara_db.prcxml"))
			{
				fs::remove(mHandler.getPath() + "/ui/param/database/ui_chara_db.prcxml");
				log->LogText("> NOTE: ui_chara_db.prcxml is not needed, previous one was deleted.");
			}
			else
			{
				log->LogText("> NOTE: ui_chara_db.prcxml is not needed.");
			}
		}
	}
	else if (!mHandler.hasChar())
	{
		log->LogText("> N/A: Mod is empty, cannot create a prcx!");
	}
	else
	{
		log->LogText("> N/A: No additional slots detected.");
	}
}

void MainFrame::onMenuClose(wxCommandEvent& evt)
{
	Close(true);
}

void MainFrame::onClose(wxCloseEvent& evt)
{
	evt.Skip();
}

void MainFrame::onTestPressed(wxCommandEvent& evt)
{
	mHandler.test();
}

void MainFrame::onDeskPressed(wxCommandEvent& evt)
{
	mHandler.remove_desktop_ini();
}

MainFrame::~MainFrame()
{
	// Remove?
}
