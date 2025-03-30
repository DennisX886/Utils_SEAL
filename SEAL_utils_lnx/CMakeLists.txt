cmake_minimum_required(VERSION 3.10)
project(utils_lnx)
find_package(SEAL 4.1.2 REQUIRED)
find_package(NLopt  REQUIRED)
add_executable(lnx_calculate src/main.cpp )
include_directories(include)
target_link_libraries(lnx_calculate SEAL::seal)
target_link_libraries(lnx_calculate ${NLOPT_LIBRARIES})

#如果需要添加一个新的库：
#首先在github上下载zip,解压后在库的文件夹里创建build文件夹，并在build文件夹中编译一次
#然后修改CMakeLists.txt：假设库的名字时lib_name，那么在CMakeLists.txt中添加：
#find_package(lib_name REQUIRED)
#target_link_libraries(lnx_calculate lib_name::lib_name)
#最后在用到该库的代码片段中include该库的头文件
