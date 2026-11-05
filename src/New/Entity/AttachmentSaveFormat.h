#pragma once
#include <cstdint>

namespace AttachmentSaveFormat
{
	// Legacy records start with a saved type pointer (null or aligned to four
	// bytes). An odd magic distinguishes the new format without consuming a
	// legacy field or guessing from the enclosing block's remaining length.
	inline constexpr std::uint32_t ObjectMagic = 0x314F5441u;
	inline constexpr std::uint32_t TypeMagic = 0x31545441u;
	inline constexpr std::uint32_t Revision = 1;

	template<class Reader>
	bool Read(Reader& stream, std::uint32_t magic, bool& modern)
	{
		modern = false;
		if (!stream.Success()) return false;
		std::uint32_t first = 0;
		if (!stream.Peek(first)) { stream.Fail(); return false; }
		if (first != magic)
		{
			if (first & 3u) { stream.Fail(); return false; }
			return true;
		}
		std::uint32_t version = 0;
		if (!stream.Load(first) || !stream.Load(version) || version != Revision)
		{ stream.Fail(); return false; }
		modern = true;
		return true;
	}

	template<class Writer>
	void Write(Writer& stream, std::uint32_t magic)
	{
		stream.Save(magic);
		stream.Save(Revision);
	}
}
