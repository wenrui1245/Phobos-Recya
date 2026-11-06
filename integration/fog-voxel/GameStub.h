#pragma once
#include <vector>
#include <utility>
#include <cstdint>
#include <functional>

struct CoordStruct { int X,Y,Z; bool operator==(const CoordStruct&) const = default; };
struct Point2D { int X,Y; };
struct RectangleStruct { int X,Y,Width,Height; };
struct VoxelCacheStruct { short X,Y; unsigned short Width,Height; void* Buffer; };
struct SHPStruct {};
struct SHPFile { char bytes[32]; };
class ConvertClass {};
template<typename T> struct ArrayStub { int Count = 0; T** Items = nullptr; };
class ColorScheme { public: ConvertClass* LightConvert; static inline ArrayStub<ColorScheme> Array; };
class BuildingTypeClass
{
public:
	bool TurretAnimIsVoxel = false, BarrelAnimIsVoxel = false;
	int ArrayIndex = -1;
	char ID[32] = "TestBuilding";
	ArrayStub<ColorScheme>* Palette = nullptr;
	static inline ArrayStub<BuildingTypeClass> Array;
};
enum class AbstractType { Unit, Building };
enum class BlitterFlags : unsigned { Darken=0x1, Centered=0x200, ZRemap=0x10 };
inline BlitterFlags operator|(BlitterFlags a, BlitterFlags b) { return BlitterFlags(unsigned(a)|unsigned(b)); }
enum class ZGradient : int { Ground=0 };
class TechnoClass { public: virtual AbstractType WhatAmI() { return AbstractType::Building; } };
class BuildingClass : public TechnoClass
{
public:
	BuildingTypeClass* Type = nullptr;
	bool IsFogged = false;
	CoordStruct Location {};
	std::function<void(const Point2D&,const RectangleStruct&)> OnDraw;
	CoordStruct GetRenderCoords() const { return Location; }
	void Draw(const Point2D& point,const RectangleStruct& clip) { if(OnDraw) OnDraw(point,clip); }
};
class Surface
{
public:
	int Width=0,Height=0,Pitch=0,Locks=0;
	std::vector<unsigned char> Pixels;
	int GetBytesPerPixel() const { return 1; }
	int GetPitch() const { return Pitch; }
	void* Lock(int x,int y) { ++Locks; return Pixels.data()+y*Pitch+x; }
	bool Unlock() { --Locks; return true; }
};
class FoggedObjectClass {};
class ScenarioClass
{
public:
	struct { bool FogOfWar = true; } SpecialFlags;
	static ScenarioClass* Instance;
};
inline ScenarioClass Scenario;
inline ScenarioClass* ScenarioClass::Instance = &Scenario;
class TacticalClass
{
public:
	static inline Point2D Camera {};
	static TacticalClass* Instance;
	std::pair<Point2D,bool> CoordsToClient(const CoordStruct& coord)
	{ return { {coord.X-Camera.X,coord.Y-Camera.Y-coord.Z}, true }; }
};
inline TacticalClass Tactical;
inline TacticalClass* TacticalClass::Instance = &Tactical;
namespace Unsorted { inline int CurrentFrame = 1; }
class DSurface
{
public:
	struct Call { Point2D Point; unsigned Flags; int Z,Intensity,Tint; std::vector<unsigned char> Pixels; };
	static DSurface* Temp;
	static inline RectangleStruct ViewBounds {0,0,800,600};
	static inline std::vector<Call> Calls;
	void DrawSHP(ConvertClass*,SHPStruct* shape,int,const Point2D* point,const RectangleStruct*,
		BlitterFlags flags,int,int z,ZGradient,int intensity,int tint,void*,int,int,int)
	{
		auto pixels = reinterpret_cast<const unsigned char*>(shape);
		unsigned length = 32;
		for (int y=0;y<(pixels[4]|(unsigned(pixels[5])<<8));++y)
			length += pixels[length]|(unsigned(pixels[length+1])<<8);
		Calls.push_back({*point,unsigned(flags),z,intensity,tint,{pixels,pixels+length}});
	}
};
inline DSurface TempSurface;
inline DSurface* DSurface::Temp = &TempSurface;
