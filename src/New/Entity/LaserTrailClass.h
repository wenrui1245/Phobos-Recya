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
	// 仅动画尾焰使用：记录上一次判定的移动状态，用于起步/刹车动画只触发一次（瞬态，不存档）
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
		IsMoving { false }
	{ }

	bool Update(CoordStruct location);
	bool SpawnAnim(std::vector<AnimTypeClass*> const& animTypes, CoordStruct behindPos, CoordStruct currentPos);

	bool Load(PhobosStreamReader& stm, bool registerForChange);
	bool Save(PhobosStreamWriter& stm) const;

private:
	template <typename T>
	bool Serialize(T& stm);
};
