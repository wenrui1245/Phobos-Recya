#include "AttachmentClass.h"

#include <Dir.h>
#include <BulletClass.h>
#include <BulletTypeClass.h>
#include <WarheadTypeClass.h>

#include <ObjBase.h>

#include <Ext/Techno/Body.h>
#include <Locomotion/AttachmentLocomotionClass.h>

std::vector<AttachmentClass*> AttachmentClass::Array;

void AttachmentClass::TransformChildFace(const DirStruct& dir, int time)
{
	this->StartDir = this->GetChildFace();
	this->DesiredDir = dir;
	this->TransDirTimer.Start(time);
}

DirStruct AttachmentClass::GetChildFace() const
{
	if (this->TransDirTimer.TimeLeft && this->TransDirTimer.HasTimeLeft())
	{
		const short diff = static_cast<short>(static_cast<short>(this->DesiredDir.Raw) - static_cast<short>(this->StartDir.Raw));
		return DirStruct { (this->DesiredDir.Raw - diff * this->TransDirTimer.GetTimeLeft() / this->TransDirTimer.TimeLeft) };
	}

	return this->DesiredDir;
}

DirStruct AttachmentClass::GetChildDirection() const
{
	const auto pParent = this->Parent;
	DirStruct childDir = this->GetType()->IsOnTurret ? pParent->SecondaryFacing.Current() : pParent->PrimaryFacing.Current();

	return DirStruct { (childDir.Raw + this->GetChildFace().Raw) };
}

void AttachmentClass::TransformChildFLH(const CoordStruct& flh, int time)
{
	this->StartFLH = this->GetChildFLH();
	this->DesiredFLH = flh;
	this->TransFLHTimer.Start(time);
}

CoordStruct AttachmentClass::GetChildFLH() const
{
	if (this->TransFLHTimer.TimeLeft && this->TransFLHTimer.HasTimeLeft())
	{
		const double ratio = static_cast<double>(this->TransFLHTimer.GetTimeLeft()) / this->TransFLHTimer.TimeLeft;
		return (this->DesiredFLH - (this->DesiredFLH - this->StartFLH) * ratio);
	}

	return this->DesiredFLH;
}

CoordStruct AttachmentClass::GetChildLocation() const
{
	return TechnoExt::GetFLHAbsoluteCoords(this->Parent, this->GetChildFLH(), this->GetType()->IsOnTurret);
}

AttachmentClass::~AttachmentClass()
{
	if (auto* registry = AttachmentRuntime::Lifetimes<AttachmentClass>()) registry->Remove(this);
	// clean up non-owning references
	if (this->Child)
	{
		auto const pChildExt = TechnoExt::Fetch(this->Child);
		pChildExt->ParentAttachment = nullptr;
	}

	auto position = std::find(AttachmentClass::Array.begin(), AttachmentClass::Array.end(), this);

	if (position != AttachmentClass::Array.end())
		AttachmentClass::Array.erase(position);
}

void AttachmentClass::Initialize()
{
	if (this->Child)
		return;

	if (this->GetType()->RespawnAtCreation)
		this->CreateChild();
}

void AttachmentClass::CreateChild()
{
	if (!this->Parent)
		return;
	if (auto const pChildType = this->GetChildType())
	{
		if (pChildType->WhatAmI() != AbstractType::UnitType)
			return;

		if (const auto pTechno = static_cast<TechnoClass*>(pChildType->CreateObject(this->Parent->Owner)))
		{
			this->AttachChild(pTechno);
		}
		else
		{
			Debug::Log("[" __FUNCTION__ "] Failed to create child %s of parent %s!\n",
				pChildType->ID, this->Parent->GetTechnoType()->ID);
		}
	}
}

void AttachmentClass::UpdateAll()
{
 if (auto* registry = AttachmentRuntime::Lifetimes<AttachmentClass>())
  registry->Run(Array, [](AttachmentClass* attachment) { attachment->AI(); });
}

