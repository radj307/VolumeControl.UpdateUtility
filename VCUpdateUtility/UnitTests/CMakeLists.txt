# VCUpdateUtility
cmake_minimum_required(VERSION 3.20)

include(CTest)

add_test(NAME CLI_HELP COMMAND VCUpdateUtility --help)
set_property(TEST CLI_HELP PROPERTY PASS_REGULAR_EXPRESSION "VCUpdateUtility  v(?:.+\s*\n)+OPTIONS:")
