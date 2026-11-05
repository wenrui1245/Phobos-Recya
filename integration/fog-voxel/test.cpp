#include "GameStub.h"
#include "MockSwizzle.h"
#include <Misc/FoggedVoxel.h>
#include <Misc/FoggedVoxelData.h>
#include <Utilities/Stream.h>
#include <Utilities/Debug.h>
#include <cassert>
#include <cstdio>

void __cdecl Debug::Log(const char*, ...) {}

struct Snapshot
{
	char padding[0x30] {};
	int Kind = 6;
	CoordStruct Position { 300, 200, 0 };
	RectangleStruct Bounds { 290, 190, 20, 20 };
	char Padding50[0xC] {};
	void* RecordVTable = nullptr;
	void** Records = nullptr;
	int Capacity = 0;
	char VectorFlags[4] {};
	int Count = 0, Increment = 1;
	bool Visible = true;
	char Padding75[3] {};
};

// At 4D10D3 EBX is the source building and EDI is its snapshot. The native
// animation loop later overwrites ESP+54 with the AnimType at 4D119A.
struct MockRegisters
{
	BuildingClass* Building;
	FoggedObjectClass* Snapshot;
	void* ReusedStackArgument;
	template<typename T> T EBX() const { return reinterpret_cast<T>(Building); }
	template<typename T> T EDI() const { return reinterpret_cast<T>(Snapshot); }
	template<typename T> T Stack(unsigned offset) const
	{ assert(offset==0x54); return reinterpret_cast<T>(ReusedStackArgument); }
};
#define GET(type, name, reg) type name = R->reg<type>()
#define GET_STACK(type, name, offset) type name = R->Stack<type>(offset)
#include "ctor-hook.inc"
struct MockEntryRegisters
{
	BuildingClass* Building;
	bool Visible;
	template<typename T> T ECX() const { return reinterpret_cast<T>(Building); }
	template<typename T> T Stack(unsigned offset) const { assert(offset==0xC); return T(Visible); }
};
#include "entry-hook.inc"
#undef GET
#undef GET_STACK