void AttachmentClass::AI()
{
 // Removed parents may stay in their sinking/death phase before their extension
 // is destroyed. Settle the configured child outcome once; never respawn here.
 if (!this->Parent)
 {
  this->RespawnTimer.Stop();
  this->Destroy(nullptr);
  return;
 }

	AttachmentTypeClass* pType = this->GetType();

	if (!this->Child)
	{
		if (pType->RespawnDelay == 0)
		{
			this->CreateChild();
		}
		else if (pType->RespawnDelay > 0)
		{
			if (!this->RespawnTimer.HasStarted())
			{
				this->RespawnTimer.Start(pType->RespawnDelay);
			}
			else if (this->RespawnTimer.Completed())
			{
				this->CreateChild();
				this->RespawnTimer.Stop();
			}
		}
	}

	if (const auto pChild = this->Child)
	{
		const auto pParent = this->Parent;

		if (pChild->InLimbo && !pParent->InLimbo)
			this->Unlimbo();
		else if (!pChild->InLimbo && pParent->InLimbo)
			this->Limbo();

		pChild->SetLocation(this->GetChildLocation());
		pChild->PrimaryFacing.SetCurrent(this->GetChildDirection());
		// TODO handle secondary facing in case the turret is idle

		FootClass* pParentAsFoot = abstract_cast<FootClass*, true>(pParent);
		FootClass* pChildAsFoot = abstract_cast<FootClass*, true>(pChild);

		if (pParentAsFoot && pChildAsFoot)
			pChildAsFoot->TubeIndex = pParentAsFoot->TubeIndex;

		if (pType->InheritTarget)
		{
			auto childInheritTarget = [pParent, pChild, pType]()
				{
					const auto pTarget = pParent->Target;

					if (pType->InheritTarget_Force)
					{
						if (pChild->Target != pTarget)
						{
							pChild->SetTarget(pTarget);
							pChild->QueueMission((pTarget ? Mission::Attack : Mission::Guard), true);
						}

						return;
					}

					if (pChild->GetCurrentMission() != Mission::Guard)
						pChild->QueueMission(Mission::Guard, true);

					const auto pChildTarget = pChild->Target;

					if (pTarget && pChildTarget != pTarget && pChild->IsCloseEnoughToAttack(pTarget))
					{
						const auto fireError = pChild->GetFireErrorWithoutRange(pTarget, pChild->SelectWeapon(pTarget));

						if (fireError != FireError::CANT && fireError != FireError::ILLEGAL)
						{
							pChild->SetTarget(pTarget);
							return;
						}
					}

					if (pChildTarget && !pChild->IsCloseEnoughToAttack(pChildTarget))
						pChild->SetTarget(nullptr);
				};
			childInheritTarget();
		}

		if (pType->InheritStateEffects)
		{
			pChild->IsFallingDown = pParent->IsFallingDown;
			pChild->WasFallingDown = pParent->WasFallingDown;
			pChild->CloakState = pParent->CloakState;
			pChild->WarpingOut = pParent->WarpingOut;
			pChild->unknown_280 = pParent->unknown_280; // sth related to teleport
			pChild->BeingWarpedOut = pParent->BeingWarpedOut;
			pChild->Deactivated = pParent->Deactivated;
			pChild->Flash(pParent->Flashing.DurationRemaining);

			pChild->IronCurtainTimer = pParent->IronCurtainTimer;
			pChild->IdleActionTimer = pParent->IdleActionTimer;
			pChild->IronTintTimer = pParent->IronTintTimer;
			pChild->CloakDelayTimer = pParent->CloakDelayTimer;
			pChild->ChronoLockRemaining = pParent->ChronoLockRemaining;
			pChild->Berzerk = pParent->Berzerk;
			pChild->BerzerkDurationLeft = pParent->BerzerkDurationLeft;
			pChild->ChronoWarpedByHouse = pParent->ChronoWarpedByHouse;
			pChild->EMPLockRemaining = pParent->EMPLockRemaining;
			pChild->ShouldLoseTargetNow = pParent->ShouldLoseTargetNow;
		}

		if (pType->InheritOwner)
			pChild->SetOwningHouse(pParent->GetOwningHouse(), false);
	}
}

// Called in Kill_Cargo, handles logics for parent destruction on children
void AttachmentClass::Destroy(TechnoClass* pSource)
{
 if (!this->Child)
  return;
 auto* child = this->Child;
 auto* type = this->GetType();
 const auto weapon = type->DestructionWeapon_Child.Get();
 const bool inherit = type->InheritDestruction;
 const auto mission = type->ParentDestructionMission.Get();
 auto* ext = TechnoExt::Fetch(child);
 if (ext->ParentAttachment == this)
  ext->ParentAttachment = nullptr;
 this->Child = nullptr;
 // Everything needed is on the stack before callbacks. Either object, including
 // the attachment owner, may disappear while a destruction weapon executes.
 AttachmentRuntime::Watch<TechnoClass> target(child), source(pSource);
 if (weapon)
  TechnoExt::FireWeaponAtSelf(target.Get(), weapon);
 if (inherit && target.Get())
  TechnoExt::Kill(target.Get(), source.Get());
 else if (target.Get() && !target.Get()->InLimbo && mission != Mission::None)
  target.Get()->QueueMission(mission, false);
}

