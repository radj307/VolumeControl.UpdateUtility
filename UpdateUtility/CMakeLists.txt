# VCUpdateUtility/UpdateUtility
cmake_minimum_required(VERSION 3.20)

FILE(MAKE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/rc")


include(VersionTag)
MAKE_VERSION_HEADER("${CMAKE_CURRENT_SOURCE_DIR}/rc/version.h" "VCUpdateUtility" "${VCUpdateUtility_VERSION}")

#if (WIN32)
#	SET(RC_FILE "${CMAKE_CURRENT_SOURCE_DIR}/rc/VCUpdateUtility.rc")
#	FILE(TOUCH "${RC_FILE}")
#	include(ResourceMaker)
#
#	MAKE_STRINGRC_VERSIONINFO(
#		_versioninfo_stringrc
#		"${VCUpdateUtility_VERSION}"
#		"GPLv3"
#		"radj307"
#		"Volume Control Update Utility"
#		"The self-update utility for Volume Control."
#	)

#	MAKE_RESOURCE("${RC_FILE}" "${_versioninfo_stringrc}")
#endif()

set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
include(FetchContent)
FetchContent_Declare(cpr GIT_REPOSITORY https://github.com/libcpr/cpr.git
                         GIT_TAG db351ffbbadc6c4e9239daaa26e9aefa9f0ec82d) # The commit hash for 1.8.3
FetchContent_MakeAvailable(cpr)


file(GLOB SRCS
	CONFIGURE_DEPENDS
	RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
	"*.c*"
)
file(GLOB HEADERS
	CONFIGURE_DEPENDS
	RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
	"*.c*"
)

# CREATE EXECUTABLE
add_executable(VCUpdateUtility "${SRCS}" "${RC_FILE}")

# SET PROPERTIES
set_property(TARGET VCUpdateUtility PROPERTY CXX_STANDARD 20)
set_property(TARGET VCUpdateUtility PROPERTY CXX_STANDARD_REQUIRED ON)
set_property(TARGET VCUpdateUtility PROPERTY POSITION_INDEPENDENT_CODE ON)
if (MSVC)
	target_compile_options(VCUpdateUtility PUBLIC "/Zc:__cplusplus" "/Zc:preprocessor")
endif()

# LINK DEPENDENCIES
target_link_libraries(VCUpdateUtility PRIVATE shared TermAPI filelib optlib cpr::cpr)

target_sources(VCUpdateUtility PUBLIC "${HEADERS}")

include(PackageInstaller)
INSTALL_EXECUTABLE(VCUpdateUtility "${CMAKE_INSTALL_PREFIX}/bin/")
