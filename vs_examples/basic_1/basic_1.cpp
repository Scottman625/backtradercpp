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
        if (time_index() % 50 == 3) {
            std::cout << "time_index: " << time_index() << std::endl;
            // 遍歷所有股票數據
            try {
                for (const auto &stock : datas()) {
                    const std::string &ticker = stock.first; // 股票代號
                    const std::vector<PriceFeedData> &data_vector = stock.second;
                    // std::cout << "ticker: " << ticker << std::endl;
                    // 確保有最新數據並操作資產
                    if (!data_vector.empty()) {
                        const PriceFeedData &latest_data =
                            data_vector.back(); // 取最後一個元素，即最新的數據
                        // for (int j = 0; j < latest_data.data.open.size(); ++j) {
                        // std::cout << "ticker: " << ticker
                        //           << "open price: " << latest_data.data.open << std::endl;
                        buy(0, stoi(ticker), latest_data.data.open, 100, time_index());
                    }
                }
            } catch (const std::exception &e) {
                std::cout << "Error: " << std::endl;
                std::cerr << e.what() << '\n';
            }
        }

        // 每 100 個時間步長的第 40 個時間步執行一次賣出操作
        if (time_index() % 50 == 40) {
            std::cout << "time_index: " << time_index() << std::endl;

            try {

                for (const auto &stock : datas()) {
                    const std::string &ticker = stock.first;
                    const std::vector<PriceFeedData> &data_vector = stock.second;
                    // std::cout << "ticker: " << ticker << std::endl;
                    if (!data_vector.empty()) {
                        const PriceFeedData &latest_data =
                            data_vector.back(); // 取最後一個元素，即最新的數據
                        // std::cout << "ticker: " << ticker
                        //           << " close price: " << latest_data.data.close << std::endl;
                        close(0, stoi(ticker), latest_data.data.close, time_index());
                    }
                }
            } catch (const std::exception &e) {
                std::cout << "Error: " << std::endl;
                std::cerr << e.what() << '\n';
            }
        }
    }
};

struct DataFrame {
    std::vector<std::vector<std::string>> data;
    std::vector<std::string> columns;
};

void printVector(const std::vector<std::string> &vec, const std::string &label) {
    std::cout << label << ": ";
    for (const auto &str : vec) {
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

void runBacktrader(const std::vector<std::vector<std::string>> &data,
                   const std::vector<std::string> &columns, const int assets) {
    SetConsoleOutputCP(CP_UTF8);

    try {
        // Initialize next_index_date_change shared pointer
        auto next_index_date_change = std::make_shared<bool>(false);

        // Initialize PriceData with the new parameter
        std::shared_ptr<feeds::PriceData> priceData = std::make_shared<feeds::PriceData>(
            data, columns, assets, next_index_date_change, feeds::TimeStrConv::delimited_date);

        std::cout << "PriceData initialized" << std::endl;

        Cerebro cerebro(*priceData);
        std::cout << "Adding broker..." << std::endl;

        cerebro.add_broker(broker::BaseBroker(5000000, 0.0005, 0.001).set_feed(*priceData));
        std::cout << "Adding strategy..." << std::endl;

        cerebro.add_strategy(std::make_shared<SimpleStrategy>());
        std::cout << "Running strategy..." << std::endl;

        cerebro.run();
    } catch (const std::exception &e) {
        // std::cerr << "Error: " << e.what() << std::endl;
    }
}

void runAll(const std::vector<DataFrame> &data_list, const int assets) {
    auto start = std::chrono::high_resolution_clock::now();
    std::cout << "Running runAll" << std::endl;

    // Ensure GIL is acquired in this thread to convert Python objects to C++ objects
    py::gil_scoped_acquire acquire;

    // 合并所有传递过来的数据
    std::vector<std::vector<std::string>> merged_data;
    std::vector<std::string> merged_columns;

    for (const auto &data : data_list) {
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
        runBacktrader(merged_data, merged_columns, assets);
    } catch (const std::exception &e) {
        std::cerr << "Error in running backtrader: " << e.what() << std::endl;
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;
    std::cout << "C++ code elapsed time : " << elapsed.count() << " seconds.\n";
}

PYBIND11_MODULE(backtradercpp, m) {

    py::class_<DataFrame>(m, "DataFrame")
        .def(py::init<>())
        .def_readwrite("data", &DataFrame::data)
        .def_readwrite("columns", &DataFrame::columns);

    m.def("runBacktrader", &runBacktrader, "Run the backtrader strategy", py::arg("data"),
          py::arg("columns"), py::arg("assets"));

    m.def("runAll", &runAll, "Process a list of CustomObject", py::arg("data_list"),
          py::arg("assets"));
}