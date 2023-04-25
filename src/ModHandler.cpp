#pragma once

#include "nlohmann/json.hpp"
#include "ModHandler.h"
#include <string>
#include <filesystem>
#include <fstream>
#include <thread>
#include <codecvt>
#include <queue>
namespace fs = std::filesystem;
using std::string, std::ifstream, std::ofstream;
using std::queue;

//

wxString getSlotFromInt(int slot)
{
	if (slot > 9)
	{
		return to_string(slot);
	}
	else if (slot >= 0)
	{
		return ("0" + to_string(slot));
	}
	else
	{
		return "-1";
	}
}

// Constructor
ModHandler::ModHandler(wxLogTextCtrl* log): log(log)
{
	fileTypes.push_back("effect");
	fileTypes.push_back("fighter");
	fileTypes.push_back("sound");
	fileTypes.push_back("ui");
}

// Switches between smash-path and system-path.
string ModHandler::convertPath(string input)
{
	std::replace(input.begin(), input.end(), '\\', '/');

	if (input.find(path) != string::npos)
	{
		return input.substr(path.size() + 1);
	}
	else
	{
		return path + '/' + input;
	}
}

void ModHandler::addFile(string charcode, string fileType, int slot, string file)
{
	for (int i = 0; i < charcode.length(); i++)
	{
		charcode[i] = tolower(charcode[i]);
	}

	if (charcode == "popo" || charcode == "nana")
	{
		charcode = "ice_climber";
	}

	if (charNames.find(charcode) == charNames.end())
	{
		charCodes[charcode] = charcode;
		charNames[charcode] = charcode;
	}

	std::replace(file.begin(), file.end(), '\\', '/');

	mod[charcode][fileType][slot].insert(file);
}

void ModHandler::readFiles(string wxPath)
{
	path = wxPath;
	std::replace(path.begin(), path.end(), '\\', '/');

	for (const auto& i : fs::directory_iterator(path))
	{
		string fileType = i.path().filename().string();

		if (fileType == "effect")
		{
			for (const auto& j : fs::directory_iterator(i))
			{
				if (j.path().filename() == "fighter")
				{
					// Folder = [charcode] (i.e. ike, koopa, etc.)
					for (const auto& k : fs::directory_iterator(j))
					{
						string charcode = k.path().filename().string();

						// Effect Files/Folders
						if (k.is_directory())
						{
							for (const auto& l : fs::directory_iterator(k))
							{
								string effectFile = l.path().filename().string();
								string slot;

								// effectFile is an eff file
								if (effectFile.rfind(".eff") != std::string::npos)
								{
									// One-Slotted eff file
									// ef_[charcode]_c[slot].eff
									// ef_ + _c + .eff = 9
									if (effectFile.size() > (9 + charcode.size()))
									{
										// ef_[charcode]_c[slot].eff
										// ef_ + _c = 5
										slot = effectFile.substr(5 + charcode.size(), effectFile.find(".eff") - (5 + charcode.size()));
									}
									// effectFile is not One-Slotted
									else
									{
										slot = "all";
									}

								}
								// effectFile is a folder
								else
								{
									// model folder requires special treatment
									if (effectFile == "model")
									{
										for (const auto& m : fs::directory_iterator(l))
										{
											string effectFile = m.path().filename().string();
											auto cPos = effectFile.rfind("_c");

											// Folder is one slotted
											if (cPos != string::npos && cPos != effectFile.find("_c"))
											{
												slot = effectFile.substr(cPos + 2);
											}
											else
											{
												slot = "all";
											}

											addFile(charcode, fileType, slot, m.path().string());
										}

										continue;
									}
									// Character specific folder, likely a Trail folder (One-Slotted)
									else if (effectFile.find("_c") != string::npos)
									{
										// [character_specific_effect]_c[slot]
										// _c = 2
										slot = effectFile.substr(effectFile.rfind("_c") + 2);
									}
									// Character specific folder, likely a Trail folder (NOT One-Slotted)
									else
									{
										slot = "all";
									}
								}

								string file = l.path().string();

								addFile(charcode, fileType, slot, file);
							}
						}
					}
				}
			}
		}
		else if (fileType == "fighter")
		{
			// Folder = [charcode] (i.e. ike, koopa, etc.)
			for (const auto& j : fs::directory_iterator(i))
			{
				string charcode = j.path().filename().string();

				// Folder = [folder type] (i.e. model, motion etc.)
				if (j.is_directory())
				{
					for (const auto& k : fs::directory_iterator(j))
					{
						// Folder = [character_specfic_folder] (i.e. body, sword, etc.)
						if (k.is_directory())
						{
							for (const auto& l : fs::directory_iterator(k))
							{
								// Folder = c[slot]
								if (l.is_directory())
								{
									for (const auto& m : fs::directory_iterator(l))
									{
										if (m.is_directory())
										{
											// c[slot]
											// c = 1
											string slot = m.path().filename().string().substr(1);
											string file = m.path().string();

											addFile(charcode, fileType, slot, file);
										}
									}
								}
							}
						}
					}
				}
			}
		}
		else if (fileType == "sound")
		{
			for (const auto& j : fs::directory_iterator(i))
			{
				if (j.path().filename() == "bank")
				{
					for (const auto& k : fs::directory_iterator(j))
					{
						// Folder = fighter || fighter_voice
						if (k.is_directory() && (k.path().filename() == "fighter" || k.path().filename() == "fighter_voice"))
						{
							for (const auto& l : fs::directory_iterator(k))
							{
								string filename = l.path().filename().string();
								string file = l.path().string();

								if (filename.find("vc_") == 0 || filename.find("se_") == 0)
								{
									// vc_[charcode] || se_[charcode]
									// vc_ = 3       || se_ = 3
									size_t dotPos = filename.rfind('.');

									// Slotted
									if (dotPos != string::npos)
									{
										if (isdigit(filename[dotPos - 1]))
										{
											string charcode = filename.substr(3, filename.substr(3).find("_"));

											// [...]_c[slot].[filetype]
											// _c = 2
											size_t cPos = filename.rfind("_c");

											if (cPos != string::npos)
											{
												string slot = filename.substr(cPos + 2, filename.rfind(".") - cPos - 2);
												addFile(charcode, fileType, slot, file);
											}
											else
											{
												log->LogText("> " + filename + "'s slot could not be determined!");
											}
										}
										else
										{
											string charcode = filename.substr(3, dotPos - 3);
											addFile(charcode, fileType, "all", file);
										}
									}
									else
									{
										log->LogText("> " + filename + "is not a valid sound file!");
									}
								}
							}
						}
					}
				}
			}
		}
		else if (fileType == "ui")
		{
			for (const auto& j : fs::directory_iterator(i))
			{
				if (j.path().filename() == "replace" || j.path().filename() == "replace_patch")
				{
					for (const auto& k : fs::directory_iterator(j))
					{
						if (k.path().filename() == "chara")
						{
							string charaPath = k.path().string();

							for (const auto& l : fs::directory_iterator(k))
							{
								// Folder = chara_[x]
								if (l.is_directory())
								{
									// File = chara_[x]_[charcode]_[slot].bntx
									for (const auto& m : fs::directory_iterator(l))
									{
										string charcode = m.path().filename().string().substr(m.path().filename().string().find('_', 6) + 1);

										int underscoreIndex = charcode.rfind('_');

										if (underscoreIndex != string::npos)
										{
											string slot = charcode.substr(underscoreIndex + 1, 2);
											charcode = charcode.substr(0, underscoreIndex);

											if (charcode == "eflame_only" || charcode == "eflame_first")
											{
												charcode = "eflame";
											}
											else if (charcode == "elight_only" || charcode == "elight_first")
											{
												charcode = "elight";
											}

											string file = m.path().string();
											addFile(charcode, fileType, slot, file);
										}
									}
								}
							}
						}
					}
				}
			}
		}
		else
		{
			// Other fileTypes are not currently supported
			// log->LogText("> " + fileType + " is currently not supported, no changes will be made to " + fileType);
			continue;
		}
	}
}

