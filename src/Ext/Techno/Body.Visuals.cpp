#include "Body.h"

#include <DriveLocomotionClass.h>
#include <ShipLocomotionClass.h>
#include <Interop/Underwater.h>
#include <JumpjetLocomotionClass.h>

#include <Ext/Anim/Body.h>
#include <Ext/SWType/Body.h>
#include <Ext/House/Body.h>

void TechnoExt::DrawSelfHealPips(TechnoClass* pThis, Point2D* pLocation, RectangleStruct* pBounds)
{
	if (!RulesExt::Global()->GainSelfHealAllowMultiplayPassive && pThis->Owner->Type->MultiplayPassive)
		return;

	auto const pTypeExt = TechnoExt::Fetch(pThis)->TypeExtData;

	if (pTypeExt->SelfHealGainType.isset() && pTypeExt->SelfHealGainType.Get() == SelfHealGainType::NoHeal)
		return;

	bool drawPip = false;
	bool isInfantryHeal = false;
	int selfHealFrames = 0;
	const bool hasInfantrySelfHeal = pTypeExt->SelfHealGainType.isset() && pTypeExt->SelfHealGainType.Get() == SelfHealGainType::Infantry;
	const bool hasUnitSelfHeal = pTypeExt->SelfHealGainType.isset() && pTypeExt->SelfHealGainType.Get() == SelfHealGainType::Units;
	auto const whatAmI = pThis->WhatAmI();
	auto const pType = pTypeExt->OwnerObject();
	const bool isOrganic = (whatAmI == AbstractType::Infantry || (pType->Organic && whatAmI == AbstractType::Unit));

	auto hasSelfHeal = [pThis](const bool infantryHeal)
		{
			auto const pOwner = pThis->Owner;

			auto haveHeal = [infantryHeal](HouseClass* pHouse)
				{
					return (infantryHeal ? pHouse->InfantrySelfHeal > 0 : pHouse->UnitsSelfHeal > 0);
				};

			if (haveHeal(pOwner))
				return true;

			const bool isCampaign = SessionClass::IsCampaign();
			const bool fromPlayer = RulesExt::Global()->GainSelfHealFromPlayerControl && isCampaign && (pOwner->IsHumanPlayer || pOwner->IsInPlayerControl);
			const bool fromAllies = RulesExt::Global()->GainSelfHealFromAllies;

			if (fromPlayer || fromAllies)
			{
				auto checkHouse = [fromPlayer, fromAllies, isCampaign, pOwner](HouseClass* pHouse)
					{
						if (pHouse == pOwner)
							return false;

						return (fromPlayer && (pHouse->IsHumanPlayer || pHouse->IsInPlayerControl)) // pHouse->IsControlledByCurrentPlayer()
							|| (fromAllies && (!isCampaign || (!pHouse->IsHumanPlayer && !pHouse->IsInPlayerControl)) && pHouse->IsAlliedWith(pOwner));
					};

				for (auto const pHouse : HouseClass::Array)
				{
					if (checkHouse(pHouse) && haveHeal(pHouse))
						return true;
				}
			}

			return false;
		};

	if ((hasInfantrySelfHeal || (isOrganic && !hasUnitSelfHeal)) && hasSelfHeal(true))
	{
		drawPip = true;
		selfHealFrames = RulesClass::Instance->SelfHealInfantryFrames;
		isInfantryHeal = true;
	}
	else if ((hasUnitSelfHeal || (whatAmI == AbstractType::Unit && !isOrganic)) && hasSelfHeal(false))
	{
		drawPip = true;
		selfHealFrames = RulesClass::Instance->SelfHealUnitFrames;
	}

	if (drawPip)
	{
		Valueable<Point2D> pipFrames;
		bool isSelfHealFrame = false;
		int xOffset = 0;
		int yOffset = 0;

		if (Unsorted::CurrentFrame % selfHealFrames <= 5
			&& pThis->Health < pType->Strength)
		{
			isSelfHealFrame = true;
		}

		if (whatAmI == AbstractType::Unit || whatAmI == AbstractType::Aircraft)
		{
			auto& offset = RulesExt::Global()->Pips_SelfHeal_Units_Offset.Get();
			pipFrames = RulesExt::Global()->Pips_SelfHeal_Units;
			xOffset = offset.X;
			yOffset = offset.Y + pType->PixelSelectionBracketDelta;
		}
		else if (whatAmI == AbstractType::Infantry)
		{
			auto& offset = RulesExt::Global()->Pips_SelfHeal_Infantry_Offset.Get();
			pipFrames = RulesExt::Global()->Pips_SelfHeal_Infantry;
			xOffset = offset.X;
			yOffset = offset.Y + pType->PixelSelectionBracketDelta;
		}
		else
		{
			const auto pBldType = static_cast<BuildingClass*>(pThis)->Type;
			const int fHeight = pBldType->GetFoundationHeight(false);
			const int yAdjust = -Unsorted::CellHeightInPixels / 2;

			auto& offset = RulesExt::Global()->Pips_SelfHeal_Buildings_Offset.Get();
			pipFrames = RulesExt::Global()->Pips_SelfHeal_Buildings;
			xOffset = offset.X + Unsorted::CellWidthInPixels / 2 * fHeight;
			yOffset = offset.Y + yAdjust * fHeight + pBldType->Height * yAdjust;
		}

		const int pipFrame = isInfantryHeal ? pipFrames.Get().X : pipFrames.Get().Y;

		Point2D position = { pLocation->X + xOffset, pLocation->Y + yOffset };

		auto flags = BlitterFlags::bf_400 | BlitterFlags::Centered;

		if (isSelfHealFrame)
			flags = flags | BlitterFlags::Darken;

		DSurface::Temp->DrawSHP(FileSystem::PALETTE_PAL, FileSystem::PIPS_SHP,
		pipFrame, &position, pBounds, flags, 0, 0, ZGradient::Ground, 1000, 0, 0, 0, 0, 0);
	}
}

void TechnoExt::DrawInsignia(TechnoClass* pThis, Point2D* pLocation, RectangleStruct* pBounds)
{
	auto pTechnoTypeExt = TechnoExt::Fetch(pThis)->TypeExtData;
	auto pTechnoType = pTechnoTypeExt->OwnerObject();
	auto pOwner = pThis->Owner;
	const bool isObserver = HouseClass::IsCurrentPlayerObserver();

	if (pThis->IsDisguised() && !pThis->IsClearlyVisibleTo(HouseClass::CurrentPlayer) && !(isObserver
		|| EnumFunctions::CanTargetHouse(RulesExt::Global()->DisguiseBlinkingVisibility, HouseClass::CurrentPlayer, pOwner)))
	{
		if (const auto pType = TechnoTypeExt::GetTechnoType(pThis->Disguise))
		{
			pTechnoType = pType;
			pTechnoTypeExt = TechnoTypeExt::Fetch(pType);
			pOwner = pThis->DisguisedAsHouse;
		}
		else if (pThis->Disguise->WhatAmI() == AbstractType::TerrainType && (!isObserver && !pOwner->IsAlliedWith(HouseClass::CurrentPlayer)))
		{
			return;
		}
	}

	const bool isVisibleToPlayer = (pOwner && pOwner->IsAlliedWith(HouseClass::CurrentPlayer))
		|| isObserver || pTechnoTypeExt->Insignia_ShowEnemy.Get(RulesExt::Global()->EnemyInsignia);

	if (!isVisibleToPlayer)
		return;

	const auto& insigniaData = pTechnoTypeExt->InsigniaData;
	Point2D offset = *pLocation;
	ConvertClass* pPalette = FileSystem::PALETTE_PAL;
	SHPStruct* pShapeFile = FileSystem::PIPS_SHP;
	int defaultFrameIndex = -1;
	bool isCustomInsignia = false;

	if (const auto pCustomShapeFile = insigniaData.GetShape(pThis))
	{
		pShapeFile = pCustomShapeFile;
		defaultFrameIndex = 0;
		isCustomInsignia = true;
	}

	const auto rank = pThis->Veterancy.GetRemainingLevel();

	if (const auto pPal = insigniaData.GetPalette(rank))
		pPalette = pPal;

	Vector3D<int> insigniaFrames = insigniaData.GetFrames();
	int frameIndex = insigniaData.GetFrame(pThis);

	if (pTechnoType->Passengers > 0 && pTechnoTypeExt->Insignia_PassengersData.size() > 0)
	{
		const int passengersIndex = Math::min(pTechnoTypeExt->Passengers_BySize
			? pThis->Passengers.GetTotalSize()
			: pThis->Passengers.NumPassengers, pTechnoType->Passengers);
		const auto& passengersInsigniaData = pTechnoTypeExt->Insignia_PassengersData[passengersIndex];

		if (const auto pCustomShapeFile = passengersInsigniaData.GetShape(pThis))
		{
			pShapeFile = pCustomShapeFile;
			defaultFrameIndex = 0;
			isCustomInsignia = true;
		}

		if (const auto pPal = passengersInsigniaData.GetPalette(rank))
			pPalette = pPal;

		const int frame = passengersInsigniaData.GetFrame(pThis);

		if (frame != -1)
			frameIndex = frame;

		const auto& frames = passengersInsigniaData.GetFrames();

		if (frames != Vector3D<int>(-1, -1, -1))
			insigniaFrames = frames;
	}

	if (pTechnoType->Gunner && pTechnoTypeExt->Insignia_WeaponData.size() > 0)
	{
		const int weaponIndex = pThis->CurrentWeaponNumber;
		const auto& weaponInsigniaData = pTechnoTypeExt->Insignia_WeaponData[weaponIndex];

		if (const auto pCustomShapeFile = weaponInsigniaData.GetShape(pThis))
		{
			pShapeFile = pCustomShapeFile;
			defaultFrameIndex = 0;
			isCustomInsignia = true;
		}

		if (const auto pPal = weaponInsigniaData.GetPalette(rank))
			pPalette = pPal;

		const int frame = weaponInsigniaData.GetFrame(pThis);

		if (frame != -1)
			frameIndex = frame;

		const auto& frames = weaponInsigniaData.GetFrames();

		if (frames != Vector3D<int>(-1, -1, -1))
			insigniaFrames = frames;
	}

	int insigniaFrame = insigniaFrames.X;

	switch (rank)
	{
	case Rank::Elite:
		defaultFrameIndex = !isCustomInsignia ? 15 : defaultFrameIndex;
		insigniaFrame = insigniaFrames.Z;
		break;

	case Rank::Veteran:
		defaultFrameIndex = !isCustomInsignia ? 14 : defaultFrameIndex;
		insigniaFrame = insigniaFrames.Y;
		break;

	default:
		break;
	}

	frameIndex = frameIndex == -1 ? insigniaFrame : frameIndex;

	if (frameIndex == -1)
		frameIndex = defaultFrameIndex;

	if (frameIndex != -1 && pShapeFile)
	{
		switch (pThis->WhatAmI())
		{
		case AbstractType::Infantry:
			offset += RulesExt::Global()->DrawInsignia_AdjustPos_Infantry;
			break;
		case AbstractType::Building:
			if (RulesExt::Global()->DrawInsignia_AdjustPos_BuildingsAnchor.isset())
				offset = GetBuildingSelectBracketPosition(pThis, pTechnoType, RulesExt::Global()->DrawInsignia_AdjustPos_BuildingsAnchor) + RulesExt::Global()->DrawInsignia_AdjustPos_Buildings;
			else
				offset += RulesExt::Global()->DrawInsignia_AdjustPos_Buildings;
			break;
		default:
			offset += RulesExt::Global()->DrawInsignia_AdjustPos_Units;
			break;
		}

		offset.Y += RulesExt::Global()->DrawInsignia_UsePixelSelectionBracketDelta ? pTechnoType->PixelSelectionBracketDelta : 0;
		DSurface::Temp->DrawSHP(pPalette, pShapeFile, frameIndex, &offset, pBounds, BlitterFlags(0xE00), 0, -2, ZGradient::Ground, 1000, 0, 0, 0, 0, 0);
	}
}

