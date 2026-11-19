#pragma once

#include <algorithm>
#include <cstdint>

namespace FoggedViewport
{
	// RectangleStruct has no intersection helper. Use wide sums so negative
	// dirty origins and rectangles near the integer limits cannot overflow.
	template<typename Rect>
	bool Clip(Rect& dirty, const Rect& viewport)
	{
		if (dirty.Width <= 0 || dirty.Height <= 0
			|| viewport.Width <= 0 || viewport.Height <= 0)
			return false;

		const auto left = std::max<std::int64_t>(dirty.X, viewport.X);
		const auto top = std::max<std::int64_t>(dirty.Y, viewport.Y);
		const auto right = std::min(std::int64_t(dirty.X) + dirty.Width,
			std::int64_t(viewport.X) + viewport.Width);
		const auto bottom = std::min(std::int64_t(dirty.Y) + dirty.Height,
			std::int64_t(viewport.Y) + viewport.Height);
		if (right <= left || bottom <= top)
			return false;

		dirty = { static_cast<int>(left), static_cast<int>(top),
			static_cast<int>(right - left), static_cast<int>(bottom - top) };
		return true;
	}
}
