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
#include "SolidityUpgrade.h"

#include "Upgrade060.h"

#include <liblangutil/Exceptions.h>
#include <liblangutil/SourceReferenceFormatterHuman.h>

#include <libsolidity/ast/AST.h>

#include <boost/filesystem.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/algorithm/string.hpp>

namespace po = boost::program_options;
namespace fs = boost::filesystem;

using namespace std;

namespace dev
{
namespace solidity
{

static string const g_argHelp = "help";
static string const g_argVersion = "version";
static string const g_argInputFile = "input-file";
static string const g_argIgnoreMissingFiles = "ignore-missing";
static string const g_argAcceptSafe = "accept-safe";
static string const g_argAcceptUnsafe = "accept-unsafe";
static string const g_argShortLog = "short-log";
static string const g_argAllowPaths = "allow-paths";

namespace
{
	AnsiColorized normal()
	{
		return AnsiColorized(cout, true, {});
	}

	AnsiColorized normalBold()
	{
		return AnsiColorized(cout, true, {formatting::BOLD});
	}

	AnsiColorized cyan()
	{
		return AnsiColorized(cout, true, {formatting::CYAN});
	}

	AnsiColorized yellow()
	{
		return AnsiColorized(cout, true, {formatting::YELLOW});
	}

	AnsiColorized magenta()
	{
		return AnsiColorized(cout, true, {formatting::MAGENTA});
	}
}

bool SolidityUpgrade::parseArguments(int _argc, char** _argv)
{
	po::options_description desc(R"(solidity-upgrade, the Solidity upgrade assistant.

The solidity-upgrade tool can help upgrade smart contracts to breaking language features.

It does not support all breaking changes for each version,
but will hopefully assist upgrading your contracts to the desired Solidity version.

List of supported breaking changes:

0.5.0
	none

0.6.0
	- abstract contracts (safe)
	- override / virtual (unsafe)


solidity-upgrade is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY. Please be careful when running upgrades on
your contracts.

Usage: solidity-upgrade [options] contract.sol

Allowed options)",
		po::options_description::m_default_line_length,
		po::options_description::m_default_line_length - 23
	);
	desc.add_options()
		(g_argHelp.c_str(), "Show help message and exit.")
		(g_argVersion.c_str(), "Show version and exit.")
		(g_argShortLog.c_str(), "Shortens output of upgrade patches.")
		(g_argAcceptSafe.c_str(), "Accept all *safe* changes and write to input file.")
		(g_argAcceptUnsafe.c_str(), "Accept all *unsafe* changes and write to input file.")
		(
			g_argAllowPaths.c_str(),
			po::value<string>()->value_name("path(s)"),
			"Allow a given path for imports. A list of paths can be supplied by separating them with a comma. Defaults to \"*\""
		);

	po::options_description allOptions = desc;
	allOptions.add_options()("input-file", po::value<vector<string>>(), "input file");

	po::positional_options_description filesPositions;
	filesPositions.add("input-file", -1);

	// parse the compiler arguments
	try
	{
		po::command_line_parser cmdLineParser(_argc, _argv);
		cmdLineParser.style(po::command_line_style::default_style & (~po::command_line_style::allow_guessing));
		cmdLineParser.options(allOptions).positional(filesPositions);
		po::store(cmdLineParser.run(), m_args);
	}
	catch (po::error const& _exception)
	{
		cout << _exception.what() << endl;
		return false;
	}

	if (m_args.count(g_argHelp) || (isatty(fileno(stdin)) && _argc == 1))
	{
		cout << endl;
		cout << desc;
		return false;
	}

	if (m_args.count(g_argAllowPaths))
	{
		vector<string> paths;
		for (string const& path: boost::split(paths, m_args[g_argAllowPaths].as<string>(), boost::is_any_of(",")))
		{
			auto filesystem_path = boost::filesystem::path(path);
			// If the given path had a trailing slash, the Boost filesystem
			// path will have it's last component set to '.'. This breaks
			// path comparison in later parts of the code, so we need to strip
			// it.
			if (filesystem_path.filename() == ".")
				filesystem_path.remove_filename();
			m_allowedDirectories.push_back(filesystem_path);
		}
	}