void TechnoExt::DrawFactoryProgress(BuildingClass* pThis, RectangleStruct* pBounds, Point2D basePosition)
{
	const auto pRulesExt = RulesExt::Global();

	if (!pRulesExt->FactoryProgressDisplay)
		return;

	const auto pType = pThis->Type;
	const auto pHouse = pThis->Owner;
	FactoryClass* pPrimaryFactory = nullptr;
	FactoryClass* pSecondaryFactory = nullptr;

	if (pHouse->IsControlledByHuman())
	{
		if (!pThis->IsPrimaryFactory)
			return;

		switch (pType->Factory)
		{
		case AbstractType::BuildingType:
			pPrimaryFactory = pHouse->Primary_ForBuildings;
			pSecondaryFactory = pHouse->Primary_ForDefenses;
			break;
		case AbstractType::InfantryType:
			pPrimaryFactory = pHouse->Primary_ForInfantry;
			break;
		case AbstractType::UnitType:
			pPrimaryFactory = pType->Naval ? pHouse->Primary_ForShips : pHouse->Primary_ForVehicles;
			break;
		case AbstractType::AircraftType:
			pPrimaryFactory = pHouse->Primary_ForAircraft;
			break;
		default:
			return;
		}
	}
	else // AIs have no Primary factories
	{
		pPrimaryFactory = pThis->Factory;

		if (!pPrimaryFactory)
			return;
	}

	const bool havePrimary = pPrimaryFactory && pPrimaryFactory->Object;
	const bool haveSecondary = pSecondaryFactory && pSecondaryFactory->Object;

	if (!havePrimary && !haveSecondary)
		return;

	const auto maxLength = pType->GetFoundationHeight(false) * 15 >> 1;
	const auto location = basePosition + Point2D { 6, (3 + pType->PixelSelectionBracketDelta) } + pRulesExt->FactoryProgressDisplay_Offset.Get();

	if (havePrimary)
	{
		auto position = location;

		DrawFrameStruct pDraw
		{
			static_cast<int>((static_cast<double>(pPrimaryFactory->GetProgress()) / 54) * maxLength),
			pRulesExt->FactoryProgressDisplay_Pips,
			pRulesExt->ProgressDisplay_Buildings_PipsShape.Get(),
			0,
			-1,
			nullptr,
			maxLength,
			0,
			&position,
			pBounds
		};

		TechnoExt::DrawVanillaStyleBuildingBar(&pDraw);
	}

	if (haveSecondary)
	{
		auto position = havePrimary ? location + Point2D { 6, 3 } : location;

		DrawFrameStruct pDraw
		{
			static_cast<int>((static_cast<double>(pSecondaryFactory->GetProgress()) / 54) * maxLength),
			pRulesExt->FactoryProgressDisplay_Pips,
			pRulesExt->ProgressDisplay_Buildings_PipsShape.Get(),
			0,
			-1,
			nullptr,
			maxLength,
			0,
			&position,
			pBounds
		};

		TechnoExt::DrawVanillaStyleBuildingBar(&pDraw);
	}
}

void TechnoExt::DrawSuperProgress(BuildingClass* pThis, RectangleStruct* pBounds, Point2D basePosition)
{
	const auto pRulesExt = RulesExt::Global();

	if (!pRulesExt->MainSWProgressDisplay)
		return;

	const auto pType = pThis->Type;
	const auto pOwner = pThis->Owner;
	const auto superIndex = pType->SuperWeapon;
	const auto pSuper = (superIndex != -1) ? pOwner->Supers.GetItem(superIndex) : nullptr;

	if (!pSuper || !SWTypeExt::Fetch(pSuper->Type)->IsAvailable(pOwner))
		return;

	const auto maxLength = pType->GetFoundationHeight(false) * 15 >> 1;
	auto position = basePosition + Point2D { 6, (3 + pType->PixelSelectionBracketDelta) } + pRulesExt->MainSWProgressDisplay_Offset.Get();

	DrawFrameStruct pDraw
	{
		static_cast<int>((static_cast<double>(pSuper->AnimStage()) / 54) * maxLength),
		pRulesExt->MainSWProgressDisplay_Pips,
		pRulesExt->ProgressDisplay_Buildings_PipsShape.Get(),
		0,
		-1,
		nullptr,
		maxLength,
		0,
		&position,
		pBounds
	};

	TechnoExt::DrawVanillaStyleBuildingBar(&pDraw);
}

void TechnoExt::DrawIronCurtainProgress(TechnoClass* pThis, RectangleStruct* pBounds, Point2D basePosition, bool isBuilding, bool isInfantry)
{
	if (!pThis->IsIronCurtained())
		return;

	const auto pRulesExt = RulesExt::Global();

	if (!pRulesExt->InvulnerableDisplay)
		return;

	const auto timer = &pThis->IronCurtainTimer;

	if (isBuilding)
	{
		const auto pBuilding = static_cast<BuildingClass*>(pThis);
		const auto pType = pBuilding->Type;
		const auto maxLength = pType->GetFoundationHeight(false) * 15 >> 1;
		const auto offset = pRulesExt->InvulnerableDisplay_Buildings_Offset.Get();
		auto position = basePosition + Point2D { offset.X, pType->PixelSelectionBracketDelta + offset.Y };

		DrawFrameStruct pDraw
		{
			static_cast<int>((static_cast<double>(timer->GetTimeLeft()) / timer->TimeLeft) * maxLength + 0.99),
			(pThis->ForceShielded ? pRulesExt->InvulnerableDisplay_Buildings_Pips.Get().X : pRulesExt->InvulnerableDisplay_Buildings_Pips.Get().Y),
			pRulesExt->ProgressDisplay_Buildings_PipsShape.Get(),
			0,
			-1,
			nullptr,
			maxLength,
			-1,
			&position,
			pBounds
		};

		if (offset == Point2D::Empty && (pThis->IsSelected || pThis->IsMouseHovering)) // Layer fix
		{
			RulesClass* const pRules = RulesClass::Instance;
			const auto ratio = pBuilding->GetHealthPercentage();
			pDraw.MidLength = static_cast<int>(ratio * maxLength);
			pDraw.MidFrame = (ratio > pRules->ConditionYellow) ? 1 : (ratio > pRules->ConditionRed ? 2 : 4);
			pDraw.MidPipSHP = FileSystem::PIPS_SHP;
			pDraw.BrdFrame = 0;
		}

		TechnoExt::DrawVanillaStyleBuildingBar(&pDraw);
	}
	else
	{
		const int maxLength = isInfantry ? 8 : 17;
		auto position = basePosition + Point2D { 0, pThis->GetTechnoType()->PixelSelectionBracketDelta + 2 } + pRulesExt->InvulnerableDisplay_Others_Offset.Get();

		DrawFrameStruct pDraw
		{
			static_cast<int>((static_cast<double>(timer->GetTimeLeft()) / timer->TimeLeft) * maxLength + 0.99),
			(pThis->ForceShielded ? pRulesExt->InvulnerableDisplay_Others_Pips.Get().X : pRulesExt->InvulnerableDisplay_Others_Pips.Get().Y),
			pRulesExt->ProgressDisplay_Others_PipsShape.Get(),
			0,
			-1,
			nullptr,
			maxLength,
			-1,
			&position,
			pBounds
		};

		TechnoExt::DrawVanillaStyleFootBar(&pDraw);
	}
}

void TechnoExt::DrawTemporalProgress(TechnoClass* pThis, RectangleStruct* pBounds, Point2D basePosition, bool isBuilding, bool isInfantry)
{
	const auto pTemporal = pThis->TemporalTargetingMe;

	if (!pTemporal)
		return;

	const auto pRulesExt = RulesExt::Global();

	if (!pRulesExt->TemporalLifeDisplay)
		return;

	if (isBuilding)
	{
		const auto pBuilding = static_cast<BuildingClass*>(pThis);
		const auto pType = pBuilding->Type;
		const auto maxLength = pType->GetFoundationHeight(false) * 15 >> 1;
		const auto offset = pRulesExt->TemporalLifeDisplay_Buildings_Offset.Get();
		auto position = basePosition + Point2D { offset.X, pType->PixelSelectionBracketDelta + offset.Y };

		DrawFrameStruct pDraw
		{
			static_cast<int>((static_cast<double>(pTemporal->WarpRemaining) / (pType->Strength * 10)) * maxLength + 0.99),
			pRulesExt->TemporalLifeDisplay_Buildings_Pips,
			pRulesExt->ProgressDisplay_Buildings_PipsShape.Get(),
			0,
			-1,
			nullptr,
			maxLength,
			-1,
			&position,
			pBounds
		};

		if (offset == Point2D::Empty && (pThis->IsSelected || pThis->IsMouseHovering)) // Layer fix
		{
			RulesClass* const pRules = RulesClass::Instance;
			const auto ratio = pBuilding->GetHealthPercentage();
			pDraw.MidLength = static_cast<int>(ratio * maxLength);
			pDraw.MidFrame = (ratio > pRules->ConditionYellow) ? 1 : (ratio > pRules->ConditionRed ? 2 : 4);
			pDraw.MidPipSHP = FileSystem::PIPS_SHP;
			pDraw.BrdFrame = 0;
		}

		TechnoExt::DrawVanillaStyleBuildingBar(&pDraw);
	}
	else
	{
		const int maxLength = isInfantry ? 8 : 17;
		const auto pType = pThis->GetTechnoType();
		auto position = basePosition + Point2D { 0, (pType->PixelSelectionBracketDelta + 2) } + pRulesExt->TemporalLifeDisplay_Others_Offset.Get();

		DrawFrameStruct pDraw
		{
			static_cast<int>((static_cast<double>(pTemporal->WarpRemaining) / (pType->Strength * 10)) * maxLength + 0.99),
			pRulesExt->TemporalLifeDisplay_Others_Pips,
			pRulesExt->ProgressDisplay_Others_PipsShape.Get(),
			0,
			-1,
			nullptr,
			maxLength,
			-1,
			&position,
			pBounds
		};

		TechnoExt::DrawVanillaStyleFootBar(&pDraw);
	}
}

