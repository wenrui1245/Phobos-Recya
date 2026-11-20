/* Local light batching and lazy terrain palettes.
 * The original ambient refresh was ported from MagicShiba/MO_ExperienceEnhancements.
 */
#include "LightConvFix.h"
#include "LightContribution.h"

#include <CellClass.h>
#include <GameOptionsClass.h>
#include <LightSourceClass.h>
#include <MapClass.h>
#include <Phobos.h>
#include <ScenarioClass.h>
#include <Unsorted.h>
#include <Utilities/Debug.h>
#include <Helpers/Macro.h>

#include <bitset>
#include <unordered_map>
#include <vector>

namespace
{
	using namespace LightContribution;

	struct CellWeight
	{
		int Index;
		int Weight;
	};

	struct Source
	{
		Geometry Area {};
		Channels Light {};
		std::vector<CellWeight> Cells;
		bool Seen = false;
	};

	struct Ambient
	{
		int Level;
		TintStruct Tint;
		bool operator==(const Ambient& other) const
		{
			return Level == other.Level && Tint == other.Tint;
		}
	};

	static_assert(MapClass::MaxCells == MaxCells);
	std::unordered_map<const LightSourceClass*, Source> Sources;
	std::vector<Total> Totals;
	std::vector<unsigned> CellVersions;
	std::vector<int> DirtyCells;
	std::bitset<MaxCells> Dirty;
	unsigned AmbientVersion = 0;
	Ambient LastAmbient {};
	int LastDetail = -1;
	bool Pending = false;
	bool PendingAmbient = false;
	bool CacheValid = false;
	bool NativeFallback = false;
	const CellClass* Refreshing = nullptr;

	struct Measurements
	{
		unsigned Batches = 0;
		unsigned SourcesRead = 0;
		unsigned Deltas = 0;
		unsigned LocalCells = 0;
		unsigned AmbientCells = 0;
		unsigned ConverterChanges = 0;
		LONGLONG Ticks = 0;
		DWORD Since = 0;
	} Stats;

	bool Ready()
	{
		const auto& cells = MapClass::Instance.Cells;
		return Unsorted::ScenarioStarted && ScenarioClass::Instance && cells.Items && cells.Capacity == MaxCells;
	}

	int CellIndex(const CellStruct& cell)
	{
		if (static_cast<unsigned>(cell.X) >= MapWidth || static_cast<unsigned>(cell.Y) >= MapWidth)
			return -1;
		return cell.Y * MapWidth + cell.X;
	}

	Channels ReadChannels(const LightSourceClass& light)
	{
		return { light.LightIntensity, light.LightTint.Red, light.LightTint.Green, light.LightTint.Blue };
	}

	Geometry ReadGeometry(const LightSourceClass& light)
	{
		return { light.Location.X, light.Location.Y, light.LightVisibility };
	}

	void UpdateAmbientVersion()
	{
		const auto scen = ScenarioClass::Instance;
		if (!scen)
			return;
		const Ambient current { scen->AmbientCurrent, scen->NormalLighting.Tint };
		if (PendingAmbient || current != LastAmbient)
		{
			PendingAmbient = false;
			LastAmbient = current;
			if (++AmbientVersion == 0)
			{
				std::fill(CellVersions.begin(), CellVersions.end(), 0);
				AmbientVersion = 1;
			}
		}
	}

	void ApplySource(const Source& source, const Channels& previous, const Channels& next, bool markDirty)
	{
		Stats.Deltas += static_cast<unsigned>(source.Cells.size());
		for (const auto [index, weight] : source.Cells)
		{
			if (Apply(Totals[index], weight, previous, next) && markDirty && !Dirty[index])
			{
				Dirty.set(index);
				DirtyCells.push_back(index);
			}
		}
	}

	void SynchronizeSources(int detail, bool markDirty)
	{
		if (Totals.empty())
		{
			Totals.resize(MaxCells);
			CellVersions.resize(MaxCells);
		}

		NativeFallback = false;
		for (const auto light : LightSourceClass::Array)
		{
			++Stats.SourcesRead;
			if (!light || !light->Activated || light->DetailLevel > detail)
				continue;
			const auto area = ReadGeometry(*light);
			const auto channels = ReadChannels(*light);
			if (!Supported(area, channels))
			{
				NativeFallback = true;
				continue;
			}

			auto& source = Sources[light];
			source.Seen = true;
			if (source.Area == area)
			{
				if (source.Light != channels)
					ApplySource(source, source.Light, channels, markDirty);
			}
			else
			{
				ApplySource(source, source.Light, {}, markDirty);
				source.Area = area;
				source.Cells.clear();
				ForEachCell(area, [](double distance) { return Game::F2I(distance); },
					[&](int index, int weight) { source.Cells.push_back({ index, weight }); });
				ApplySource(source, {}, channels, markDirty);
			}
			source.Light = channels;
		}

		for (auto it = Sources.begin(); it != Sources.end();)
		{
			if (!it->second.Seen)
			{
				ApplySource(it->second, it->second.Light, {}, markDirty);
				it = Sources.erase(it);
			}
			else
			{
				it->second.Seen = false;
				++it;
			}
		}
		LastDetail = detail;
		CacheValid = true;
	}

	void Refresh(CellClass* cell, int index)
	{
		const auto previous = cell->LightConvert;
		const auto saved = Refreshing;
		Refreshing = cell;
		cell->InitLightConvert();
		Refreshing = saved;
		CellVersions[index] = AmbientVersion;
		Stats.ConverterChanges += previous != cell->LightConvert;
	}

