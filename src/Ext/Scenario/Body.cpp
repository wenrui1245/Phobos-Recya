#include "Body.h"

#include <VeinholeMonsterClass.h>

#include <Ext/House/Body.h>
#include <Misc/Weather.h>
#include <Ext/HouseType/Body.h>
#include <Ext/Rules/Body.h>
#include <HouseClass.h>
#include <ThemeClass.h>
#include <Ext/SWType/Body.h>

std::unique_ptr<ScenarioExt::ExtData> ScenarioExt::Data = nullptr;

bool ScenarioExt::CellParsed = false;

void ScenarioExt::ExtData::SetVariableToByID(bool bIsGlobal, int nIndex, char bState)
{
	auto& dict = Global()->Variables[bIsGlobal];

	auto itr = dict.find(nIndex);

	if (itr != dict.end() && itr->second.Value != bState)
	{
		itr->second.Value = bState;
		ScenarioClass::Instance->VariablesChanged = true;
		if (!bIsGlobal)
			TagClass::NotifyLocalChanged(nIndex);
		else
			TagClass::NotifyGlobalChanged(nIndex);
	}
}

void ScenarioExt::ExtData::GetVariableStateByID(bool bIsGlobal, int nIndex, char* pOut)
{
	auto& dict = Global()->Variables[bIsGlobal];

	auto itr = dict.find(nIndex);
	if (itr != dict.end())
		*pOut = static_cast<char>(itr->second.Value);
}

void ScenarioExt::ExtData::ReadVariables(bool bIsGlobal, CCINIClass* pINI)
{
	if (!bIsGlobal) // Local variables need to be read again
		Global()->Variables[false].clear();
	else if (Global()->Variables[true].size() != 0) // Global variables had been loaded, DO NOT CHANGE THEM
		return;

	const int nCount = pINI->GetKeyCount("VariableNames");
	for (int i = 0; i < nCount; ++i)
	{
		const auto pKey = pINI->GetKeyName("VariableNames", i);
		int nIndex;
		if (sscanf_s(pKey, "%d", &nIndex) == 1)
		{
			auto& var = Global()->Variables[bIsGlobal][nIndex];
			pINI->ReadString("VariableNames", pKey, pKey, Phobos::readBuffer);
			char* buffer;
			strcpy_s(var.Name, strtok_s(Phobos::readBuffer, ",", &buffer));
			if (auto pState = strtok_s(nullptr, ",", &buffer))
				var.Value = atoi(pState);
			else
				var.Value = 0;
		}
	}
}

// you've inspired something controversial
void ScenarioExt::ExtData::SaveVariablesToFile(bool isGlobal)
{
	CCINIClass fINI {};
	CCFileClass file { isGlobal ? "globals.ini" : "locals.ini" };

	if (file.Exists())
		fINI.ReadCCFile(&file);
	else
		file.CreateFileA();

	for (const auto& [_,varext] : Global()->Variables[isGlobal])
		fINI.WriteInteger(ScenarioClass::Instance->FileName, varext.Name, varext.Value, false);

	fINI.WriteCCFile(&file);
	file.Close();
}

void ScenarioExt::Allocate(ScenarioClass* pThis)
{
	Data = std::make_unique<ScenarioExt::ExtData>(pThis);
}

void ScenarioExt::Remove(ScenarioClass* pThis)
{
	Data = nullptr;
}

void ScenarioExt::LoadFromINIFile(ScenarioClass* pThis, CCINIClass* pINI)
{
	Data->LoadFromINI(pINI);

	for (auto const pHouse : HouseClass::Array)
	{
		HouseExt::Fetch(pHouse)->FreeRadar = ScenarioClass::Instance->FreeRadar;
	}
}

void ScenarioExt::ExtData::UpdateAutoDeathObjectsInLimbo()
{
	for (auto const pExt : this->AutoDeathObjects)
	{
		auto const pTechno = pExt->OwnerObject();

		if (!pTechno->IsInLogic && pTechno->IsAlive && pTechno->Health > 0)
			pExt->CheckDeathConditions(true);
	}
}

void ScenarioExt::ExtData::UpdateTransportReloaders()
{
	for (auto const pExt : this->TransportReloaders)
	{
		auto const pTechno = pExt->OwnerObject();

		if (pTechno->IsAlive && pTechno->Health > 0 && pTechno->Transporter && pTechno->Transporter->IsInLogic)
			pTechno->Reload();
	}
}

