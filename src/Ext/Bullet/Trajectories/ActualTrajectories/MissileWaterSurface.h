#pragma once
#include <cstdint>
// TrajectoryType is the last field in each bounded BulletType extension block.
// Disabled settings keep the legacy bytes; enabled settings append a tagged tail.
namespace MissileWaterSurface {
inline constexpr std::uint32_t TailMagic=0x31534657u;
template<class Reader> bool Load(Reader& stream,bool& enabled) {
 enabled=false;
 if(!stream.Success())return false;
 if(stream.AtEndOfBlock())return true;
 std::uint32_t magic=0;std::uint8_t value=0;
 if(!stream.Load(magic) || magic!=TailMagic || !stream.Load(value) || value!=1 || !stream.AtEndOfBlock())return false;
 enabled=true;return stream.Success();
}
template<class Writer> bool Save(Writer& stream,bool enabled) {
 if(enabled){stream.Save(TailMagic);stream.Save(std::uint8_t{1});}
 return stream.Success();
}
}
