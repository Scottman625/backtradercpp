# backtradercpp -- Multi-Stock Backtesting Library in C++ 20
# backtradercpp -- 支援多股回測的 C++ 20 回測框架

[English](#english) | [繁體中文](#繁體中文)

# English

This library was originally inspired by [backtrader](https://github.com/kilasuelika/backtradercpp/). To make backtesting more closely simulate real trading conditions, I decided to add multi-stock backtesting functionality.

## Installation

### `1. Required Tools`
1. Visual Studio 2022 (ensure C++ development tools are installed)
2. CMake (3.10 or newer)
3. Python 3.11 (must match the Python version in vcpkg) #For testing

### `2. Install vcpkg`

This is a header-only library. However, you need to install some dependencies. On Windows:
```bash
# 1. Clone vcpkg to your chosen directory
git clone https://github.com/Microsoft/vcpkg.git

# 2. Enter vcpkg directory
cd vcpkg

# 3. Run bootstrap-vcpkg.bat
.\bootstrap-vcpkg.bat

# 4. Install required packages (x64-windows)
.\vcpkg install boost:x64-windows fmt:x64-windows libfort:x64-windows range-v3:x64-windows pybind11:x64-windows
```

Note: After installation, remember to set the vcpkg root directory path in the `CMAKE_TOOLCHAIN_FILE` variable in CMakeLists.txt.

### `3. Install Python Dependencies`
```
pip install pybind11
```

### `4. Project Configuration and Compilation`
```bash
# 1. Create build directory
mkdir build
cd build

# 2. Run CMake configuration
# First set CMAKE_TOOLCHAIN_FILE environment variable to point to your vcpkg.cmake
set CMAKE_TOOLCHAIN_FILE=<your-vcpkg-path>/scripts/buildsystems/vcpkg.cmake
cmake -S .. -B .

# 3. Build project
cmake --build . --config Release
```

###  Multi-Stock Backtesting Example

```
python test/backtrader_test.py
```

---

# 繁體中文

這個函式庫原本是受到 [backtrader](https://github.com/kilasuelika/backtradercpp/) 啟發而開發。為了在回測時更加貼近實際交易的模擬情況，我決定加入多股回測的功能。

## 安裝

### `1. 必要工具安裝`
1. Visual Studio 2022 (確保安裝 C++ 開發工具)
2. CMake (3.10 或更新版本)
3. Python 3.11(需要匹配vcpkg中的python版本) #測試用

### `2. 安裝 vcpkg`

這是一個僅包含標頭檔的函式庫。但您需要安裝一些相依套件。在 Windows 上:
```bash
# 1. 克隆 vcpkg 到您選擇的目錄
git clone https://github.com/Microsoft/vcpkg.git

# 2. 進入 vcpkg 目錄
cd vcpkg

# 3. 執行 bootstrap-vcpkg.bat
.\bootstrap-vcpkg.bat

# 4. 安裝必要的套件 (x64-windows)
.\vcpkg install boost:x64-windows fmt:x64-windows libfort:x64-windows range-v3:x64-windows pybind11:x64-windows
```

注意：安裝完成後，請記得將 vcpkg 的根目錄路徑設置到 CMakeLists.txt 中的 `CMAKE_TOOLCHAIN_FILE` 變數。

### `3. 安裝 Python 相關套件`
```
pip install pybind11
```

### `4. 專案配置與編譯`
```bash
# 1. 創建建構目錄
mkdir build
cd build

# 2. 執行 CMake 配置
# 先設置環境變數 CMAKE_TOOLCHAIN_FILE 指向您的 vcpkg.cmake
set CMAKE_TOOLCHAIN_FILE=<您的vcpkg路徑>/scripts/buildsystems/vcpkg.cmake
cmake -S .. -B .

# 3. 建構專案
cmake --build . --config Release
```

### 多股回測範例

```
python test/backtrader_test.py
```

