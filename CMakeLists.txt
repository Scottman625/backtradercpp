cmake_minimum_required(VERSION 3.10)
project(MyProject)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# 使用 vcpkg 工具鏈檔案
set(CMAKE_TOOLCHAIN_FILE "D:/code/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "Vcpkg toolchain file")

set(CMAKE_PREFIX_PATH "D:/code/vcpkg/installed/x64-windows" CACHE STRING "Vcpkg installation prefix")
set(fmt_DIR "D:/code/vcpkg/installed/x64-windows/include/fmt")
set(BOOST_ROOT "D:/code/vcpkg/installed/x64-windows")
set(BOOST_LIBRARYDIR "D:/code/vcpkg/installed/x64-windows/lib")
set(libfort_DIR "D:/code/vcpkg/installed/x64-windows/include/libfort")

# 首先尋找 Python
find_package(Python 3.11 REQUIRED COMPONENTS Interpreter Development)

# 獲取本機 pybind11 的路徑
execute_process(
    COMMAND "${Python_EXECUTABLE}" -c "import pybind11; print(pybind11.get_cmake_dir())"
    OUTPUT_VARIABLE PYBIND11_CMAKE_DIR
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

# 設置 pybind11 的 CMake 目錄
set(pybind11_DIR "${PYBIND11_CMAKE_DIR}")

# 尋找 pybind11
find_package(pybind11 CONFIG REQUIRED)

find_package(fmt CONFIG REQUIRED)

# 查找並鏈接 Boost 函式庫
set(Boost_USE_STATIC_LIBS ON) # 强制使用靜態函式庫
find_package(Boost 1.85.0 REQUIRED COMPONENTS system filesystem serialization)

# 查找并链接 libfort 函式庫（確保您的 vcpkg 已安裝 libfort）
find_package(libfort CONFIG REQUIRED)

# 包含 range-v3 库的頭文件路徑
find_package(range-v3 CONFIG REQUIRED)

# 設置編譯標誌
add_compile_options("/bigobj") # 添加編譯選項 /bigobj

# 設置 Python Debug 標誌
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_definitions(PYTHON_DEBUG_LIB)
endif()

# 確保使用正確的 Python 函式庫
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(Python_LIBRARY_SUFFIX "_d")
endif()

# 建立共享函式庫
pybind11_add_module(backtradercpp
    vs_examples/basic_1/basic_1.cpp
)

# 設置正確的運行時函式庫
if(MSVC)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        set_target_properties(backtradercpp PROPERTIES
            MSVC_RUNTIME_LIBRARY "MultiThreadedDebugDLL")
    else()
        set_target_properties(backtradercpp PROPERTIES
            MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")
    endif()
endif()

# 鏈接 fmt, Boost, 和 libfort
target_link_libraries(backtradercpp PRIVATE range-v3)
target_link_libraries(backtradercpp ${DataFrame_LIBRARIES})
target_link_libraries(backtradercpp PRIVATE fmt::fmt Boost::filesystem Boost::system Boost::serialization libfort::fort)
target_link_libraries(backtradercpp PRIVATE pybind11::module)