#include <Utilities/Stream.h>
#include <Utilities/Debug.h>
void __cdecl Debug::Log(const char*,...){}
#include <Ext/Bullet/Trajectories/ActualTrajectories/MissileWaterSurface.h>
#include <cassert>
#include <cstdio>
#include <cstdint>
#include <vector>
int main(){
 for(bool enabled:{false,true}){
  PhobosByteStream bytes;PhobosStreamWriter writer(bytes);const std::uint32_t prefix=0x12345678u;writer.Save(prefix);
  assert(MissileWaterSurface::Save(writer,enabled)&&writer.Success());assert(bytes.Size()==(enabled?9u:4u));
  PhobosStreamReader reader(bytes);std::uint32_t got=0;assert(reader.Load(got)&&got==prefix);bool loaded=!enabled;
  assert(MissileWaterSurface::Load(reader,loaded)&&loaded==enabled&&reader.ExpectEndOfBlock());
 }
 PhobosByteStream full;PhobosStreamWriter writer(full);assert(MissileWaterSurface::Save(writer,true));
 auto copy=full;std::vector<unsigned char> raw(copy.Size());assert(copy.Read(raw.data(),raw.size()));
 for(size_t cut=0;cut<raw.size();++cut){PhobosByteStream partial;partial.Write(raw.data(),cut);PhobosStreamReader reader(partial);bool loaded=true;assert(MissileWaterSurface::Load(reader,loaded)==(cut==0)&&!loaded);}
 for(int bad:{0,1,2}){auto raw2=raw;if(bad==0)raw2[0]^=1;if(bad==1)raw2[4]=2;if(bad==2)raw2.push_back(8);PhobosByteStream bytes;bytes.Write(raw2.data(),raw2.size());PhobosStreamReader reader(bytes);bool loaded=true;assert(!MissileWaterSurface::Load(reader,loaded)&&!loaded);}
 puts("PASS: actual Phobos stream reader/writer and optional Missile type tail; legacy/default layout, enabled round trip, truncated and malformed tails.");
}
