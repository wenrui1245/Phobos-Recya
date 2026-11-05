#include <cassert>
#include <cstdio>
struct CellClass {static constexpr int BridgeHeight=256;bool Bridge=false;bool ContainsBridge(){return Bridge;}};
struct Foot {int Location=0,Height=256;bool IsFallingDown=false;int GetHeight(){return Height;}};
struct MapClass {inline static CellClass* Current=nullptr;struct Ref {CellClass* TryGetCellAt(int){return Current;}};inline static Ref Instance;};
struct AttachmentLocomotionClass {Foot* LinkedTo;bool ShouldBeOnBridge();};
#include "bridge.inc"
int main(){
 Foot unit;AttachmentLocomotionClass loco{&unit};
 assert(!loco.ShouldBeOnBridge());
 CellClass cell;MapClass::Current=&cell;assert(!loco.ShouldBeOnBridge());
 cell.Bridge=true;assert(loco.ShouldBeOnBridge());
 unit.Height=255;assert(!loco.ShouldBeOnBridge());
 unit.Height=256;unit.IsFallingDown=true;assert(!loco.ShouldBeOnBridge());
 puts("PASS: actual bridge query, invalid cell, no bridge, valid bridge, below bridge and falling states.");
}