void TechnoExt::DrawVanillaStyleFootBar(DrawFrameStruct* pDraw)
{
	const auto pLocation = pDraw->Location;
	const auto pBounds = pDraw->Bounds;

	if (pDraw->BrdFrame >= 0)
	{
		pLocation->X += 17;
		DSurface::Temp->DrawSHP(FileSystem::PALETTE_PAL, FileSystem::PIPBRD_SHP, pDraw->BrdFrame, pLocation, pBounds, BlitterFlags(0xE00), 0, 0, ZGradient::Ground, 1000, 0, 0, 0, 0, 0);
		pLocation->X -= 15;
	}
	else
	{
		pLocation->X += 2;
	}

	pLocation->Y += 1;

	const auto topLength = pDraw->TopLength;
	const auto midLength = pDraw->MidLength;
	const auto maxLength = pDraw->MaxLength;

	auto length = topLength > maxLength ? maxLength : topLength;

	if (pDraw->TopFrame >= 0 && pDraw->TopPipSHP)
	{
		for (auto drawIdx = length; drawIdx > 0 ; --drawIdx, pLocation->X += 2)
			DSurface::Temp->DrawSHP(FileSystem::PALETTE_PAL, pDraw->TopPipSHP, pDraw->TopFrame, pLocation, pBounds, BlitterFlags(0x600), 0, 0, ZGradient::Ground, 1000, 0, 0, 0, 0, 0);
	}

	length = midLength > maxLength ? maxLength - length : midLength - length;

	if (pDraw->MidFrame >= 0 && pDraw->MidPipSHP)
	{
		for (auto drawIdx = length; drawIdx > 0 ; --drawIdx, pLocation->X += 2)
			DSurface::Temp->DrawSHP(FileSystem::PALETTE_PAL, pDraw->MidPipSHP, pDraw->MidFrame, pLocation, pBounds, BlitterFlags(0x600), 0, 0, ZGradient::Ground, 1000, 0, 0, 0, 0, 0);
	}
}

void TechnoExt::DrawVanillaStyleBuildingBar(DrawFrameStruct* pDraw)
{
	const auto pLocation = pDraw->Location;
	++pLocation->X;
	const auto pBounds = pDraw->Bounds;

	const auto topLength = pDraw->TopLength;
	const auto midLength = pDraw->MidLength;
	const auto maxLength = pDraw->MaxLength;

	auto length = topLength > maxLength ? maxLength : topLength;

	if (pDraw->TopFrame >= 0 && pDraw->TopPipSHP)
	{
		for (auto drawIdx = length; drawIdx > 0 ; --drawIdx, pLocation->X -= 4, pLocation->Y += 2)
			DSurface::Temp->DrawSHP(FileSystem::PALETTE_PAL, pDraw->TopPipSHP, pDraw->TopFrame, pLocation, pBounds, BlitterFlags(0x600), 0, 0, ZGradient::Ground, 1000, 0, 0, 0, 0, 0);
	}

	length = midLength > maxLength ? maxLength - length : midLength - length;

	if (pDraw->MidFrame >= 0 && pDraw->MidPipSHP)
	{
		for (auto drawIdx = length; drawIdx > 0 ; --drawIdx, pLocation->X -= 4, pLocation->Y += 2)
			DSurface::Temp->DrawSHP(FileSystem::PALETTE_PAL, pDraw->MidPipSHP, pDraw->MidFrame, pLocation, pBounds, BlitterFlags(0x600), 0, 0, ZGradient::Ground, 1000, 0, 0, 0, 0, 0);
	}

	length = length >= 0 ? maxLength - midLength : maxLength - topLength;

	if (pDraw->BrdFrame >= 0)
	{
		for (auto drawIdx = length; drawIdx > 0 ; --drawIdx, pLocation->X -= 4, pLocation->Y += 2)
			DSurface::Temp->DrawSHP(FileSystem::PALETTE_PAL, FileSystem::PIPS_SHP, pDraw->BrdFrame, pLocation, pBounds, BlitterFlags(0x600), 0, 0, ZGradient::Ground, 1000, 0, 0, 0, 0, 0);
	}
}

Point2D TechnoExt::GetScreenLocation(TechnoClass* pThis)
{
	return TacticalClass::Instance->CoordsToClient(pThis->GetCoords()).first;
}

Point2D TechnoExt::GetFootSelectBracketPosition(TechnoClass* pThis, Anchor anchor, bool isInfantry)
{
	const int length = isInfantry ? 8 : 17;
	Point2D position = GetScreenLocation(pThis);

	RectangleStruct bracketRect =
	{
		position.X - length + (length == 8) + 1,
		position.Y - 28 + (length == 8),
		length * 2,
		length * 3
	};

	return anchor.OffsetPosition(bracketRect);
}

Point2D TechnoExt::GetBuildingSelectBracketPosition(TechnoClass* pThis, TechnoTypeClass* pType, BuildingSelectBracketPosition bracketPosition)
{
	const auto pBuildingType = static_cast<BuildingTypeClass*>(pType);
	Point2D position = GetScreenLocation(pThis);
	CoordStruct dim2 = CoordStruct::Empty;
	pBuildingType->Dimension2(&dim2);
	dim2 = { -dim2.X / 2, dim2.Y / 2, dim2.Z };
	const Point2D positionFix = TacticalClass::CoordsToScreen(dim2);

	const int foundationWidth = pBuildingType->GetFoundationWidth();
	const int foundationHeight = pBuildingType->GetFoundationHeight(false);
	const int height = pBuildingType->Height * 12;
	const int lengthW = foundationWidth * 7 + foundationWidth / 2;
	const int lengthH = foundationHeight * 7 + foundationHeight / 2;

	position.X += positionFix.X + 3 + lengthH * 4;
	position.Y += positionFix.Y + 4 - lengthH * 2;

	switch (bracketPosition)
	{
	case BuildingSelectBracketPosition::Top:
		break;
	case BuildingSelectBracketPosition::LeftTop:
		position.X -= lengthH * 4;
		position.Y += lengthH * 2;
		break;
	case BuildingSelectBracketPosition::LeftBottom:
		position.X -= lengthH * 4;
		position.Y += lengthH * 2 + height;
		break;
	case BuildingSelectBracketPosition::Bottom:
		position.Y += lengthW * 2 + lengthH * 2 + height;
		break;
	case BuildingSelectBracketPosition::RightBottom:
		position.X += lengthW * 4;
		position.Y += lengthW * 2 + height;
		break;
	case BuildingSelectBracketPosition::RightTop:
		position.X += lengthW * 4;
		position.Y += lengthW * 2;
	default:
		break;
	}

	return position;
}

void TechnoExt::DrawSelectBox(TechnoClass* pThis, const Point2D* pLocation, const RectangleStruct* pBounds, bool drawBefore)
{
	const auto whatAmI = pThis->WhatAmI();
	const auto pTypeExt = TechnoExt::Fetch(pThis)->TypeExtData;
	const auto pType = pTypeExt->OwnerObject();
	SelectBoxTypeClass* pSelectBox = nullptr;

	if (pTypeExt->SelectBox.isset())
		pSelectBox = pTypeExt->SelectBox.Get();
	else if (whatAmI == InfantryClass::AbsID)
		pSelectBox = RulesExt::Global()->DefaultInfantrySelectBox.Get();
	else if (whatAmI != BuildingClass::AbsID)
		pSelectBox = RulesExt::Global()->DefaultUnitSelectBox.Get();

	if (!pSelectBox || pSelectBox->DrawAboveTechno == drawBefore)
		return;

	const bool canSee = HouseClass::IsCurrentPlayerObserver() ? pSelectBox->VisibleToHouses_Observer : EnumFunctions::CanTargetHouse(pSelectBox->VisibleToHouses, pThis->Owner, HouseClass::CurrentPlayer);

	if (!canSee)
		return;

	const double healthPercentage = pThis->GetHealthPercentage();
	const auto defaultFrame = whatAmI == InfantryClass::AbsID ? Vector3D<int> { 1, 1, 1 } : Vector3D<int> { 0, 0, 0 };

	const auto pSurface = DSurface::Temp;
	const auto flags = (drawBefore ? BlitterFlags::Flat | BlitterFlags::Alpha : BlitterFlags::Nonzero | BlitterFlags::MultiPass) | BlitterFlags::Centered | pSelectBox->Translucency;
	const int zAdjust = drawBefore ? pThis->GetZAdjustment() - 2 : 0;
	const auto pGroundShape = pSelectBox->GroundShape.Get();

	if ((pGroundShape || pSelectBox->GroundLine) && whatAmI != BuildingClass::AbsID && (pSelectBox->Ground_AlwaysDraw || pThis->IsInAir()))
	{
		auto [point, visible] = TacticalClass::Instance->CoordsToClient(pThis->GetRenderCoords());
		const auto pFoot = static_cast<FootClass*>(pThis);
		if(pThis->WhatAmI()==AbstractType::Aircraft)
			point.Y += TacticalClass::AdjustForZ(pFoot->GetHeight());
		else if (!UnderwaterInterop::IsPhysicallySubmerged(pThis))
			point += pFoot->Locomotor->Shadow_Point();
		// Ground selection decorations follow a submerged body. The ordinary
		// ship Shadow_Point would cancel its negative height back to the surface.
		// Shape/Offset/GroundOffset, visibility and draw ordering remain native.
		if (visible && pGroundShape)
		{
			const auto pPalette = pSelectBox->GroundPalette.GetOrDefaultConvert(FileSystem::PALETTE_PAL);

			const Vector3D<int> frames = pSelectBox->GroundFrames.Get(defaultFrame);
			const int frame = healthPercentage > RulesClass::Instance->ConditionYellow ? frames.X : healthPercentage > RulesClass::Instance->ConditionRed ? frames.Y : frames.Z;

			const Point2D drawPoint = point + pSelectBox->GroundOffset;
			pSurface->DrawSHP(pPalette, pGroundShape, frame, &drawPoint, pBounds, flags, 0, zAdjust, ZGradient::Ground, 1000, 0, nullptr, 0, 0, 0);
		}

		if (pSelectBox->GroundLine)
		{
			Point2D start = *pLocation; // Copy to prevent be modified
			const int color = Drawing::RGB_To_Int(pSelectBox->GroundLineColor.Get(healthPercentage));

			if (pSelectBox->GroundLine_Dashed)
				pSurface->DrawDashed(&start, &point, color, 0);
			else if (Line_In_Bounds(&start, &point, &DSurface::ViewBounds))
				pSurface->DrawLine(&start, &point, color);
		}
	}

	if (const auto pShape = pSelectBox->Shape.Get())
	{
		const auto pPalette = pSelectBox->Palette.GetOrDefaultConvert(FileSystem::PALETTE_PAL);

		const Vector3D<int> frames = pSelectBox->Frames.Get(defaultFrame);
		const int frame = healthPercentage > RulesClass::Instance->ConditionYellow ? frames.X : healthPercentage > RulesClass::Instance->ConditionRed ? frames.Y : frames.Z;

		const Point2D offset = whatAmI == InfantryClass::AbsID ? Point2D { 8, -3 } : Point2D { 1, -4 };
		Point2D drawPoint = *pLocation + offset + pSelectBox->Offset;

		if (pSelectBox->DrawAboveTechno)
			drawPoint.Y += pType->PixelSelectionBracketDelta;

		pSurface->DrawSHP(pPalette, pShape, frame, &drawPoint, pBounds, flags, 0, zAdjust, ZGradient::Ground, 1000, 0, nullptr, 0, 0, 0);
	}
}