	void ReportMeasurements()
	{
		const auto now = GetTickCount();
		if (!Stats.Since)
			Stats.Since = now;
		if (now - Stats.Since < 5000)
			return;
		if (Stats.Batches || Stats.AmbientCells)
		{
			LARGE_INTEGER frequency {};
			QueryPerformanceFrequency(&frequency);
			const double batchMs = frequency.QuadPart ? 1000.0 * Stats.Ticks / frequency.QuadPart : 0.0;
			Debug::Log("[Lighting] batches=%u sourceChecks=%u cellDeltas=%u localCells=%u ambientCells=%u paletteChanges=%u batchMs=%.2f\n",
				Stats.Batches, Stats.SourcesRead, Stats.Deltas, Stats.LocalCells, Stats.AmbientCells,
				Stats.ConverterChanges, batchMs);
		}
		Stats = {};
		Stats.Since = now;
	}

	void BeforeDraw()
	{
		if (!Phobos::Config::LightConvert_AutoFix || !Ready())
			return;
		UpdateAmbientVersion();
		const int detail = GameOptionsClass::Instance.DetailLevel;
		if (Pending || !CacheValid || detail != LastDetail)
		{
			LARGE_INTEGER start {}, end {};
			QueryPerformanceCounter(&start);
			SynchronizeSources(detail, Pending || CacheValid);
			Pending = false;
			for (const int index : DirtyCells)
			{
				if (const auto cell = MapClass::Instance.Cells.Items[index])
				{
					Refresh(cell, index);
					++Stats.LocalCells;
				}
				Dirty.reset(index);
			}
			DirtyCells.clear();
			QueryPerformanceCounter(&end);
			Stats.Ticks += end.QuadPart - start.QuadPart;
			++Stats.Batches;
		}
		ReportMeasurements();
	}

	bool QueueLight(const LightSourceClass& light, bool deferred)
	{
		if (!Phobos::Config::LightConvert_AutoFix || !Ready())
			return false;
		const bool first = !Pending;
		Pending = true;
		if (deferred || !Supported(ReadGeometry(light), ReadChannels(light)))
			return false;
		if (first)
			MapClass::Instance.MarkNeedsRedraw(1);
		return true;
	}

	DWORD Recalculate(DWORD after)
	{
		// Retain the engine's synchronous scalar update. Only the extra
		// LightConvert rebuild is deferred until a cell is drawn.
		reinterpret_cast<void(__fastcall*)(MapClass*)>(0x4AE4C0)(&MapClass::Instance);
		if (Phobos::Config::LightConvert_AutoFix)
			UpdateAmbientVersion();
		return after;
	}
}

void LightConv_Reset()
{
	Sources.clear();
	Totals.clear();
	CellVersions.clear();
	DirtyCells.clear();
	Dirty.reset();
	LastDetail = -1;
	AmbientVersion = 0;
	LastAmbient = {};
	Pending = PendingAmbient = CacheValid = NativeFallback = false;
	Refreshing = nullptr;
	Stats = {};
}

DEFINE_HOOK(0x53ADDB, LightConv_Recalc, 0x5)
{
	return Recalculate(0x53ADE0);
}

DEFINE_HOOK(0x55B4C6, LightConv_Step, 0x5)
{
	return Recalculate(0x55B4CB);
}

DEFINE_HOOK(0x67E6C4, LightConv_LoadReady, 0x6)
{
	PendingAmbient = true;
	return 0;
}

DEFINE_HOOK(0x683E75, LightConv_ScenStart, 0x7)
{
	PendingAmbient = true;
	return 0;
}

// Native activation and detail checks have passed, and the aligned frame
// is established. Preserve the engine's queued-light mode and its epilogue.
DEFINE_HOOK(0x554B33, LightSourceClass_QueueLightingRefresh, 0xA)
{
	GET(const LightSourceClass*, light, EDI);
	GET_BASE(const bool, deferred, 0x8);
	return QueueLight(*light, deferred) ? 0x554D16 : 0;
}

// Retint has finished, and the tactical terrain/object passes are next.
DEFINE_HOOK(0x6D4476, TacticalClass_FlushLightChanges, 0x7)
{
	BeforeDraw();
	return 0;
}

// Both the dirty-cell and the terrain redraw paths read the cell palette.
DEFINE_HOOK_AGAIN(0x480350, CellClass_RefreshAmbientPaletteOnDraw, 0x7)
DEFINE_HOOK(0x4801F0, CellClass_RefreshAmbientPaletteOnDraw, 0x5)
{
	GET(CellClass*, cell, ECX);
	if (CacheValid && Phobos::Config::LightConvert_AutoFix && Ready())
	{
		const int index = CellIndex(cell->MapCoords);
		if (index >= 0 && MapClass::Instance.Cells.Items[index] == cell && CellVersions[index] != AmbientVersion)
		{
			Refresh(cell, index);
			++Stats.AmbientCells;
		}
	}
	return 0;
}

// The native prologue initialized ambient and tint. Replace only the additive
// source loop. The engine still handles cell height, clamping and converters.
DEFINE_HOOK(0x48427D, CellClass_UseCachedLightContributions, 0x6)
{
	GET(const CellClass*, cell, EDI);
	if (cell != Refreshing || NativeFallback)
		return 0;
	const auto& total = Totals[CellIndex(cell->MapCoords)];
	GET_STACK(int*, light, 0x44);
	GET_STACK(int*, red, 0x54);
	GET_STACK(int*, green, 0x58);
	GET_STACK(int*, blue, 0x5C);
	*light = static_cast<int>(static_cast<std::uint32_t>(*light) + total[0]);
	*red = static_cast<int>(static_cast<std::uint32_t>(*red) + total[1]);
	*green = static_cast<int>(static_cast<std::uint32_t>(*green) + total[2]);
	*blue = static_cast<int>(static_cast<std::uint32_t>(*blue) + total[3]);
	return 0x48445F;
}