// Mod Getters
wxArrayString ModHandler::getCharacters() const
{
	wxArrayString characters;

	for (auto i = mod.begin(); i != mod.end(); i++)
	{
		if (charNames.find(i->first) != charNames.end())
		{
			characters.Add(charNames.find(i->first)->second);
		}
		else
		{
			log->LogText("> " + i->first + "'s name could not be found!");
		}
	}

	std::sort(characters.begin(), characters.end());

	return characters;
}

wxArrayString ModHandler::getFileTypes(string charcode) const
{
	wxArrayString fileTypes;

	auto charIter = mod.find(charcode);

	if (charIter != mod.end())
	{
		for (auto i = charIter->second.begin(); i != charIter->second.end(); i++)
		{
			fileTypes.Add(i->first);
		}
	}

	return fileTypes;
}

wxArrayString ModHandler::getSlots(string charcode) const
{
	// Stores slots, later converted to wxArrayString
	set<wxString> slotsSet;

	auto charIter = mod.find(charcode);
	bool special = false;

	if (charcode == "popo" || charcode == "nana")
	{
		charIter = mod.find("ice_climber");
		special = true;
	}

	if (charIter != mod.end())
	{
		for (int i = 0; i < fileTypes.size(); i++)
		{
			auto fileTypeIter = charIter->second.find(fileTypes[i]);

			if (fileTypeIter != charIter->second.end())
			{
				for (auto j = fileTypeIter->second.begin(); j != fileTypeIter->second.end(); j++)
				{
					if (special)
					{
						for (auto k = j->second.begin(); k != j->second.end(); k++)
						{
							string label;

							if (fileTypeIter->first == "effect" || fileTypeIter->first == "fighter")
							{
								label = "/" + charcode + "/";
							}
							else if (fileTypeIter->first == "ui" || fileTypeIter->first == "sound")
							{
								label = "_" + charcode + "_";
							}

							if (k->rfind(label) != string::npos)
							{
								slotsSet.insert(j->first);
								break;
							}
						}
					}
					else
					{
						slotsSet.insert(j->first);
					}
				}
			}
		}
	}

	wxArrayString slots;

	for (auto i = slotsSet.begin(); i != slotsSet.end(); i++)
	{
		slots.Add(i->ToStdString());
	}

	return slots;
}

wxArrayString ModHandler::getSlots(string charcode, string fileType) const
{
	wxArrayString slots;

	auto charIter = mod.find(charcode);
	bool special = false;

	if (charcode == "popo" || charcode == "nana")
	{
		charIter = mod.find("ice_climber");
		special = true;
	}

	if (charIter != mod.end())
	{
		auto fileTypeIter = charIter->second.find(fileType);

		if (fileTypeIter != charIter->second.end())
		{
			for (auto i = fileTypeIter->second.begin(); i != fileTypeIter->second.end(); i++)
			{
				if (special)
				{
					for (auto j = i->second.begin(); j != i->second.end(); j++)
					{
						string label;

						if (fileTypeIter->first == "effect" || fileTypeIter->first == "fighter")
						{
							label = "/" + charcode + "/";
						}
						else if (fileTypeIter->first == "ui" || fileTypeIter->first == "sound")
						{
							label = "_" + charcode + "_";
						}

						if (j->rfind(label) != string::npos)
						{
							slots.Add(i->first);
							break;
						}
					}
				}
				else
				{
					slots.Add(i->first);
				}
			}
		}
	}

	return slots;
}

wxArrayString ModHandler::getSlots(string charcode, wxArrayString fileTypes) const
{
	wxArrayString initSlots;
	wxArrayString slots;

	auto charIter = mod.find(charcode);
	bool special = false;

	if (charcode == "popo" || charcode == "nana")
	{
		charIter = mod.find("ice_climber");
		special = true;
	}
	else
	{

	}

	// Get slots for one file type
	if (!fileTypes.empty() && charIter != mod.end())
	{
		// Priorotize file types besides "effect" (it has "all")
		if (fileTypes[0] == "effect" && fileTypes.size() > 1)
		{
			initSlots = getSlots(charcode, fileTypes[1].ToStdString());
		}
		else
		{
			initSlots = getSlots(charcode, fileTypes[0].ToStdString());
		}

		if (fileTypes.size() <= 1)
		{
			return initSlots;
		}
	}
	else
	{
		return slots;
	}

	// Add to slots if available in each fileType
	for (int i = 0; i < initSlots.size(); i++)
	{
		bool foundInAll = true;

		for (int j = 0; j < fileTypes.size(); j++)
		{
			auto fileTypeIter = charIter->second.find(fileTypes[j].ToStdString());

			if (fileTypeIter != charIter->second.end())
			{
				auto slotIter = fileTypeIter->second.find(initSlots[i].ToStdString());

				// Slot from different fileType was not found in current fileType
				if (slotIter == fileTypeIter->second.end())
				{
					if (fileTypes[j] != "effect")
					{
						foundInAll = false;
					}
					// FileType is effect and an "all" version does not exist.
					else if (fileTypeIter->second.find("all") == fileTypeIter->second.end())
					{
						foundInAll = false;
					}
				}
				else if (special)
				{
					for (auto k = slotIter->second.begin(); k != slotIter->second.end(); k++)
					{
						string label;

						if (fileTypeIter->first == "effect" || fileTypeIter->first == "fighter")
						{
							label = "/" + charcode + "/";
						}
						else if (fileTypeIter->first == "ui" || fileTypeIter->first == "sound")
						{
							label = "_" + charcode + "_";
						}

						if (k->rfind(label) != string::npos)
						{
							break;
						}
					}

					foundInAll = false;
				}
			}
			else
			{
				foundInAll = false;
			}
		}

		if (foundInAll)
		{
			slots.Add(initSlots[i]);
		}
	}

	return slots;
}

string ModHandler::getBaseSlot(string charcode, string addSlot)
{
	if (stoi(addSlot) <= 7)
	{
		return addSlot;
	}
	else if (baseSlots.find(charcode) != baseSlots.end())
	{
		for (auto i = baseSlots[charcode].begin(); i != baseSlots[charcode].end(); i++)
		{
			if (i->second.find(addSlot) != i->second.end())
			{
				return i->first;
			}
		}
	}

	return "-1";
}

map<string, set<string>> ModHandler::getAllSlots(string charcode)
{
	map<string, set<string>> allSlots;

	if (charcode == "all")
	{
		for (auto i = mod.begin(); i != mod.end(); i++)
		{
			wxArrayString slots = getSlots(i->first);

			for (auto j = slots.begin(); j != slots.end(); j++)
			{
				if (*j == "all")
				{
					continue;
				}

				allSlots[i->first].insert(j->ToStdString());
			}
		}
	}
	else
	{
		wxArrayString slots = getSlots("charcode");

		for (auto j = slots.begin(); j != slots.end(); j++)
		{
			if (*j == "all")
			{
				continue;
			}

			allSlots[charcode].insert(j->ToStdString());
		}
	}


	return allSlots;
}

// Mod Verifiers
bool ModHandler::hasFileType(string fileType) const
{
	for (auto i = mod.begin(); i != mod.end(); i++)
	{
		for (auto j = i->second.begin(); j != i->second.end(); j++)
		{
			if (j->first == fileType)
			{
				return true;
			}
		}
	}

	return false;
}

bool ModHandler::hasAdditionalSlot() const
{
	for (auto i = mod.begin(); i != mod.end(); i++)
	{
		for (auto j = i->second.begin(); j != i->second.end(); j++)
		{
			for (auto k = j->second.begin(); k != j->second.end(); k++)
			{
				try
				{
					if (stoi(k->first) > 7)
					{
						// Ignore kirby slots that only contain copy files.
						if (i->first == "kirby" && j->first == "fighter")
						{
							bool additional = false;

							for (auto l = k->second.begin(); l != k->second.end(); l++)
							{
								if (l->find("copy_") == string::npos)
								{
									additional = true;
									break;
								}
							}

							if (additional)
							{
								return true;
							}
							else
							{
								continue;
							}
						}

						return true;
					}
				}
				catch (...)
				{
					// Likely "all" slot from effect. Ignore it.
				}
			}
		}
	}

	return false;
}

