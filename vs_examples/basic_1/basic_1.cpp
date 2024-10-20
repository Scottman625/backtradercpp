#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>
#include "../../include/backtradercpp/Cerebro.hpp"
#include "../../include/backtradercpp/DataFeeds.hpp"
#include <pybind11/stl.h>
#include <iostream>
#include <fmt/core.h>
#include <fmt/format.h>
#include <windows.h>
#include <thread>
#include <chrono>
#include <memory>
#include <exception>
#include <nlohmann/json.hpp>

// 使用 nlohmann/json 库
using json = nlohmann::json;

namespace py = pybind11;

using namespace backtradercpp;

struct SimpleStrategy : strategy::GenericStrategy {
    void run() override {
        // 每 50 個時間步長執行一次買入操作
        std::cout << "time_index(): " << time_index() << std::endl;
        if (time_index() % 2 == 0) {
            // 遍歷所有股票數據
            for (const auto& stock : datas()) {
                const std::string& ticker = stock.first;  // 股票代號
                const std::vector<PriceFeedData>& data_vector = stock.second;

                // 確保有數據並操作資產
                for (int j = 0; j < data_vector.size(); ++j) {
                    const PriceFeedData& data = data_vector[j];  // 每個時間步的數據
                    if (data.valid.coeff(j)) {
                        // 執行買入操作
                        buy(ticker, j, data.data.open.coeff(j), 100);
                    }
                }
            }
        }

        // 每 100 個時間步長的第 19 個時間步執行一次賣出操作
        if (time_index() % 2 == 1) {
            for (const auto& stock : datas()) {
                const std::string& ticker = stock.first;
                const std::vector<PriceFeedData>& data_vector = stock.second;

                for (int j = 0; j < data_vector.size(); ++j) {
                    const PriceFeedData& data = data_vector[j];
                    if (data.valid.coeff(j)) {
                        // 執行賣出操作
                        close(ticker, j, data.data.open.coeff(j));
                    }
                }
            }
        }
    }
};


struct DataFrame {
    std::vector<std::vector<std::string>> data;
    std::vector<std::string> columns;
};

void printVector(const std::vector<std::string>& vec, const std::string& label) {
    std::cout << label << ": ";
    for (const auto& str : vec) {
        std::cout << str << " ";
    }
    std::cout << std::endl;
}

void setConsoleUtf8() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#else
    // Unix-like systems usually do not need to set console output encoding
#endif
}

// struct CustomObject {
//     py::array_t<double> array;
//     std::vector<std::string> vec1;
//     std::vector<std::string> vec2;
//     std::vector<std::string> vec3;
// };

// struct DataObject {
//     std::vector<std::vector<double>> ohlc_data;
//     std::vector<std::string> date_vector;
//     std::vector<std::string> stock_name_vector;
//     std::vector<std::string> stock_vector;
// };

// // 将 DataObject 序列化为 JSON 字符串
// std::string serializeData(const DataObject& data) {
//     json j;
//     j["ohlc_data"] = data.ohlc_data;
//     j["date_vector"] = data.date_vector;
//     j["stock_name_vector"] = data.stock_name_vector;
//     j["stock_vector"] = data.stock_vector;
//     return j.dump(); // 返回 JSON 字符串
// }

// // 从 JSON 字符串反序列化为 DataObject
// DataObject deserializeData(const std::string& dataStr) {
//     json j = json::parse(dataStr);
//     DataObject data;
//     data.ohlc_data = j["ohlc_data"].get<std::vector<std::vector<double>>>();
//     data.date_vector = j["date_vector"].get<std::vector<std::string>>();
//     data.stock_name_vector = j["stock_name_vector"].get<std::vector<std::string>>();
//     data.stock_vector = j["stock_vector"].get<std::vector<std::string>>();
//     return data;
// }


// Helper function to convert std::string to std::wstring
// std::wstring stringToWstring(const std::string& str) {
//     int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
//     std::wstring wstrTo(size_needed, 0);
//     MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
//     return wstrTo;
// }

// void runBacktrader(py::array_t<double> ohlc_data, 
//                    const std::vector<std::string>& date_vector, 
//                    const std::vector<std::string>& stock_name_vector, 
//                    const std::vector<std::string>& stock_vector) {
//     SetConsoleOutputCP(CP_UTF8);
//     std::cout << "Running runBacktrader" << std::endl;

//     try {

//         // Initialize PriceData
//         std::shared_ptr<feeds::PriceData> priceData = std::make_shared<feeds::PriceData>(ohlc_data, date_vector, stock_name_vector, stock_vector, feeds::TimeStrConv::delimited_date);

//         // Create and configure Cerebro
//         Cerebro cerebro;
//         std::cout << "Adding broker..." << std::endl;
//         cerebro.add_broker(
//             broker::BaseBroker(5000000, 0.0005, 0.001)
//                 .set_feed(*priceData)
//         );
//         cerebro.add_strategy(std::make_shared<SimpleStrategy>());
//         cerebro.run();
//     } catch (const std::exception &e) {
//         std::cerr << "Error: " << e.what() << std::endl;
//     }
// }

