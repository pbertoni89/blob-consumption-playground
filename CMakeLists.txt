cmake_minimum_required (VERSION 3.16)

project (blob-consumption-playground)

set (CMAKE_CXX_STANDARD 17)

find_package (Boost 1.65.0 REQUIRED COMPONENTS log log_setup program_options thread)

add_executable (pool main-pool.cpp utils.cpp utils.h)
add_executable (async main-async.cpp utils.cpp utils.h)
add_executable (cv main-cv.cpp utils.cpp utils.h)

target_link_libraries (pool PRIVATE ${Boost_LOG_LIBRARY} ${Boost_PROGRAM_OPTIONS_LIBRARY} ${Boost_LOG_SETUP_LIBRARY})
target_link_libraries (async PRIVATE ${Boost_LOG_LIBRARY} ${Boost_PROGRAM_OPTIONS_LIBRARY} ${Boost_LOG_SETUP_LIBRARY})
target_link_libraries (cv PRIVATE ${Boost_LOG_LIBRARY} ${Boost_PROGRAM_OPTIONS_LIBRARY} ${Boost_LOG_SETUP_LIBRARY})