void AttachmentClass::UnInit()
{
	if (const auto pChild = this->Child)
	{
		auto const pChildExt = TechnoExt::Fetch(pChild);
		pChildExt->ParentAttachment = nullptr;

		pChild->KillPassengers(nullptr);
		pChild->Limbo();
		pChild->UnInit();

		this->Child = nullptr;
	}
}

void AttachmentClass::ChildDestroyed()
{
	if (this->Child)
	{
		if (auto const pChildExt = TechnoExt::Fetch(this->Child))
			pChildExt->ParentAttachment = nullptr;

		AttachmentTypeClass* pType = this->GetType();

		this->Child = nullptr;

		if (const auto pWeapon = this->Parent ? pType->DestructionWeapon_Parent.Get() : nullptr)
			TechnoExt::FireWeaponAtSelf(this->Parent, pWeapon);
	}
}

void AttachmentClass::Unlimbo()
{
	if (this->Child && this->Parent)
	{
		++Unsorted::ScenarioInit;
		this->Child->Unlimbo(this->GetChildLocation(), this->GetChildDirection().GetDir());
		--Unsorted::ScenarioInit;
	}
}

void AttachmentClass::Limbo()
{
	if (this->Child)
		this->Child->Limbo();
}

bool AttachmentClass::AttachChild(TechnoClass* pChild)
{
	if (this->Child)
		return false;

	if (pChild->WhatAmI() != AbstractType::Unit)
		return false;

	if (auto const pChildAsFoot = abstract_cast<FootClass*, true>(pChild))
	{
		if (IPersistPtr pLocoPersist = pChildAsFoot->Locomotor)
		{
			CLSID locoCLSID { };

			if (SUCCEEDED(pLocoPersist->GetClassID(&locoCLSID)) && locoCLSID != __uuidof(AttachmentLocomotionClass))
				LocomotionClass::ChangeLocomotorTo(pChildAsFoot, __uuidof(AttachmentLocomotionClass));
		}
	}

	this->Child = pChild;

	auto pChildExt = TechnoExt::Fetch(pChild);
	pChildExt->ParentAttachment = this;

	// bandaid for jitterless drawing. TODO fix properly
	// pChild->GetTechnoType()->DisableVoxelCache = true;
	// pChild->GetTechnoType()->DisableShadowCache = true;

	AttachmentTypeClass* pType = this->GetType();

	if (pType->InheritOwner)
	{
		if (auto pController = pChild->MindControlledBy)
			pController->CaptureManager->FreeUnit(pChild);
	}

	return true;
}

bool AttachmentClass::DetachChild()
{
	if (this->Child)
	{
		AttachmentTypeClass* pType = this->GetType();

		if (!this->Child->InLimbo && pType->ParentDetachmentMission != Mission::None)
			this->Child->QueueMission(pType->ParentDetachmentMission, false);

		// FIXME this won't work probably
		if (pType->InheritOwner)
			this->Child->SetOwningHouse(this->Parent->GetOriginalOwner(), false);

		// remove the attachment locomotor manually just to be safe
		if (auto const pChildAsFoot = abstract_cast<FootClass*, true>(this->Child))
			LocomotionClass::End_Piggyback(pChildAsFoot->Locomotor);

		auto pChildExt = TechnoExt::Fetch(this->Child);
		pChildExt->ParentAttachment = nullptr;
		this->Child = nullptr;

		return true;
	}

	return false;
}


void AttachmentClass::PointerGotInvalid(void* ptr, bool removed)
{
	if (!removed)
		return;

	AttachmentRuntime::Watch<TechnoClass>::Invalidate(ptr);
	for (const auto pAttachment : AttachmentClass::Array)
		pAttachment->InvalidatePointer(ptr);
}

void AttachmentClass::InvalidatePointer(void* ptr)
{
	if (this->Parent == ptr)
 {
  this->RespawnTimer.Stop();
  if (this->Child)
  {
   auto* ext = TechnoExt::TryFetch(this->Child);
   if (ext && ext->ParentAttachment == this)
    ext->ParentAttachment = nullptr;
  }
 }
 AnnounceInvalidPointer(this->Parent, ptr);
	AnnounceInvalidPointer(this->Child, ptr);
}

#pragma region Save/Load

template <typename T>
bool AttachmentClass::Serialize(T& stm)
{
	return stm
		.Process(this->Type)
		.Process(this->Parent)
		.Process(this->Child)
		.Process(this->RespawnTimer)
		.Process(this->StartFLH)
		.Process(this->DesiredFLH)
		.Process(this->TransFLHTimer)
		.Process(this->StartDir)
		.Process(this->DesiredDir)
		.Process(this->TransDirTimer)
		.Success();
}

bool AttachmentClass::Load(PhobosStreamReader& stm, bool RegisterForChange)
{
	return Serialize(stm);
}

