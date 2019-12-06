/*
	This file is part of solidity.

	solidity is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	solidity is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with solidity.  If not, see <http://www.gnu.org/licenses/>.
*/
#pragma once

#include <libdevcore/AnsiColorized.h>

#include <liblangutil/SourceLocation.h>

#include <algorithm>

namespace dev
{
namespace solidity
{

class UpgradeChange
{
public:
	enum class Level {
		Safe,
		Unsafe
	};

	UpgradeChange(
		Level _level,
		langutil::SourceLocation _location,
		std::string _patch
	)
	:
		m_location(_location),
		m_source(_location.source->source()),
		m_patch(_patch),
		m_level(_level) {}

	~UpgradeChange() {}

	langutil::SourceLocation const& location() { return m_location; }
	std::string source() const { return m_source; }
	std::string patch() { return m_patch; }
	Level level() const { return m_level; }

	void apply();
	void log(bool const _shorten) const;
private:
	langutil::SourceLocation m_location;
	std::string m_source;
	std::string m_patch;
	Level m_level;

	static std::string shortenSource(std::string const& _source);
};
}
}