void ScenarioExt::ExtData::RegisterAutoDeath(TechnoClass* pTechno)
{
	if (auto const pExt = TechnoExt::Fetch(pTechno))
	{
		if (pExt->TypeExtData->AutoDeath_Behavior.isset())
		{
			auto& vec = this->AutoDeathObjects;
			if (std::find(vec.begin(), vec.end(), pExt) == vec.end())
				vec.push_back(pExt);
		}
	}
}

// =============================
// load / save

void ScenarioExt::ExtData::LoadFromINIFile(CCINIClass* const pINI)
{
	auto pThis = this->OwnerObject();

	INI_EX maINI(pINI);
	INI_EX ruINI(CCINIClass::INI_Rules);

	if (SessionClass::IsCampaign())
	{
		Nullable<bool> SP_MCVRedeploy;
		SP_MCVRedeploy.Read(maINI, GameStrings::Basic, GameStrings::MCVRedeploys);
		if (!SP_MCVRedeploy.isset())
			SP_MCVRedeploy.Read(ruINI, GameStrings::Basic, GameStrings::MCVRedeploys);
		GameModeOptionsClass::Instance.MCVRedeploy = SP_MCVRedeploy.Get(false);

		CCINIClass ini_missionmd {};
		ini_missionmd.LoadFromFile(GameStrings::MISSIONMD_INI);
		auto const scenarioName = pThis->FileName;

		// Override rankings
		pThis->ParTimeEasy = ini_missionmd.ReadTime(scenarioName, "Ranking.ParTimeEasy", pThis->ParTimeEasy);
		pThis->ParTimeMedium = ini_missionmd.ReadTime(scenarioName, "Ranking.ParTimeMedium", pThis->ParTimeMedium);
		pThis->ParTimeDifficult = ini_missionmd.ReadTime(scenarioName, "Ranking.ParTimeHard", pThis->ParTimeDifficult);
		ini_missionmd.ReadString(scenarioName, "Ranking.UnderParTitle", pThis->UnderParTitle, pThis->UnderParTitle);
		ini_missionmd.ReadString(scenarioName, "Ranking.UnderParMessage", pThis->UnderParMessage, pThis->UnderParMessage);
		ini_missionmd.ReadString(scenarioName, "Ranking.OverParTitle", pThis->OverParTitle, pThis->OverParTitle);
		ini_missionmd.ReadString(scenarioName, "Ranking.OverParMessage", pThis->OverParMessage, pThis->OverParMessage);

		this->ShowBriefing = pINI->ReadBool(GameStrings::Basic, "ShowBriefing", this->ShowBriefing);
		this->BriefingTheme = pINI->ReadTheme(GameStrings::Basic, "BriefingTheme", this->BriefingTheme);
	}
}

template <typename T>
void ScenarioExt::ExtData::Serialize(T& Stm)
{
	Stm
		.Process(this->Waypoints)
		.Process(this->Variables[0])
		.Process(this->Variables[1])
		.Process(this->ShowBriefing)
		.Process(this->BriefingTheme)
		.Process(this->AutoDeathObjects)
		.Process(this->TransportReloaders)
		.Process(this->SWSidebar_Enable)
		.Process(this->SWSidebar_Indices)
		.Process(this->CanBuildNowCount)
		.Process(this->OwnerBitfield_BuildingType)
		.Process(this->OwnerBitfield_InfantryType)
		.Process(this->OwnerBitfield_VehicleType)
		.Process(this->OwnerBitfield_NavyType)
		.Process(this->OwnerBitfield_AircraftType)
		.Process(this->BaseNormalTechnos)
		.Process(this->OwnedUniqueTechnos)
		.Process(this->Smudges)
		.Process(this->RecordMessages)
		.Process(this->DefaultLS640BkgdName)
		.Process(this->DefaultLS800BkgdName)
		.Process(this->DefaultLS800BkgdPal)
		.Process(this->LimboLaunchers)
		.Process(this->TriggerTypePlayerAtXOwners)
		.Process(this->UndergroundTracker)
		.Process(this->SpecialTracker)
		.Process(this->FallingDownTracker)
		.Process(this->EVAIndex)
		.Process(this->BattleAdvantage_PlayerAdvantage)
		.Process(this->BattleAdvantage_PlayerDisadvantage)
		.Process(this->BattleAdvantage_PlayerOverwhelming)
		.Process(this->BattleAdvantage_PlayerStatus)
		.Process(this->FiringAnimUpdateCount)
		.Process(this->MissionTimer_Type)
		.Process(this->MissionTimer_Variable)
		.Process(this->MissionTimer_Reverse)
		;
}

