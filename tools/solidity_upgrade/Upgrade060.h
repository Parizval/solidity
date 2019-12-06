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

#include "UpgradeSuite.h"

#include <libsolidity/analysis/ContractLevelChecker.h>
#include <libsolidity/ast/ASTVisitor.h>

namespace langutil
{
class ErrorReporter;
}

class TypeChecker;


namespace dev
{
namespace solidity
{

///
/// \brief The StrictAssembly class
///
class StrictAssembly: public ParseUpgrade {
public:
	using ParseUpgrade::ParseUpgrade;

	void parse(std::string const& _source);
private:

};

///
/// \brief The NatspecNamedReturnUpgrade class
///
class NatspecNamedReturn: public ParseUpgrade {
public:
	using ParseUpgrade::ParseUpgrade;

	void parse(std::string const& _source);
private:
};

///
/// \brief The AbstractContractUpgrade class
///
class AbstractContract: public AnalysisUpgrade {
public:
	using AnalysisUpgrade::AnalysisUpgrade;

	void analyze(SourceUnit const& _sourceUnit) { _sourceUnit.accept(*this); }
private:
	void endVisit(ContractDefinition const& _contract);
};

///
/// \brief The OverrideFunction class
///
class OverridingFunction: public AnalysisUpgrade {
public:
	using AnalysisUpgrade::AnalysisUpgrade;

	void analyze(SourceUnit const& _sourceUnit) { _sourceUnit.accept(*this); }
private:
	void endVisit(ContractDefinition const& _contract);
};

class VirtualFunction: public AnalysisUpgrade {
public:
	using AnalysisUpgrade::AnalysisUpgrade;

	void analyze(SourceUnit const& _sourceUnit) { _sourceUnit.accept(*this); }
private:
	void endVisit(FunctionDefinition const& _function);
};

///
/// \brief The ArrayLengthUpgrade class
///
class ArrayLength: public AnalysisUpgrade {
public:
	using AnalysisUpgrade::AnalysisUpgrade;

	void analyze(SourceUnit const& _sourceUnit) { _sourceUnit.accept(*this); }
private:
	void endVisit(Assignment const& _assignment);
};


///
/// \brief The Upgrade060 suite
///
class Upgrade060: public UpgradeSuite
{
public:
	void analyze(
		SourceUnit const& _sourceUnit,
		std::string const& _source,
		std::vector<UpgradeChange>& _changes
	)
	{
		AbstractContract{_source, _changes}.analyze(_sourceUnit);
		OverridingFunction{_source, _changes}.analyze(_sourceUnit);
		VirtualFunction{_source, _changes}.analyze(_sourceUnit);
	}
};

}
}
