#pragma once
// Native sidebar insertion uses <= for equal names. std::sort requires a strict
// comparator: preserve native ordering, then break ties by immutable type index.
namespace SWSidebarOrder {
template<class Native> bool Before(int left,int right,bool leftPriority,bool rightPriority,Native native) {
 if(left==right)return false;
 if(leftPriority!=rightPriority)return leftPriority;
 const bool forward=native(left,right),reverse=native(right,left);
 return forward!=reverse?forward:left<right;
}
}