// TODO: Remove duplicate code
bool ModHandler::hasAdditionalSlot(string charcode) const
{
	auto charIter = mod.find(charcode);

	if (charIter != mod.end())
	{
		for (auto i = charIter->second.begin(); i != charIter->second.end(); i++)
		{
			for (auto j = i->second.begin(); j != i->second.end(); j++)
			{
				try
				{
					if (stoi(j->first) > 7)
					{
						// Ignore kirby slots that only contain copy files.
						if (i->first == "kirby" && j->first == "fighter")
						{
							bool additional = false;

							for (auto k = j->second.begin(); k != j->second.end(); k++)
							{
								if (k->find("copy_") == string::npos)
								{
									additional = true;
									break;
								}
							}

							if (additional)
							{
								return true;
							}
							else
							{
								continue;
							}
						}

						return true;
					}
				}
				catch (...)
				{
					// Likely "all" slot from effect. Ignore it.
				}
			}
		}
	}

	return false;
}

bool ModHandler::hasSlot(string charcode, string slot) const
{
	// Conver vector to wxArray
	wxArrayString fileTypes;

	for (int i = 0; i < this->fileTypes.size(); i++)
	{
		fileTypes.Add(this->fileTypes[i]);
	}

	// Filetypes is from the ModHandler members
	return this->hasSlot(charcode, fileTypes, slot);
}

bool ModHandler::hasSlot(string charcode, wxArrayString fileTypes, string slot) const
{
	auto charIter = mod.find(charcode);
	bool special = false;

	if (charcode == "popo" || charcode == "nana")
	{
		charIter = mod.find("ice_climber");
		special = true;
	}

	if (charIter != mod.end())
	{
		for (int i = 0; i < fileTypes.size(); i++)
		{
			auto fileTypeIter = charIter->second.find(fileTypes[i].ToStdString());

			if (fileTypeIter != charIter->second.end())
			{
				auto slotIter = fileTypeIter->second.find(slot);

				if (slotIter != fileTypeIter->second.end())
				{
					if (special)
					{
						for (auto j = slotIter->second.begin(); j != slotIter->second.end(); j++)
						{
							string label;

							if (fileTypeIter->first == "effect" || fileTypeIter->first == "fighter")
							{
								label = "/" + charcode + "/";
							}
							else if (fileTypeIter->first == "ui" || fileTypeIter->first == "sound")
							{
								label = "_" + charcode + "_";
							}

							if (j->rfind(label) != string::npos)
							{
								return true;
							}
						}
					}
					else
					{
						return true;
					}
				}
			}
		}
	}

	return false;
}

// Mod Modifiers
void ModHandler::adjustFiles(string action, string charcode, wxArrayString fileTypes, string initSlot, string finalSlot)
{
	auto charIter = mod.find(charcode);

	if (charIter != mod.end())
	{
		for (int i = 0; i < fileTypes.size(); i++)
		{
			auto fileTypeIter = charIter->second.find(fileTypes[i].ToStdString());

			if (fileTypeIter != charIter->second.end())
			{
				auto slotIter = fileTypeIter->second.find(initSlot);

				if (slotIter != fileTypeIter->second.end())
				{
					for (auto j = slotIter->second.begin(); j != slotIter->second.end(); j++)
					{
						std::filesystem::path initPath(*j);
						std::filesystem::path finalPath(*j);

						if (fileTypes[i] == "effect")
						{
							// One-Slotted Effect
							if (initSlot != "all")
							{
								// eff file
								if (j->find(".eff") != std::string::npos)
								{
									string fileName = finalPath.filename().string();
									fileName = fileName.substr(0, fileName.rfind("_")) + "_c" + finalSlot + ".eff";
									finalPath.replace_filename(fileName);
								}
								// Effect_specific folder
								else
								{
									string folderName = finalPath.filename().string();
									folderName = folderName.substr(0, folderName.size() - initSlot.size()).append(finalSlot);
									finalPath.replace_filename(folderName);
								}
							}
							// NOT One-Slotted Effect
							else
							{
								initPath = *j;
								finalPath = *j;

								if (j->find(".eff") != std::string::npos)
								{
									// .eff = 4
									string fileName = finalPath.filename().string();
									fileName = fileName.substr(0, fileName.size() - 4) + "_c" + finalSlot + ".eff";
									finalPath.replace_filename(fileName);
								}
								else
								{
									finalPath.replace_filename(finalPath.filename().string() + "_c" + finalSlot);
								}
							}
						}
						else if (fileTypes[i] == "fighter")
						{
							finalPath.replace_filename("c" + finalSlot);
						}
						else if (fileTypes[i] == "sound")
						{
							// se_[charcode]_c[slot].[filetype]
							// vc_[charcode]_c[slot].[filetype]
							// vc_[charcode]_cheer_c[slot].[filetype]
							string fileName = finalPath.filename().string();
							size_t dotPos = fileName.find(".");

							if (isdigit(fileName[dotPos - 1]))
							{
								fileName = fileName.substr(0, fileName.rfind("_c") + 2) + finalSlot;
								fileName += initPath.extension().string();
							}
							else
							{
								fileName = fileName.substr(0, dotPos) + "_c" + finalSlot + initPath.extension().string();
							}

							finalPath.replace_filename(fileName);
						}
						else if (fileTypes[i] == "ui")
						{
							// chara_[x]_[charcode]_[slot].bntx
							// . + b + n + t + x = 5
							string fileName = finalPath.filename().string();
							fileName = fileName.substr(0, fileName.size() - initSlot.size() - 5) + finalSlot;
							fileName += initPath.extension().string();
							finalPath.replace_filename(fileName);
						}
						else
						{
							log->LogText("> Error! " + charcode + " has no " + fileTypes[i] + " file type!");
							return;
						}

						try
						{
							if (action == "move")
							{
								std::filesystem::rename(initPath, finalPath);
							}
							else if (action == "duplicate")
							{
								std::filesystem::copy(initPath, finalPath, fs::copy_options::recursive);
							}
							else if (action == "delete")
							{
								std::filesystem::remove_all(initPath);
							}
							else
							{
								log->LogText("> Error! " + action + " not found!");
								return;
							}

							if (action != "delete")
							{
								string resultPath = finalPath.string();
								std::replace(resultPath.begin(), resultPath.end(), '\\', '/');

								mod[charcode][fileTypes[i].ToStdString()][finalSlot].insert(resultPath);
							}
						}
						catch (...)
						{
							if (action == "move")
							{
								log->LogText("> Error! " + initPath.string() + " could not be renamed!");
							}
							else if (action == "duplicate")
							{
								log->LogText("> Error! " + initPath.string() + " could not be copied!");
							}
							else if (action == "delete")
							{
								log->LogText("Error! " + initPath.string() + " could not be deleted!");
							}
							else
							{
								log->LogText("> Error! " + action + " not found!");
							}

							return;
						}
					}
				}
			}
			else
			{
				continue;
			}

			if (action == "move")
			{
				mod[charcode][fileTypes[i].ToStdString()].extract(initSlot);
			}
			else if (action == "delete")
			{
				mod[charcode][fileTypes[i].ToStdString()].extract(initSlot);

				if (mod[charcode][fileTypes[i].ToStdString()].empty())
				{
					mod[charcode].extract(fileTypes[i].ToStdString());

					if (fileTypes[i] == "fighter")
					{
						std::filesystem::remove_all((path + "/fighter/" + charcode));
					}

					// Delete empty folders
					// TODO: Check folder before deletion
					/*
					if (!hasFileType(fileTypes[i]))
					{
						std::filesystem::remove_all((rootPath + "/" + fileTypes[i]).ToStdString());
					}
					*/
				}

				if (mod[charcode].empty())
				{
					mod.extract(charcode);
				}
			}
		}
	}
	else
	{
		log->LogText("> Error! " + charcode + " does not exist!");
	}

	if (action == "move")
	{
		if (finalSlot == "all")
		{
			log->LogText("> Success! " + charcode + "'s default effect was moved to c" + finalSlot + "!");
		}
		else
		{
			log->LogText("> Success! " + charcode + "'s c" + initSlot + " was moved to c" + finalSlot + "!");
		}
	}
	else if (action == "duplicate")
	{
		if (finalSlot == "all")
		{
			log->LogText("> Success! " + charcode + "'s default effect was duplicated to c" + finalSlot + "!");
		}
		else
		{
			log->LogText("> Success! " + charcode + "'s c" + initSlot + " was duplicated to c" + finalSlot + "!");
		}
	}
	else if (action == "delete")
	{
		if (finalSlot == "all")
		{
			log->LogText("> Success! " + charcode + "'s default effect was deleted!");
		}
		else
		{
			log->LogText("> Success! " + charcode + "'s c" + initSlot + " was deleted!");
		}
	}
	else
	{
		if (finalSlot == "all")
		{
			log->LogText("> Success ? " + charcode + "'s default effect was " + action + "-ed to c" + finalSlot + "!");
		}
		else
		{
			log->LogText("> Success? " + charcode + "'s c" + initSlot + " was " + action + "-ed to c" + finalSlot + "!");
		}
	}
}