bool AttachmentClass::Save(PhobosStreamWriter& stm) const
{
	return const_cast<AttachmentClass*>(this)->Serialize(stm);
}

#pragma endregion

void AttachmentTransformGroup::Trasform(AttachmentClass* pAttachment, const std::vector<AttachmentTransformGroup>& trasformPairs, HouseClass* pOwner)
{
	for (const auto& [types, affectedHouses, flhTime, flh, rotationTime, rotation] : trasformPairs)
	{
		if (pOwner && !EnumFunctions::CanTargetHouse(affectedHouses, pOwner, pAttachment->Parent->Owner))
			continue;

		for (const auto& type : types)
		{
			if (type != pAttachment->Type)
				continue;

			if (flhTime >= 0)
				pAttachment->TransformChildFLH(flh, flhTime);

			if (rotationTime >= 0)
				pAttachment->TransformChildFace(DirStruct(rotation), rotationTime);

			return;
		}
	}

	return;
}

bool AttachmentTransformGroup::Load(PhobosStreamReader& stm, bool registerForChange)
{
	return this->Serialize(stm);
}

bool AttachmentTransformGroup::Save(PhobosStreamWriter& stm) const
{
	return const_cast<AttachmentTransformGroup*>(this)->Serialize(stm);
}

void AttachmentTransformGroup::Parse(std::vector<AttachmentTransformGroup>& list, INI_EX& exINI, const char* pSection, AffectedHouse defaultAffectHouse)
{
	for (size_t i = 0; ; ++i)
	{
		char tempBuffer[48];
		ValueableVector<AttachmentTypeClass*> types;
		Nullable<AffectedHouse> affectedHouses;
		Valueable<int> flhTime;
		Valueable<CoordStruct> flh;
		Valueable<int> rotationTime;
		Valueable<DirType> rotation;
		_snprintf_s(tempBuffer, sizeof(tempBuffer), "AttachmentTransform%d.Types", i);
		types.Read(exINI, pSection, tempBuffer);
		_snprintf_s(tempBuffer, sizeof(tempBuffer), "AttachmentTransform%d.AffectedHouses", i);
		affectedHouses.Read(exINI, pSection, tempBuffer);
		_snprintf_s(tempBuffer, sizeof(tempBuffer), "AttachmentTransform%d.FLHTime", i);
		flhTime.Read(exINI, pSection, tempBuffer);
		_snprintf_s(tempBuffer, sizeof(tempBuffer), "AttachmentTransform%d.FLH", i);
		flh.Read(exINI, pSection, tempBuffer);
		_snprintf_s(tempBuffer, sizeof(tempBuffer), "AttachmentTransform%d.RotationTime", i);
		rotationTime.Read(exINI, pSection, tempBuffer);
		_snprintf_s(tempBuffer, sizeof(tempBuffer), "AttachmentTransform%d.Rotation", i);
		rotation.Read(exINI, pSection, tempBuffer);

		if (types.empty() || flhTime < 0 && rotationTime < 0)
			break;

		if (!affectedHouses.isset())
			affectedHouses = defaultAffectHouse;

		list.emplace_back(types, affectedHouses, flhTime, flh, rotationTime, rotation);
	}
	ValueableVector<AttachmentTypeClass*> types;
	Nullable<AffectedHouse> affectedHouses;
	Valueable<int> flhTime;
	Valueable<CoordStruct> flh;
	Valueable<int> rotationTime;
	Valueable<DirType> rotation;
	types.Read(exINI, pSection, "AttachmentTransform.Types");
	affectedHouses.Read(exINI, pSection, "AttachmentTransform.AffectedHouses");
	flhTime.Read(exINI, pSection, "AttachmentTransform.FLHTime");
	flh.Read(exINI, pSection, "AttachmentTransform.FLH");
	rotationTime.Read(exINI, pSection, "AttachmentTransform.RotationTime");
	rotation.Read(exINI, pSection, "AttachmentTransform.Rotation");
	if (!types.empty() && (flhTime >= 0 || rotationTime >= 0))
	{
		if (!affectedHouses.isset())
			affectedHouses = defaultAffectHouse;

		if (list.size())
			list[0] = { types, affectedHouses, flhTime, flh, rotationTime, rotation };
		else
			list.emplace_back(types, affectedHouses, flhTime, flh, rotationTime, rotation);
	}
}

template <typename T>
bool AttachmentTransformGroup::Serialize(T& stm)
{
	return stm
		.Process(this->Types)
		.Process(this->AffectedHouses)
		.Process(this->FLHTime)
		.Process(this->FLH)
		.Process(this->RotationTime)
		.Process(this->Rotation)
		.Success();
}
