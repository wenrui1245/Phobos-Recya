#pragma once
#include <cstdint>
// BulletType writes TrajectoryType both inline and at the end of its block.
// A missing marker must leave the cursor at the next BulletType field.
namespace MissileWaterSurface {
inline constexpr std::uint32_t TailMagic=0x31534657u;
template<class Reader> bool Load(Reader& stream,bool& enabled) {
 enabled=false;
 if(!stream.Success())return false;
 std::uint32_t magic=0;std::uint8_t value=0;
 if(!stream.Peek(magic) || magic!=TailMagic)return true;
 if(!stream.Load(magic) || !stream.Load(value) || value!=1)return false;
 enabled=true;return stream.Success();
}
template<class Writer> bool Save(Writer& stream,bool enabled) {
 if(enabled){stream.Save(TailMagic);stream.Save(std::uint8_t{1});}
 return stream.Success();
}
}
