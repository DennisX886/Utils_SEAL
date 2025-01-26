#cmake_minimum_required(VERSION 3.10)
#project(SEAL_test)
#set(CMAKE_CXX_STANDARD 17)
#find_library(SEAL_LIBRARY seal
 #            PATHS ${CMAKE_HOME_DIRECTORY}/SEAL/native/src)
#if (SEAL_INCLUDE_DIRS AND SEAL_LIBRARY)
 #   message(STATUS "SEAL library found:")
 #   message(STATUS "  Includes: ${SEAL_INCLUDE_DIRS}")
 #   message(STATUS "  Libraries: ${SEAL_LIBRARY}")
#else()
 #   message(FATAL_ERROR "SEAL library not found. Please check the SEAL library installation path.")
#endif()

#include_directories(/home/dennisx/SEAL/native/src)
#include_directories(/home/dennisx/SEAL/native/src/seal/util)
#include_directories(/home/dennisx/SEAL/build/thirdparty/msgsl-src/include)
#include_directories(/home/dennisx/SEAL/build/lib)
#add_executable(lnx_exe src/lnx.cpp)
#link_directories(/home/dennisx/SEAL/build/lib)
#target_link_libraries(lnx_exe ${SEAL_LIBRARY})
cmake_minimum_required(VERSION 3.10)
project(SEAL_test)

# 查找 SEAL 库
find_package(SEAL 4.1.2 REQUIRED)

# 添加可执行文件
add_executable(lnx_exe src/lnx.cpp)

# 链接 SEAL 库
target_link_libraries(lnx_exe SEAL::seal)
