#include "rc/version.h"
#include "StreamRedirect.hpp"
#include "Global.hpp"
#include "win.h"

#include <TermAPI.hpp>
#include <envpath.hpp>
#include <cpr/cpr.h>

#include <iostream>

#if OS_WIN
#include <Windows.h>
#else
#error VCUpdateUtility does not support non-windows operating systems!
#endif

#define APP_MUTEX_IDENTIFIER "VolumeControlSingleInstance"

HANDLE appMutex{ nullptr };

int main(const int argc, char** argv)
{
	std::cout << term::EnableANSI;

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

		if (!Global.quiet) {
			std::cout << Global.Palette.get_msg() << "Target URL:  '" << Global.Palette(Color::URL) << Global.url << Global.Palette() << '\'' << std::endl;
			std::cout << Global.Palette.get_msg() << "Output Path: '" << Global.Palette(Color::PATH) << Global.out.generic_string() << Global.Palette() << '\'';
			if (Global.out.is_relative())
				std::cout << "  ( '" << Global.Palette(Color::PATH) << (myPath / Global.out).generic_string() << Global.Palette() << "' )" << std::endl;
			else std::cout << std::endl;
		}

		// send a GET request for the file
		cpr::Session session;
		session.SetUrl(Global.url);
		session.ResponseStringReserve(Global.bufferLenBytes);
		session.SetHeader(cpr::Header{ { "User-Agent", "curl/7.64.1" }, { "accept", "application/octet-stream" } });

		// make a backup if that is enabled
		if (!Global.noBackup && file::exists(Global.out)) {
			Global.backup = std::filesystem::path{ Global.out.generic_string() + ".backup" };
			std::filesystem::rename(Global.out, Global.backup);
			if (!Global.quiet)
				std::cout << Global.Palette.get_msg() << "Created backup '" << Global.Palette(Color::PATH) << Global.out << Global.Palette() << "' => '" << Global.Palette(Color::PATH) << Global.backup << Global.Palette() << '\'' << std::endl;
		}

		const TIMEP tBegin{ CLK::now() };

		// open a file stream
		std::ofstream ofs{ Global.out, std::ios_base::binary };
		// download the file
		auto response{ session.Download(ofs) };

		const TIMEP tEnd{ CLK::now() };

		std::cout << Global.Palette.get_msg() << "Download completed after " << Global.Palette(Color::ELAPSED_TIME) << std::chrono::duration_cast<DUR>(tEnd - tBegin).count() << Global.Palette() << " ms" << std::endl;

		// get the file length
		auto len{ ofs.tellp() };
		// close the file stream
		ofs.flush();
		ofs.close();

		bool error{ false };
		// check the response code
		switch (response.status_code) {
		case 200://< success
			std::cerr << Global.Palette.get_msg() << "Received Success Response Code " << Global.Palette(Color::STATUS_OK) << 200 << Global.Palette() << '\n';
			break;
		case 400:
			std::cerr << Global.Palette.get_error() << "Received Error Response Code " << Global.Palette(Color::STATUS_ERROR) << 400 << Global.Palette() << " (Bad Request)" << std::endl;
			error = true;
			break;
		case 401:
			std::cerr << Global.Palette.get_error() << "Received Error Response Code " << Global.Palette(Color::STATUS_ERROR) << 401 << Global.Palette() << " (Unauthorized)" << std::endl;
			error = true;
			break;
		case 403:
			std::cerr << Global.Palette.get_error() << "Received Error Response Code " << Global.Palette(Color::STATUS_ERROR) << 403 << Global.Palette() << " (Forbidden)" << std::endl;
			error = true;
			break;
		case 404:
			std::cerr << Global.Palette.get_error() << "Received Error Response Code " << Global.Palette(Color::STATUS_ERROR) << 404 << Global.Palette() << " (Not Found)" << std::endl;
			error = true;
			break;
		default:
			std::cerr << Global.Palette.get_warn() << "Received Unexpected Response Code " << Global.Palette(Color::STATUS_UNEXPECTED) << response.status_code << Global.Palette() << std::endl;
			break;
		}

		// file length is 0, or the request status code indicates an error
		if (error || len == 0) {
			// remove any corrupted file
			if (file::exists(Global.out))
				std::filesystem::remove(Global.out);

			// restore the file from backup
			if (file::exists(Global.backup)) {
				std::filesystem::rename(Global.backup, Global.out);
				if (!Global.quiet)
					std::cout << Global.Palette.get_msg() << "Restored '" << Global.Palette(Color::PATH) << Global.backup << Global.Palette() << "' => '" << Global.Palette(Color::PATH) << Global.out << Global.Palette() << '\'' << std::endl;
			}

			return 2;
		}
		// Successful:
		if (!Global.quiet)
			std::cout << Global.Palette.get_msg() << "Wrote " << Global.Palette(Color::SIZE) << len << Global.Palette() << " bytes to '" << Global.out.generic_string() << "'" << std::endl;

		// delete the backup file once we're done
		if (!Global.keepBackup && file::exists(Global.backup)) {
			if (std::filesystem::remove(Global.backup))
				std::cout << Global.Palette.get_msg() << "Deleted Backup '" << Global.backup.generic_string() << '\'' << std::endl;
			else // failed to delete file
				std::cout << Global.Palette.get_warn() << "Failed to delete backup '" << Global.backup.generic_string() << '\'' << std::endl;
		}

		// restart the program if enabled
		if (Global.restart) {
			if (start_process(Global.out.generic_string() + " --cleanup"))
				std::cout << Global.Palette.get_msg() << "(Re)started '" << Global.out.filename().generic_string() << "'." << std::endl;
			else std::cerr
				<< Global.Palette.get_error() << "Failed to (re)start '" << Global.out.filename().generic_string() << "'!\n"
				<< Global.Palette.get_placeholder() << "Error Message:  '" << GetLastErrorAsString() << '\'' << std::endl;
		}

		// Release mutex
		if (appMutex != nullptr)
			CloseHandle(appMutex);

		// pause
		if (Global.pauseBeforeExit && !Global.useLogRedirect) {
			std::cout << "Press any key to exit..." << std::endl;
			while (!term::kbhit()) {}
		}

		return 0;
	} catch (const std::exception& ex) {
		std::cerr << Global.Palette.get_fatal() << ex.what() << std::endl;
		// Release mutex
		if (appMutex != nullptr)
			CloseHandle(appMutex);
		// pause
		if (Global.pauseBeforeExit && !Global.useLogRedirect) {
			std::cout << "Press any key to exit..." << std::endl;
			while (!term::kbhit()) {}
		}

		return 1;
	} catch (...) {
		std::cerr << Global.Palette.get_fatal() << "An undefined exception occurred!" << std::endl;
		// Release mutex
		if (appMutex != nullptr)
			CloseHandle(appMutex);
		// pause
		if (Global.pauseBeforeExit && !Global.useLogRedirect) {
			std::cout << "Press any key to exit..." << std::endl;
			while (!term::kbhit()) {}
		}

		return 1;
	}
}
