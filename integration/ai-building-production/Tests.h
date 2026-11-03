inline int Checks = 0;
inline void Check(bool ok, const char* name)
{
	if (!ok) throw std::runtime_error(name);
	++Checks;
}
struct Fixture
{
	BuildingTypeClass Type;
	HouseClass House;
	HouseExt Ext {&House};
	Fixture()
	{
		BuildingTypeClass::Array = {};
		BuildingTypeClass::Array.Add(&Type);
		House.ConYards.Add(1);
		House.Base.BaseNodes.Add({0, {10, 20}});
		Unsorted::CurrentFrame = 0;
		DemandSucceeds = PlaceSucceeds = PlaceDeletesFactory = true;
		PlaceCalls = 0;
		NativeFlag = 1;
	}
	~Fixture() { Ext.ClearAIDefenseFactory(); }
	void Start()
	{
		if (auto node = DualBuildingTemp::PickAvailableBuildingNode(&House))
			DispatchDefenseForTest(&House, node);
	}
	void Ready()
	{
		Start();
		for (int i = 0; i < 3; ++i) Ext.AIDefenseFactory->Tick();
		Check(Ext.AIDefenseFactory->Done, "production must start and advance");
	}
};
// A stream double exercises the exact contiguous save chain extracted from
// HouseExt::Serialize, including remapping the engine-owned factory pointer.
struct Stream
{
	std::vector<unsigned char> Data;
	size_t Offset = 0;
	bool Reading = false;
	FactoryClass* RemapFrom = nullptr;
	FactoryClass* RemapTo = nullptr;
	template<class T> Stream& Process(T& v)
	{
		if (Reading) { std::memcpy(&v, Data.data() + Offset, sizeof(v)); Offset += sizeof(v); }
		else { auto p = reinterpret_cast<const unsigned char*>(&v); Data.insert(Data.end(), p, p + sizeof(v)); }
		return *this;
	}
	Stream& Process(FactoryClass*& v)
	{
		Process<FactoryClass*>(v);
		if (Reading && v == RemapFrom) v = RemapTo;
		return *this;
	}
	Stream& Process(std::vector<CellStruct>& v)
	{
		size_t n = v.size(); Process(n);
		if (Reading) v.resize(n);
		for (auto& c : v) Process(c);
		return *this;
	}
};
int main()
{
	try
	{
		{
			Fixture f; f.Ready();
			f.Ext.UpdateAIDefenseProduction();
			Check(PlaceCalls == 1 && PlacedAt == CellStruct {10, 20}, "native placement uses planned coordinates");
			Check(!f.Ext.AIDefenseFactory && f.Ext.ProducingDefenseBuildingTypeIndex == -1, "factory deletion must not leave a stale job");
			Check(f.House.Base.BaseNodes.Items[0].Placed, "successful node must be marked");
			Check(f.House.ProducingBuildingTypeIndex == 77, "successful defense must preserve main production");
			Unsorted::CurrentFrame = 40; f.Ext.UpdateAIDefenseProduction();
			Check(!f.Ext.AIDefenseFactory && PlaceCalls == 1, "built node must not be charged again");
		}
		{
			Fixture f; PlaceDeletesFactory = false; f.Ready(); f.Ext.UpdateAIDefenseProduction();
			Check(!f.Ext.AIDefenseFactory && f.Ext.ProducingDefenseBuildingTypeIndex == -1, "successful native placement may retain empty factory");
		}
		{
			Fixture f; f.Ready(); PlaceSucceeds = false;
			f.Ext.UpdateAIDefenseProduction();
			Check(PlaceCalls == 1 && f.Ext.AIDefensePlaceAttempts == 1, "count completed placement failures");
			for (int frame = 1; frame < 30; ++frame) { Unsorted::CurrentFrame = frame; f.Ext.UpdateAIDefenseProduction(); }
			Check(PlaceCalls == 1, "retry interval must suppress repeated work");
			for (int i = 1; i < 8; ++i) { Unsorted::CurrentFrame = i * 30; f.Ext.UpdateAIDefenseProduction(); }
			Check(PlaceCalls == 8 && !f.Ext.AIDefenseFactory, "abandon after eight actual failures");
			Check(f.Ext.AIDefenseAbandonedNodes.size() == 1, "blocked node must be remembered");
			Check(f.House.ProducingBuildingTypeIndex == 77, "abandon must preserve main production slot");
			Unsorted::CurrentFrame = 300; f.Ext.UpdateAIDefenseProduction();
			Check(!f.Ext.AIDefenseFactory, "abandoned node must not restart");
		}
		{
			Fixture f; f.Start(); auto factory = f.Ext.AIDefenseFactory;
			factory->Tick(); auto progress = factory->Progress;
			f.Type.Builder.HasPower = false; f.Ext.UpdateAIDefenseProduction(); factory->Tick();
			Check(factory->IsSuspended && factory->Progress == progress, "power loss suspends paid progress");
			f.Type.Builder.HasPower = true; f.Ext.UpdateAIDefenseProduction(); factory->Tick();
			Check(!factory->IsSuspended && factory->Progress == progress + 1, "restoring power resumes the same factory");
			f.House.Production = false; f.Ext.UpdateAIDefenseProduction(); factory->Tick();
			Check(factory->IsSuspended && factory->Progress == progress + 1, "AI Production flag suspends progress");
			f.House.Production = true; f.Ext.UpdateAIDefenseProduction();
			Check(!factory->IsSuspended, "AI Production restart resumes progress");
		}
		for (int cause = 0; cause < 3; ++cause)
		{
			Fixture f; f.Start(); f.Ext.AIDefenseFactory->Tick();
			if (cause == 0) f.House.ConYards.Count = 0;
			if (cause == 1) f.House.Human = true;
			if (cause == 2) f.House.Defeated = true;
			f.Ext.UpdateAIDefenseProduction();
			Check(!f.Ext.AIDefenseFactory && !f.House.Primary_ForDefenses, "yard loss, human takeover or defeat clears job");
			Check(f.House.ProducingBuildingTypeIndex == 77, "cancellation must not clear main slot");
		}
		{
			Fixture f; f.Start(); auto factory = f.Ext.AIDefenseFactory;
			delete factory;
			Check(!f.Ext.AIDefenseFactory, "external factory deletion invalidates pointer");
			f.Ext.UpdateAIDefenseProduction();
			Check(f.Ext.ProducingDefenseBuildingTypeIndex == -1 && !f.Ext.AIDefenseFactory, "lost factory must not be recreated from a type index");
		}
		{
			Fixture f; auto foreign = new FactoryClass; foreign->Owner = &f.House;
			f.House.Primary_ForDefenses = foreign; f.Start();
			Check(!f.Ext.AIDefenseFactory && f.House.Primary_ForDefenses == foreign, "do not overwrite another defense factory");
			delete foreign;
		}
		{
			Fixture f; f.Start(); auto factory = f.Ext.AIDefenseFactory;
			HouseClass other; factory->Owner = &other;
			f.Ext.UpdateAIDefenseProduction();
			Check(!f.Ext.AIDefenseFactory && LiveFactories.contains(factory), "ownership change must not delete another owner's factory");
			Check(other.ProducingBuildingTypeIndex == 77, "ownership mismatch must preserve the other main slot");
			delete factory;
		}
		{
			Fixture f; f.Ready(); f.Type.HasBuilder = false;
			f.Ext.UpdateAIDefenseProduction();
			Check(PlaceCalls == 0 && f.Ext.AIDefensePlaceAttempts == 0, "missing powered builder is not a failed placement");
			f.Type.HasBuilder = true; f.Ext.UpdateAIDefenseProduction();
			Check(PlaceCalls == 1 && !f.Ext.AIDefenseFactory, "restored builder places the completed product");
		}
		{
			Fixture f; f.Start();
			f.Ext.ProducingDefenseBuildingTypeIndex = 1000; f.Ext.UpdateAIDefenseProduction();
			Check(!f.Ext.AIDefenseFactory, "invalid type index must not access type array");
		}
		{
			Fixture f; DemandSucceeds = false; f.Start();
			Check(!f.Ext.AIDefenseFactory && f.Ext.ProducingDefenseBuildingTypeIndex == -1, "failed startup releases factory");
		}
		{
			Fixture f;f.Type.Hosted=true;BuildingClass host;f.House.Buildings.push_back(&host);HostCompatible=false;
			f.Start();Check(!f.Ext.AIDefenseFactory,"incompatible host must not create paid work");HostCompatible=true;
			f.Ext.LimboBuildings.insert(&host);f.Start();Check(!f.Ext.AIDefenseFactory,"hidden limbo host is not a placement location");f.Ext.LimboBuildings.clear();
 f.Ready();Check(f.House.Primary_ForDefenses==f.Ext.AIDefenseFactory,"Combat upgrade must occupy defense queue");
			f.Ext.UpdateAIDefenseProduction();Check(PlacedAt==host.Location,"upgrade placement must use compatible host coordinates");
			Check(f.House.ProducingBuildingTypeIndex==77&&!f.Ext.AIDefenseFactory,"upgrade completion preserves ordinary line");
		}
		{ Fixture f;auto* external=new FactoryClass;external->Owner=&f.House;external->Object=new TechnoClass{&f.Type};f.House.Primary_ForDefenses=external;external->Paid=20;const int before=f.House.Money;external->AbandonProduction();Check(f.House.ProducingBuildingTypeIndex==77,"external defense cancellation preserves main selection" );Check(f.House.Money==before+20,"external defense cancellation retains refund" );delete external;}
		for (int skip = 0; skip < 5; ++skip)
		{
			Fixture f;
			if (skip == 0) f.Type.Hosted = true; // No compatible host: reject without charging.
			if (skip == 1) f.Type.BuildCat = BuildCat::DontCare;
			if (skip == 2) f.House.Money = 0;
			if (skip == 3) f.Type.Buildable = false;
			if (skip == 4) f.Type.HasBuilder = false;
			f.Start(); Check(!f.Ext.AIDefenseFactory, "respect upgrades, categories, funds and native builder eligibility");
		}
		{
			Fixture f;
			Check(DualBuildingTemp::PickAvailableBuildingNode(&f.House) != nullptr, "unclaimed defense must reach native scheduling");
			f.Start();
			Check(DualBuildingTemp::PickAvailableBuildingNode(&f.House) == nullptr, "native scheduler skips only the claimed defense job");
			f.Type.Hosted = true;
			Check(DualBuildingTemp::PickAvailableBuildingNode(&f.House) == nullptr, "claimed Combat upgrades remain on defense line");
			f.Type.Hosted = false; f.House.Human = true;
			Check(DualBuildingTemp::PickAvailableBuildingNode(&f.House) != nullptr, "human controller keeps original node selection");
			f.House.Human = false; f.House.Production = false;
			Check(DualBuildingTemp::PickAvailableBuildingNode(&f.House) != nullptr, "disabled AI production keeps original node selection");
		}
		{
			Fixture f; auto& node = f.House.Base.BaseNodes.Items[0]; node.Placed = true;
			Check(!DualBuildingTemp::CanPickNode(&f.House, 0), "placed nodes use native repick eligibility");
			NativeFlag = 0;
			Check(DualBuildingTemp::CanPickNode(&f.House, 0), "honor native global flag for placed nodes");
			NativeFlag = 1; f.House.Repick = true;
			NativeRepickCell = {30, 40};
			f.Start();
			Check(f.Ext.AIDefenseNodeCoords == CellStruct {30, 40}, "rebuild uses native revised coordinates");
			node.Attempts = 4;
			for (int i = 0; i < 3; ++i) f.Ext.AIDefenseFactory->Tick();
			f.Ext.UpdateAIDefenseProduction();
			Check(node.Placed && node.Attempts == 0, "successful rebuild clears old node placement failures");
		}
		{
			Fixture f; f.Start(); f.Ext.AIDefenseFactory->Tick();
			auto original = f.Ext.AIDefenseFactory;
			f.Ext.AIDefensePlaceAttempts = 3; f.Ext.AIDefenseNextPlaceFrame = 120;
			f.Ext.AIDefenseAbandonedNodes.push_back({15, 25});
			Stream stream; f.Ext.TestSerialize(stream);
			HouseClass loadedHouse; loadedHouse.ConYards.Add(1); loadedHouse.Base = f.House.Base;
			HouseExt loaded(&loadedHouse);
			auto restored = new FactoryClass;
			restored->Owner = &loadedHouse; restored->Object = new TechnoClass {&f.Type};
			restored->Progress = original->Progress; restored->Paid = original->Paid;
			restored->IsSuspended = original->IsSuspended;
			loadedHouse.Primary_ForDefenses = restored;
			stream.Reading = true; stream.RemapFrom = original; stream.RemapTo = restored;
			loaded.TestSerialize(stream);
			Check(loaded.AIDefenseFactory == restored, "saved factory pointer must be remapped");
			Check(loaded.AIDefenseNodeCoords == CellStruct {10, 20} && loaded.AIDefensePlaceAttempts == 3
				&& loaded.AIDefenseNextPlaceFrame == 120 && loaded.AIDefenseAbandonedNodes.size() == 1, "resume coordinates, retries, timer and abandoned nodes");
			auto created = CreatedFactories; loaded.UpdateAIDefenseProduction(); restored->Tick();
			Check(CreatedFactories == created && restored->Progress == 2, "load resumes saved progress without creating another factory");
			loaded.ClearAIDefenseFactory();
		}
		{
			// Every type is affordable/buildable. The late superweapon must be
			// held by native plan order, not accidentally by its prerequisites.
			Fixture f;
			BuildingTypeClass power, tower, tech;
			power.ArrayIndex = 1; power.BuildCat = BuildCat::DontCare;
			tower.ArrayIndex = 2;
			tech.ArrayIndex = 3; tech.BuildCat = BuildCat::DontCare;
			BuildingTypeClass::Array.Add(&power); BuildingTypeClass::Array.Add(&tower); BuildingTypeClass::Array.Add(&tech);
			f.House.Base.BaseNodes = {};
			f.House.Base.BaseNodes.Add({1, {1, 1}});
			f.House.Base.BaseNodes.Add({-1, {2, 2}});
			f.House.Base.BaseNodes.Add({3, {3, 3}});
			f.House.Base.BaseNodes.Add({0, {4, 4}}); // late Combat superweapon
			f.Ext.UpdateAIDefenseProduction();
			Check(!f.Ext.AIDefenseFactory, "idle defense line cannot scan ahead to a future superweapon");
			auto node = DualBuildingTemp::PickAvailableBuildingNode(&f.House);
			Check(node == &f.House.Base.BaseNodes.Items[0] && DispatchDefenseForTest(&f.House, node) == 0,
				"ordinary first node keeps native dispatch");
			f.House.Base.Built.insert(0);
			node = DualBuildingTemp::PickAvailableBuildingNode(&f.House);
			Check(node->BuildingTypeIndex == -1 && DispatchDefenseForTest(&f.House, node) == 0,
				"native defense placeholder cannot be bypassed by later Combat buildings");
			// Emulate the native AI's completed placeholder expansion, then call
			// the actual final-dispatch body. No AI planning is duplicated here.
			node->BuildingTypeIndex = 2;
			Check(DispatchDefenseForTest(&f.House, node) == 0x4FEA48 && f.Ext.ProducingDefenseBuildingTypeIndex == 2,
				"first accepted defense is the expanded native tower, not the future superweapon");
			node = DualBuildingTemp::PickAvailableBuildingNode(&f.House);
			Check(node->BuildingTypeIndex == 3 && DispatchDefenseForTest(&f.House, node) == 0,
				"ordinary next building may run alongside the already accepted defense");
			f.House.ProducingBuildingTypeIndex = 3;
			for (int i = 0; i < 3; ++i) f.Ext.AIDefenseFactory->Tick();
			f.Ext.UpdateAIDefenseProduction();
			Check(!f.Ext.AIDefenseFactory && f.House.ProducingBuildingTypeIndex == 3,
				"finishing the tower preserves the ordinary line");
			Unsorted::CurrentFrame = 60; f.Ext.UpdateAIDefenseProduction();
			Check(!f.Ext.AIDefenseFactory, "empty defense slot does not refill itself while native main job is active");
			f.House.Base.Built.insert(2); f.House.ProducingBuildingTypeIndex = -1;
			node = DualBuildingTemp::PickAvailableBuildingNode(&f.House);
			Check(node->BuildingTypeIndex == 0 && DispatchDefenseForTest(&f.House, node) == 0x4FEA48
				&& f.Ext.ProducingDefenseBuildingTypeIndex == 0, "superweapon remains permitted when native plan actually reaches it");
		}
		for (int sentinel : {-1, -2, -3})
		{
			Fixture f;
			f.House.Base.BaseNodes.Items.insert(f.House.Base.BaseNodes.Items.begin(), {sentinel, {1, 1}});
			++f.House.Base.BaseNodes.Count;
			f.Ext.UpdateAIDefenseProduction();
			Check(!f.Ext.AIDefenseFactory, "native sentinel must not be crossed by an independent scan");
			auto node = DualBuildingTemp::PickAvailableBuildingNode(&f.House);
			Check(node->BuildingTypeIndex == sentinel && DispatchDefenseForTest(&f.House, node) == 0,
				"native wall, stop and defense nodes retain original handling");
		}
		{
			Fixture f;
			f.House.Base.BaseNodes.Add({0, {30, 40}});
			f.Start(); auto first = f.Ext.AIDefenseFactory;
			auto second = DualBuildingTemp::PickAvailableBuildingNode(&f.House);
			Check(second && second->MapCoords == CellStruct {30, 40}, "claim skips only matching type AND coordinates");
			Check(DispatchDefenseForTest(&f.House, second) == 0x4FEA48 && f.Ext.AIDefenseFactory == first
				&& f.Ext.AIDefenseNodeCoords == CellStruct {10, 20}, "busy defense slot cannot be overwritten by another native job");
		}
		{
			Fixture f; BuildingTypeClass cheaper;
			cheaper.ArrayIndex = 1; cheaper.Cost = 1;
			BuildingTypeClass::Array.Add(&cheaper); f.House.Base.BaseNodes.Add({1, {30, 40}});
			f.Type.Cost = 20000; f.Start();
			Check(!f.Ext.AIDefenseFactory && f.Ext.ProducingDefenseBuildingTypeIndex == -1,
				"insufficient money for current native defense cannot pick a cheaper future node");
			Check(DualBuildingTemp::PickAvailableBuildingNode(&f.House)->BuildingTypeIndex == 0,
				"unstarted native defense stays at the head of the plan");
			f.House.Money = 30000; Unsorted::CurrentFrame = 30; f.Start();
			Check(f.Ext.ProducingDefenseBuildingTypeIndex == 0, "native head job starts when affordable");
		}
		Check(DispatchDefenseForTest(nullptr, nullptr) == 0, "null node keeps native path without dereferencing house");
		Check(LiveFactories.empty() && CreatedFactories == DeletedFactories, "all factories released exactly once");
		for (auto product : PlacedObjects) delete product;
		std::printf("PASS: %d controller/dispatch/stream checks (fake engine; no game launched).\n", Checks);
		return 0;
	}
	catch (const std::exception& e) { std::printf("FAIL: %s\n", e.what()); return 1; }
}