void TechnoExt::ProcessDigitalDisplays(TechnoClass* pThis)
{
	if (!Phobos::Config::DigitalDisplay_Enable)
		return;

	const auto pTypeExt = TechnoExt::Fetch(pThis)->TypeExtData;

	if (pTypeExt->DigitalDisplay_Disable)
		return;

	int length = 17;
	ValueableVector<DigitalDisplayTypeClass*>* pDisplayTypes = nullptr;
	const auto whatAmI = pThis->WhatAmI();

	if (!pTypeExt->DigitalDisplayTypes.empty())
	{
		pDisplayTypes = &pTypeExt->DigitalDisplayTypes;
	}
	else
	{
		switch (whatAmI)
		{
		case AbstractType::Building:
		{
			pDisplayTypes = &RulesExt::Global()->Buildings_DefaultDigitalDisplayTypes;
			const auto pBuildingType = static_cast<BuildingTypeClass*>(pTypeExt->OwnerObject());
			const int height = pBuildingType->GetFoundationHeight(false);
			length = height * 7 + height / 2;
			break;
		}
		case AbstractType::Infantry:
		{
			pDisplayTypes = &RulesExt::Global()->Infantry_DefaultDigitalDisplayTypes;
			length = 8;
			break;
		}
		case AbstractType::Unit:
		{
			pDisplayTypes = &RulesExt::Global()->Vehicles_DefaultDigitalDisplayTypes;
			break;
		}
		case AbstractType::Aircraft:
		{
			pDisplayTypes = &RulesExt::Global()->Aircraft_DefaultDigitalDisplayTypes;
			break;
		}
		default:
			return;
		}
	}

	const auto pType = pTypeExt->OwnerObject();
	const auto pShield = TechnoExt::Fetch(pThis)->Shield.get();
	const bool hasShield = pShield && !pShield->IsBrokenAndNonRespawning();
	const bool isBuilding = whatAmI == AbstractType::Building;
	const bool isInfantry = whatAmI == AbstractType::Infantry;

	for (DigitalDisplayTypeClass*& pDisplayType : *pDisplayTypes)
	{
		if (!pDisplayType->CanShow(pThis))
			continue;

		int value = -1;
		int maxValue = 0;

		TechnoExt::GetValuesForDisplay(pThis, pType, pDisplayType->InfoType, value, maxValue, pDisplayType->InfoIndex);

		if (value <= -1 || maxValue <= 0)
			continue;

		const auto divisor = pDisplayType->ValueScaleDivisor.Get(pDisplayType->ValueAsTimer ? 15 : 1);

		if (divisor > 1)
		{
			value = Math::max(value / divisor, value ? 1 : 0);
			maxValue = Math::max(maxValue / divisor, 1);
		}

		Point2D position = isBuilding
			? TechnoExt::GetBuildingSelectBracketPosition(pThis, pType, pDisplayType->AnchorType_Building)
			: TechnoExt::GetFootSelectBracketPosition(pThis, pDisplayType->AnchorType, isInfantry);
		position.Y += pType->PixelSelectionBracketDelta;

		if (pDisplayType->InfoType == DisplayInfoType::Shield)
			position.Y += pShield->GetType()->BracketDelta;

		pDisplayType->Draw(position, length, value, maxValue, isBuilding, isInfantry, hasShield);
	}
}

void TechnoExt::GetValuesForDisplay(TechnoClass* pThis, TechnoTypeClass* pType, DisplayInfoType infoType, int& value, int& maxValue, int infoIndex)
{
	switch (infoType)
	{
	case DisplayInfoType::Health:
	{
		value = pThis->Health;
		maxValue = pType->Strength;

		if (pThis->Disguised && !pThis->Owner->IsAlliedWith(HouseClass::CurrentPlayer))
			TechnoExt::GetDigitalDisplayFakeHealth(pThis, value, maxValue);

		break;
	}
	case DisplayInfoType::Shield:
	{
		const auto pShield = TechnoExt::Fetch(pThis)->Shield.get();

		if (!pShield || pShield->IsBrokenAndNonRespawning())
			return;

		value = pShield->GetHP();
		maxValue = pShield->GetType()->Strength.Get();
		break;
	}
	case DisplayInfoType::Ammo:
	{
		value = pThis->Ammo;
		maxValue = pType->Ammo;
		break;
	}
	case DisplayInfoType::MindControl:
	{
		const auto pCaptureManager = pThis->CaptureManager;

		if (!pCaptureManager)
			return;

		value = pCaptureManager->GetControlledCount();
		maxValue = pCaptureManager->MaxControlNodes;
		break;
	}
	case DisplayInfoType::Spawns:
	{
		const auto pSpawnManager = pThis->SpawnManager;

		if (!pSpawnManager || !pType->Spawns)
			return;

		if (infoIndex == 1)
			value = pSpawnManager->CountDockedSpawns();
		else if (infoIndex == 2)
			value = pSpawnManager->CountLaunchingSpawns();
		else
			value = pSpawnManager->CountAliveSpawns();

		maxValue = pType->SpawnsNumber;
		break;
	}
	case DisplayInfoType::Passengers:
	{
		value = pThis->Passengers.NumPassengers;
		maxValue = pType->Passengers;
		break;
	}
	case DisplayInfoType::Tiberium:
	{
		if (infoIndex && infoIndex <= TiberiumClass::Array.Count)
			value = static_cast<int>(pThis->Tiberium.GetAmount(infoIndex - 1));
		else
			value = static_cast<int>(pThis->Tiberium.GetTotalAmount());

		maxValue = pType->Storage;
		break;
	}
	case DisplayInfoType::Experience:
	{
		value = static_cast<int>(pThis->Veterancy.Veterancy * RulesClass::Instance->VeteranRatio * pType->GetCost());
		maxValue = static_cast<int>(2.0 * RulesClass::Instance->VeteranRatio * pType->GetCost());
		break;
	}
	case DisplayInfoType::Occupants:
	{
		if (pThis->WhatAmI() != AbstractType::Building)
			return;

		const auto pBuildingType = static_cast<BuildingTypeClass*>(pType);

		if (!pBuildingType->CanBeOccupied)
			return;

		value = static_cast<BuildingClass*>(pThis)->Occupants.Count;
		maxValue = pBuildingType->MaxNumberOccupants;
		break;
	}
	case DisplayInfoType::GattlingStage:
	{
		if (!pType->IsGattling)
			return;

		value = pThis->GattlingValue ? pThis->CurrentGattlingStage + 1 : 0;
		maxValue = pType->WeaponStages;
		break;
	}
	case DisplayInfoType::ROF:
	{
		if (!pThis->IsArmed())
			return;

		const auto& timer = pThis->RearmTimer;
		value = timer.GetTimeLeft();
		maxValue = timer.TimeLeft;
		break;
	}
	case DisplayInfoType::Reload:
	{
		if (pType->Ammo <= 0)
			return;

		const auto& timer = pThis->ReloadTimer;
		value = (pThis->Ammo >= pType->Ammo) ? 0 : timer.GetTimeLeft();
		maxValue = timer.TimeLeft ? timer.TimeLeft : ((pThis->Ammo || pType->EmptyReload <= 0) ? pType->Reload : pType->EmptyReload);
		break;
	}
	case DisplayInfoType::SpawnTimer:
	{
		const auto pSpawnManager = pThis->SpawnManager;

		if (!pSpawnManager || !pType->Spawns || pType->SpawnsNumber <= 0)
			return;

		if (infoIndex && infoIndex <= pSpawnManager->SpawnedNodes.Count)
		{
			value = pSpawnManager->SpawnedNodes[infoIndex - 1]->SpawnTimer.GetTimeLeft();
		}
		else
		{
			for (int i = 0; i < pSpawnManager->SpawnedNodes.Count; ++i)
			{
				const auto pSpawnNode = pSpawnManager->SpawnedNodes[i];

				if (pSpawnNode->Status == SpawnNodeStatus::Dead)
				{
					const int time = pSpawnNode->SpawnTimer.GetTimeLeft();

					if (!value || time < value)
						value = time;
				}
			}
		}

		maxValue = pSpawnManager->RegenRate;
		break;
	}
	case DisplayInfoType::GattlingTimer:
	{
		if (!pType->IsGattling)
			return;

		const auto thisStage = pThis->CurrentGattlingStage;
		const auto& stage = pThis->Veterancy.IsElite() ? pType->EliteStage : pType->WeaponStage;

		value = pThis->GattlingValue;
		maxValue = stage[thisStage];

		if (thisStage > 0)
		{
			value -= stage[thisStage - 1];
			maxValue -= stage[thisStage - 1];
		}

		break;
	}
	case DisplayInfoType::ProduceCash:
	{
		if (pThis->WhatAmI() != AbstractType::Building || static_cast<BuildingTypeClass*>(pType)->ProduceCashAmount <= 0)
			return;

		const auto& timer = static_cast<BuildingClass*>(pThis)->CashProductionTimer;
		value = timer.GetTimeLeft();
		maxValue = timer.TimeLeft;
		break;
	}
	case DisplayInfoType::PassengerKill:
	{
		const auto pExt = TechnoExt::Fetch(pThis);

		if (!pExt->TypeExtData->PassengerDeletionType)
			return;

		const auto& timer = pExt->PassengerDeletionTimer;
		value = timer.GetTimeLeft();
		maxValue = timer.TimeLeft;
		break;
	}
	case DisplayInfoType::AutoDeath:
	{
		const auto pExt = TechnoExt::Fetch(pThis);
		const auto pTypeExt = pExt->TypeExtData;

		if (!pTypeExt->AutoDeath_Behavior.isset())
			return;

		if (pTypeExt->AutoDeath_AfterDelay > 0)
		{
			const auto& timer = pExt->AutoDeathTimer;
			value = timer.GetTimeLeft();
			maxValue = timer.TimeLeft;
		}
		else if (pTypeExt->AutoDeath_OnAmmoDepletion)
		{
			value = pThis->Ammo;
			maxValue = pType->Ammo;
		}

		break;
	}
	case DisplayInfoType::SuperWeapon:
	{
		if (pThis->WhatAmI() != AbstractType::Building)
			return;

		auto getSuperTimer = [pThis, pType, infoIndex]() -> CDTimerClass*
		{
			const auto pHouse = pThis->Owner;
			const auto pBuildingType = static_cast<BuildingTypeClass*>(pType);
			const auto pBuildingTypeExt = BuildingTypeExt::Fetch(pBuildingType);

			if (infoIndex && infoIndex <= pBuildingTypeExt->GetSuperWeaponCount())
			{
				if (infoIndex == 1)
				{
					if (pBuildingType->SuperWeapon != -1)
						return &pHouse->Supers.GetItem(pBuildingType->SuperWeapon)->RechargeTimer;
				}
				else if (infoIndex == 2)
				{
					if (pBuildingType->SuperWeapon2 != -1)
						return &pHouse->Supers.GetItem(pBuildingType->SuperWeapon2)->RechargeTimer;
				}
				else
				{
					const auto& superWeapons = pBuildingTypeExt->SuperWeapons;
					return &pHouse->Supers.GetItem(superWeapons[infoIndex - 3])->RechargeTimer;
				}

				return nullptr;
			}

			if (pBuildingType->SuperWeapon != -1)
				return &pHouse->Supers.GetItem(pBuildingType->SuperWeapon)->RechargeTimer;
			else if (pBuildingType->SuperWeapon2 != -1)
				return &pHouse->Supers.GetItem(pBuildingType->SuperWeapon2)->RechargeTimer;

			const auto& superWeapons = pBuildingTypeExt->SuperWeapons;
			return superWeapons.size() > 0 ? &pHouse->Supers.GetItem(superWeapons[0])->RechargeTimer : nullptr;
		};
		if (const auto pTimer = getSuperTimer())
		{
			value = pTimer->GetTimeLeft();
			maxValue = pTimer->TimeLeft;
		}

		break;
	}
	case DisplayInfoType::IronCurtain:
	{
		if (!pThis->IsIronCurtained())
			return;

		const auto& timer = pThis->IronCurtainTimer;
		value = timer.GetTimeLeft();
		maxValue = timer.TimeLeft;
		break;
	}
	case DisplayInfoType::TemporalLife:
	{
		const auto pTemporal = pThis->TemporalTargetingMe;

		if (!pTemporal)
			return;

		value = pTemporal->WarpRemaining;
		maxValue = pType->Strength * 10;
		break;
	}
	case DisplayInfoType::FactoryProcess:
	{
		if (pThis->WhatAmI() != AbstractType::Building)
			return;

		auto getFactory = [pThis, pType, infoIndex]() -> FactoryClass*
		{
			const auto pHouse = pThis->Owner;
			const auto pBuildingType = static_cast<BuildingTypeClass*>(pType);

			if (infoIndex == 1)
			{
				if (!pHouse->IsControlledByHuman())
					return static_cast<BuildingClass*>(pThis)->Factory;
				else if (pThis->IsPrimaryFactory)
					return pHouse->GetPrimaryFactory(pBuildingType->Factory, pBuildingType->Naval, BuildCat::DontCare);
			}
			else if (infoIndex == 2)
			{
				if (pHouse->IsControlledByHuman() && pThis->IsPrimaryFactory && pBuildingType->Factory == AbstractType::BuildingType)
					return pHouse->Primary_ForDefenses;
			}
			else if (!pHouse->IsControlledByHuman())
			{
				return static_cast<BuildingClass*>(pThis)->Factory;
			}
			else if (pThis->IsPrimaryFactory)
			{
				const auto pFactory = pHouse->GetPrimaryFactory(pBuildingType->Factory, pBuildingType->Naval, BuildCat::DontCare);

				if (pFactory && pFactory->Object)
					return pFactory;
				else if (pBuildingType->Factory == AbstractType::BuildingType)
					return pHouse->Primary_ForDefenses;
			}

			return nullptr;
		};
		if (const auto pFactory = getFactory())
		{
			if (pFactory->Object)
			{
				value = pFactory->GetProgress();
				maxValue = 54;
			}
		}

		break;
	}
	default:
	{
		value = pThis->Health;
		maxValue = pType->Strength;

		if (pThis->Disguised && !pThis->Owner->IsAlliedWith(HouseClass::CurrentPlayer))
			TechnoExt::GetDigitalDisplayFakeHealth(pThis, value, maxValue);

		break;
	}
	}
}

