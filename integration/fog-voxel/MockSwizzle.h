#pragma once
#include <vector>
#include <utility>
#include <cassert>

// Isolate the game's deferred pointer remapper. The production module still
// registers the addresses of its real owner fields after its vector is stable.
namespace MockSwizzle
{
	inline std::vector<std::pair<void**,void*>> Pending;
	inline void Resolve(const std::vector<std::pair<void*,void*>>& changes = {})
	{
		for (auto [slot,old] : Pending)
		{
			void* replacement = old;
			for (auto [from,to] : changes)
				if (from==old) { replacement=to; break; }
			assert(*slot==nullptr);
			*slot=replacement;
		}
		Pending.clear();
	}
}

class PhobosSwizzle
{
public:
	template<typename T> static void RegisterPointerForChange(T*& pointer)
	{
		MockSwizzle::Pending.emplace_back(reinterpret_cast<void**>(&pointer),pointer);
		pointer=nullptr;
	}
};