void ModHandler::removeDesktopINI()
{
	int count = 0;

	queue<fs::directory_entry> folders;
	folders.push(fs::directory_entry(path));

	while (!folders.empty())
	{
		for (const auto& i : fs::directory_iterator(folders.front()))
		{
			if (i.is_directory())
			{
				folders.push(i);
			}
			else
			{
				string filename = i.path().filename().string();

				for (int i = 0; i < filename.size(); i++)
				{
					filename[i] = tolower(filename[i]);
				}

				if (filename == "desktop.ini")
				{
					fs::remove(i);
					count++;

					string path = i.path().string();
					std::replace(path.begin(), path.end(), '\\', '/');
					path = path.substr(path.size());

					log->LogText("> Success: Deleted " + path);
				}
			}
		}

		folders.pop();
	}

	if (count == 0)
	{
		log->LogText("> Success: No desktop.ini files were found.");
	}
	else
	{
		log->LogText("> Success: " + to_string(count) + " desktop.ini files were deleted.");
	}
}

// Config Getters
map<string, set<string>> ModHandler::getAddSlots()
{
	map<string, set<string>> additionalSlots;

	for (auto i = mod.begin(); i != mod.end(); i++)
	{
		for (auto j = i->second.begin(); j != i->second.end(); j++)
		{
			for (auto k = j->second.begin(); k != j->second.end(); k++)
			{
				try
				{
					if (std::stoi(k->first) > 7)
					{
						// Ignore kirby slots that only contain copy files.
						if (i->first == "kirby" && j->first == "fighter")
						{
							for (auto l = k->second.begin(); l != k->second.end(); l++)
							{
								if (l->find("copy_") == string::npos)
								{
									break;
								}
							}

							continue;
						}

						additionalSlots[i->first].insert(k->first);
					}

					// TODO: Possibly incorporate base slot selection for cloud, ike, etc.
					// NOTE: Might not be needed as newDirFile currently accounts for all files of choosen slot
				}
				catch (...)
				{
					// Invalid slot (likely "all" from effect)
				}
			}
		}
	}

	return additionalSlots;
}

void ModHandler::getNewDirSlots
(
	// charcode, base-slot, actual-slots
	const map<string, map<string, set<string>>>& baseSlots,
	// files
	vector<string>& newDirInfos,
	// files
	vector<string>& newDirInfosBase,
	// charcode, slot, file, files
	map<string, map<string, map<string, set<string>>>>& shareToVanilla,
	// charcode, slot, file, files
	map<string, map<string, map<string, set<string>>>>& shareToAdded,
	// charcode, slot, section-label, files
	map<string, map<string, map<string, set<string>>>>& newDirFiles
)
{
	return;
}

void ModHandler::createConfig(map<string, map<string, set<string>>>& baseSlots)
{
	vector<string> newDirInfos;
	vector<string> newDirInfosBase;
	map<string, map<string, map<string, set<string>>>> shareToVanilla;
	map<string, map<string, map<string, set<string>>>> shareToAdded;
	map<string, map<string, map<string, set<string>>>> newDirFiles;

	this->getNewDirSlots(baseSlots, newDirInfos, newDirInfosBase, shareToVanilla, shareToAdded, newDirFiles);

	if (!newDirInfos.empty() || !newDirInfosBase.empty() || !shareToVanilla.empty() || !shareToAdded.empty() || !newDirFiles.empty())
	{
		bool first = true;

		ofstream configFile;
		configFile.open(path + "/" + "config.json", std::ios::out | std::ios::trunc);

		if (configFile.is_open())
		{
			configFile << "{";

			if (!newDirInfos.empty())
			{
				configFile << "\n\t\"new-dir-infos\": [";

				for (auto i = newDirInfos.begin(); i != newDirInfos.end(); i++)
				{
					if (first)
					{
						first = false;
					}
					else
					{
						configFile << ",";
					}

					configFile << "\n\t\t" << *i;
				}

				configFile << "\n\t]";
			}

			if (!newDirInfosBase.empty())
			{
				if (!first)
				{
					configFile << ",";
					first = true;
				}

				configFile << "\n\t\"new-dir-infos-base\": {";

				for (auto i = newDirInfosBase.begin(); i != newDirInfosBase.end(); i++)
				{
					if (first)
					{
						first = false;
					}
					else
					{
						configFile << ",";
					}

					configFile << "\n\t\t" << *i;
				}

				configFile << "\n\t}";
			}

			vector sectionType = { &shareToVanilla, &shareToAdded, &newDirFiles };
			for (auto i = 0; i < sectionType.size(); i++)
			{
				if (!((*sectionType[i]).empty()))
				{
					if (!first)
					{
						configFile << ",";
						first = true;
					}

					if (i == 0)
					{
						configFile << "\n\t\"share-to-vanilla\": {";
					}
					else if (i == 1)
					{
						configFile << "\n\t\"share-to-added\": {";
					}
					else
					{
						configFile << "\n\t\"new-dir-files\": {";
					}

					for (auto j = sectionType[i]->begin(); j != sectionType[i]->end(); j++)
					{
						for (auto k = j->second.begin(); k != j->second.end(); k++)
						{
							for (auto l = k->second.begin(); l != k->second.end(); l++)
							{
								bool firstBox = true;

								for (auto m = l->second.begin(); m != l->second.end(); m++)
								{
									if (first)
									{
										first = false;
									}
									else
									{
										configFile << ",";
									}

									if (firstBox)
									{
										configFile << "\n\t\t" + l->first << ": [";
										firstBox = false;
									}

									configFile << "\n\t\t\t" << *m;
								}

								if (!firstBox)
								{
									configFile << "\n\t\t]";
								}
								else
								{
									if (first)
									{
										first = false;
									}
									else
									{
										configFile << ",";
									}

									configFile << "\n\t\t" + l->first << ": []";
								}
							}
						}
					}

					configFile << "\n\t}";
				}
			}

			configFile << "\n}";
			configFile.close();

			log->LogText("> Success: Config file was created!");
		}
		else
		{
			log->LogText("> Error: " + (path + "/" + "config.json") + " could not be opened!");
		}
	}
	else
	{
		log->LogText("> N/A: Config file is not needed!");
	}
}

