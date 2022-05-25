#include "rc/version.h"
#include "StreamRedirect.hpp"
#include "win.h"

#include <TermAPI.hpp>
#include <ParamsAPI2.hpp>
#include <envpath.hpp>
#include <cpr/cpr.h>

#include <iostream>
#include <Windows.h>

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

/// @brief	Static global variables.
static struct {
	bool quiet{ false };
	std::string url;
	std::filesystem::path out, backup;
	std::ofstream logRedirect;
	bool useLogRedirect{ false };
	bool noBackup{ false };
	bool keepBackup{ false };
	bool restart{ false };
	long bufferLenBytes{ 1024 };

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

		// URL
		if (const auto& urlArg{ args.typegetv_any<opt::Flag, opt::Option>('u', "url") }; urlArg.has_value())
			Global.url = urlArg.value();

		// OUT
		if (const auto& outArg{ args.typegetv_any<opt::Flag, opt::Option>('o', "out") }; outArg.has_value())
			Global.out = std::filesystem::path{ outArg.value() };

		// SIZE
		if (const auto& sizeArg{ args.typegetv_any<opt::Flag, opt::Option>('s', "size") }; sizeArg.has_value())
			Global.bufferLenBytes = str::stol(sizeArg.value());

		// REDIRECT
		if (const auto& redirectArg{ args.typegetv_any<opt::Option>("redirect") }; redirectArg.has_value()) {
			Global.useLogRedirect = true;
			Global.logRedirect = std::ofstream{ redirectArg.value() };
		}
	}
} Global;

#define APP_MUTEX_IDENTIFIER "VolumeControlSingleInstance"

HANDLE appMutex{ nullptr };