void ScenarioExt::ExtData::LoadFromStream(PhobosStreamReader& Stm)
{
	Extension<ScenarioClass>::LoadFromStream(Stm);
	this->Serialize(Stm);
}

void ScenarioExt::ExtData::SaveToStream(PhobosStreamWriter& Stm)
{
	Global()->EVAIndex = VoxClass::EVAIndex;

	Extension<ScenarioClass>::SaveToStream(Stm);
	this->Serialize(Stm);
}

// =============================
// container hooks

DEFINE_HOOK(0x683549, ScenarioClass_CTOR, 0x9)
{
	GET(ScenarioClass*, pItem, EAX);

	ScenarioExt::Allocate(pItem);

	ScenarioExt::Global()->Waypoints.clear();
	ScenarioExt::Global()->Variables[0].clear();
	ScenarioExt::Global()->Variables[1].clear();
	ScenarioExt::Global()->BattleAdvantage_PlayerAdvantage = 0.0f;
	ScenarioExt::Global()->BattleAdvantage_PlayerDisadvantage = 0.0f;
	ScenarioExt::Global()->BattleAdvantage_PlayerOverwhelming = 0.0f;
	ScenarioExt::Global()->BattleAdvantage_PlayerStatus = PlayerAdvantageStatus::Normal;
	ScenarioExt::Global()->TriggerTypePlayerAtXOwners.clear();

	return 0;
}

DEFINE_HOOK(0x6BEB7D, ScenarioClass_DTOR, 0x6)
{
	GET(ScenarioClass*, pItem, ESI);

	ScenarioExt::Remove(pItem);
	return 0;
}

IStream* ScenarioExt::g_pStm = nullptr;

DEFINE_HOOK_AGAIN(0x689470, ScenarioClass_SaveLoad_Prefix, 0x5)
DEFINE_HOOK(0x689310, ScenarioClass_SaveLoad_Prefix, 0x5)
{
	GET_STACK(IStream*, pStm, 0x4);

	ScenarioExt::g_pStm = pStm;

	return 0;
}

DEFINE_HOOK(0x689669, ScenarioClass_Load_Suffix, 0x6)
{
	auto buffer = ScenarioExt::Global();

	PhobosByteStream Stm(0);
	if (Stm.ReadBlockFromStream(ScenarioExt::g_pStm))
	{
		PhobosStreamReader Reader(Stm);

		if (Reader.Expect(ScenarioExt::Canary) && Reader.RegisterChange(buffer))
			buffer->LoadFromStream(Reader);
	}

	return 0;
}

DEFINE_HOOK(0x68945B, ScenarioClass_Save_Suffix, 0x8)
{
	auto buffer = ScenarioExt::Global();
	PhobosByteStream saver(sizeof(*buffer));
	PhobosStreamWriter writer(saver);

	writer.Expect(ScenarioExt::Canary);
	writer.RegisterChange(buffer);

	buffer->SaveToStream(writer);
	saver.WriteBlockToStream(ScenarioExt::g_pStm);

	return 0;
}

DEFINE_HOOK(0x68AD2F, ScenarioClass_LoadFromINI, 0x5)
{
	GET(ScenarioClass*, pItem, ESI);
	GET(CCINIClass*, pINI, EDI);

	ScenarioExt::LoadFromINIFile(pItem, pINI);

	// 移植自 MO_ExperienceEnhancements: 地图自带 [Phobos.Weather] 节覆盖全局天气配置
	Weather_ApplyScenConfig(pINI);
	return 0;
}

