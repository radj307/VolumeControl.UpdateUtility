#pragma once
#include "rc/version.h"

#include <ParamsAPI2.hpp>
#include <palette.hpp>

#include <filesystem>
#include <fstream>
#include <string>

/// @brief	Helper functor that prints the usage guide.
struct Help {
	const std::string& _name;

	Help(const std::string& programName) : _name{ programName } {}

	friend std::ostream& operator<<(std::ostream& os, const Help& h)
	{
		return os
			<< "VCUpdateUtility  v" << VCUpdateUtility_VERSION_EXTENDED << '\n'
			<< "  Volume Control Update Utility\n"
			<< '\n'
			<< "USAGE:\n"
			<< "  " << h._name << " <OPTIONS>" << '\n'
			<< '\n'
			<< "OPTIONS:\n"
			<< "  -h, --help             Prints this help display, then exits." << '\n'
			<< "  -v, --version          Prints the current version number, then exits." << '\n'
			<< "  -q, --quiet            Prevents most console output." << '\n'
			<< "  -u, --url <URI>        Specifies the target download URL." << '\n'
			<< "  -o, --out <PATH>       Specifies the output file location." << '\n'
			<< "                           If this already exists, it is renamed by appending '.backup' before downloading." << '\n'
			<< "  -s, --size <BYTES>     Specifies the amount of memory to reserve for streaming the file. (RECOMMENDED)" << '\n'
			<< "  -r, --restart          Attempts to start the new instance before the program exits." << '\n'
			<< "  -p, --pause            Waits for the user to press a key before exiting. This does nothing if '--redirect' is set." << '\n'
			<< "  -n, --no-color         Disables ANSI color escape sequences in terminal output."
			<< "      --redirect <PATH>  Redirects console output to the specified file." << '\n'
			<< "      --no-backup        Prevents a backup from being made at all." << '\n'
			<< "      --keep-backup      Doesn't delete the backup file once the download has successfully completed." << '\n'
			<< '\n'
			<< "RETURNS:\n"
			<< "  0                     Success" << '\n'
			<< "  1                     Failure" << '\n'
			<< "  2                     Download Error" << '\n'
			;
	}
};

enum class Color : char {
	STATUS_OK,
	STATUS_ERROR,
	STATUS_UNEXPECTED,
	ELAPSED_TIME,
	URL,
	PATH,
	SIZE,
};

/// @brief	Static global variables.
static struct {
	/// @brief	Prevents most console output
	bool quiet{ false };
	std::string url;
	std::filesystem::path out, backup;
	std::ofstream logRedirect;
	bool useLogRedirect{ false };
	bool noBackup{ false };
	bool keepBackup{ false };
	bool restart{ false };
	long bufferLenBytes{ 1024 };
	bool pauseBeforeExit{ false };
	term::palette<Color> Palette{
		{ Color::STATUS_OK, color::setcolor::green },
		{ Color::STATUS_ERROR, color::setcolor::red },
		{ Color::STATUS_UNEXPECTED, color::setcolor(color::orange) },
		{ Color::ELAPSED_TIME, color::setcolor::yellow },
		{ Color::URL, color::setcolor::cyan },
		{ Color::PATH, color::setcolor::yellow },
		{ Color::SIZE, color::setcolor::yellow },
	};

	/**
	 * @brief		Performs value initialization for the static Global struct.
	 * @param args	The arguments object from main.
	 */
	static void init(opt::ParamsAPI2 const& args)
	{
		// BOOLEANS
		Global.quiet = args.check_any<opt::Flag, opt::Option>('q', "quiet");
		Global.noBackup = args.check_any<opt::Option>("no-backup");
		Global.keepBackup = args.check_any<opt::Option>("keep-backup");
		Global.restart = args.check_any<opt::Flag, opt::Option>('r', "restart");
		Global.pauseBeforeExit = args.check_any<opt::Flag, opt::Option>('p', "pause");

		// URL
		if (const auto& urlArg{ args.typegetv_any<opt::Flag, opt::Option>('u', "url") }; urlArg.has_value())
			Global.url = urlArg.value();

		// OUT
		if (const auto& outArg{ args.typegetv_any<opt::Flag, opt::Option>('o', "out") }; outArg.has_value())
			Global.out = std::filesystem::path{ outArg.value() };

		// SIZE
		if (const auto& sizeArg{ args.typegetv_any<opt::Flag, opt::Option>('s', "size") }; sizeArg.has_value())
			Global.bufferLenBytes = str::stol(sizeArg.value());

		// NO-COLOR
		if (args.check_any<opt::Flag, opt::Option>('n', "no-color"))
			Global.Palette.setEnabled(false);

		// REDIRECT
		if (const auto& redirectArg{ args.typegetv_any<opt::Option>("redirect") }; redirectArg.has_value()) {
			Global.useLogRedirect = true;
			Global.logRedirect = std::ofstream{ redirectArg.value() };
			// Disable escape sequences when redirecting to a file
			Global.Palette.setEnabled(false);
		}
	}
} Global;