void TechnoExt::GetDigitalDisplayFakeHealth(TechnoClass* pThis, int& value, int& maxValue)
{
	if (TechnoExt::Fetch(pThis)->TypeExtData->DigitalDisplay_Health_FakeAtDisguise.Get(RulesExt::Global()->DigitalDisplay_Health_FakeAtDisguise))
	{
		if (const auto pType = TechnoTypeExt::GetTechnoType(pThis->Disguise))
		{
			const int newMaxValue = pType->Strength;
			const double ratio = static_cast<double>(value) / maxValue;
			value = static_cast<int>(ratio * newMaxValue);
			maxValue = newMaxValue;
		}
	}
}

void TechnoExt::ShowPromoteAnim(TechnoClass* pThis)
{
	auto const pTypeExt = TechnoExt::Fetch(pThis)->TypeExtData;
	auto const& veteranAnims = !pTypeExt->Promote_VeteranAnimation.empty() ? pTypeExt->Promote_VeteranAnimation : RulesExt::Global()->Promote_VeteranAnimation;
	auto const& eliteAnims = !pTypeExt->Promote_EliteAnimation.empty() ? pTypeExt->Promote_EliteAnimation : RulesExt::Global()->Promote_EliteAnimation;

	if (pThis->Veterancy.GetRemainingLevel() == Rank::Veteran && !veteranAnims.empty())
		AnimExt::CreateRandomAnim(veteranAnims, pThis->GetCenterCoords(), pThis, pThis->Owner, true, true);
	else if (!eliteAnims.empty())
		AnimExt::CreateRandomAnim(eliteAnims, pThis->GetCenterCoords(), pThis, pThis->Owner, true, true);
}

void TechnoExt::DrawExtraImage(TechnoClass* pThis, CellClass* pCell, const CoordStruct& coords, DirStruct dir)
{
	if (!pThis->IsAlive || pThis->Health <= 0 || pThis->IsSinking || pThis->IsCrashing)
		return;

	const auto level = pCell->Level * Unsorted::LevelHeight;

	if (coords.Z < level)
		return;

	const auto pair = TacticalClass::Instance->CoordsToClient(coords);

	if (!pair.second)
		return;

	const bool inAir = coords.Z > (level + Unsorted::CellHeight);
	const auto slope = inAir ? 0 : pCell->SlopeIndex;
	const auto action = (!inAir && !pCell->ContainsBridge() && pCell->LandType == LandType::Water) ? Sequence::Swim : Sequence::Ready;
	TechnoExt::DrawExtraImage(pThis, pair.first, DSurface::ViewBounds, dir, true, action, slope);
}

void TechnoExt::DrawExtraImage(TechnoClass* pThis, const Point2D& location, const RectangleStruct& bounds, DirStruct dir, bool transparent, Sequence action, int tilt)
{
	if (bounds.Width <= 0 || bounds.Height <= 0)
		return;

	const auto& viewBounds = DSurface::ViewBounds;

	if (viewBounds.Width <= 0 || viewBounds.Height <= 0)
		return;

	auto renderBounds = viewBounds;

	if (viewBounds.X < bounds.X)
	{
		renderBounds.X = bounds.X;
		renderBounds.Width += viewBounds.X - bounds.X;
	}

	const int right = bounds.X + bounds.Width;
	if (renderBounds.X + renderBounds.Width > right)
		renderBounds.Width = right - renderBounds.X;

	if (renderBounds.Width <= 0)
		return;

	if (viewBounds.Y < bounds.Y)
	{
		renderBounds.Y = bounds.Y;
		renderBounds.Height += viewBounds.Y - bounds.Y;
	}

	const int down = bounds.Y + bounds.Height;
	if (renderBounds.Y + renderBounds.Height > down)
		renderBounds.Height = down - renderBounds.Y;

	if (renderBounds.Height <= 0)
		return;

	auto renderLocation = location;

	if (bounds.X > DSurface::ViewBounds.X)
		renderLocation.X += DSurface::ViewBounds.X - renderBounds.X;

	if (bounds.Y > DSurface::ViewBounds.Y)
		renderLocation.Y += DSurface::ViewBounds.Y - renderBounds.Y;

	switch (pThis->WhatAmI())
	{
	case AbstractType::Unit:

		TechnoExt::DrawExtraImage(static_cast<UnitClass*>(pThis), &renderLocation, &renderBounds, dir, transparent, tilt);
		break;

	case AbstractType::Infantry:

		TechnoExt::DrawExtraImage(static_cast<InfantryClass*>(pThis), &renderLocation, &renderBounds, dir, transparent, action);
		break;

	case AbstractType::Aircraft:

		TechnoExt::DrawExtraImage(static_cast<AircraftClass*>(pThis), &renderLocation, &renderBounds, dir, transparent);
		break;

	// case AbstractType::Building:

	default:
		break;
	}
}

