#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace LightContribution
{
	constexpr int MapWidth = 512;
	constexpr int CellSize = 256;
	constexpr int MaxCells = MapWidth * MapWidth;
	using Channels = std::array<int, 4>;
	using Total = std::array<std::uint32_t, 4>;

	struct Geometry
	{
		int X, Y, Radius;
		bool operator==(const Geometry&) const = default;
	};

	inline bool Supported(const Geometry& f, const Channels& channels)
	{
		return f.Radius > 0 && f.Radius <= 46340
			&& f.X >= 0 && f.X < MapWidth * CellSize && f.Y >= 0 && f.Y < MapWidth * CellSize
			&& std::all_of(channels.begin(), channels.end(), [](int value)
			{ return value >= -2147483 && value <= 2147483; });
	}

	// The native routine rejects points outside the squared radius and then
	// truncates distance, attenuation, and each channel at distinct stages.
	template<typename Round, typename Visit>
	void ForEachCell(const Geometry& f, Round round, Visit visit)
	{
		const int left = std::max(0, (f.X - f.Radius - CellSize / 2) / CellSize);
		const int right = std::min(MapWidth - 1, (f.X + f.Radius - CellSize / 2) / CellSize);
		const int top = std::max(0, (f.Y - f.Radius - CellSize / 2) / CellSize);
		const int bottom = std::min(MapWidth - 1, (f.Y + f.Radius - CellSize / 2) / CellSize);
		const auto radiusSquared = static_cast<std::int64_t>(f.Radius) * f.Radius;
		for (int y = top; y <= bottom; ++y)
		{
			const int dy = y * CellSize + CellSize / 2 - f.Y;
			for (int x = left; x <= right; ++x)
			{
				const int dx = x * CellSize + CellSize / 2 - f.X;
				const auto squared = static_cast<std::int64_t>(dx) * dx + static_cast<std::int64_t>(dy) * dy;
				if (squared > radiusSquared)
					continue;
				const int distance = round(std::sqrt(static_cast<double>(squared)));
				const int weight = (f.Radius - distance) * 1000 / f.Radius;
				if (weight > 0)
					visit(y * MapWidth + x, weight);
			}
		}
	}

	inline bool Apply(Total& total, int weight, const Channels& previous, const Channels& next)
	{
		bool changed = false;
		for (std::size_t i = 0; i < total.size(); ++i)
		{
			const int delta = next[i] * weight / 1000 - previous[i] * weight / 1000;
			total[i] += static_cast<std::uint32_t>(delta);
			changed |= delta != 0;
		}
		return changed;
	}
}