void runBacktrader(const std::vector<std::vector<std::string>>& data, 
                   const std::vector<std::string>& columns ) {
    SetConsoleOutputCP(CP_UTF8);
    // std::cout << "Running runBacktrader" << std::endl;

    try {
        // Initialize PriceData
        std::shared_ptr<feeds::PriceData> priceData = std::make_shared<feeds::PriceData>(data, columns, feeds::TimeStrConv::delimited_date);
        // for(int i = 0; i < data.size(); i++){
        //     if (i <=10){for(int j = 0; j < data[i].size(); j++){
        //         std::cout << data[i][j] << " ";
        //     }
        //     std::cout <<"==================================="<< std::endl;
        //     }
        // }
        // Create and configure Cerebro
        Cerebro cerebro;
        std::cout << "Adding broker..." << std::endl;
        cerebro.add_broker(
            broker::BaseBroker(5000000, 0.0005, 0.001)
                .set_feed(*priceData)
        );
        std::cout << "Adding strategy..." << std::endl;
        cerebro.add_strategy(std::make_shared<SimpleStrategy>());
        std::cout << "Running strategy..." << std::endl;
        cerebro.run();
    } catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
}

void runAll(const std::vector<DataFrame>& data_list) {
    auto start = std::chrono::high_resolution_clock::now();
    std::cout << "Running runAll" << std::endl;

    // Ensure GIL is acquired in this thread to convert Python objects to C++ objects
    py::gil_scoped_acquire acquire;

    // 合并所有传递过来的数据
    std::vector<std::vector<std::string>> merged_data;
    std::vector<std::string> merged_columns;

    for (const auto& data : data_list) {
        try {
            // 将每个 data 的内容合并到 merged_data 中
            merged_data.insert(merged_data.end(), data.data.begin(), data.data.end());

            // 确保列名只有一次（假设列名相同）
            if (merged_columns.empty()) {
                merged_columns = data.columns;
            }
        } catch (const std::exception &e) {
            std::cerr << "Error in merging data: " << e.what() << std::endl;
        }
    }

    try {
        // 运行 backtrader，只执行一次
        runBacktrader(merged_data, merged_columns);
    } catch (const std::exception &e) {
        std::cerr << "Error in running backtrader: " << e.what() << std::endl;
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;
    std::cout << "C++ code elapsed time : " << elapsed.count() << " seconds.\n";
}




// void runAll(const std::vector<CustomObject>& data_list) {
//     auto start = std::chrono::high_resolution_clock::now();
//     std::vector<std::thread> threads;
//     std::cout << "Running runAll" << std::endl;

//     // Ensure GIL is acquired in this thread to convert Python objects to C++ objects
//     py::gil_scoped_acquire acquire;

//     for (const auto& data : data_list) {
//         try {
//             // Convert py::array_t<double> to std::vector<std::vector<double>>
//             py::buffer_info buf = data.array.request();
//             std::vector<std::vector<double>> ohlc_data(buf.shape[0], std::vector<double>(buf.shape[1]));
//             double* ptr = static_cast<double*>(buf.ptr);
//             for (size_t i = 0; i < buf.shape[0]; ++i) {
//                 for (size_t j = 0; j < buf.shape[1]; ++j) {
//                     ohlc_data[i][j] = ptr[i * buf.shape[1] + j];
//                 }
//             }
//             DataObject mydata = {ohlc_data, data.vec1, data.vec2, data.vec3};

//             // Create a new thread to runBacktrader with C++ objects
//             threads.emplace_back([mydata]() {
//                 try {
//                     // std::cout << "Starting thread for stock code: " << mydata.stock_vector[0] << std::endl;
//                     runBacktrader(mydata.ohlc_data, mydata.date_vector, mydata.stock_name_vector, mydata.stock_vector);
//                     // std::cout << "Finished thread for stock code: " << mydata.stock_vector[0] << std::endl;
//                 } catch (const std::exception &e) {
//                     std::cerr << "Thread error: " << e.what() << std::endl;
//                 }
//             });
//         } catch (const std::exception &e) {
//             std::cerr << "Error in converting data: " << e.what() << std::endl;
//         }
//     }

//     for (auto& t : threads) {
//         if (t.joinable()) {
//             t.join();
//         }
//     }

//     auto end = std::chrono::high_resolution_clock::now();
//     std::chrono::duration<double> elapsed = end - start;
//     std::cout << "C++ code elapsed time : " << elapsed.count() << " seconds.\n";
// }



// void checkCoreUsage() {
// #ifdef _WIN32
//     DWORD_PTR processAffinityMask;
//     DWORD_PTR systemAffinityMask;
//     if (GetProcessAffinityMask(GetCurrentProcess(), &processAffinityMask, &systemAffinityMask)) {
//         // Do something with the masks
//     } else {
//         std::cerr << "Failed to get process affinity mask (" << GetLastError() << ").\n";
//     }
// #else
//     // Unix-like systems may use sysconf or other methods to get CPU info
//     long nprocs = sysconf(_SC_NPROCESSORS_ONLN);
//     if (nprocs == -1) {
//         perror("sysconf");
//     } else {
//         std::cout << "Number of processors: " << nprocs << std::endl;
//     }
// #endif
// }

PYBIND11_MODULE(backtradercpp, m) {

    py::class_<DataFrame>(m, "DataFrame")
        .def(py::init<>())
        .def_readwrite("data", &DataFrame::data)
        .def_readwrite("columns", &DataFrame::columns);

    m.def("runBacktrader", &runBacktrader, "Run the backtrader strategy",
          py::arg("data"), py::arg("columns"));
    m.def("runAll", &runAll, "Process a list of CustomObject");
    // m.def("checkCoreUsage", &checkCoreUsage, "Check the number of cores being used");
}