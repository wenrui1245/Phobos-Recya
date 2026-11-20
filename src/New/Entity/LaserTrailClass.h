#pragma once

#include <New/Type/LaserTrailTypeClass.h>

class LaserTrailClass
{
public:
	LaserTrailTypeClass* Type;
	bool Visible;
	bool Cloaked;
	CoordStruct FLH;
	bool IsOnTurret;
	ColorStruct CurrentColor;
	Nullable<CoordStruct> LastLocation;
	bool Intrinsic;
	HouseClass* Owner;
	// 仅动画尾焰使用：上一帧位置与移动状态，用于区分起步/行进/刹车动画（瞬态，不存档）
	Nullable<CoordStruct> LastFrameLocation;
	bool IsMoving;

	LaserTrailClass(LaserTrailTypeClass* pTrailType, HouseClass* pHouse = nullptr,
		CoordStruct flh = { 0, 0, 0 }, bool isOnTurret = false) :
		Type { pTrailType }
		, Visible { true }
		, Cloaked { false }
		, FLH { flh }
		, IsOnTurret { isOnTurret }
		, CurrentColor { pTrailType->Color }
		, LastLocation {}
		, Intrinsic { true }
		, Owner { pHouse }
		, LastFrameLocation {}
		, IsMoving { false }
	{
		if (this->Type->IsHouseColor && pHouse)
			this->CurrentColor = pHouse->LaserColor;
	}

	LaserTrailClass() :
		Type {},
		Visible {},
		Cloaked {},
		FLH {},
		IsOnTurret {},
		CurrentColor {},
		LastLocation {},
		Intrinsic {},
		Owner {},
		LastFrameLocation {},
		IsMoving { false }
	{ }

	bool Update(CoordStruct location);

	bool Load(PhobosStreamReader& stm, bool registerForChange);
	bool Save(PhobosStreamWriter& stm) const;

private:
	bool DrawAnim(std::vector<AnimTypeClass*> const& animTypes, CoordStruct behindPos, CoordStruct currentPos);

	template <typename T>
	bool Serialize(T& stm);
};