void ModHandler::patchXMLSlots(map<string, int>& maxSlots)
{
	if (!maxSlots.empty())
	{
		ifstream input("ui_chara_db.xml");
		ofstream output("ui_chara_db_EDIT.xml");

		auto additionalSlots = getAddSlots();
		vector<int> defaultCs;

		if (input.is_open() && output.is_open())
		{
			string charcode;
			string line;

			bool changeActive = false;

			while (!input.eof())
			{
				getline(input, line);

				if (changeActive)
				{
					if (line.find("\"color_num\"") != string::npos)
					{
						line = "      <byte hash=\"color_num\">" + to_string(maxSlots[charcode]) + "</byte>";
					}
					else if (line.find("<byte hash=\"c0") != string::npos)
					{
						defaultCs.push_back(line[line.find('>') + 1]);

						// Last c reached, start adding new ones
						if (line.find("c07") != string::npos)
						{
							output << line << "\n";

							string lineBeg = "      <byte hash=\"c";
							string lineMid;
							if (line.find("c07_group") != string::npos)
							{
								lineMid = "_group\">";
							}
							else
							{
								lineMid = "_index\">";
							}
							string lineEnd = "</byte>";

							for (auto i = additionalSlots[charcode].begin(); i != additionalSlots[charcode].end(); i++)
							{
								char val;

								// Find base slot
								for (auto j = baseSlots[charcode].begin(); j != baseSlots[charcode].end(); j++)
								{
									if (j->second.find(*i) != j->second.end())
									{
										val = defaultCs[stoi(j->first)];
										break;
									}
								}

								output << lineBeg << *i << lineMid << val << lineEnd << "\n";
							}

							defaultCs.clear();
							continue;
						}
					}
					else if (line.find("</struct>") != string::npos)
					{
						changeActive = false;
					}
				}
				else if (line.find("\"name_id\"") != string::npos)
				{
					int begin = line.find(">") + 1;
					int end = line.find("<", begin);
					charcode = line.substr(begin, end - begin);

					if (maxSlots.find(charcode) != maxSlots.end())
					{
						changeActive = true;
					}
				}

				output << line << "\n";
			}

			input.close();
			output.close();

			fs::remove(fs::current_path() / "ui_chara_db.xml");
			fs::rename(fs::current_path() / "ui_chara_db_EDIT.xml", fs::current_path() / "ui_chara_db.xml");
		}
		else
		{
			if (!input.is_open())
			{
				log->LogText("> Error: " + fs::current_path().string() + "/ui_chara_db.xml could not be opened!");
			}

			if (!output.is_open())
			{
				log->LogText("> Error: " + fs::current_path().string() + "/ui_chara_db_EDIT.xml could not be opened!");
			}
		}
	}
}

void ModHandler::patchXMLNames(map<string, map<int, Name>>& names)
{
	if (!names.empty())
	{
		ifstream uiVanilla("ui_chara_db.xml");
		ofstream uiEdit("ui_chara_db_EDIT.xml");
		ofstream msg("msg_name.xmsbt", ios::out | ios::binary);

		if (uiVanilla.is_open() && uiEdit.is_open() && msg.is_open())
		{
			// UTF-16 LE BOM
			unsigned char smarker[2];
			smarker[0] = 0xFF;
			smarker[1] = 0xFE;

			msg << smarker[0];
			msg << smarker[1];

			msg.close();

			wofstream msgUTF("msg_name.xmsbt", ios::binary | ios::app);
			msgUTF.imbue(std::locale(msgUTF.getloc(), new std::codecvt_utf16<wchar_t, 0x10ffff, std::little_endian>));

			if (!msgUTF.is_open())
			{
				log->LogText("> " + fs::current_path().string() + "/msg_name.xmsbt could not be opened!");
				return;
			}

			outputUTF(msgUTF, "<?xml version=\"1.0\" encoding=\"utf-16\"?>");
			outputUTF(msgUTF, "\n<xmsbt>");

			string charcode;
			string line;

			while (!uiVanilla.eof())
			{
				getline(uiVanilla, line);

				if (line.find("\"name_id\"") != string::npos)
				{
					int begin = line.find(">") + 1;
					int end = line.find("<", begin);
					charcode = line.substr(begin, end - begin);

					auto charIter = names.find(charcode);

					if (charIter != names.end())
					{
						uiEdit << line << "\n";

						string tempLine;

						auto i = charIter->second.begin();
						while (i != charIter->second.end())
						{
							if (tempLine.find("\"n07_index\"") == string::npos)
							{
								getline(uiVanilla, line);
								tempLine = line;

								// Output first n07 if not in change list
								if (tempLine.find("\"n07_index\"") != string::npos && i->first != 7)
								{
									uiEdit << line << "\n";
								}
							}

							string label;

							if (i->first > 7)
							{
								label = "\"n07_index\"";
							}
							else
							{
								label = "\"n0" + to_string(i->first) + "_index\"";
							}

							if (tempLine.find(label) != string::npos)
							{
								string slot;
								string nameSlot = to_string(i->first + 8);

								if (nameSlot.size() == 1)
								{
									nameSlot = "0" + nameSlot;
								}

								if (i->first > 9)
								{
									slot = to_string(i->first);
								}
								else
								{
									slot = "0" + to_string(i->first);
								}

								line = "\t<byte hash=\"n" + slot + "_index\">" + nameSlot + "</byte>";

								if (nameSlot == "08")
								{
									outputUTF(msgUTF, "\n\t<entry label=\"nam_chr3_" + nameSlot + "_" + charcode + "\">");
									outputUTF(msgUTF, "\n\t\t<text>");
									outputUTF(msgUTF, i->second.cssName, true);
									outputUTF(msgUTF, "</text>");
									outputUTF(msgUTF, "\n\t</entry>");
								}

								outputUTF(msgUTF, "\n\t<entry label=\"nam_chr1_" + nameSlot + "_" + charcode + "\">");
								outputUTF(msgUTF, "\n\t\t<text>");
								outputUTF(msgUTF, i->second.cspName, true);
								outputUTF(msgUTF, "</text>");
								outputUTF(msgUTF, "\n\t</entry>");

								outputUTF(msgUTF, "\n\t<entry label=\"nam_chr2_" + nameSlot + "_" + charcode + "\">");
								outputUTF(msgUTF, "\n\t\t<text>");
								outputUTF(msgUTF, i->second.vsName, true);
								outputUTF(msgUTF, "</text>");
								outputUTF(msgUTF, "\n\t</entry>");

								if (charcode != "eflame_first" && charcode != "elight_first")
								{
									outputUTF(msgUTF, "\n\t<entry label=\"nam_stage_name_" + nameSlot + "_" + charcode + "\">");
									outputUTF(msgUTF, "\n\t\t<text>");
									outputUTF(msgUTF, i->second.stageName, true);
									outputUTF(msgUTF, "</text>");
									outputUTF(msgUTF, "\n\t</entry>");
								}

								i++;
							}

							uiEdit << line << "\n";
						}
					}
				}
				else
				{
					uiEdit << line << "\n";
				}
			}

			outputUTF(msgUTF, "\n</xmsbt>");

			uiVanilla.close();
			uiEdit.close();
			msgUTF.close();

			fs::remove(fs::current_path() / "ui_chara_db.xml");
			fs::rename(fs::current_path() / "ui_chara_db_EDIT.xml", fs::current_path() / "ui_chara_db.xml");

			fs::create_directories(path + "/ui/message/");
			fs::rename(fs::current_path() / "msg_name.xmsbt", path + "/ui/message/msg_name.xmsbt");
		}
		else
		{
			if (!uiVanilla.is_open())
			{
				log->LogText("> Error: " + fs::current_path().string() + "/ui_chara_db.xml could not be opened!");
			}

			if (!uiEdit.is_open())
			{
				log->LogText("> Error: " + fs::current_path().string() + "/ui_chara_db_EDIT.xml could not be opened!");
			}

			if (!msg.is_open())
			{
				log->LogText("> Error: " + fs::current_path().string() + "/msg_name.xmsbt could not be opened!");
			}
		}
	}
}

