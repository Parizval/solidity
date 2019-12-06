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

#include "UpgradeChange.h"

#include <libsolidity/ast/ASTVisitor.h>

#include <regex>

namespace langutil
{
class ErrorReporter;
}

namespace dev
{
namespace solidity
{

enum class FormatType
{
	Inline,
	Multiline,
	ReturnInline
};

namespace util
{
	inline bool isMultiline(std::string const& _functionSource, std::string const& _keyword)
	{
		std::regex multiline{"(\\b" + _keyword + "\\b$)"};
		return regex_search(_functionSource, multiline);
	}

	inline std::string placeAfterFunctionHeaderKeyword(
		langutil::SourceLocation const& _location,
		std::string const& _headerKeyword,
		std::string const& _keyword
	)
	{
		std::string codeBefore = _location.source->source().substr(
			_location.start,
			_location.end - _location.start
		);
		bool isMultiline = util::isMultiline(codeBefore, _headerKeyword);

		std::string toAppend = isMultiline ? ("\n" + _keyword) : (" " + _keyword);
		std::regex keywordRegex{"(\\b" + _headerKeyword + "\\b)"};
		return regex_replace(codeBefore, keywordRegex, _headerKeyword + toAppend);
	}
}

///
/// \brief The Upgrade class
///
class Upgrade
{
public:
	Upgrade(
		std::string const& _source,
		std::vector<UpgradeChange>& _changes
	):
		m_source(_source),
		m_changes(_changes)
	{}

protected:
	std::string const& m_source;
	std::vector<UpgradeChange>& m_changes;
};

///
/// \brief The ParseUpgrade class
///
class ParseUpgrade: public Upgrade
{
public:
	using Upgrade::Upgrade;

	void parse(SourceUnit const&) {}
};

///
/// \brief The Upgrade class
///
class AnalysisUpgrade: public Upgrade, public ASTConstVisitor
{
public:
	using Upgrade::Upgrade;

	void analyze(SourceUnit const&) {}
};

///
/// \brief The generic Upgrade suite
///
class UpgradeSuite
{
public:
	void parse(
		SourceUnit const& _sourceUnit,
		std::string const& _source,
		std::vector<UpgradeChange>& _changes
	);

	void analyze(
		SourceUnit const& _sourceUnit,
		std::string const& _source,
		std::vector<UpgradeChange>& _changes
	);
};
}
}
