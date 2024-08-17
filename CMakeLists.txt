cmake_minimum_required(VERSION 3.10)
project(MyProject)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# 使用 vcpkg 工具链文件
set(CMAKE_TOOLCHAIN_FILE "/Users/houkiyoshiketsu/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "Vcpkg toolchain file")

set(pybind11_DIR "/opt/homebrew/opt/pybind11")
set(fmt_DIR "/Users/houkiyoshiketsu/vcpkg/installed/arm64-osx/share/fmt")
set(Boost_DIR "/opt/homebrew/opt/boost/include/boost")
set(libfort_DIR "/Users/houkiyoshiketsu/vcpkg/installed/arm64-osx/share/libfort")
set(range-v3_DIR "/Users/houkiyoshiketsu/vcpkg/installed/arm64-osx/share/range-v3")
set(DataFrame_DIR "/Users/houkiyoshiketsu/vcpkg/installed/arm64-osx/share/dataframe")

set(Boost_NO_BOOST_CMAKE ON)

include_directories("/Users/houkiyoshiketsu/vcpkg/installed/arm64-osx/include/eigen3")
include_directories("/Users/houkiyoshiketsu/vcpkg/installed/arm64-osx/include")
include_directories("/Users/houkiyoshiketsu/vcpkg/installed/arm64-osx/include")

# 查找 pybind11
find_package(pybind11 CONFIG REQUIRED)

# 查找并链接 fmt 库
find_package(fmt CONFIG REQUIRED)

# 查找并链接 Boost 库
set(Boost_USE_STATIC_LIBS ON) # 强制使用静态库
find_package(Boost 1.85.0 REQUIRED COMPONENTS system filesystem serialization)

# 查找并链接 libfort 库
find_package(libfort CONFIG REQUIRED)

# 查找并链接 range-v3 库
find_package(range-v3 CONFIG REQUIRED)

# 查找并链接 DataFrame 库
find_package(DataFrame REQUIRED)

# 设置编译标志
add_compile_options("-fPIC") # macOS 上通常使用 -fPIC 标志

# 创建共享库
pybind11_add_module(backtradercpp
    vs_examples/basic_1/basic_1.cpp
)

# 链接库
target_link_libraries(backtradercpp PRIVATE range-v3)
target_link_libraries(backtradercpp ${DataFrame_LIBRARIES})
target_link_libraries(backtradercpp PRIVATE fmt::fmt Boost::filesystem Boost::system Boost::serialization libfort::fort)
target_link_libraries(backtradercpp PRIVATE pybind11::module)