void ModHandler::patchXMLAnnouncer(map<string, map<int, string>>& announcers)
{
	if (!announcers.empty())
	{
		ifstream uiVanilla("ui_chara_db.xml");
		ofstream uiEdit("ui_chara_db_EDIT.xml");

		if (uiVanilla.is_open() && uiEdit.is_open())
		{
			string charcode;
			string line;

			while (!uiVanilla.eof())
			{
				getline(uiVanilla, line);

				if (line.find("\"name_id\"") != string::npos)
				{
					int begin = line.find(">") + 1;
					int end = line.find("<", begin);
					charcode = line.substr(begin, end - begin);

					auto charIter = announcers.find(charcode);

					if (charIter != announcers.end())
					{
						uiEdit << line << "\n";

						vector<string> vanillaLabels;
						string label = "";
						bool action = false;

						// Deal with characall_label
						while (line.find("</struct>") == string::npos)
						{
							getline(uiVanilla, line);
							uiEdit << line << endl;

							// Store vanillaLabels
							if (line.find("\"characall_label") != string::npos)
							{
								auto startPos = line.find('>') + 1;
								vanillaLabels.push_back(line.substr(startPos, line.rfind("<") - startPos));
							}

							if (line.find("\"characall_label_c07\"") != string::npos)
							{
								label = "characall_label_c";
								action = true;
							}
							else if (line.find("\"characall_label_article_c07\"") != string::npos)
							{
								label = "characall_label_article_c";
								action = true;
							}

							if (action)
							{
								for (auto i = charIter->second.begin(); i != charIter->second.end(); i++)
								{
									string slot = (i->first + 8 > 9 ? "" : "0") + to_string(i->first + 8);

									if (i->second == "Default")
									{
										string temp = (i->first > 9 ? "" : "0") + to_string(i->first);

										// Find base slot
										if (i->first > 7)
										{
											for (auto j = baseSlots[charcode].begin(); j != baseSlots[charcode].end(); j++)
											{
												if (j->second.find(temp) != j->second.end())
												{
													temp = vanillaLabels[stoi(j->first)];
													break;
												}
											}
										}
										else
										{
											temp = vanillaLabels[i->first];
										}

										// Replace with 00's announcer if 0#'s announcer is empty
										if (temp == "")
										{
											temp = vanillaLabels[0];
										}

										line = "      <hash40 hash=\"" + label + slot + "\">" + temp + "</hash40>";
									}
									else
									{
										line = "      <hash40 hash=\"" + label + slot + "\">" + i->second + "</hash40>";
									}

									uiEdit << line << "\n";
								}

								vanillaLabels.clear();
								action = false;
							}
						}
					}
				}
				else
				{
					uiEdit << line << endl;
				}
			}

			uiVanilla.close();
			uiEdit.close();

			fs::remove(fs::current_path() / "ui_chara_db.xml");
			fs::rename(fs::current_path() / "ui_chara_db_EDIT.xml", fs::current_path() / "ui_chara_db.xml");
		}
		else
		{
			if (!uiVanilla.is_open())
			{
				log->LogText("> Error: " + fs::current_path().string() + "/ui_chara_db.xml could not be opened!");
			}

			if (!uiEdit.is_open())
			{
				log->LogText("> Error: " + fs::current_path().string() + "/ui_chara_db_EDIT.xml could not be opened!");
			}
		}
	}
}

void ModHandler::patchXMLInkColors(map<int, InklingColor>& inklingColors)
{
	if (!inklingColors.empty())
	{
		ifstream effectVanilla("effect.xml");
		ofstream effectEdit("effect_EDIT.xml");

		if (effectVanilla.is_open() && effectEdit.is_open())
		{
			string line;

			while (!effectVanilla.eof())
			{
				getline(effectVanilla, line);

				if (line.find("hash=\"ink_") != string::npos)
				{
					char action;

					if (line.find("ink_effect_color") != string::npos)
					{
						action = 'E';
					}
					else
					{
						action = 'A';
					}

					bool lastStruct = false;
					int slot = -1;

					effectEdit << line << endl;

					auto i = inklingColors.begin();
					while (i != inklingColors.end())
					{
						if (!lastStruct)
						{
							getline(effectVanilla, line);
							slot = stoi(line.substr(line.find("\"") + 1, line.rfind("\"") - line.find("\"") - 1));
						}

						if (i->first == slot || lastStruct)
						{
							// <struct index = "[X]">
							effectEdit << "    <struct index=\"" << to_string(i->first) << "\">" << endl;

							if (action == 'E')
							{
								effectEdit << "      <float hash=\"r\">" << (i->second.effect.Red() / 255.0) << "</float>" << endl;
								effectEdit << "      <float hash=\"g\">" << (i->second.effect.Green() / 255.0) << "</float>" << endl;
								effectEdit << "      <float hash=\"b\">" << (i->second.effect.Blue() / 255.0) << "</float>" << endl;
							}
							else
							{
								effectEdit << "      <float hash=\"r\">" << (i->second.arrow.Red() / 255.0) << "</float>" << endl;
								effectEdit << "      <float hash=\"g\">" << (i->second.arrow.Green() / 255.0) << "</float>" << endl;
								effectEdit << "      <float hash=\"b\">" << (i->second.arrow.Blue() / 255.0) << "</float>" << endl;
							}

							if (!lastStruct)
							{
								// Read r, g, b, struct
								for (int i = 0; i < 4; i++)
								{
									getline(effectVanilla, line);
								}
							}

							effectEdit << "    </struct>" << endl;

							i++;
						}
						else
						{
							// <struct index = "[X]">
							effectEdit << line << endl;

							// Read and write r, g, b, struct
							for (int i = 0; i < 4; i++)
							{
								getline(effectVanilla, line);
								effectEdit << line << endl;
							}

							if (slot == 7)
							{
								lastStruct = true;
							}
						}
					}
				}
				else
				{
					effectEdit << line << endl;
				}
			}

			effectVanilla.close();
			effectEdit.close();

			fs::remove(fs::current_path() / "effect.xml");
			fs::rename(fs::current_path() / "effect_EDIT.xml", fs::current_path() / "effect.xml");
		}
		else
		{
			if (!effectVanilla.is_open())
			{
				log->LogText("> Error: " + fs::current_path().string() + "/effect.xml could not be opened!");
			}

			if (!effectEdit.is_open())
			{
				log->LogText("> Error: " + fs::current_path().string() + "/effect_EDIT.xml could not be opened!");
			}
		}
	}
}

