#pragma once
#include <cstdint>
#include <unordered_map>
#include <utility>
#include <vector>
// Runtime identities only: no save fields, pointer probes or global object scans.
namespace AttachmentRuntime {
template<class T> class Registry {
 std::unordered_map<T*,std::uint64_t> Live;
 std::uint64_t Next=0;
public:
 void Add(T* p){if(!++Next)++Next;Live[p]=Next;}
 void Remove(T* p){Live.erase(p);}
 template<class Update> void Run(const std::vector<T*>& array,Update update){
  std::vector<std::pair<T*,std::uint64_t>> snapshot;snapshot.reserve(array.size());
  for(auto* p:array){auto it=Live.find(p);if(it!=Live.end())snapshot.emplace_back(p,it->second);}
  for(auto [p,id]:snapshot){auto it=Live.find(p);if(it!=Live.end() && it->second==id)update(p);}
 }
};
template<class T> Registry<T>* Lifetimes(){
 // Extension owners can be destroyed late in CRT teardown. Close access before
 // releasing the registry; late attachment destructors must not touch it.
 static bool closed=false;
 if(closed)return nullptr;
 struct Storage:Registry<T>{bool& Closed;explicit Storage(bool& c):Closed(c){}~Storage(){Closed=true;}};
 static Storage registry(closed);return &registry;
}
template<class T> class Watch {
 inline static Watch* Top=nullptr;
 Watch* Previous;
 T* Value;
public:
 explicit Watch(T* value):Previous(Top),Value(value){Top=this;}
 ~Watch(){Top=Previous;}
 Watch(const Watch&)=delete;Watch& operator=(const Watch&)=delete;
 T* Get()const{return Value;}
 static void Invalidate(void* p){for(auto* it=Top;it;it=it->Previous)if(it->Value==p)it->Value=nullptr;}
};
}