void TechnoExt::DrawExtraImage(UnitClass* pThis, Point2D* pLocation, RectangleStruct* pBounds, DirStruct dir, bool transparent, int tilt)
{
	const int select = TacticalClass::Instance->SelectableCount;
	TacticalClass::Instance->SelectableCount = 500;
	const bool lightning = LightningStorm::Active;
	LightningStorm::Active = false;
	const auto psy = PsyDom::Status;
	PsyDom::Status = PsychicDominatorStatus::Inactive;
	const auto nuke = NukeFlash::Status;
	NukeFlash::Status = NukeFlashStatus::Inactive;

	const bool warp = pThis->BeingWarpedOut;
	pThis->BeingWarpedOut = transparent;

	const auto pExt = TechnoExt::Fetch(pThis);
	const auto pType = pThis->Type;

	const bool shadow = pType->NoShadow;
	pType->NoShadow = true;

	const bool berzerk = pThis->Berzerk;
	pThis->Berzerk = false;
	const auto airstrike = pExt->AirstrikeTargetingMe;
	pExt->AirstrikeTargetingMe = nullptr;
	const int iron = pThis->IronCurtainTimer.TimeLeft;
	pThis->IronCurtainTimer.TimeLeft = 0;
	const int flash = pThis->Flashing.DurationRemaining;
	pThis->Flashing.DurationRemaining = 0;
	const bool disguised = pThis->Disguised;
	pThis->Disguised = false;
	const auto disguise = pThis->DisguiseCreationFrame;
	pThis->DisguiseCreationFrame = Unsorted::CurrentFrame + 1;
	const bool warping = pThis->WarpingOut;
	pThis->WarpingOut = false;
	const auto temporal = pThis->TemporalTargetingMe;
	pThis->TemporalTargetingMe = nullptr;
	const auto cloak = pThis->CloakState;
	pThis->CloakState = CloakState::Uncloaked;
	const float arf = pThis->AngleRotatedForwards;
	pThis->AngleRotatedForwards = 0.0f;
	const float ars = pThis->AngleRotatedSideways;
	pThis->AngleRotatedSideways = 0.0f;

	const bool deployed = pThis->Deployed;
	pThis->Deployed = false;
	const bool unloading = pThis->Unloading;
	pThis->Unloading = false;

	const bool bridge = pThis->OnBridge;
	pThis->OnBridge = false;
	const bool map = pThis->IsOnMap;
	pThis->IsOnMap = false;
	const auto coords = pThis->Location;
	pThis->Location = CoordStruct { INT_MAX, INT_MAX, INT_MAX };
	const char tube = pThis->TubeIndex;
	pThis->TubeIndex = -1;

	if (tilt < 0 || tilt > 20) tilt = 0;
	const auto slope = MapClass::InvalidCell.SlopeIndex;
	MapClass::InvalidCell.SlopeIndex = static_cast<BYTE>(tilt);
	struct LocomotionRampTemp { int CurrentRamp; int Rate; };
	struct LocomotionFaceTemp { DirStruct DesiredFacing; DirStruct ROT; double Speed; };
	const auto iLoco = pThis->Locomotor.GetInterfacePtr();
	const auto pDrLoco = locomotion_cast<DriveLocomotionClass*>(iLoco);
	const auto pShLoco = locomotion_cast<ShipLocomotionClass*>(iLoco);
	const auto pAdLoco = locomotion_cast<AdvancedDriveLocomotionClass*>(iLoco);
	const auto pJjLoco = locomotion_cast<JumpjetLocomotionClass*>(iLoco);
	const auto rampTemp = (pDrLoco ? LocomotionRampTemp(pDrLoco->CurrentRamp, pDrLoco->SlopeTimer.Rate)
		: (pShLoco ? LocomotionRampTemp(pShLoco->CurrentRamp, pShLoco->SlopeTimer.Rate)
		: (pAdLoco ? LocomotionRampTemp(pAdLoco->CurrentRamp, pAdLoco->SlopeTimer.Rate) : LocomotionRampTemp())));
	const auto faceTemp = pJjLoco
		? LocomotionFaceTemp(pJjLoco->LocomotionFacing.DesiredFacing, pJjLoco->LocomotionFacing.ROT, pJjLoco->CurrentSpeed)
		: LocomotionFaceTemp();
	if (pDrLoco)
	{
		pDrLoco->CurrentRamp = tilt;
		pDrLoco->SlopeTimer.Rate = 0;
	}
	else if (pShLoco)
	{
		pShLoco->CurrentRamp = tilt;
		pShLoco->SlopeTimer.Rate = 0;
	}
	else if (pAdLoco)
	{
		pAdLoco->CurrentRamp = tilt;
		pAdLoco->SlopeTimer.Rate = 0;
	}
	else if (pJjLoco)
	{
		pJjLoco->LocomotionFacing.DesiredFacing = dir;
		pJjLoco->LocomotionFacing.ROT = DirStruct(0);
		pJjLoco->CurrentSpeed = 0.0;
	}

	const auto bodyDes = pThis->PrimaryFacing.DesiredFacing;
	const auto bodyROT = pThis->PrimaryFacing.ROT;
	const auto turretDes = pThis->SecondaryFacing.DesiredFacing;
	const auto turretROT = pThis->SecondaryFacing.ROT;
	pThis->PrimaryFacing.DesiredFacing = dir;
	pThis->PrimaryFacing.ROT = DirStruct(0);
	pThis->SecondaryFacing.DesiredFacing = dir;
	pThis->SecondaryFacing.ROT = DirStruct(0);

	pThis->DrawIt(pLocation, pBounds);

	pThis->PrimaryFacing.DesiredFacing = bodyDes;
	pThis->PrimaryFacing.ROT = bodyROT;
	pThis->SecondaryFacing.DesiredFacing = turretDes;
	pThis->SecondaryFacing.ROT = turretROT;

	if (pDrLoco)
	{
		pDrLoco->CurrentRamp = rampTemp.CurrentRamp;
		pDrLoco->SlopeTimer.Rate = rampTemp.Rate;
	}
	else if (pShLoco)
	{
		pShLoco->CurrentRamp = rampTemp.CurrentRamp;
		pShLoco->SlopeTimer.Rate = rampTemp.Rate;
	}
	else if (pAdLoco)
	{
		pAdLoco->CurrentRamp = rampTemp.CurrentRamp;
		pAdLoco->SlopeTimer.Rate = rampTemp.Rate;
	}
	else if (pJjLoco)
	{
		pJjLoco->LocomotionFacing.DesiredFacing = faceTemp.DesiredFacing;
		pJjLoco->LocomotionFacing.ROT = faceTemp.ROT;
		pJjLoco->CurrentSpeed = faceTemp.Speed;
	}
	MapClass::InvalidCell.SlopeIndex = slope;

	pThis->OnBridge = bridge;
	pThis->IsOnMap = map;
	pThis->Location = coords;
	pThis->TubeIndex = tube;

	pThis->Deployed = deployed;
	pThis->Unloading = unloading;

	pThis->Berzerk = berzerk;
	pExt->AirstrikeTargetingMe = airstrike;
	pThis->IronCurtainTimer.TimeLeft = iron;
	pThis->Flashing.DurationRemaining = flash;
	pThis->Disguised = disguised;
	pThis->DisguiseCreationFrame = disguise;
	pThis->WarpingOut = warping;
	pThis->TemporalTargetingMe = temporal;
	pThis->CloakState = cloak;
	pThis->AngleRotatedForwards = arf;
	pThis->AngleRotatedSideways = ars;

	pType->NoShadow = shadow;

	pThis->BeingWarpedOut = warp;

	LightningStorm::Active = lightning;
	PsyDom::Status = psy;
	NukeFlash::Status = nuke;
	TacticalClass::Instance->SelectableCount = select;
}

void TechnoExt::DrawExtraImage(InfantryClass* pThis, Point2D* pLocation, RectangleStruct* pBounds, DirStruct dir, bool transparent, Sequence action)
{
	const int select = TacticalClass::Instance->SelectableCount;
	TacticalClass::Instance->SelectableCount = 500;
	const bool lightning = LightningStorm::Active;
	LightningStorm::Active = false;
	const auto psy = PsyDom::Status;
	PsyDom::Status = PsychicDominatorStatus::Inactive;
	const auto nuke = NukeFlash::Status;
	NukeFlash::Status = NukeFlashStatus::Inactive;

	const bool warp = pThis->BeingWarpedOut;
	pThis->BeingWarpedOut = transparent;

	const auto pExt = TechnoExt::Fetch(pThis);
	const auto pType = pThis->Type;

	const bool shadow = pType->NoShadow;
	pType->NoShadow = true;

	const bool berzerk = pThis->Berzerk;
	pThis->Berzerk = false;
	const auto airstrike = pExt->AirstrikeTargetingMe;
	pExt->AirstrikeTargetingMe = nullptr;
	const int iron = pThis->IronCurtainTimer.TimeLeft;
	pThis->IronCurtainTimer.TimeLeft = 0;
	const int flash = pThis->Flashing.DurationRemaining;
	pThis->Flashing.DurationRemaining = 0;
	const bool disguised = pThis->Disguised;
	pThis->Disguised = false;
	const auto disguise = pThis->DisguiseCreationFrame;
	pThis->DisguiseCreationFrame = Unsorted::CurrentFrame + 1;
	const bool warping = pThis->WarpingOut;
	pThis->WarpingOut = false;
	const auto temporal = pThis->TemporalTargetingMe;
	pThis->TemporalTargetingMe = nullptr;
	const auto cloak = pThis->CloakState;
	pThis->CloakState = CloakState::Uncloaked;

	const bool bridge = pThis->OnBridge;
	pThis->OnBridge = false;
	const bool map = pThis->IsOnMap;
	pThis->IsOnMap = false;
	const auto coords = pThis->Location;
	pThis->Location = CoordStruct { INT_MAX, INT_MAX, INT_MAX };
	const char tube = pThis->TubeIndex;
	pThis->TubeIndex = -1;

	const int anim = pThis->Animation.Value;
	const auto sequence = pThis->SequenceAnim;
	pThis->Animation.Value = 0;
	pThis->SequenceAnim = action;

	struct LocomotionFaceTemp { DirStruct DesiredFacing; DirStruct ROT; };
	const auto pJjLoco = locomotion_cast<JumpjetLocomotionClass*>(pThis->Locomotor);
	const auto faceTemp = pJjLoco ? LocomotionFaceTemp(pJjLoco->LocomotionFacing.DesiredFacing, pJjLoco->LocomotionFacing.ROT) : LocomotionFaceTemp();
	if (pJjLoco)
	{
		pJjLoco->LocomotionFacing.DesiredFacing = dir;
		pJjLoco->LocomotionFacing.ROT = DirStruct(0);
	}

	const auto bodyDes = pThis->PrimaryFacing.DesiredFacing;
	const auto bodyROT = pThis->PrimaryFacing.ROT;
	pThis->PrimaryFacing.DesiredFacing = dir;
	pThis->PrimaryFacing.ROT = DirStruct(0);

	pThis->DrawIt(pLocation, pBounds);

	pThis->PrimaryFacing.DesiredFacing = bodyDes;
	pThis->PrimaryFacing.ROT = bodyROT;

	if (pJjLoco)
	{
		pJjLoco->LocomotionFacing.DesiredFacing = faceTemp.DesiredFacing;
		pJjLoco->LocomotionFacing.ROT = faceTemp.ROT;
	}

	pThis->Animation.Value = anim;
	pThis->SequenceAnim = sequence;

	pThis->OnBridge = bridge;
	pThis->IsOnMap = map;
	pThis->Location = coords;
	pThis->TubeIndex = tube;

	pThis->Berzerk = berzerk;
	pExt->AirstrikeTargetingMe = airstrike;
	pThis->IronCurtainTimer.TimeLeft = iron;
	pThis->Flashing.DurationRemaining = flash;
	pThis->Disguised = disguised;
	pThis->DisguiseCreationFrame = disguise;
	pThis->WarpingOut = warping;
	pThis->TemporalTargetingMe = temporal;
	pThis->CloakState = cloak;

	pType->NoShadow = shadow;

	pThis->BeingWarpedOut = warp;

	LightningStorm::Active = lightning;
	PsyDom::Status = psy;
	NukeFlash::Status = nuke;
	TacticalClass::Instance->SelectableCount = select;
}