void ModHandler::createPRCXML(map<string, map<int, Name>>& names, map<string, map<int, string>>& announcers, map<string, int>& maxSlots)
{
	if (!maxSlots.empty() || !announcers.empty() || !names.empty())
	{
		ifstream uiVanilla("ui_chara_db.xml");
		ofstream uiEdit("ui_chara_db.prcxml");
		ofstream msg;

		if (!names.empty())
		{
			msg.open("msg_name.xmsbt", ios::out | ios::binary);

			if (msg.is_open())
			{
				// UTF-16 LE BOM
				unsigned char smarker[2];
				smarker[0] = 0xFF;
				smarker[1] = 0xFE;

				msg << smarker[0];
				msg << smarker[1];

				msg.close();
			}
			else
			{
				log->LogText("> Error: " + fs::current_path().string() + "/msg_name.xmsbt could not be opened!");
			}
		}

		if (uiVanilla.is_open() && uiEdit.is_open())
		{
			wofstream msgUTF;
			if (!names.empty())
			{
				msgUTF.open("msg_name.xmsbt", ios::binary | ios::app);
				msgUTF.imbue(std::locale(msgUTF.getloc(), new std::codecvt_utf16<wchar_t, 0x10ffff, std::little_endian>));

				if (!msgUTF.is_open())
				{
					log->LogText("> " + fs::current_path().string() + "/msg_name.xmsbt could not be opened!");
					return;
				}

				outputUTF(msgUTF, "<?xml version=\"1.0\" encoding=\"utf-16\"?>");
				outputUTF(msgUTF, "\n<xmsbt>");
			}

			uiEdit << "<?xml version=\"1.0\" encoding=\"UTF-16\"?>";
			uiEdit << "\n<struct>";
			uiEdit << "\n\t<list hash=\"db_root\">";

			string charcode;
			string line;
			string currIndex = "";
			char status = 0;

			while (!uiVanilla.eof())
			{
				getline(uiVanilla, line);

				if (line.find("<struct index=") != string::npos)
				{
					// TODO: Make efficent
					currIndex = line.substr(line.find('"') + 1, line.rfind('"') - line.find('"') - 1);
				}
				else if (line.find("\"name_id\"") != string::npos)
				{
					int begin = line.find(">") + 1;
					int end = line.find("<", begin);
					charcode = line.substr(begin, end - begin);

					// Deal with max-slots first
					if (maxSlots.find(charcode) != maxSlots.end())
					{
						status = 1;
						uiEdit << "\n\t\t<struct index=\"" << currIndex << "\">";
						uiEdit << "\n\t\t\t<byte hash=\"color_num\">" << to_string(maxSlots[charcode]) << "</byte>";
					}

					// Deal with names second
					if (!names.empty() && names.find(charcode) != names.end())
					{
						if (status != 1)
						{
							uiEdit << "\n\t\t<struct index=\"" << currIndex << "\">";
							status = 1;
						}
						auto charIter = names.find(charcode);

						auto i = charIter->second.begin();
						while (i != charIter->second.end())
						{
							string slot;
							string nameSlot = to_string(i->first + 8);

							if (nameSlot.size() == 1)
							{
								nameSlot = "0" + nameSlot;
							}

							if (i->first > 9)
							{
								slot = to_string(i->first);
							}
							else
							{
								slot = "0" + to_string(i->first);
							}

							uiEdit << "\n\t\t\t<byte hash=\"n" << slot << "_index\">" << nameSlot << "</byte>";

							if (nameSlot == "08")
							{
								outputUTF(msgUTF, "\n\t<entry label=\"nam_chr3_" + nameSlot + "_" + charcode + "\">");
								outputUTF(msgUTF, "\n\t\t<text>");
								outputUTF(msgUTF, i->second.cssName, true);
								outputUTF(msgUTF, "</text>");
								outputUTF(msgUTF, "\n\t</entry>");
							}

							outputUTF(msgUTF, "\n\t<entry label=\"nam_chr1_" + nameSlot + "_" + charcode + "\">");
							outputUTF(msgUTF, "\n\t\t<text>");
							outputUTF(msgUTF, i->second.cspName, true);
							outputUTF(msgUTF, "</text>");
							outputUTF(msgUTF, "\n\t</entry>");

							outputUTF(msgUTF, "\n\t<entry label=\"nam_chr2_" + nameSlot + "_" + charcode + "\">");
							outputUTF(msgUTF, "\n\t\t<text>");
							outputUTF(msgUTF, i->second.vsName, true);
							outputUTF(msgUTF, "</text>");
							outputUTF(msgUTF, "\n\t</entry>");

							if (charcode != "eflame_first" && charcode != "elight_first")
							{
								outputUTF(msgUTF, "\n\t<entry label=\"nam_stage_name_" + nameSlot + "_" + charcode + "\">");
								outputUTF(msgUTF, "\n\t\t<text>");
								outputUTF(msgUTF, i->second.stageName, true);
								outputUTF(msgUTF, "</text>");
								outputUTF(msgUTF, "\n\t</entry>");
							}

							i++;
						}
					}

					// Deal with announcers third
					if (!announcers.empty() && announcers.find(charcode) != announcers.end())
					{
						if (status != 1)
						{
							uiEdit << "\n\t\t<struct index=\"" << currIndex << "\">";
							status = 1;
						}
						auto charIter = announcers.find(charcode);

						vector<string> vanillaLabels;
						string label = "";
						bool action = false;

						// Deal with characall_label
						while (line.find("shop_item_tag") == string::npos)
						{
							getline(uiVanilla, line);

							// Store vanillaLabels
							if (line.find("\"characall_label") != string::npos)
							{
								auto startPos = line.find('>') + 1;
								vanillaLabels.push_back(line.substr(startPos, line.rfind("<") - startPos));
							}

							if (line.find("\"characall_label_c07\"") != string::npos)
							{
								label = "characall_label_c";
								action = true;
							}
							else if (line.find("\"characall_label_article_c07\"") != string::npos)
							{
								label = "characall_label_article_c";
								action = true;
							}

							if (action)
							{
								for (auto i = charIter->second.begin(); i != charIter->second.end(); i++)
								{
									string slot = (i->first + 8 > 9 ? "" : "0") + to_string(i->first + 8);

									if (i->second == "Default")
									{
										string temp = (i->first > 9 ? "" : "0") + to_string(i->first);

										// Find base slot
										if (i->first > 7)
										{
											for (auto j = baseSlots[charcode].begin(); j != baseSlots[charcode].end(); j++)
											{
												if (j->second.find(temp) != j->second.end())
												{
													temp = vanillaLabels[stoi(j->first)];
													break;
												}
											}
										}
										else
										{
											temp = vanillaLabels[i->first];
										}

										// Replace with 00's announcer if 0#'s announcer is empty
										if (temp == "")
										{
											temp = vanillaLabels[0];
										}

										line = "<hash40 hash=\"" + label + slot + "\">" + temp + "</hash40>";
									}
									else
									{
										line = "<hash40 hash=\"" + label + slot + "\">" + i->second + "</hash40>";
									}

									uiEdit << "\n\t\t\t" << line;
								}

								vanillaLabels.clear();
								action = false;
							}
						}
					}
				}
				else if (line.find("</struct>") != string::npos)
				{
					if (status == 0)
					{
						uiEdit << "\n\t\t<hash40 index=\"" << currIndex << "\">dummy</hash40>";

					}
					else if (status == 1)
					{
						uiEdit << "\n\t\t</struct>";
						status = 0;
					}
					else
					{
						status = 0;
					}

					// Skip everything else as demon is the last charcode.
					if (currIndex == "120")
					{
						break;
					}
				}
			}

			if (!names.empty())
			{
				outputUTF(msgUTF, "\n</xmsbt>");
				msgUTF.close();

				fs::create_directories(path + "/ui/message/");
				fs::rename(fs::current_path() / "msg_name.xmsbt", path + "/ui/message/msg_name.xmsbt");
			}


			uiEdit << "\n\t</list>";
			uiEdit << "\n</struct>";

			uiVanilla.close();
			uiEdit.close();

			fs::remove(fs::current_path() / "ui_chara_db.xml");
		}
		else
		{
			if (!uiVanilla.is_open())
			{
				log->LogText("> Error: " + fs::current_path().string() + "/ui_chara_db.xml could not be opened!");
			}

			if (!uiEdit.is_open())
			{
				log->LogText("> Error: " + fs::current_path().string() + "/ui_chara_db_EDIT.xml could not be opened!");
			}
		}
	}
}

void ModHandler::createInkPRCXML(map<int, InklingColor>& inklingColors)
{
	if (!inklingColors.empty())
	{
		ifstream effectVanilla("effect.prcxml");
		ofstream effectEdit("effect_EDIT.prcxml");

		if (effectVanilla.is_open() && effectEdit.is_open())
		{
			string line;

			while (!effectVanilla.eof())
			{
				getline(effectVanilla, line);

				if (line.find("hash=\"ink_") != string::npos)
				{
					char action;

					if (line.find("ink_effect_color") != string::npos)
					{
						action = 'E';
					}
					else
					{
						action = 'A';
					}

					effectEdit << line << endl;

					auto iter = inklingColors.begin();

					// Fill out 0-7
					for (int i = 0; i < 8; i++)
					{
						auto iter = inklingColors.find(i);

						if (iter != inklingColors.end())
						{
							effectEdit << "    <struct index=\"" << to_string(i) << "\">" << endl;

							if (action == 'E')
							{
								effectEdit << "      <float hash=\"r\">" << (iter->second.effect.Red() / 255.0) << "</float>" << endl;
								effectEdit << "      <float hash=\"g\">" << (iter->second.effect.Green() / 255.0) << "</float>" << endl;
								effectEdit << "      <float hash=\"b\">" << (iter->second.effect.Blue() / 255.0) << "</float>" << endl;
							}
							else
							{
								effectEdit << "      <float hash=\"r\">" << (iter->second.arrow.Red() / 255.0) << "</float>" << endl;
								effectEdit << "      <float hash=\"g\">" << (iter->second.arrow.Green() / 255.0) << "</float>" << endl;
								effectEdit << "      <float hash=\"b\">" << (iter->second.arrow.Blue() / 255.0) << "</float>" << endl;
							}

							effectEdit << "    </struct>" << endl;
						}
						else
						{
							effectEdit << "    <hash40 index=\"" << to_string(i) << "\">dummy</hash40>" << endl;
						}
					}
					
					// Move iter to earliest additional slot
					while (iter->first <= 7 && iter != inklingColors.end())
					{
						iter++;
					}

					while (iter != inklingColors.end())
					{
						effectEdit << "    <struct index=\"" << to_string(iter->first) << "\">" << endl;

						if (action == 'E')
						{
							effectEdit << "      <float hash=\"r\">" << (iter->second.effect.Red() / 255.0) << "</float>" << endl;
							effectEdit << "      <float hash=\"g\">" << (iter->second.effect.Green() / 255.0) << "</float>" << endl;
							effectEdit << "      <float hash=\"b\">" << (iter->second.effect.Blue() / 255.0) << "</float>" << endl;
						}
						else
						{
							effectEdit << "      <float hash=\"r\">" << (iter->second.arrow.Red() / 255.0) << "</float>" << endl;
							effectEdit << "      <float hash=\"g\">" << (iter->second.arrow.Green() / 255.0) << "</float>" << endl;
							effectEdit << "      <float hash=\"b\">" << (iter->second.arrow.Blue() / 255.0) << "</float>" << endl;
						}

						effectEdit << "    </struct>" << endl;
					}
				}
				else
				{
					effectEdit << line << endl;
				}
			}

			effectVanilla.close();
			effectEdit.close();
		}
		else
		{
			if (!effectVanilla.is_open())
			{
				log->LogText("> Error: " + fs::current_path().string() + "/effect.prcxml could not be opened!");
			}

			if (!effectEdit.is_open())
			{
				log->LogText("> Error: " + fs::current_path().string() + "/effect_EDIT.prcxml could not be opened!");
			}
		}
	}
}

