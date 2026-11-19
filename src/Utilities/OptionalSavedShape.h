#pragma once

#include "TemplateDef.h"
#include "SavegameDef.h"

// Use only for visuals with an existing null-shape fallback. The on-disk
// representation is identical to the regular SHP pointer: presence and filename.
class OptionalSavedShape : public Valueable<SHPStruct*>
{
public:
	using Valueable<SHPStruct*>::Valueable;

	bool Load(PhobosStreamReader& stm, bool)
	{
		unsigned char present = 0;
		if (!stm.Load(present) || present > 1)
			return false;

		if (!present)
		{
			this->Value = nullptr;
			this->MissingName.clear();
			return true;
		}

		size_t length = 0;
		if (!stm.Load(length) || !length || length > stm.Remaining())
			return false;

		std::string name(length, '\0');
		if (!stm.Read(reinterpret_cast<unsigned char*>(name.data()), length)
			|| name.find('\0') != std::string::npos)
			return false;

		this->Value = FileSystem::LoadSHPFile(name.c_str());
		this->MissingName.clear();
		if (!this->Value)
		{
			// Keep the unresolved reference for a subsequent save, so the art
			// works again the next time it is available. Never log a saved path.
			this->MissingName = std::move(name);
			Debug::Log("save-load: optional display SHP unavailable; using visual fallback\n");
		}
		return true;
	}

	bool Save(PhobosStreamWriter& stm) const
	{
		if (this->Value || this->MissingName.empty())
			return Valueable<SHPStruct*>::Save(stm);

		return Savegame::WritePhobosStream(stm, true)
			&& Savegame::WritePhobosStream(stm, this->MissingName);
	}

private:
	std::string MissingName;
};
