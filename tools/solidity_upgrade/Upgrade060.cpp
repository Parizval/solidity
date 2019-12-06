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
#include "Upgrade060.h"

#include <liblangutil/ErrorReporter.h>

#include <libyul/AsmData.h>

#include <regex>

using namespace std;

class TypeChecker;
class ContractLevelChecker;

namespace dev
{
namespace solidity
{

using namespace langutil;

void AbstractContract::endVisit(ContractDefinition const& _contract)
{
	bool isFullyImplemented = _contract.annotation().unimplementedFunctions.empty();

	if (
		!isFullyImplemented &&
		!_contract.abstract() &&
		!_contract.isInterface()
	)
	{
		// TODO make this more memory-friendly by just passing a reference to
		// the source parts everywhere.
		auto location = _contract.location();

		string codeBefore = location.source->source().substr(location.start, location.end - location.start);
		string codeAfter = "abstract " + codeBefore;

		m_changes.push_back(
			UpgradeChange{UpgradeChange::Level::Safe, location, codeAfter}
		);
	}
}

void OverridingFunction::endVisit(ContractDefinition const& _contract)
{
	ErrorList errors;
	ErrorReporter errorReporter{errors};
	ContractLevelChecker contractChecker{errorReporter};

	contractChecker.check(_contract);

	auto const& funcSet = contractChecker.inheritedFunctions(&_contract);
	// auto const& modSet = ContractLevelChecker::inheritedModifiers(&_contract);

	for (FunctionDefinition const* function: _contract.definedFunctions())
	{
		if (!function->isConstructor())
		{
			// Build list of expected contracts
			for (auto [begin, end] = funcSet.equal_range(function); begin != end; begin++)
			{
				auto& super = (**begin);
				FunctionTypePointer functionType = FunctionType(*function).asCallableFunction(false);
				FunctionTypePointer superType = FunctionType(super).asCallableFunction(false);

				if (functionType && functionType->hasEqualParameterTypes(*superType))
				{
					if (!function->overrides())
					{
						string codeAfter = util::placeAfterFunctionHeaderKeyword(
							function->location(),
							Declaration::visibilityToString(function->visibility()),
							"override"
						);

						m_changes.push_back(
							UpgradeChange{UpgradeChange::Level::Unsafe, function->location(), codeAfter}
						);
					}
				}

				if (!super.virtualSemantics())
				{
					string codeAfter = util::placeAfterFunctionHeaderKeyword(
						function->location(),
						Declaration::visibilityToString(function->visibility()),
						"virtual"
					);

					m_changes.push_back(
						UpgradeChange{UpgradeChange::Level::Unsafe, function->location(), codeAfter}
					);
				}
			}
		}
	}
}

void VirtualFunction::endVisit(FunctionDefinition const& _function)
{
	if (!_function.virtualSemantics())
	{
		string codeAfter = util::placeAfterFunctionHeaderKeyword(
			_function.location(),
			Declaration::visibilityToString(_function.visibility()),
			"virtual"
		);

		m_changes.push_back(
			UpgradeChange{UpgradeChange::Level::Unsafe, _function.location(), codeAfter}
		);
	}
}

void ArrayLength::endVisit(Assignment const& _assignment)
{
	if (auto memberAccess = dynamic_cast<MemberAccess const*>(&_assignment.leftHandSide()))
	{
		if (
			dynamic_cast<ArrayType const*>(memberAccess->expression().annotation().type) &&
			memberAccess->memberName() == "length" &&
			memberAccess->annotation().lValueRequested
		)
		{
			auto location = _assignment.location();
			string patch = location.source->source().substr(location.start, location.end);

			m_changes.push_back(
				UpgradeChange{UpgradeChange::Level::Unsafe, location, ""}
			);
		}
	}
}

}
}