void TechnoExt::DrawExtraImage(AircraftClass* pThis, Point2D* pLocation, RectangleStruct* pBounds, DirStruct dir, bool transparent)
{
	const int select = TacticalClass::Instance->SelectableCount;
	TacticalClass::Instance->SelectableCount = 500;
	const bool lightning = LightningStorm::Active;
	LightningStorm::Active = false;
	const auto psy = PsyDom::Status;
	PsyDom::Status = PsychicDominatorStatus::Inactive;
	const auto nuke = NukeFlash::Status;
	NukeFlash::Status = NukeFlashStatus::Inactive;

	const bool warp = pThis->BeingWarpedOut;
	pThis->BeingWarpedOut = transparent;

	const auto pExt = TechnoExt::Fetch(pThis);
	const auto pType = pThis->Type;

	const bool shadow = pType->NoShadow;
	pType->NoShadow = true;
	const bool drop = pType->IsDropship;
	pType->IsDropship = true;

	const bool berzerk = pThis->Berzerk;
	pThis->Berzerk = false;
	const auto airstrike = pExt->AirstrikeTargetingMe;
	pExt->AirstrikeTargetingMe = nullptr;
	const int iron = pThis->IronCurtainTimer.TimeLeft;
	pThis->IronCurtainTimer.TimeLeft = 0;
	const int flash = pThis->Flashing.DurationRemaining;
	pThis->Flashing.DurationRemaining = 0;
	const bool disguised = pThis->Disguised;
	pThis->Disguised = false;
	const auto disguise = pThis->DisguiseCreationFrame;
	pThis->DisguiseCreationFrame = Unsorted::CurrentFrame + 1;
	const bool warping = pThis->WarpingOut;
	pThis->WarpingOut = false;
	const auto temporal = pThis->TemporalTargetingMe;
	pThis->TemporalTargetingMe = nullptr;
	const auto cloak = pThis->CloakState;
	pThis->CloakState = CloakState::Uncloaked;
	const float arf = pThis->AngleRotatedForwards;
	pThis->AngleRotatedForwards = 0.0f;
	const float ars = pThis->AngleRotatedSideways;
	pThis->AngleRotatedSideways = 0.0f;

	const bool bridge = pThis->OnBridge;
	pThis->OnBridge = false;
	const bool map = pThis->IsOnMap;
	pThis->IsOnMap = false;
	const auto coords = pThis->Location;
	pThis->Location = CoordStruct { INT_MAX, INT_MAX, INT_MAX };
	const char tube = pThis->TubeIndex;
	pThis->TubeIndex = -1;

	struct LocomotionFaceTemp { DirStruct DesiredFacing; DirStruct ROT; double Speed; };
	const auto iLoco = pThis->Locomotor.GetInterfacePtr();
	const auto pJjLoco = locomotion_cast<JumpjetLocomotionClass*>(iLoco);
	const auto faceTemp = pJjLoco
		? LocomotionFaceTemp(pJjLoco->LocomotionFacing.DesiredFacing, pJjLoco->LocomotionFacing.ROT, pJjLoco->CurrentSpeed)
		: LocomotionFaceTemp();
	if (pJjLoco)
	{
		pJjLoco->LocomotionFacing.DesiredFacing = dir;
		pJjLoco->LocomotionFacing.ROT = DirStruct(0);
		pJjLoco->CurrentSpeed = 0.0;
	}

	const auto flyDes = pThis->PrimaryFacing.DesiredFacing;
	const auto flyROT = pThis->PrimaryFacing.ROT;
	const auto bodyDes = pThis->SecondaryFacing.DesiredFacing;
	const auto bodyROT = pThis->SecondaryFacing.ROT;
	pThis->PrimaryFacing.DesiredFacing = dir;
	pThis->PrimaryFacing.ROT = DirStruct(0);
	pThis->SecondaryFacing.DesiredFacing = dir;
	pThis->SecondaryFacing.ROT = DirStruct(0);

	pThis->DrawIt(pLocation, pBounds);

	pThis->PrimaryFacing.DesiredFacing = flyDes;
	pThis->PrimaryFacing.ROT = flyROT;
	pThis->SecondaryFacing.DesiredFacing = bodyDes;
	pThis->SecondaryFacing.ROT = bodyROT;

	if (pJjLoco)
	{
		pJjLoco->LocomotionFacing.DesiredFacing = faceTemp.DesiredFacing;
		pJjLoco->LocomotionFacing.ROT = faceTemp.ROT;
		pJjLoco->CurrentSpeed = faceTemp.Speed;
	}

	pThis->OnBridge = bridge;
	pThis->IsOnMap = map;
	pThis->Location = coords;
	pThis->TubeIndex = tube;

	pThis->Berzerk = berzerk;
	pExt->AirstrikeTargetingMe = airstrike;
	pThis->IronCurtainTimer.TimeLeft = iron;
	pThis->Flashing.DurationRemaining = flash;
	pThis->Disguised = disguised;
	pThis->DisguiseCreationFrame = disguise;
	pThis->WarpingOut = warping;
	pThis->TemporalTargetingMe = temporal;
	pThis->CloakState = cloak;
	pThis->AngleRotatedForwards = arf;
	pThis->AngleRotatedSideways = ars;

	pType->NoShadow = shadow;
	pType->IsDropship = drop;

	pThis->BeingWarpedOut = warp;

	LightningStorm::Active = lightning;
	PsyDom::Status = psy;
	NukeFlash::Status = nuke;
	TacticalClass::Instance->SelectableCount = select;
}

