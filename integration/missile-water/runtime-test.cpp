#include <cassert>
#include <cstdio>
#include <vector>
#include <cstring>
#include <cstdint>
#include <Ext/Bullet/Trajectories/ActualTrajectories/MissileWaterSurface.h>
struct CoordStruct{int X,Y,Z;};
enum class LandType{Clear,Water,Beach};
struct Cell{LandType LandType;int Height;};
struct MapClass{
 static MapClass Instance;Cell Cells[4]{{LandType::Water,0},{LandType::Clear,0},{LandType::Beach,0},{LandType::Water,256}};int Queries=0;
 Cell* TryGetCellAt(const CoordStruct& p){++Queries;return p.X>=0&&p.X<4?&Cells[p.X]:nullptr;}
 int GetCellFloorHeight(const CoordStruct& p){auto* c=TryGetCellAt(p);return c?c->Height:0;}
};MapClass MapClass::Instance;
enum class TrajectoryStatus:unsigned{None=0,Detonate=1,Vanish=2};
TrajectoryStatus operator|(TrajectoryStatus a,TrajectoryStatus b){return static_cast<TrajectoryStatus>(static_cast<unsigned>(a)|static_cast<unsigned>(b));}
unsigned operator&(TrajectoryStatus a,TrajectoryStatus b){return static_cast<unsigned>(a)&static_cast<unsigned>(b);}
enum class TrajectoryCheckReturnType{Detonate,SkipGameCheck};
struct BulletClass{TrajectoryStatus Status=TrajectoryStatus::None;};
namespace BulletExt{BulletClass* Fetch(BulletClass* b){return b;}}
struct PhobosTrajectory{
 BulletClass* Bullet;virtual bool GetCanHitGround()const{return true;}virtual bool CanHitGroundAt(const CoordStruct&)const{return GetCanHitGround();}
 TrajectoryCheckReturnType OnDetonateUpdate(const CoordStruct&);bool FastGround(const CoordStruct&,bool)const;bool LowGround(const CoordStruct&)const;
};
struct MissileType{bool IgnoreWaterSurface=false;};
struct MissileTrajectory:PhobosTrajectory{MissileType* Type;bool CanHitGroundAt(const CoordStruct&)const override;};
#include "production-fragments.inc"
struct Writer{
 std::vector<unsigned char> Bytes;bool Good=true;bool Success()const{return Good;}
 template<class T>void Save(const T& t){if(Good){auto* p=reinterpret_cast<const unsigned char*>(&t);Bytes.insert(Bytes.end(),p,p+sizeof t);}}
};
struct Reader{
 const std::vector<unsigned char>& Bytes;size_t Pos=0;bool Good=true;
 bool Success()const{return Good;}bool AtEndOfBlock()const{return Pos==Bytes.size();}
 template<class T>bool Load(T& t){if(sizeof t>Bytes.size()-Pos){Good=false;return false;}memcpy(&t,Bytes.data()+Pos,sizeof t);Pos+=sizeof t;return true;}
};
int main(){
 BulletClass b;MissileType type;MissileTrajectory missile;missile.Type=&type;missile.Bullet=&b;
 PhobosTrajectory native;native.Bullet=&b;
 for(bool enabled:{false,true})for(int x:{-1,0,1,2,3})for(int z:{-512,-128,-16,-15,0,16,128,240,256,512}){
  type.IgnoreWaterSurface=enabled;CoordStruct p{x,0,z};auto* cell=MapClass::Instance.TryGetCellAt(p);
  const int floor=cell?cell->Height:0;const bool exempt=enabled&&cell&&cell->LandType==LandType::Water;
  assert(missile.CanHitGroundAt(p)==!exempt);
  assert(missile.OnDetonateUpdate(p)==(!exempt&&floor>=z+16?TrajectoryCheckReturnType::Detonate:TrajectoryCheckReturnType::SkipGameCheck));
  assert(missile.FastGround(p,true)==(!exempt&&z+16<floor));assert(!missile.FastGround(p,false));
  assert(missile.LowGround(p)==(!exempt&&floor>=z));
  assert(native.OnDetonateUpdate(p)==(floor>=z+16?TrajectoryCheckReturnType::Detonate:TrajectoryCheckReturnType::SkipGameCheck));
 }
 type.IgnoreWaterSurface=false;MapClass::Instance.Queries=0;assert(missile.CanHitGroundAt({0,0,-128})&&MapClass::Instance.Queries==0);
 type.IgnoreWaterSurface=true;
 for(auto status:{TrajectoryStatus::Detonate,TrajectoryStatus::Vanish}){b.Status=status;assert(missile.OnDetonateUpdate({0,0,-128})==TrajectoryCheckReturnType::Detonate);}b.Status=TrajectoryStatus::None;
 // High-speed water -> land -> water still catches the intervening land sample.
 int first=-1;for(int x:{0,1,3})if(missile.FastGround({x,0,-128},true)){first=x;break;}assert(first==1);
 for(bool enabled:{false,true}){
  Writer w;const std::uint32_t prefix=0x12345678;w.Save(prefix);const auto legacy=w.Bytes;assert(MissileWaterSurface::Save(w,enabled));
  assert(enabled?w.Bytes.size()==legacy.size()+5:w.Bytes==legacy);
  Reader r{w.Bytes,legacy.size()};bool loaded=!enabled;assert(MissileWaterSurface::Load(r,loaded)&&loaded==enabled&&r.AtEndOfBlock());
 }
 Writer enabled;assert(MissileWaterSurface::Save(enabled,true));
 for(size_t n=0;n<enabled.Bytes.size();++n){auto bytes=enabled.Bytes;bytes.resize(n);Reader r{bytes};bool value=true;assert(MissileWaterSurface::Load(r,value)==(n==0));assert(!value);}
 for(int which:{0,1,2}){auto bytes=enabled.Bytes;if(which==0)bytes[0]^=1;if(which==1)bytes.back()=0;if(which==2)bytes.push_back(9);Reader r{bytes};bool value=true;assert(!MissileWaterSurface::Load(r,value)&&!value);}
 Writer bad;bad.Good=false;assert(!MissileWaterSurface::Save(bad,true));Reader failed{enabled.Bytes};failed.Good=false;bool value=true;assert(!MissileWaterSurface::Load(failed,value)&&!value);
 puts("PASS: actual ground-check fragments; opt-in/off, below/above and raised water, land/beach/off-map, low/high-speed samples, existing detonation flags, legacy/default byte compatibility, enabled tail round-trip, malformed/truncated tail rejection.");
}
