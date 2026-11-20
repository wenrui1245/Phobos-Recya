#include "LaserTrailClass.h"

#include <AnimClass.h>
#include <ScenarioClass.h>
#include <Dir.h>
#include <YRMath.h>

#include <Ext/EBolt/Body.h>

// Draws LaserTrail if the conditions are suitable.
// Returns true if drawn, false otherwise.
bool LaserTrailClass::Update(CoordStruct location)
{
	if (Phobos::Config::HideLaserTrailEffects && this->Type->IsHideable)
		return false;

	auto const pType = this->Type;
	bool result = false;

	// 动画尾焰依靠帧间位移区分起步、行进与刹车三种状态
	if (pType->DrawType == LaserTrailDrawType::Anim && this->LastFrameLocation.isset())
	{
		CoordStruct const lastFrame = this->LastFrameLocation.Get();
		bool const inMotion = location.DistanceFromSquared(lastFrame) > 1.0;

		if (this->Visible && !this->Cloaked)
		{
			if (inMotion && !this->IsMoving)
				result |= this->DrawAnim(pType->Anim_Start, lastFrame, location);
			else if (!inMotion && this->IsMoving)
				result |= this->DrawAnim(pType->Anim_Stop, lastFrame, location);
		}

		this->IsMoving = inMotion;
	}

	if (pType->DrawType == LaserTrailDrawType::Anim)
		this->LastFrameLocation = location;

	const double segmentLength = (double)pType->SegmentLength;

	if (!this->LastLocation.isset())
	{
		// The trail was just inited
		this->LastLocation = location;
	}
	else if (location.DistanceFromSquared(this->LastLocation.Get()) > static_cast<double>(segmentLength) * segmentLength) // TODO reimplement IgnoreVertical properly?
	{
		// We spawn new laser segment if the distance is long enough, the game will do the rest - Kerbiter
		if (this->Visible && !this->Cloaked && (pType->IgnoreVertical ? (abs(location.X - this->LastLocation.Get().X) > 16 || abs(location.Y - this->LastLocation.Get().Y) > 16) : true))
		{
			if (pType->DrawType == LaserTrailDrawType::Anim)
			{
				this->DrawAnim(pType->Anim_While, this->LastLocation.Get(), location);
			}
			else if (pType->DrawType == LaserTrailDrawType::Laser)
			{
				const auto pLaser = GameCreate<LaserDrawClass>(
					this->LastLocation.Get(), location,
					this->CurrentColor, ColorStruct { 0, 0, 0 }, ColorStruct { 0, 0, 0 },
					pType->FadeDuration.Get(64));

				pLaser->Thickness = pType->Thickness;
				pLaser->IsHouseColor = true;
				pLaser->IsSupported = pType->IsIntense;
			}
			else if (pType->DrawType == LaserTrailDrawType::EBolt)
			{
				const auto pBolt = GameCreate<EBolt>();
				const auto pBoltExt = EBoltExt::ExtMap.Find(pBolt);
				const auto& boltDisable = pType->Bolt_Disable;
				const auto& boltColor = pType->Bolt_Color;

				const int alternateIdx = pType->IsAlternateColor ? 5 : 10;
				const int defaultAlternate = EBoltExt::GetDefaultColor_Int(FileSystem::PALETTE_PAL, alternateIdx);
				const int defaultWhite = EBoltExt::GetDefaultColor_Int(FileSystem::PALETTE_PAL, 15);

				for (int idx = 0; idx < 3; ++idx)
				{
					if (boltDisable[idx])
						pBoltExt->Disable[idx] = true;
					else if (boltColor[idx].isset())
						pBoltExt->Color[idx] = boltColor[idx].Get();
					else
						pBoltExt->Color[idx] = Drawing::Int_To_RGB(idx < 2 ? defaultAlternate : defaultWhite);
				}

				pBoltExt->Arcs = pType->Bolt_Arcs;
				pBolt->Lifetime = 1 << (std::clamp(pType->FadeDuration.Get(17), 1, 31) - 1);
				pBolt->AlternateColor = pType->IsAlternateColor;

				pBolt->Fire(this->LastLocation, location, pType->Bolt_ZAdjust);
			}
			else if (pType->DrawType == LaserTrailDrawType::RadBeam)
			{
				const auto pRadBeam = RadBeam::Allocate(RadBeamType::RadBeam);
				pRadBeam->SetCoordsSource(this->LastLocation);
				pRadBeam->SetCoordsTarget(location);
				pRadBeam->Period = pType->FadeDuration.Get(15);
				pRadBeam->Amplitude = pType->Beam_Amplitude;

				const ColorStruct beamColor = pType->Beam_Color.Get(RulesClass::Instance->RadColor);
				pRadBeam->SetColor(beamColor);
			}

			result = true;
		}

		this->LastLocation = location;
	}

	return result;
}

// 生成一段动画尾焰，返回是否成功创建动画。
bool LaserTrailClass::DrawAnim(std::vector<AnimTypeClass*> const& animTypes, CoordStruct behindPos, CoordStruct currentPos)
{
	auto const count = animTypes.size();

	if (count == 0)
		return false;

	size_t index = 0;

	if (count > 1)
	{
		if (count % 8 == 0)
		{
			// 与 KratosPP 行为一致：帧数为 8 的倍数时按水平朝向选帧，每 8 帧为一组
			DirStruct const dir { Math::atan2(static_cast<double>(currentPos.Y - behindPos.Y),
				static_cast<double>(currentPos.X - behindPos.X)) };
			index = (dir.GetFacing<8>() + count / 8) % count;
		}
		else
		{
			index = static_cast<size_t>(ScenarioClass::Instance->Random.RandomRanged(0, static_cast<int>(count) - 1));
		}
	}

	AnimTypeClass* const pAnimType = animTypes[index];

	if (!pAnimType)
		return false;

	AnimClass* const pAnim = GameCreate<AnimClass>(pAnimType, currentPos);

	if (!pAnim)
		return false;

	// 动画归属尾焰所属方，保证所属色重映射与正常生成的动画一致
	if (this->Owner)
		pAnim->Owner = this->Owner;

	return true;
}

#pragma region Save/Load

template <typename T>
bool LaserTrailClass::Serialize(T& stm)
{
	return stm
		.Process(this->Type)
		.Process(this->Visible)
		.Process(this->Cloaked)
		.Process(this->FLH)
		.Process(this->IsOnTurret)
		.Process(this->CurrentColor)
		.Process(this->LastLocation)
		.Process(this->Intrinsic)
		.Process(this->Owner)
		.Success();
};

bool LaserTrailClass::Load(PhobosStreamReader& stm, bool RegisterForChange)
{
	return Serialize(stm);
}

bool LaserTrailClass::Save(PhobosStreamWriter& stm) const
{
	return const_cast<LaserTrailClass*>(this)->Serialize(stm);
}

#pragma endregion