// Show Cameo：在选中的载具/建筑上方绘制乘员（Passengers）与驻军（Occupier）图标
// 移植自上游 PR #2272，适配本仓库：TechnoTypeExt::Fetch、三参数括号定位函数、CameoPal
void TechnoExt::DrawCameos(TechnoClass* pThis)
{
	if (ObjectClass::CurrentObjects.Count != 1)
		return;

	if (ObjectClass::CurrentObjects.GetItem(0) != pThis)
		return;

	const auto whatAmI = pThis->WhatAmI();

	const auto pType = pThis->GetTechnoType();
	const auto pTypeExt = TechnoTypeExt::Fetch(pType);
	const auto pRulesExt = RulesExt::Global();

	// Per-unit explicitly disabled - skip entirely
	if (pTypeExt->ShowCameo.isset() && !pTypeExt->ShowCameo.Get())
		return;

	// Determine if permanent mode is on (permanent mode bypasses master switch and toggleable)
	const bool permanentMode = pTypeExt->ShowCameo.isset()
		? pTypeExt->ShowCameo.Get()
		: pRulesExt->ShowCameo;

	// Master switch check (skipped for permanent mode)
	if (!permanentMode && !Phobos::Config::ShowCameo_Enable)
		return;

	// Toggleable check (skipped for permanent mode)
	if (!permanentMode && !pRulesExt->ShowCameo_Toggleable)
		return;

	// Collect cameos: passengers + occupants
	// Passengers are stored in LIFO linked list; for buildings we reverse to FIFO
	// so that Y flip produces correct top-to-bottom layout.
	// Occupants are stored in FIFO DynamicVectorClass and do NOT need reversing.
	std::vector<std::pair<TechnoTypeClass*, int>> cameoCounts;

	// Step 1: Collect passengers into a temp vector (LIFO order)
	std::vector<std::pair<TechnoTypeClass*, int>> passengerCounts;
	for (auto pUnit = pThis->Passengers.GetFirstPassenger(); pUnit; pUnit = abstract_cast<FootClass*>(pUnit->NextObject))
	{
		const auto pUnitType = pUnit->GetTechnoType();
		auto it = std::find_if(passengerCounts.begin(), passengerCounts.end(),
			[pUnitType](const auto& pair) { return pair.first == pUnitType; });
		if (it != passengerCounts.end())
			it->second++;
		else
			passengerCounts.emplace_back(pUnitType, 1);
	}

	// For buildings, reverse passenger order: LIFO -> FIFO
	if (whatAmI == AbstractType::Building)
		std::reverse(passengerCounts.begin(), passengerCounts.end());

	// Merge passengers into cameoCounts
	for (const auto& item : passengerCounts)
		cameoCounts.emplace_back(item.first, item.second);

	// Step 2: Collect occupants (FIFO order, no reverse needed)
	if (whatAmI == AbstractType::Building)
	{
		const auto pBuilding = static_cast<BuildingClass*>(pThis);
		for (int i = 0; i < pBuilding->Occupants.Count; i++)
		{
			const auto pOccupant = pBuilding->Occupants.GetItem(i);
			const auto pOccupantType = pOccupant->GetTechnoType();
			auto it = std::find_if(cameoCounts.begin(), cameoCounts.end(),
				[pOccupantType](const auto& pair) { return pair.first == pOccupantType; });
			if (it != cameoCounts.end())
				it->second++;
			else
				cameoCounts.emplace_back(pOccupantType, 1);
		}
	}

	if (cameoCounts.empty())
		return;

	const auto bracketPos = whatAmI == AbstractType::Building
		? GetBuildingSelectBracketPosition(pThis, pType, BuildingSelectBracketPosition::Top)
		: GetFootSelectBracketPosition(pThis, Anchor(HorizontalPosition::Center, VerticalPosition::Top), pThis->WhatAmI() == AbstractType::Infantry);
	Point2D offset = pTypeExt->ShowCameo_BottomOffset.isset()
		? pTypeExt->ShowCameo_BottomOffset.Get()
		: pRulesExt->ShowCameo_BottomOffset.Get();
	Point2D basePos = bracketPos;
	basePos.X += offset.X;
	basePos.Y += offset.Y + pType->PixelSelectionBracketDelta;

	const int perRow = pTypeExt->ShowCameo_PerRow.isset()
		? pTypeExt->ShowCameo_PerRow.Get()
		: pRulesExt->ShowCameo_PerRow.Get();
	const int count = static_cast<int>(cameoCounts.size());
	const int rows = (count + perRow - 1) / perRow;

	// Get overlap/offset/overlapPrimary settings
	const Point2D overlapXY = pTypeExt->ShowCameo_OverlapXY.isset()
		? pTypeExt->ShowCameo_OverlapXY.Get()
		: pRulesExt->ShowCameo_OverlapXY.Get();
	const Point2D cameoOffsetXY = pTypeExt->ShowCameo_OffsetXY.isset()
		? pTypeExt->ShowCameo_OffsetXY.Get()
		: pRulesExt->ShowCameo_OffsetXY.Get();
	const bool overlapPrimary = pTypeExt->ShowCameo_OverlapPrimary.isset()
		? pTypeExt->ShowCameo_OverlapPrimary.Get()
		: pRulesExt->ShowCameo_OverlapPrimary.Get();
	// overlapPrimary=true: primary (earlier) covers secondary (later)
	// overlapPrimary=false: secondary (later) covers primary (earlier)

	// Pre-compute canvas sizes for each cameo (standard 60x48, or custom SHP dimensions)
	struct CameoCanvas { int width; int height; };
	std::vector<CameoCanvas> canvases;
	canvases.reserve(count);
	for (const auto& item : cameoCounts)
	{
		const auto pCameoType = item.first;
		const auto pCameoTypeExt = TechnoTypeExt::Fetch(pCameoType);
		int w = 60, h = 48;
		if (SHPStruct* pCustomCameo = pCameoTypeExt->ShowCameo_CustomShape)
		{
			w = pCustomCameo->Width;
			h = pCustomCameo->Height;
		}
		else if (auto pCustomPCX = pCameoTypeExt->ShowCameo_CustomPCX.GetSurface())
		{
			w = pCustomPCX->GetWidth();
			h = pCustomPCX->GetHeight();
		}
		canvases.push_back({ w, h });
	}

	// Step 1-2-3: Calculate positions sequentially (row layout + OverlapXY + OffsetXY)
	// Overlap formula: overlap = coveredCanvasSize * (OverlapXY% / 100) + OffsetXY
	// OverlapPrimary=true: overlap based on secondary cameo's size (secondary is covered)
	// OverlapPrimary=false: overlap based on primary cameo's size (primary is covered)
	// Base gap between adjacent icons: 2 pixels (hGap / vGap)
	const int hGap = 2;
	const int vGap = 2;
	// Iterate from bottom row to top row, so that each row can reference the row below it
	std::vector<Point2D> cameoPositions(count);
	for (int row = 0; row < rows; row++)
	{
		const int firstIdx = row * perRow;
		const int lastIdx = Math::min(firstIdx + perRow - 1, count - 1);
		for (int idx = firstIdx; idx <= lastIdx; idx++)
		{
			const int col = idx - firstIdx;
			const int curW = canvases[idx].width;
			const int curH = canvases[idx].height;

			if (row == 0)
			{
				// Bottom row: anchor at base position (bottom edge fixed at basePos.Y)
				if (col == 0)
				{
					cameoPositions[idx].X = basePos.X;
					cameoPositions[idx].Y = basePos.Y - curH;
				}
				else
				{
					// Same row: position to the right of the cameo directly to the left
					const int prevIdx = idx - 1;
					const int prevX = cameoPositions[prevIdx].X;
					const int prevW = canvases[prevIdx].width;

					const int overlapX = overlapPrimary
						? (curW * overlapXY.X / 100 + cameoOffsetXY.X)
						: (prevW * overlapXY.X / 100 + cameoOffsetXY.X);

					cameoPositions[idx].X = prevX + prevW + hGap - overlapX;
					cameoPositions[idx].Y = cameoPositions[prevIdx].Y;
				}
			}
			else
			{
				if (col == 0)
				{
					// First in row: position above the cameo directly below
					const int belowIdx = (row - 1) * perRow;
					const int belowY = cameoPositions[belowIdx].Y;
					const int belowH = canvases[belowIdx].height;

					const int overlapY = overlapPrimary
						? (curH * overlapXY.Y / 100 + cameoOffsetXY.Y)
						: (belowH * overlapXY.Y / 100 + cameoOffsetXY.Y);

					cameoPositions[idx].X = basePos.X;
					cameoPositions[idx].Y = belowY - curH - vGap + overlapY;
				}
				else
				{
					// Same row: position to the right of the cameo directly to the left
					const int prevIdx = idx - 1;
					const int prevX = cameoPositions[prevIdx].X;
					const int prevW = canvases[prevIdx].width;

					const int overlapX = overlapPrimary
						? (curW * overlapXY.X / 100 + cameoOffsetXY.X)
						: (prevW * overlapXY.X / 100 + cameoOffsetXY.X);

					cameoPositions[idx].X = prevX + prevW + hGap - overlapX;
					cameoPositions[idx].Y = cameoPositions[prevIdx].Y;
				}
			}
		}
	}

	// Step 4: Center each row horizontally
	for (int row = 0; row < rows; row++)
	{
		const int firstIdx = row * perRow;
		const int lastIdx = Math::min(firstIdx + perRow - 1, count - 1);
		const int rowWidth = cameoPositions[lastIdx].X + canvases[lastIdx].width - cameoPositions[firstIdx].X;
		const int centerOffset = basePos.X - rowWidth / 2 - cameoPositions[firstIdx].X;
		for (int i = firstIdx; i <= lastIdx; i++)
			cameoPositions[i].X += centerOffset;
	}

	// For buildings, flip Y axis: first-entered goes to top, last-entered to bottom
	if (whatAmI == AbstractType::Building)
	{
		int topY = INT_MAX;
		for (int i = 0; i < count; i++)
			topY = Math::min(topY, cameoPositions[i].Y);

		for (int i = 0; i < count; i++)
			cameoPositions[i].Y = basePos.Y - canvases[i].height - (cameoPositions[i].Y - topY);
	}

	const auto pSurface = DSurface::Composite;
	RectangleStruct bounds = DSurface::Composite->GetRect();

	// Determine BlitterFlags for translucency
	const TranslucencyLevel translucency = pTypeExt->ShowCameo_Translucency.isset()
		? pTypeExt->ShowCameo_Translucency.Get()
		: pRulesExt->ShowCameo_Translucency;
	const BlitterFlags translucencyFlag = translucency.GetBlitterFlags();

	// For buildings, draw in reverse so that visual top (earlier-entered) covers visual bottom.
	// This is needed because Y flip reverses the visual order but overlap calculation
	// still follows the original cameoCounts (bottom-to-top) order.
	int idx = (whatAmI == AbstractType::Building) ? count - 1 : 0;
	const int idxEnd = (whatAmI == AbstractType::Building) ? -1 : count;
	const int idxStep = (whatAmI == AbstractType::Building) ? -1 : 1;
	for (; idx != idxEnd; idx += idxStep)
	{
		const auto& item = cameoCounts[idx];
		const auto pCameoType = item.first;
		const int cameoCount = item.second;
		const int curW = canvases[idx].width;
		const int curH = canvases[idx].height;

		Point2D iconPos = cameoPositions[idx];

		// Skip icons whose any part exceeds the screen top boundary (Y < 0) to prevent crash
		// Only the top boundary matters because cameos are always above the selection bracket
		if (iconPos.Y < 0)
			continue;

		bool drawn = false;

		const auto pCameoTypeExt = TechnoTypeExt::Fetch(pCameoType);

		// Use custom ShowCameo SHP if specified
		if (SHPStruct* pCustomCameo = pCameoTypeExt->ShowCameo_CustomShape)
		{
			const auto pCameoConvert = pCameoTypeExt->CameoPal.GetOrDefaultConvert(FileSystem::CAMEO_PAL);
			const auto pConvert = pCameoTypeExt->ShowCameo_CustomPalette.GetOrDefaultConvert(pCameoConvert);
			const int frameCount = pCustomCameo->Frames > 0 ? pCustomCameo->Frames : 1;
			const int frame = (Unsorted::CurrentFrame % frameCount);
			pSurface->DrawSHP(
				pConvert,
				pCustomCameo,
				frame,
				&iconPos,
				&bounds,
				BlitterFlags::bf_400 | BlitterFlags::Alpha | translucencyFlag,
				0, 0,
				ZGradient::Ground,
				1000, 0, nullptr, 0, 0, 0
			);
			drawn = true;
		}

		if (!drawn)
		{
			if (const auto pCustomPCX = pCameoTypeExt->ShowCameo_CustomPCX.GetSurface())
			{
				RectangleStruct pcxBounds = { iconPos.X, iconPos.Y, curW, curH };
				PCX::Instance.BlitToSurface(&pcxBounds, pSurface, pCustomPCX);
				drawn = true;
			}
		}

		if (!drawn)
		{
			if (const auto pCameoPCX = pCameoTypeExt->CameoPCX.GetSurface())
			{
				RectangleStruct pcxBounds = { iconPos.X, iconPos.Y, curW, curH };
				PCX::Instance.BlitToSurface(&pcxBounds, pSurface, pCameoPCX);
				drawn = true;
			}
		}

		if (!drawn)
		{
			SHPStruct* pCameo = pCameoType->GetCameo();
			if (pCameo)
			{
				const auto pConvert = pCameoTypeExt->CameoPal.GetOrDefaultConvert(FileSystem::CAMEO_PAL);
				pSurface->DrawSHP(
					pConvert,
					pCameo,
					0,
					&iconPos,
					&bounds,
					BlitterFlags::bf_400 | BlitterFlags::Alpha | translucencyFlag,
					0, 0,
					ZGradient::Ground,
					1000, 0, nullptr, 0, 0, 0
				);
				drawn = true;
			}
		}

		if (drawn)
		{
			wchar_t countText[16];
			_snwprintf_s(countText, _countof(countText), _TRUNCATE, L"%d", cameoCount);

			Point2D textPos = { iconPos.X + curW - 4, iconPos.Y + 2 };

			pSurface->DrawTextA(
				countText,
				&bounds,
				&textPos,
				Drawing::RGB_To_Int({ 0, 255, 0 }),
				0,
				TextPrintType::Center | TextPrintType::FullShadow
			);
		}
	}
}