int main(const int argc, char** argv)
{
	using CLK = std::chrono::high_resolution_clock;
	using DUR = std::chrono::duration<double, std::milli>;
	using TIMEP = std::chrono::time_point<CLK, DUR>;

	// Acquire multi-process multi lock
	appMutex = CreateMutex(
		NULL,
		TRUE,
		APP_MUTEX_IDENTIFIER
	);

	// Begin update procedure
	try {
		WaitForSingleObject(appMutex, INFINITE);

		opt::ParamsAPI2 args{ argc, argv, 'u', "url", 'o', "out", 's', "size", "redirect" };
		env::PATH env;
		const auto& [myPath, myName] { env.resolve_split(argv[0]) };
		Global.init(args);

		// redirect console output to a file, if enabled.
		StreamRedirect out{ std::cout }, err{ std::cerr };
		if (Global.useLogRedirect) {
			out.redirect(Global.logRedirect.rdbuf());
			err.redirect(Global.logRedirect.rdbuf());
		}

		// help
		if (args.check_any<opt::Flag, opt::Option>('h', "help") || args.empty()) {
			std::cout << Help(myName.generic_string()) << std::endl;
			return 0;
		}
		// version
		else if (args.check_any<opt::Flag, opt::Option>('v', "version")) {
			if (!Global.quiet) std::cout << "VCUpdateUtility  v";
			std::cout << VCUpdateUtility_VERSION_EXTENDED << '\n';
			return 0;
		}

		// validate inputs
		if (Global.url.empty())
			throw make_exception("No target URL was specified!");
		else if (Global.out.empty())
			throw make_exception("No output path was specified!");

		std::cout << term::get_msg() << "Target URL:  '" << Global.url << '\'' << std::endl;
		std::cout << term::get_msg() << "Output Path: '" << Global.out.generic_string() << '\'';
		if (Global.out.is_relative())
			std::cout << "  ( '" << (myPath / Global.out).generic_string() << "' )" << std::endl;
		else std::cout << std::endl;

		// send a GET request for the file
		cpr::Session session;
		session.SetUrl(Global.url);
		session.ResponseStringReserve(Global.bufferLenBytes);
		session.SetHeader(cpr::Header{ { "User-Agent", "curl/7.64.1" }, { "accept", "application/octet-stream" } });

		// make a backup if that is enabled
		if (!Global.noBackup && file::exists(Global.out)) {
			Global.backup = std::filesystem::path{ Global.out.generic_string() + ".backup" };
			std::filesystem::rename(Global.out, Global.backup);
		}

		const TIMEP tBegin{ CLK::now() };

		// open a file stream
		std::ofstream ofs{ Global.out, std::ios_base::binary };
		// download the file
		auto response{ session.Download(ofs) };

		const TIMEP tEnd{ CLK::now() };

		std::cout << term::get_msg() << "Download completed after " << term::setcolor::yellow << std::chrono::duration_cast<DUR>(tEnd - tBegin).count() << term::setcolor::reset << " ms" << std::endl;

		// get the file length
		auto len{ ofs.tellp() };
		// close the file stream
		ofs.flush();
		ofs.close();

		bool error{ false };
		// check the response code
		switch (response.status_code) {
		case 200://< success
			std::cerr << term::get_msg() << "Received Success Response Code " << term::setcolor::green << 200 << term::setcolor::reset << '\n';
			break;
		case 400:
			std::cerr << term::get_error() << "Received Error Response Code " << term::setcolor::red << 400 << term::setcolor::reset << " (Bad Request)" << std::endl;
			error = true;
			break;
		case 401:
			std::cerr << term::get_error() << "Received Error Response Code " << term::setcolor::red << 401 << term::setcolor::reset << " (Unauthorized)" << std::endl;
			error = true;
			break;
		case 403:
			std::cerr << term::get_error() << "Received Error Response Code " << term::setcolor::red << 403 << term::setcolor::reset << " (Forbidden)" << std::endl;
			error = true;
			break;
		case 404:
			std::cerr << term::get_error() << "Received Error Response Code " << term::setcolor::red << 404 << term::setcolor::reset << " (Not Found)" << std::endl;
			error = true;
			break;
		default:
			std::cerr << term::get_warn() << "Received Unexpected Response Code " << term::setcolor::intense_yellow << response.status_code << term::setcolor::reset << std::endl;
			error = true;
			break;
		}


		// file length is 0
		if (error || len == 0) {
			// remove any corrupted file
			if (file::exists(Global.out))
				std::filesystem::remove(Global.out);

			// restore the file from backup
			if (file::exists(Global.backup)) {
				std::filesystem::rename(Global.backup, Global.out);
				std::cout << term::get_msg() << "Restored original file from backup." << std::endl;
			}

			return 2;
		}
		// Successful:
		std::cout << term::get_msg() << "Wrote " << term::setcolor::yellow << len << term::setcolor::reset << " bytes to '" << Global.out.generic_string() << "'" << std::endl;

		// delete the backup file once we're done
		if (!Global.keepBackup && file::exists(Global.backup)) {
			if (std::filesystem::remove(Global.backup))
				std::cout << term::get_msg() << "Deleted '" << Global.backup.generic_string() << '\'' << std::endl;
			else // failed to delete file
				std::cout << term::get_warn() << "Failed to delete '" << Global.backup.generic_string() << '\'' << std::endl;
		}

		// restart the program if enabled
		if (Global.restart) {
			if (start_process(Global.out.generic_string() + " --cleanup"))
				std::cout << term::get_msg() << "Started '" << Global.out.filename().generic_string() << "'." << std::endl;
			else {
				std::cerr
					<< term::get_error() << "Failed to start '" << Global.out.filename().generic_string() << "'!\n"
					<< term::get_placeholder() << "Error Message:  '" << GetLastErrorAsString() << '\'' << std::endl;
				;
			}
		}

		if (appMutex != nullptr)
			CloseHandle(appMutex);
		return 0;
	} catch (const std::exception& ex) {
		std::cerr << term::get_fatal() << ex.what() << std::endl;

		if (appMutex != nullptr)
			CloseHandle(appMutex);
		return 1;
	} catch (...) {
		std::cerr << term::get_fatal() << "An undefined exception occurred!" << std::endl;

		if (appMutex != nullptr)
			CloseHandle(appMutex);
		return 1;
	}
}