void ModHandler::outputUTF(wofstream& file, wxString str, bool parse)
{
	if (parse)
	{
		wxString result = "";

		for (auto i = str.begin(); i != str.end(); i++)
		{
			if (*i == '"')
			{
				result += "&quot;";
			}
			else if (*i == '\'')
			{
				result += "&apos;";
			}
			else if (*i == '&')
			{
				result += "&amp;";
			}
			else if (*i == '<')
			{
				result += "&lt;";
			}
			else if (*i == '>')
			{
				result += "&gt;";
			}
			else if (*i == '|')
			{
				result += '\n';
			}
			else
			{
				result += *i;
			}
		}

		file << result.ToStdWstring();
	}
	else
	{
		file << str.ToStdWstring();
	}
}

map<int, InklingColor> ModHandler::readInk()
{
	map<int, InklingColor> inkColors;

	// Read XML
	if (fs::exists(path + "/fighter/common/param/effect.prcxml"))
	{
		ifstream inFile(path + "/fighter/common/param/effect.prcxml");
		char action = 'F';
		string line;

		// Go through effect.prcxml line by line
		while (getline(inFile, line))
		{
			// If Inkling related data is found, make a copy of the data.
			if (line.find("ink_effect_color") != string::npos)
			{
				action = 'E';
			}
			else if (line.find("ink_arrow_color") != string::npos)
			{
				action = 'A';
			}

			if (action != 'F')
			{
				while (line.find("</list>") == string::npos)
				{
					if (line.find("<struct") != string::npos)
					{
						auto start = line.find("\"");
						auto end = line.rfind("\"");

						int slot = stoi(line.substr(start + 1, end - start - 1));

						getline(inFile, line);
						double red = stod(line.substr(line.find(">") + 1, line.rfind("<") - line.find(">") - 1));

						getline(inFile, line);
						double green = stod(line.substr(line.find(">") + 1, line.rfind("<") - line.find(">") - 1));

						getline(inFile, line);
						double blue = stod(line.substr(line.find(">") + 1, line.rfind("<") - line.find(">") - 1));

						// Red
						if (action == 'E')
						{
							inkColors[slot].effect.Set(red * 255, green * 255, blue * 255);
						}
						else
						{
							inkColors[slot].arrow.Set(red * 255, green * 255, blue * 255);
						}
					}

					getline(inFile, line);
				}

				action = 'F';
			}
		}
	}

	return inkColors;
}

map<string, map<string, int>> ModHandler::readBaseSlots()
{
	map<string, map<string, int>> baseSlots;

	if (fs::exists(path + "/config.json"))
	{
		ifstream inFile(path + "/config.json");

		if (inFile.is_open())
		{
			string line;

			while (line.find("\"new-dir-infos-base\"") == string::npos && !inFile.eof())
			{
				getline(inFile, line);
			}

			// Found base-slot section
			if (line.find("\"new-dir-infos-base\"") != string::npos)
			{
				while (line.find('}') == string::npos && !inFile.eof())
				{
					getline(inFile, line);

					auto camPos = line.find("/camera");

					if (camPos != string::npos)
					{
						auto beg = line.find("fighter/") + 8;
						auto end = line.find("/", beg);
						string charcode = line.substr(beg, end - beg);

						string newSlot = line.substr(end + 2, camPos - end - 2);

						end = line.find("/camera", camPos + 7) - 1;
						beg = line.find(charcode + "/c", camPos + 7) + 2 + charcode.size();
						int baseSlot = stoi(line.substr(beg, end - beg + 1));

						baseSlots[charcode][newSlot] = baseSlot;
					}
				}
			}
		}
		else
		{
			log->LogText("> ERROR: " + path + "/config.json" + "could not be opened!");
		}
	}

	return baseSlots;
}

map<string, map<string, Name>> ModHandler::readNames()
{
	map<string, map<string, Name>> names;
	int count = 0;

	if (fs::exists(path + "/ui/message/msg_name.xmsbt"))
	{
		wifstream inFile(rootPath + "/ui/message/msg_name.xmsbt", ios::binary);
		inFile.imbue(std::locale(inFile.getloc(), new std::codecvt_utf16<wchar_t, 0x10ffff, std::consume_header>));

		if (inFile.is_open())
		{
			vector<string> lines;
			int count = -2;

			// Read Header
			for (wchar_t c; inFile.get(c) && count < 0; )
			{
				if ((char(c)) == '>')
				{
					count++;
				}
			}

			lines.push_back("");

			for (wchar_t c; inFile.get(c); )
			{
				lines[count] += (char)c;

				if ((char(c)) == '>')
				{
					lines.push_back("");
					count++;
				}
			}

			char type;
			bool action = false;
			size_t beg;
			size_t end;

			// Interpret Data
			for (int i = 0; i < lines.size(); i++)
			{
				// CSS/CSP/VS
				if (lines[i].find("nam_chr") != string::npos)
				{
					beg = lines[i].find("nam_chr") + 9;
					end = lines[i].find("_", beg);
					type = lines[i][beg - 2];

					action = true;
				}
				// Stage_Name
				else if (lines[i].find("nam_stage") != string::npos)
				{
					beg = lines[i].find("nam_stage") + 15;
					end = lines[i].find("_", beg + 1);
					type = 's';

					action = true;
				}

				if (action)
				{
					string slot = to_string(stoi(lines[i].substr(beg, end - beg)) - 8);
					string charcode = lines[i].substr(end + 1, lines[i].find("\"", end + 1) - end - 1);

					if (slot.size() == 1)
					{
						slot = "0" + slot;
					}
					string name = lines[i + 2].substr(0, lines[i + 2].find("<"));

					if (type == '1')
					{
						names[charcode][slot].cspName = name;
					}
					else if (type == '2')
					{
						names[charcode][slot].vsName = name;
					}
					else if (type == '3')
					{
						names[charcode][slot].cssName = name;
					}
					else if (type == 's')
					{
						names[charcode][slot].stageName = name;
					}
					else
					{
						log->LogText("> ERROR: Unable to correctly read names from msg_name.xmsbt!");
					}

					action = false;
					i += 2;
				}
			}

			inFile.close();
		}
		else
		{
			log->LogText("> ERROR: " + path + "/ui/message/msg_name.xmsbt" + "could not be opened!");
		}
	}

	return names;
}

void ModHandler::clear()
{
	for (auto i = mod.begin(); i != mod.end(); i++)
	{
		for (auto j = i->second.begin(); j != i->second.end(); j++)
		{
			for (auto k = j->second.begin(); k != j->second.end(); k++)
			{
				k->second.clear();
			}

			j->second.clear();
		}

		i->second.clear();
	}

	mod.clear();
}