int main()
{
	using namespace FoggedVoxelData;
	// The cache owns RLE rows; their two-byte lengths INCLUDE the prefix.
	unsigned char pixels[] { 5,0,1,2,3, 6,0,4,0,1,5 };
	Image image;
	image.PaletteIndex = 0;
	assert(image.Capture(pixels, 3, 2) && image.Valid());
	pixels[2] = 99;
	assert(image.Shape[34] == 1); // no borrowed cache pointer
	for (unsigned n = 0; n < sizeof(pixels); ++n)
		assert(!RLESize(pixels, 3, 2, n));
	unsigned char badRun[] { 4,0,0,4 };
	assert(!RLESize(badRun,3,1,sizeof(badRun)));
	assert(!image.Capture(pixels, 0, 2));
	assert(!image.Capture(pixels, MaxDimension+1, 2));
	auto invalid = image; invalid.Shape[28] = 0; assert(!invalid.Valid());
	invalid = image; invalid.Shape[34] = 0; invalid.Shape[35] = 255; assert(!invalid.Valid());
	PhobosByteStream imageBytes;
	PhobosStreamWriter iw(imageBytes); image.Save(iw);
	PhobosStreamReader ir(imageBytes); Image restored;
	assert(restored.Load(ir) && ir.ExpectEndOfBlock() && restored.Shape == image.Shape);

	ConvertClass palette;
	ColorScheme scheme { &palette };
	ColorScheme* schemeItems[] { &scheme };
	ColorScheme::Array = { 1, schemeItems };
	BuildingTypeClass type;
	type.TurretAnimIsVoxel = type.BarrelAnimIsVoxel = true;
	type.ArrayIndex = 0;
	BuildingTypeClass* types[] { &type };
	BuildingTypeClass::Array = { 1, types };
	BuildingClass building;
	building.Type = &type;
	building.Location = {300,200,0};
	unsigned char barrel[] { 5,0,11,12,13 };
	unsigned char turret[] { 5,0,21,22,23 };
	VoxelCacheStruct cache { 0,0,3,1,barrel };
	RectangleStruct clip { 0,0,800,600 }, dest { 298,180,3,1 };
	unsigned args[13] {};
	args[0] = reinterpret_cast<unsigned>(&dest);
	args[5] = unsigned(-40); args[6] = 0; args[7] = 1350; args[12] = 10;
	auto capture = [&]()
	{
		FoggedVoxel::SelectPalette(&building, &palette, 0x2210);
		FoggedVoxel::Capture(&building, &cache, &clip, args);
	};
	// A building never drawn on camera still gets a fresh native overlay pass.
	building.OnDraw = [&](const Point2D&, const RectangleStruct& empty)
	{
		assert(empty.Width == 0 && empty.Height == 0);
		capture();
		// Both components can reuse the same temporary cache entry address.
		cache.Buffer = turret; dest.X = 299; dest.Y = 182; capture();
		capture(); // duplicate dirty-area redraw is not a third part
		FoggedVoxel::SelectPalette(&building,&palette,0x2001); // exclude shadows
		FoggedVoxel::Capture(&building,&cache,&clip,args);
	};
	// Both CellClass and native Building AI enter the same hook. No explicit
	// caller-side Prepare is needed; only a visible freeze samples the renderer.
	MockEntryRegisters freezeEntry { &building, true };
	assert(RunFreezeEntryHook(&freezeEntry)==0);
	Snapshot snapshot;
	void* nativeRecords[4] { &type, nullptr, nullptr, nullptr };
	snapshot.Records=nativeRecords; snapshot.Count=snapshot.Capacity=1;
	const auto fog = reinterpret_cast<FoggedObjectClass*>(&snapshot);
// A non-null native animation has reused the old building argument slot.
	// The former hook silently fails to associate the generated voxel images.
	int animationType = 123;
	MockRegisters constructor { &building, fog, &animationType };
	assert(RunFrozenConstructorHook(&constructor)==0);
	assert(snapshot.Bounds.Y == 180 && snapshot.Bounds.Height == 30);
	barrel[2] = turret[2] = 99;
	building.IsFogged = true;
	RectangleStruct dirty { 270,170,60,30 };
	FoggedVoxel::Draw(fog,&dirty);
	assert(DSurface::Calls.size() == 2);
	assert(DSurface::Calls[0].Pixels[34] == 11 && DSurface::Calls[1].Pixels[34] == 21);
	assert(DSurface::Calls[0].Point.X == 28 && DSurface::Calls[0].Point.Y == 10);
	assert(DSurface::Calls[0].Flags == 0x2000 && DSurface::Calls[0].Z == -40);
	assert(DSurface::Calls[0].Intensity == 1350 && DSurface::Calls[0].Tint == 10);
	// Reproduce the user's "frozen, then no image" sequence. Native Building AI
	// creates an invisible bookkeeping snapshot at the same coordinate. It must
	// neither replace nor remove the image owned by the visible snapshot.
	Snapshot invisible = snapshot; invisible.Visible = false;
	auto hiddenFog = reinterpret_cast<FoggedObjectClass*>(&invisible);
	FoggedVoxel::Freeze(&building, hiddenFog);
	FoggedVoxel::Remove(hiddenFog);
	DSurface::Calls.clear(); FoggedVoxel::Draw(fog,&dirty);
	assert(DSurface::Calls.size()==2 && DSurface::Calls[0].Pixels[34]==11);

	// Hidden building changes do not change frozen pixels, palette or location.
	building.Location = {700,500,0}; cache.Buffer = barrel; capture();
	TacticalClass::Camera = {50,25};
	DSurface::Calls.clear(); FoggedVoxel::Draw(fog,&dirty);
	assert(DSurface::Calls.size() == 2 && DSurface::Calls[0].Point.X == -22
		&& DSurface::Calls[0].Point.Y == -15 && DSurface::Calls[0].Pixels[34] == 11);

	PhobosByteStream saved; PhobosStreamWriter writer(saved);
	assert(FoggedVoxel::Save(writer));
	auto copy = saved; std::vector<unsigned char> raw(copy.Size());
	assert(copy.Read(raw.data(),raw.size()));
	FoggedVoxel::Clear();
	PhobosStreamReader reader(saved); assert(FoggedVoxel::Load(reader));
	Snapshot relocated = snapshot;
	auto relocatedFog=reinterpret_cast<FoggedObjectClass*>(&relocated);
	assert(MockSwizzle::Pending.size()==1);
	MockSwizzle::Resolve({ {fog,relocatedFog} });
	DSurface::Calls.clear(); FoggedVoxel::Draw(relocatedFog,&dirty);
	assert(DSurface::Calls.size() == 2 && DSurface::Calls[0].Pixels[34] == 11);
	// An obsolete pointer with identical coordinates must not erase loaded data.
	FoggedVoxel::Remove(fog);
	DSurface::Calls.clear(); FoggedVoxel::Draw(relocatedFog,&dirty); assert(DSurface::Calls.size()==2);
	FoggedVoxel::Remove(relocatedFog);
	DSurface::Calls.clear(); FoggedVoxel::Draw(fog,&dirty); assert(DSurface::Calls.empty());
	// Coordinate-only FVX1 saves remain readable, binding once to the matching
	// visible native draw-record type. Invisible snapshots cannot steal them.
	PhobosByteStream v1; PhobosStreamWriter v1w(v1);
	v1w.Save(unsigned(0x31585646)); v1w.Save(unsigned(1));
	v1w.Save(snapshot.Position); v1w.Save(type.ArrayIndex); v1w.Save(unsigned(1));
	image.Save(v1w);
	PhobosStreamReader v1r(v1); assert(FoggedVoxel::Load(v1r));
	FoggedVoxel::Remove(hiddenFog);
	DSurface::Calls.clear(); FoggedVoxel::Draw(fog,&dirty); assert(DSurface::Calls.size()==1);
	FoggedVoxel::Remove(hiddenFog);
	DSurface::Calls.clear(); FoggedVoxel::Draw(fog,&dirty); assert(DSurface::Calls.size()==1);
	FoggedVoxel::Remove(fog);
	DSurface::Calls.clear(); FoggedVoxel::Draw(fog,&dirty); assert(DSurface::Calls.empty());

	// Empty old global blocks remain readable; damaged new blocks fail closed.
	PhobosByteStream legacy; PhobosStreamReader oldReader(legacy);
	assert(FoggedVoxel::Load(oldReader));
	for (size_t n = 1; n < raw.size(); ++n)
	{
		PhobosByteStream truncated; truncated.Write(raw.data(), n);
		PhobosStreamReader tr(truncated); assert(!FoggedVoxel::Load(tr));
		DSurface::Calls.clear(); FoggedVoxel::Draw(fog,&dirty); assert(DSurface::Calls.empty());
	}
	raw[0] ^= 1;
	PhobosByteStream bad; bad.Write(raw.data(), raw.size());
	PhobosStreamReader br(bad); assert(!FoggedVoxel::Load(br));

	// Freeze preparation captures the current pose even while the camera is elsewhere.
	FoggedVoxel::Clear(); TacticalClass::Camera = {0,0};
	building.Location = {300,200,0}; building.IsFogged = false;
	building.OnDraw = [&](const Point2D&,const RectangleStruct&) { capture(); };
	++Unsorted::CurrentFrame; cache.Buffer = turret; FoggedVoxel::Prepare(&building);
	++Unsorted::CurrentFrame; cache.Buffer = barrel; FoggedVoxel::Prepare(&building);
	FoggedVoxel::Freeze(&building,fog);
	DSurface::Calls.clear(); FoggedVoxel::Draw(fog,&dirty); assert(DSurface::Calls.size()==1);
	FoggedVoxel::Remove(fog); FoggedVoxel::Prepare(&building);
	FoggedVoxel::Remove(&building); FoggedVoxel::Freeze(&building,fog);
	DSurface::Calls.clear(); FoggedVoxel::Draw(fog,&dirty); assert(DSurface::Calls.empty());

	// Uncached/recoiling barrel: copy only the rendered 8-bit voxel source region.
	Surface plain; plain.Width=4; plain.Height=2; plain.Pitch=4;
	plain.Pixels={90,91,92,93,94,31,0,32};
	RectangleStruct sourceRect {1,1,3,1}; Point2D target {28,42};
	unsigned rawArgs[11] {};
	rawArgs[0]=reinterpret_cast<unsigned>(&plain);
	rawArgs[1]=reinterpret_cast<unsigned>(&sourceRect);
	rawArgs[2]=reinterpret_cast<unsigned>(&target);
	rawArgs[3]=reinterpret_cast<unsigned>(&dirty);
	rawArgs[6]=unsigned(-7); rawArgs[7]=0; rawArgs[8]=1150; rawArgs[9]=20;
	DSurface::ViewBounds.Y=32;
	snapshot.Bounds={290,190,20,20};
	building.OnDraw=[&](const Point2D& point,const RectangleStruct& empty)
	{
		assert(point.Y==232 && !empty.Width && !empty.Height);
		FoggedVoxel::CapturePlain(&building,&palette,0x2800,rawArgs);
	};
	FoggedVoxel::Prepare(&building); assert(plain.Locks==0);
	FoggedVoxel::Freeze(&building,fog);
	assert(snapshot.Bounds.Y==180); // bounds exclude the 32-pixel top UI strip
	DSurface::Calls.clear(); FoggedVoxel::Draw(fog,&dirty);
	assert(DSurface::Calls.size()==1 && DSurface::Calls[0].Point.X==28
		&& DSurface::Calls[0].Point.Y==42 && DSurface::Calls[0].Pixels[34]==31
		&& DSurface::Calls[0].Z==-7 && DSurface::Calls[0].Intensity==1150);
	// Hidden engine freeze calls must not draw a live building or replace the
	// independent visible snapshot, even when the object position is identical.
	unsigned rendererCalls=0;
	building.OnDraw=[&](const Point2D&,const RectangleStruct&) { ++rendererCalls; };
	freezeEntry.Visible=false; assert(RunFreezeEntryHook(&freezeEntry)==0);
	assert(rendererCalls==0);
	Snapshot hiddenAgain=snapshot; hiddenAgain.Visible=false;
	auto hiddenAgainFog=reinterpret_cast<FoggedObjectClass*>(&hiddenAgain);
	FoggedVoxel::Freeze(&building,hiddenAgainFog); FoggedVoxel::Remove(hiddenAgainFog);
	DSurface::Calls.clear(); FoggedVoxel::Draw(fog,&dirty); assert(DSurface::Calls.size()==1);
	// Different visible snapshot with the same coordinate has its own lifecycle.
	building.OnDraw=[&](const Point2D&,const RectangleStruct&) { FoggedVoxel::CapturePlain(&building,&palette,0x2800,rawArgs); };
	Snapshot second=snapshot;
	auto secondFog=reinterpret_cast<FoggedObjectClass*>(&second);
	freezeEntry.Visible=true; assert(RunFreezeEntryHook(&freezeEntry)==0);
	FoggedVoxel::Freeze(&building,secondFog);
	FoggedVoxel::Remove(secondFog);
	DSurface::Calls.clear(); FoggedVoxel::Draw(fog,&dirty); assert(DSurface::Calls.size()==1);
	FoggedVoxel::Remove(fog);
	DSurface::Calls.clear(); FoggedVoxel::Draw(fog,&dirty); assert(DSurface::Calls.empty());

	// Two saved images at exactly the same position must keep independent
	// native owners after vector growth, deferred relocation and destruction.
	assert(RunFreezeEntryHook(&freezeEntry)==0); FoggedVoxel::Freeze(&building,fog);
	assert(RunFreezeEntryHook(&freezeEntry)==0); FoggedVoxel::Freeze(&building,secondFog);
	PhobosByteStream pairBytes; PhobosStreamWriter pairWriter(pairBytes);
	assert(FoggedVoxel::Save(pairWriter));
	PhobosStreamReader pairReader(pairBytes); assert(FoggedVoxel::Load(pairReader));
	assert(MockSwizzle::Pending.size()==2);
	Snapshot movedOne=snapshot, movedTwo=second;
	auto movedOneFog=reinterpret_cast<FoggedObjectClass*>(&movedOne);
	auto movedTwoFog=reinterpret_cast<FoggedObjectClass*>(&movedTwo);
	MockSwizzle::Resolve({{fog,movedOneFog},{secondFog,movedTwoFog}});
	DSurface::Calls.clear(); FoggedVoxel::Draw(movedOneFog,&dirty); FoggedVoxel::Draw(movedTwoFog,&dirty);
	assert(DSurface::Calls.size()==2);
	FoggedVoxel::Remove(movedTwoFog);
	DSurface::Calls.clear(); FoggedVoxel::Draw(movedOneFog,&dirty); assert(DSurface::Calls.size()==1);
	// Ordinary same-coordinate buildings with no VXL do not participate.
	type.TurretAnimIsVoxel=type.BarrelAnimIsVoxel=false;
	Snapshot oil=snapshot; auto oilFog=reinterpret_cast<FoggedObjectClass*>(&oil);
	FoggedVoxel::Freeze(&building,oilFog); FoggedVoxel::Remove(oilFog);
	DSurface::Calls.clear(); FoggedVoxel::Draw(movedOneFog,&dirty); assert(DSurface::Calls.size()==1);
	type.TurretAnimIsVoxel=type.BarrelAnimIsVoxel=true;
	FoggedVoxel::Remove(movedOneFog);
	DSurface::Calls.clear(); FoggedVoxel::Draw(movedOneFog,&dirty); assert(DSurface::Calls.empty());

	// Plain RLE compression supports runs longer than one byte and row padding.
	std::vector<unsigned char> zeros(2048,0); Image zeroImage;
	zeroImage.PaletteIndex=0;
	assert(zeroImage.CapturePlain(zeros.data(),1024,1024,2) && zeroImage.Valid());
	puts("PASS: both common freeze entry callers, invisible/different snapshot at same position, deferred owner relocation, FVX1 compatibility, actual constructor hook with reused native stack argument, production prepare/capture/freeze/draw/save/load, uncached and cached voxels, no prior camera draw, independent pixels, temporary-cache reuse, draw order, shadow exclusion, UI and dirty-rectangle offsets, camera movement, hidden-state isolation, lifetime cleanup, legacy empty block and every truncated save prefix.");
}