DEFINE_HOOK(0x55B4E1, LogicClass_Update_BeforeAll, 0x5)
{
	VeinholeMonsterClass::UpdateAllVeinholes();

	ScenarioExt::Global()->UpdateAutoDeathObjectsInLimbo();
	ScenarioExt::Global()->UpdateTransportReloaders();

	// SW music timers: stop music when timer completes
	for (auto const pHouse : HouseClass::Array)
	{
		if (!pHouse) { continue; }
		auto& houseExt = *HouseExt::ExtMap.Find(pHouse);
		for (size_t i = 0; i < houseExt.SuperExts.size(); ++i)
		{
			auto& swExt = houseExt.SuperExts[i];
			if (swExt.MusicActive && swExt.MusicTimer.Completed())
			{
				int configuredTheme = -1;
				SuperClass* pSuper = nullptr;
				SWTypeExt::ExtData* pTypeExt = nullptr;
				if (pHouse->Supers.Count > static_cast<int>(i))
				{
					pSuper = pHouse->Supers[static_cast<int>(i)];
					if (pSuper && pSuper->Type)
					{
						pTypeExt = SWTypeExt::ExtMap.Find(pSuper->Type);
						configuredTheme = pTypeExt->Music_Theme.Get();
					}
				}
				if (configuredTheme >= 0 && ThemeClass::Instance.CurrentTheme == configuredTheme)
				{
					// stop only if same theme and local house is affected
					AffectedHouse affected = AffectedHouse::All;
					if (pTypeExt)
					{
						affected = pTypeExt->Music_AffectedHouses.Get();
					}
					if (EnumFunctions::CanTargetHouse(affected, pHouse, HouseClass::CurrentPlayer))
					{
						ThemeClass::Instance.Stop(true);
					}
				}
				swExt.MusicTimer.Stop();
				swExt.MusicActive = false;
			}
		}
	}

	// Battle Advantage decay
	auto const pRulesExt = RulesExt::Global();

	if (pRulesExt->BattleAdvantage_Enabled)
	{
		auto const pScenarioExt = ScenarioExt::Global();
		auto const advantagedecayRate = pRulesExt->BattleAdvantage_AdvantageDecayRate;
		auto const disadvantageDecayRate = pRulesExt->BattleAdvantage_DisadvantageDecayRate;
		auto const overwhelmingDecayRate = pRulesExt->BattleAdvantage_OverwhelmingDecayRate;

		auto const intensityUpperThreshold = pRulesExt->BattleAdvantage_IntensityUpperThreshold;
		auto const intensityLowerThreshold = pRulesExt->BattleAdvantage_IntensityLowerThreshold;
		auto const casualtyRatioUpperThreshold = pRulesExt->BattleAdvantage_CasualtyRatioUpperThreshold;
		auto const casualtyRatioLowerThreshold = pRulesExt->BattleAdvantage_CasualtyRatioLowerThreshold;

		auto const overwhelmingUpperThreshold = pRulesExt->BattleAdvantage_OverwhelmingUpperThreshold;
		auto const overwhelmingLowerThreshold = pRulesExt->BattleAdvantage_OverwhelmingLowerThreshold;

		if (pScenarioExt->BattleAdvantage_PlayerAdvantage > 0.0f)
			pScenarioExt->BattleAdvantage_PlayerAdvantage = std::max(0.0f, pScenarioExt->BattleAdvantage_PlayerAdvantage - advantagedecayRate);

		if (pScenarioExt->BattleAdvantage_PlayerDisadvantage > 0.0f)
			pScenarioExt->BattleAdvantage_PlayerDisadvantage = std::max(0.0f, pScenarioExt->BattleAdvantage_PlayerDisadvantage - disadvantageDecayRate);

		if (pScenarioExt->BattleAdvantage_PlayerOverwhelming > 0.0f)
			pScenarioExt->BattleAdvantage_PlayerOverwhelming = std::max(0.0f, pScenarioExt->BattleAdvantage_PlayerOverwhelming - overwhelmingDecayRate);

		//判断当前战场形势
		auto BattleIntensity = pScenarioExt->BattleAdvantage_PlayerAdvantage + pScenarioExt->BattleAdvantage_PlayerDisadvantage;
		auto BattleAdvantage = pScenarioExt->BattleAdvantage_PlayerAdvantage / pScenarioExt->BattleAdvantage_PlayerDisadvantage;
		auto BattleOverwhelming = pScenarioExt->BattleAdvantage_PlayerOverwhelming;

		if (BattleOverwhelming > overwhelmingUpperThreshold && pScenarioExt->BattleAdvantage_PlayerStatus != PlayerAdvantageStatus::Triumphal)
		{
			//Debug::Log("[BattleAdvantage] Overwhelming: %.1f. Player is overwhelming.\n", BattleOverwhelming);
			pScenarioExt->BattleAdvantage_PlayerStatus = PlayerAdvantageStatus::Triumphal;

			const int themeIndex = HouseTypeExt::ExtMap.Find(HouseClass::CurrentPlayer->Type)->Music_Trumpet;
			if (themeIndex >= 0)
				ThemeClass::Instance.Play(themeIndex);
		}
		else if (BattleOverwhelming < overwhelmingLowerThreshold && pScenarioExt->BattleAdvantage_PlayerStatus == PlayerAdvantageStatus::Triumphal)
		{
			//Debug::Log("[BattleAdvantage] Overwhelming: %.1f. Player is no longer overwhelming.\n", BattleOverwhelming);
			pScenarioExt->BattleAdvantage_PlayerStatus = PlayerAdvantageStatus::Normal;

			pScenarioExt->BattleAdvantage_PlayerAdvantage = 0.0f;
			pScenarioExt->BattleAdvantage_PlayerDisadvantage = 0.0f;

			const int themeIndex = HouseTypeExt::ExtMap.Find(HouseClass::CurrentPlayer->Type)->Music_Trumpet;
			if (themeIndex >= 0)
				ThemeClass::Instance.Stop(true);
		}
		else if (BattleIntensity > intensityUpperThreshold && pScenarioExt->BattleAdvantage_PlayerStatus == PlayerAdvantageStatus::Normal)
		{
			//Debug::Log("[BattleAdvantage] BattleIntensity: %.1f, BattleAdvantage: %.1f. Player is at combat.\n", BattleIntensity, BattleAdvantage);
			pScenarioExt->BattleAdvantage_PlayerStatus = PlayerAdvantageStatus::Combat;

			const int themeIndex = HouseTypeExt::ExtMap.Find(HouseClass::CurrentPlayer->Type)->Music_Combat;
			if (themeIndex >= 0)
				ThemeClass::Instance.Play(themeIndex);
		}
		else if (BattleIntensity < intensityLowerThreshold && pScenarioExt->BattleAdvantage_PlayerStatus == PlayerAdvantageStatus::Combat)
		{
			//Debug::Log("[BattleAdvantage] BattleIntensity: %.1f, BattleAdvantage: %.1f. Player is no longer in combat.\n", BattleIntensity, BattleAdvantage);
			pScenarioExt->BattleAdvantage_PlayerStatus = PlayerAdvantageStatus::Normal;

			const int themeIndex = HouseTypeExt::ExtMap.Find(HouseClass::CurrentPlayer->Type)->Music_Combat;
			if (themeIndex >= 0)
				ThemeClass::Instance.Stop(true);
		}
		else if (BattleIntensity > intensityUpperThreshold && BattleAdvantage < casualtyRatioLowerThreshold && pScenarioExt->BattleAdvantage_PlayerStatus != PlayerAdvantageStatus::Losing && pScenarioExt->BattleAdvantage_PlayerStatus != PlayerAdvantageStatus::Triumphal)
		{
			//Debug::Log("[BattleAdvantage] BattleIntensity: %.1f, BattleAdvantage: %.1f. Player is at disadvantage.\n", BattleIntensity, BattleAdvantage);
			pScenarioExt->BattleAdvantage_PlayerStatus = PlayerAdvantageStatus::Losing;

			const int themeIndex = HouseTypeExt::ExtMap.Find(HouseClass::CurrentPlayer->Type)->Music_Losing;
			if (themeIndex >= 0)
				ThemeClass::Instance.Play(themeIndex);
		}
		else if (BattleIntensity > intensityLowerThreshold && BattleAdvantage > casualtyRatioUpperThreshold && pScenarioExt->BattleAdvantage_PlayerStatus == PlayerAdvantageStatus::Losing)
		{
			//Debug::Log("[BattleAdvantage] BattleIntensity: %.1f, BattleAdvantage: %.1f. Player is no longer at disadvantage.\n", BattleIntensity, BattleAdvantage);
			pScenarioExt->BattleAdvantage_PlayerStatus = PlayerAdvantageStatus::Combat;

			const int themeIndex = HouseTypeExt::ExtMap.Find(HouseClass::CurrentPlayer->Type)->Music_Combat;
			if (themeIndex >= 0)
				ThemeClass::Instance.Play(themeIndex);
		}
		else if (BattleIntensity < intensityLowerThreshold && pScenarioExt->BattleAdvantage_PlayerStatus == PlayerAdvantageStatus::Losing)
		{
			//Debug::Log("[BattleAdvantage] BattleIntensity: %.1f, BattleAdvantage: %.1f. Player is no longer at disadvantage (Combat End).\n", BattleIntensity, BattleAdvantage);
			pScenarioExt->BattleAdvantage_PlayerStatus = PlayerAdvantageStatus::Normal;

			const int themeIndex = HouseTypeExt::ExtMap.Find(HouseClass::CurrentPlayer->Type)->Music_Losing;
			if (themeIndex >= 0)
				ThemeClass::Instance.Stop(true);
		}
	}

	return 0;
}