	return true;
}

void SolidityUpgrade::printPrologue()
{
	cout << endl;
	cout << "solidity-upgrade does not support all breaking changes for each version." << endl <<
		"Please run `solidity-upgrade --help` and get a list of implemented upgrades." << endl;

	cout << endl;
	normalBold() << "Running analysis (and upgrade) on given source files..." <<
		endl << endl;
}

bool SolidityUpgrade::processInput()
{
	auto formatter = make_unique<langutil::SourceReferenceFormatterHuman>(cout, true);
	bool acceptSafe = m_args.count(g_argAcceptSafe);
	bool acceptUnafe = m_args.count(g_argAcceptUnsafe);

	// TODO Move to / share with solc commandline interface.
	ReadCallback::Callback fileReader = [this](string const&, string const& _path)
	{
		try
		{
			auto path = boost::filesystem::path(_path);
			auto canonicalPath = boost::filesystem::weakly_canonical(path);
			bool isAllowed = false;
			for (auto const& allowedDir: m_allowedDirectories)
			{
				// If dir is a prefix of boostPath, we are fine.
				if (
					std::distance(allowedDir.begin(), allowedDir.end()) <= std::distance(canonicalPath.begin(), canonicalPath.end()) &&
					std::equal(allowedDir.begin(), allowedDir.end(), canonicalPath.begin())
				)
				{
					isAllowed = true;
					break;
				}
			}
			if (!isAllowed)
				return ReadCallback::Result{false, "File outside of allowed directories."};

			if (!boost::filesystem::exists(canonicalPath))
				return ReadCallback::Result{false, "File not found."};

			if (!boost::filesystem::is_regular_file(canonicalPath))
				return ReadCallback::Result{false, "Not a valid file."};

			auto contents = dev::readFileAsString(canonicalPath.string());
			m_sourceCodes[path.generic_string()] = contents;
			return ReadCallback::Result{true, contents};
		}
		catch (Exception const& _exception)
		{
			return ReadCallback::Result{false, "Exception in read callback: " + boost::diagnostic_information(_exception)};
		}
		catch (...)
		{
			return ReadCallback::Result{false, "Unknown exception in read callback."};
		}
	};

	if (!readInputFiles())
		return false;

	resetCompiler(fileReader);
	tryCompile();

	/// Apply changes one-by-one or log them only.
	if (acceptSafe || acceptUnafe)
		acceptUpgrade();
	else
		for (auto& sourceCode: m_sourceCodes)
			analyzeAndLog(sourceCode);

	if (
		m_compiler->state() >= CompilerStack::State::CompilationSuccessful ||
		m_compiler->errors().empty()
	)
	{
		cout << endl;
		cyan() << "No errors or upgrades found!" << endl;
	}

	return true;
}

void SolidityUpgrade::analyzeAndLog(pair<string, string> const& _sourceCode)
{
	normal() << "Analyzing " << _sourceCode.first << "..." << endl;

	vector<UpgradeChange> changes;

	if (m_compiler->state() >= CompilerStack::State::AnalysisPerformed)
		Upgrade060{}.analyze(
			m_compiler->ast(_sourceCode.first),
			_sourceCode.second,
			changes
		);

	if (!changes.empty())
	{
		yellow() <<
			"Found upgrades which can be done by solidity-upgrade automatically."
			<< endl << endl;

		for (auto& change: changes)
			change.log(m_args.count(g_argShortLog));
	}
}

pair<bool, bool> SolidityUpgrade::analyzeAndUpgrade(pair<string, string> const& _sourceCode)
{
	normal() << "Analyzing and upgrading " << _sourceCode.first << "..." << endl;

	vector<UpgradeChange> changes;
	Upgrade060{}.analyze(
		m_compiler->ast(_sourceCode.first),
		_sourceCode.second,
		changes
	);

	auto applyChanges = [&](UpgradeChange& _change)
	{
		_change.apply();
		m_sourceCodes[_sourceCode.first] = _change.source();
		writeInputFile(_sourceCode.first, _change.source());
	};

	if (!changes.empty())
	{
		auto& change = changes.front();

		change.log(m_args.count(g_argShortLog));

		if (change.level() == UpgradeChange::Level::Safe)
		{
			if (m_args.count(g_argAcceptSafe))
			{
				applyChanges(change);
				return make_pair(true, true);
			}
		}
		else if (change.level() == UpgradeChange::Level::Unsafe)
		{
			if (m_args.count(g_argAcceptUnsafe))
			{
				applyChanges(change);
				return make_pair(true, true);
			}
		}
	}
	return make_pair(false, false);
}

void SolidityUpgrade::acceptUpgrade()
{
	bool recompile = false;
	bool terminate = false;

	while (!terminate && !m_compiler->errors().empty())
	{
		bool changesFound = false;

		for (auto& sourceCode: m_sourceCodes)
		{
			auto result = analyzeAndUpgrade(sourceCode);
			recompile = result.first;
			changesFound |= result.second;

			if (recompile)
				break;
		}

		terminate = !changesFound;

		if (recompile)
		{
			resetCompiler();
			tryCompile();
		}
	}
}

void SolidityUpgrade::resetCompiler()
{
	m_compiler->reset();
	m_compiler->setSources(m_sourceCodes);
	m_compiler->setParserErrorRecovery(true);
}

void SolidityUpgrade::resetCompiler(ReadCallback::Callback const& _callback)
{
	m_compiler.reset(new CompilerStack(_callback));
	m_compiler->setSources(m_sourceCodes);
	m_compiler->setParserErrorRecovery(true);
}

void SolidityUpgrade::tryCompile() const
{
	yellow() << "Running compilation phases..." << endl << endl;

	try
	{
		if (m_compiler->parse())
		{
			if (m_compiler->analyze())
				m_compiler->compile();
			else
				printErrors();
		}
		else
			printUnresolvableErrors();
	}
	catch (Exception const& _exception)
	{
		magenta() << "Exception during compilation: " << boost::diagnostic_information(_exception) << endl;
	}
	catch (std::exception const& _e)
	{
		magenta() << (_e.what() ? ": " + string(_e.what()) : ".") << endl;
	}
	catch (...)
	{
		magenta() << "Unknown exception during compilation." << endl;
	}
}

void SolidityUpgrade::printErrors() const
{
	auto formatter = make_unique<langutil::SourceReferenceFormatterHuman>(cout, true);

	magenta() <<
		"Compilation errors that solidity-upgrade may resolve occurred." <<
		endl << endl;

	for (auto const& error: m_compiler->errors())
		if (error->type() != langutil::Error::Type::Warning)
			formatter->printErrorInformation(*error);
}

void SolidityUpgrade::printUnresolvableErrors() const
{
	auto formatter = make_unique<langutil::SourceReferenceFormatterHuman>(cout, true);

	magenta() <<
		"Compilation errors that solidity-upgrade cannot resolve occurred." <<
		endl << endl;

	for (auto const& error: m_compiler->errors())
		if (error->type() != langutil::Error::Type::Warning)
			formatter->printErrorInformation(*error);
}

bool SolidityUpgrade::readInputFiles()
{
	bool ignoreMissing = m_args.count(g_argIgnoreMissingFiles);
	if (m_args.count(g_argInputFile))
		for (string path: m_args[g_argInputFile].as<vector<string>>())
		{
			auto infile = boost::filesystem::path(path);
			if (!boost::filesystem::exists(infile))
			{
				if (!ignoreMissing)
				{
					cout << infile << " is not found." << endl;
					return false;
				}
				else
					cout << infile << " is not found. Skipping." << endl;

				continue;
			}

			if (!boost::filesystem::is_regular_file(infile))
			{
				if (!ignoreMissing)
				{
					cout << infile << " is not a valid file." << endl;
					return false;
				}
				else
					cout << infile << " is not a valid file. Skipping." << endl;

				continue;
			}

			m_sourceCodes[infile.generic_string()] = dev::readFileAsString(infile.string());
			path = boost::filesystem::canonical(infile).string();
		}

	if (m_sourceCodes.size() == 0)
	{
		cout << "No input files given. If you wish to use the standard input please specify \"-\" explicitly." << endl;
		return false;
	}

	return true;
}

bool SolidityUpgrade::writeInputFile(string const& _path, string const& _source)
{
	cout << endl;
	AnsiColorized(cout, true, {formatting::YELLOW}) <<
		"Writing to input file " <<
		_path << "..." << endl;

	ofstream file(_path, ios::trunc);
	file << _source;

	return true;
}

}
}
