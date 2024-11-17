#pragma once
/*ZhouYao at 2022-09-10*/

#include "Common.hpp"
#include "util.hpp"
#include <fstream>
#include <boost/tokenizer.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <functional>
#include "util.hpp"
#include <algorithm>
#include <numeric>
#include <filesystem>
#include <unordered_set>
#include <fmt/core.h>
// #include "3rd_party/glob/glob.hpp"
// #include <ranges>
namespace py = pybind11;

namespace backtradercpp {
namespace feeds {
// CSVTabularDataImpl: A single matrix that contains multiple assets and only one type price. At
// this case, OHLC are the same value and valume are implicitly very large. time_converter: convert
// a time string to standard format: 2020-01-01 01:00:00

// 這個結構定義了兩個靜態函數，用於將日期字符串從一種格式轉換為另一種格式。
struct TimeStrConv {
    using func_type = std::function<std::string(const std::string &)>;

    // non_delimited_date函數將無分隔符的日期字符串（如"20100202"）轉換為有分隔符的日期字符串（如"2010-02-02
    // 00:00:00"）
    static std::string non_delimited_date(const std::string &date_str) {
        std::string res_date = "0000-00-00 00:00:00";
        res_date.replace(0, 4, std::string_view{date_str.data(), 4});
        res_date.replace(5, 2, std::string_view{date_str.data() + 4, 2});
        res_date.replace(8, 2, std::string_view{date_str.data() + 6, 2});
        return res_date;
    }
    // delimited_date函數做相同的事情，但是它假定輸入的日期字符串已經有分隔符。
    static std::string delimited_date(const std::string &date_str) {
        std::string res_date = "0000-00-00 00:00:00";
        res_date.replace(0, 4, std::string_view{date_str.data(), 4});
        res_date.replace(5, 2, std::string_view{date_str.data() + 5, 2});
        res_date.replace(8, 2, std::string_view{date_str.data() + 8, 2});
        return res_date;
    }
};

// 這是一個前向聲明，表示存在一個模板類別GenericFeedsAggragator，它接受三個模板參數。該類別的定義並未在這段程式碼中給出。
template <typename PriceFeedData, typename PriceData, typename BufferT>
class GenericFeedsAggragator;

// 這是一個枚舉類型，用於表示某種狀態，可能的值為Valid、Invalid和Finished。
enum State { Valid, Invalid, Finished };

template <typename T> class GenericDataImpl {
  public:
    GenericDataImpl() = default;
    explicit GenericDataImpl(TimeStrConv::func_type time_converter)
        : time_converter_(time_converter) {}

    virtual bool read() { return false; }; // Update next_row_;
    virtual T get_and_read() {
        auto res = next_;
        read();
        return res;
    }

    const auto &data() const { return next_; }
    T *data_ptr() { return &next_; }
    bool finished() const { return finished_; }
    const auto &next() const { return next_; }

    virtual void reset() { finished_ = false; }

    void set_name(const std::string &name) { name_ = name; }
    const auto &name() { return name_; }

  protected:
    TimeStrConv::func_type time_converter_;

    T next_;
    bool finished_ = false;
    std::string name_;
};

class BasePriceDataImpl : public GenericDataImpl<PriceFeedData> {
  public:
    BasePriceDataImpl() = default;
    explicit BasePriceDataImpl(TimeStrConv::func_type time_converter)
        : GenericDataImpl(time_converter) {}

    virtual ~BasePriceDataImpl() = default;

    virtual std::shared_ptr<BasePriceDataImpl> clone();

    int assets() const { return assets_; }
    virtual const std::vector<std::string> &codes() const { return codes_; }

  protected:
    friend class FeedsAggragator;

    virtual void init() { print(fg(fmt::color::yellow), "Total {} assets.\n", assets_); }
    int assets_ = 0;
    std::vector<std::string> codes_;
};

struct CSVRowParaser {
    inline static boost::escaped_list_separator<char> esc_list_sep{"", ",", "\"\'"};
    static std::vector<std::string> parse_row(const std::string &row);

    static double parse_double(const std::string &ele);
};

// For tabluar pricing data. OHLC are the same.
class CSVTabDataImpl : public BasePriceDataImpl {
  public:
    CSVTabDataImpl(const std::string &raw_data_file,
                   TimeStrConv::func_type time_converter = nullptr);
    // You have to ensure that two file have the same rows.
    CSVTabDataImpl(const std::string &raw_data_file, const std::string &adjusted_data_file,
                   TimeStrConv::func_type time_converter = nullptr);
    CSVTabDataImpl(const CSVTabDataImpl &impl_);

    bool read() override;
    void reset() override {
        BasePriceDataImpl::reset();
        init();
    }
    std::shared_ptr<BasePriceDataImpl> clone() override;

  private:
    void init() override;
    void cast_ohlc_data_(std::vector<std::string> &row_string, OHLCData &dest);

    std::ifstream raw_data_file_, adj_data_file_;
    std::string raw_data_file_name_, adj_data_file_name_;
    std::string next_row_, adj_next_row_;
};

class CSVDirDataImpl : public BasePriceDataImpl {
  public:
    // tohlc_map: column number of time, open, high, low, close
    CSVDirDataImpl(const std::string &raw_data_dir, std::array<int, 5> tohlc_map = {0, 1, 2, 3, 4},
                   TimeStrConv::func_type time_converter = nullptr);
    CSVDirDataImpl(const std::string &raw_data_dir, const std::string &adj_data_dir,
                   std::array<int, 5> tohlc_map = {0, 1, 2, 3, 4},
                   TimeStrConv::func_type time_converter = nullptr);
    CSVDirDataImpl(const CSVDirDataImpl &impl_);

    bool read() override;
    // void init_extra_data();

    // Column name will be read form files.
    CSVDirDataImpl &extra_num_col(const std::vector<std::pair<int, std::string>> &cols);

    CSVDirDataImpl &extra_str_col(const std::vector<std::pair<int, std::string>> &cols);

    CSVDirDataImpl &code_extractor(std::function<std::string(std::string)> fun) {
        // this->code_extractor_ = fun;
        for (auto &ele : codes_) {
            ele = fun(ele);
        }
        return *this;
    }

    // init() must be manunally called. Because in Cerebro::add_data_feed, the evaluation order of
    // data feed and broker are unspecified in C++ standard.
    void init() override;
    void reset() override {
        raw_data_filenames.clear();
        adj_data_filenames.clear();

        raw_files.clear();
        adj_files.clear();

        init();
    }
    std::shared_ptr<BasePriceDataImpl> clone() override;

  private:
    std::string raw_data_dir, adj_data_dir;
    std::vector<std::string> raw_data_filenames, adj_data_filenames;

    // std::vector<std::filesystem::path> raw_file_names, adj_file_names;
    std::vector<std::ifstream> raw_files, adj_files;
    std::vector<std::string> raw_line_buffer, adj_line_buffer;
    std::vector<std::vector<std::string>> raw_parsed_buffer, adj_parsed_buffer;
    std::vector<std::array<double, 4>> raw_parsed_double_buffer, adj_parsed_double_buffer;
    std::array<int, 5> tohlc_map;

    std::vector<int> extra_num_col_, extra_str_col_;
    std::vector<std::string> extra_num_col_names_, extra_str_col_names_;
    std::vector<std::reference_wrapper<VecArrXd>> extra_num_data_ref_;
    std::vector<std::reference_wrapper<std::vector<std::string>>> extra_str_data_ref_;

    std::vector<ptime> times;

    /* std::function<std::string(std::string)> code_extractor_ = [](const std::string &e) {
         return e;
     };*/

    std::vector<State> status; // Track if data in each file has been readed.
};

class PriceDataImpl : public BasePriceDataImpl {
  public:
    PriceDataImpl() { std::cout << "PriceDataImpl constructed" << std::endl; };
    PriceDataImpl(const std::vector<std::vector<std::string>> &data,
                  const std::vector<std::string> &columns,
                  const int assets,
                  std::shared_ptr<bool> next_index_date_change_,
                  TimeStrConv::func_type time_converter = nullptr)
        : data_(data), columns_(columns), assets_(assets), next_index_date_change_(next_index_date_change_),
          BasePriceDataImpl(time_converter),
          stock_data(
              std::make_shared<std::unordered_map<std::string, std::vector<PriceFeedData>>>()) {
        if (time_converter) {
            time_converter_ = time_converter;
        } else {
            time_converter_ = &TimeStrConv::delimited_date;
        }
        init();
        index = 0;
        std::cout << "PriceDataImpl constructed with args" << std::endl;
    }

    bool read() override;
    void reset() override;

    int assets() const { return assets_; }

    std::shared_ptr<BasePriceDataImpl> clone() override;

    void cast_ohlc_data_(std::vector<std::string> &row_string, OHLCData &dest);

    const std::vector<std::string> &codes() const override { return BasePriceDataImpl::codes(); }

    // 在讀取每筆資料後，將其按照股票代號進行分類

    void process_data(PriceFeedData &data) { (*stock_data)[data.ticker_].push_back(data); }

    std::vector<std::vector<std::string>> data_;
    std::vector<std::string> columns_;

    std::shared_ptr<std::unordered_map<std::string, std::vector<PriceFeedData>>>
    get_stock_data() const {
        return stock_data;
    }

    std::shared_ptr<bool> get_next_index_date_change() const { return next_index_date_change_; }

    // std::shared_ptr<int> get_index() const { return index; }

    ~PriceDataImpl() { std::cout << "PriceDataImpl destructed" << std::endl; }

  private:
    void init() override;

    // Initialize stock_data to point to an empty unordered_map
    std::shared_ptr<std::unordered_map<std::string, std::vector<PriceFeedData>>> stock_data =
        std::shared_ptr<std::unordered_map<std::string, std::vector<PriceFeedData>>>();

    std::vector<std::vector<std::string>> combined_data_;
    std::vector<std::string> unique_dates_vector_;

    std::shared_ptr<bool> next_index_date_change_;

    int clear_count = 0;
    int index;
    size_t assets_;
    TimeStrConv::func_type time_converter_;
};

// 将数据按股票代号分类处理
inline bool PriceDataImpl::read() {
    try {
        if (index >= data_.size()) {
            finished_ = true;
            return false;
        }

        // 获取当前数据行
        auto row_string = data_[index];

        if(index < data_.size() - 1) {
            auto next_row_string = data_[index + 1];
            std::string next_raw_time_str = next_row_string[2]; // 例如 "2024-05-24"
            if (next_raw_time_str != row_string[2]) {
                *next_index_date_change_ = true;
            } else {
                *next_index_date_change_ = false;
            }
        }


        // 解析股票代码和时间
        next_.ticker_ = row_string[0];
        std::string raw_time_str = row_string[2]; // 例如 "2024-05-24"
        raw_time_str += " 00:00:00";              // 添加时间部分

        try {
            next_.time = boost::posix_time::time_from_string(raw_time_str);
        } catch (const std::exception &e) {
            std::cerr << "Error parsing time: " << e.what() << "\n"
                      << "Invalid string: " << raw_time_str << std::endl;
        }

        // 提取 stock_data 中的一个日期来初始化 last_date
        static boost::gregorian::date last_date;
        if (last_date.is_not_a_date()) { // 检查 last_date 是否未初始化
            if (!stock_data->empty()) {
                last_date = stock_data->begin()->second.front().time.date(); // 取出任意日期
            } else {
                last_date = next_.time.date(); // 如果 stock_data 为空，用当前日期初始化
            }
        }

        // 当前数据的日期
        boost::gregorian::date current_date = next_.time.date();

        // 如果是新日期，清空 stock_data
        if (current_date != last_date) {

            stock_data->clear(); // 清空之前的数据
            clear_count = 0;

            last_date = current_date; // 更新 last_date 为当前日期
        }

        // 解析 OHLC 数据并更新 volume 等
        cast_ohlc_data_(row_string, next_.data);
        next_.volume.setConstant(1e12);
        next_.validate_assets();

        // 根据股票代码分组保存数据
        process_data(next_);

        ++index;
        return true;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
    }
}

inline void PriceDataImpl::reset() {
    // ohlc_data_ = py::array_t<double>();
    data_.clear();
    columns_.clear();
    init();
}

inline void PriceDataImpl::cast_ohlc_data_(std::vector<std::string> &row_string, OHLCData &dest) {
    if (row_string.size() < 5) { // 确保 row_string 至少有5个元素
        throw std::invalid_argument("Expected row_string to have at least 5 elements.");
    }

    try {
        // 修剪并解析开盘价
        boost::algorithm::trim(row_string[3]);
        dest.open = boost::lexical_cast<double>(row_string[3]);
        // std::cout << "today open: " << row_string[3] << std::endl;

        // 修剪并解析最高价
        boost::algorithm::trim(row_string[4]);
        dest.close = boost::lexical_cast<double>(row_string[4]);
        // std::cout << "today close: " << row_string[4] << std::endl;

        // 修剪并解析最低价
        boost::algorithm::trim(row_string[5]);
        dest.high = boost::lexical_cast<double>(row_string[5]);
        // std::cout << "today high: " << row_string[5] << std::endl;

        // 修剪并解析收盘价
        boost::algorithm::trim(row_string[6]);
        dest.low = boost::lexical_cast<double>(row_string[6]);
        // std::cout << "today low: " << row_string[6] << std::endl;

    } catch (const boost::bad_lexical_cast &e) {
        util::cout("Bad cast: {}\n", e.what());
    } catch (const std::exception &e) {
        util::cout("Error: {}\n", e.what());
    }
}
std::shared_ptr<BasePriceDataImpl> PriceDataImpl::clone() {
    return std::make_shared<PriceDataImpl>(*this);
}

void PriceDataImpl::init() {
    size_t rows = data_.size();
    size_t cols = data_[0].size();

    // 将 assets_ 设置为股票的数量（即行数）
    // assets_ = rows;

    std::cout << "assets_: " << assets_ << std::endl;

    // 正确初始化 codes_，确保每个股票都有唯一的代码
    codes_.resize(assets_);
    for (size_t i = 0; i < assets_; ++i) {
        // 假设股票代码位于每行的某个位置，例如第 1 列
        codes_[i] = data_[i][0]; // 假设股票代码是每行的第 0 列（可以根据实际数据调整列索引）
    }

    // Initialize combined_data_ with additional columns for date, stock name, and stock
    // next_.resize(assets_);
}

class BaseCommonDataFeedImpl : public GenericDataImpl<CommonFeedData> {
  public:
    BaseCommonDataFeedImpl(TimeStrConv::func_type time_converter)
        : GenericDataImpl(time_converter) {};
    BaseCommonDataFeedImpl(const BaseCommonDataFeedImpl &impl_) = default;
    virtual std::shared_ptr<BaseCommonDataFeedImpl> clone() {
        return std::make_shared<BaseCommonDataFeedImpl>(*this);
    }

  protected:
};

class CSVCommonDataImpl : public BaseCommonDataFeedImpl {
  public:
    CSVCommonDataImpl(const std::string &file, TimeStrConv::func_type time_converter,
                      const std::vector<int> str_cols);
    CSVCommonDataImpl(const CSVCommonDataImpl &impl_);
    bool read() override;
    void reset() override { init(); }

    std::shared_ptr<BaseCommonDataFeedImpl> clone() override;

  private:
    void init();

    std::string file, next_row_;
    std::unordered_set<int> str_cols_;
    std::vector<std::string> col_names_;
    std::ifstream data_file_;
};

// ---------------------------------------------------------------------------------

struct BaseCommonDataFeed {
    std::shared_ptr<BaseCommonDataFeedImpl> sp = nullptr;
    BaseCommonDataFeed() = default;
    BaseCommonDataFeed(std::shared_ptr<BaseCommonDataFeedImpl> sp) : sp(std::move(sp)) {}

    bool read() { return sp->read(); }
    const auto &time() const { return sp->data().time; }
    CommonFeedData *data_ptr() { return sp->data_ptr(); }

    virtual void reset() { sp->reset(); }
    const auto &name() const { return sp->name(); }

    BaseCommonDataFeed &set_name(const std::string &name) {
        sp->set_name(name);
        return *this;
    }

    virtual BaseCommonDataFeed clone() { return BaseCommonDataFeed(sp->clone()); }
};
// If no str_cols, then all columns are assumed to be numeric.
struct CSVCommonDataFeed : BaseCommonDataFeed {
    std::shared_ptr<CSVCommonDataImpl> sp;

    CSVCommonDataFeed(const std::string &file, TimeStrConv::func_type time_converter = nullptr,
                      const std::vector<int> &str_cols = {})
        : sp(std::make_shared<CSVCommonDataImpl>(file, time_converter, str_cols)) {
        set_base_sp();
    }
    CSVCommonDataFeed(std::shared_ptr<CSVCommonDataImpl> sp) : sp(std::move(sp)) { set_base_sp(); }

    void reset() override { sp->reset(); }

    CSVCommonDataFeed &set_name(const std::string &name) {
        BaseCommonDataFeed::set_name(name);
        return *this;
    }

    virtual void set_base_sp() { BaseCommonDataFeed::sp = sp; }
    /*BaseCommonDataFeed clone() override {
        return CSVCommonDataFeed(std::make_shared<CSVCommonDataImpl>(*sp));
    }*/
};

// struct BasePriceDataFeed {
//     BasePriceDataFeed() = default;

//     explicit BasePriceDataFeed(std::shared_ptr<BasePriceDataImpl> sp) : sp(std::move(sp)) {}

//     BasePriceDataFeed(const BasePriceDataFeed& other) : sp(other.sp) {}

//     BasePriceDataFeed& operator=(const BasePriceDataFeed& other) {
//         if (this != &other) {
//             sp = other.sp;
//         }
//         return *this;
//     }

//     std::shared_ptr<BasePriceDataImpl> sp = nullptr;

//     bool read() { return sp->read(); }

//     virtual void reset() { sp->reset(); }

//     PriceFeedData* data_ptr(const std::string& stock_code) {
//         // 假設 sp 是指向 PriceDataImpl 的指針，我們可以動態轉換
//         auto price_data_impl = std::dynamic_pointer_cast<PriceDataImpl>(sp);
//         if (price_data_impl) {
//             // 使用 find() 查找對應的股票代號
//             auto it = price_data_impl->stock_data.find(stock_code);
//             if (it != price_data_impl->stock_data.end()) {
//                 // 返回該股票代號對應的數據指針
//                 return &it->second[0];  // 假設每個股票代號只有一個數據
//             } else {
//                 throw std::invalid_argument("Stock code not found.");
//             }
//         }
//         throw std::runtime_error("Invalid data structure.");
//     }

//     int assets() const { return sp->assets(); }

//     const auto& time() const { return sp->data().time; }

//     const auto& codes() const { return sp->codes(); }

//     const auto& name() const { return sp->name(); }

//     BasePriceDataFeed& set_name(const std::string& name) {
//         sp->set_name(name);
//         return *this;
//     }

//     virtual BasePriceDataFeed clone() { return BasePriceDataFeed(sp->clone()); }
// };

class BasePriceDataFeed {
  public:
    BasePriceDataFeed() = default;

    explicit BasePriceDataFeed(std::shared_ptr<BasePriceDataImpl> sp)
        : sp(std::dynamic_pointer_cast<PriceDataImpl>(std::move(sp))) {
        if (!this->sp) {
            throw std::runtime_error("Failed to cast BasePriceDataImpl to PriceDataImpl.");
        }
    }

    BasePriceDataFeed(const BasePriceDataFeed &other) : sp(other.sp) {}

    BasePriceDataFeed &operator=(const BasePriceDataFeed &other) {
        if (this != &other) {
            sp = other.sp;
        }
        return *this;
    }
    std::shared_ptr<PriceDataImpl> sp = nullptr;

    // 调用 sp->assets() 获取 assets_ 值
    int assets() {
        if (sp) {
            return sp->assets();
        }
        return 0; // 如果 sp 为空，返回 0
    }

    const auto get_stock_data() const {
        if (sp) {
            return sp->get_stock_data();
        }
        return std::shared_ptr<std::unordered_map<std::string, std::vector<PriceFeedData>>>();
    }

    bool read() { return sp->read(); }

    virtual void reset() { sp->reset(); }

    const auto &codes() const { return sp->codes(); }

    const auto &assets() const { return sp->assets(); }

    const auto &time() const { return sp->data().time; }

    const auto &name() const { return sp->name(); }

    BasePriceDataFeed &set_name(const std::string &name) {
        sp->set_name(name);
        return *this;
    }

    virtual BasePriceDataFeed clone() { return BasePriceDataFeed(sp->clone()); }

    PriceFeedData *data_ptr(const std::string &code) {
        // 实现返回具体资产的数据指针
        auto price_data_impl = std::dynamic_pointer_cast<PriceDataImpl>(sp);
        if (price_data_impl) {
            // 正确访问 price_data_impl 的 stock_data 成员变量
            auto it = price_data_impl->get_stock_data()->find(code); // 解引用 shared_ptr
            if (it != price_data_impl->get_stock_data()->end()) {
                return &it->second[0]; // 返回第一个数据指针
            }
        }
        return nullptr;
    }

    // std::shared_ptr<PriceDataImpl> sp = nullptr;
};

// struct CSVDirPriceData : BasePriceDataFeed {
//     std::shared_ptr<CSVDirDataImpl> sp;

//     // 初始化 sp，使用原始数据目录和其他参数创建 CSVDirDataImpl 实例。
//     // 调用 set_base_sp() 方法，将 BasePriceDataFeed 的 sp 成员设置为当前类的 sp。
//     CSVDirPriceData(const std::string &raw_data_dir, std::array<int, 5> tohlc_map = {0, 1, 2, 3,
//     4},
//                     TimeStrConv::func_type time_converter = nullptr)
//         : sp(std::make_shared<CSVDirDataImpl>(raw_data_dir, tohlc_map, time_converter)) {
//         set_base_sp();
//     }
//     // 初始化 sp，使用原始数据目录、调整数据目录和其他参数创建 CSVDirDataImpl 实例。
//     // 调用 set_base_sp() 方法，将 BasePriceDataFeed 的 sp 成员设置为当前类的 sp。
//     CSVDirPriceData(const std::string &raw_data_dir, const std::string &adj_data_dir,
//                     std::array<int, 5> tohlc_map = {0, 1, 2, 3, 4},
//                     TimeStrConv::func_type time_converter = nullptr)
//         : sp(std::make_shared<CSVDirDataImpl>(raw_data_dir, adj_data_dir, tohlc_map,
//                                               time_converter)) {
//         set_base_sp();
//     }

//     // 功能：调用 sp 的 read() 方法读取数据。
//     bool read() { return sp->read(); }

//     // 功能：设置额外的数值列，返回当前对象的引用。
//     CSVDirPriceData &extra_num_col(const std::vector<std::pair<int, std::string>> &cols) {
//         sp->extra_num_col(cols);
//         return *this;
//     }

//     // 功能：设置额外的字符串列，返回当前对象的引用。
//     CSVDirPriceData &extra_str_col(const std::vector<std::pair<int, std::string>> &cols) {
//         sp->extra_str_col(cols);
//         return *this;
//     }

//     // 功能：设置代码提取器，返回当前对象的引用。
//     CSVDirPriceData &set_code_extractor(std::function<std::string(std::string)> fun) {
//         sp->code_extractor(fun);
//         return *this;
//     }

//     // 功能：设置数据对象的名称，返回当前对象的引用。
//     CSVDirPriceData &set_name(const std::string &name) {
//         BasePriceDataFeed::set_name(name);
//         return *this;
//     }

//     // 功能：将 BasePriceDataFeed 的 sp 成员设置为当前类的 sp。
//     void set_base_sp() { BasePriceDataFeed::sp = sp; }

//     // 功能：克隆当前对象，返回一个新的 BasePriceDataFeed 实例。
//     BasePriceDataFeed clone() { return BasePriceDataFeed(); }
// };

class PriceData : public BasePriceDataFeed {
  public:
    PriceData() : BasePriceDataFeed(std::make_shared<PriceDataImpl>()) {}
    PriceData(const std::vector<std::vector<std::string>> &data,
              const std::vector<std::string> &columns,
              const int &assets,
              std::shared_ptr<bool> next_index_date_change_,
              TimeStrConv::func_type time_converter = nullptr)
        : BasePriceDataFeed(std::make_shared<PriceDataImpl>(data, columns,assets, next_index_date_change_,
                                                            time_converter)) {}

    PriceData &set_name(const std::string &name) {
        BasePriceDataFeed::set_name(name);
        return *this;
    }

    int assets() const {
        if (sp) {
            return sp->assets(); // 调用 sp 中的 assets() 方法
        }
        return 0; // 如果 sp 为空，返回 0
    }

    BasePriceDataFeed clone() override { return BasePriceDataFeed(sp->clone()); }
};

//-------------------------------------------------------------------
template <typename PriceFeedData, typename PriceData, typename BufferT>
class GenericFeedsAggragator {
  public:
    // GenericFeedsAggragator() = default;
    // 构造函数接受两个共享指针参数

    GenericFeedsAggragator()
        : stock_data(
              std::make_shared<std::unordered_map<std::string, std::vector<PriceFeedData>>>()) { // 使用 std::make_shared<bool>(false) 初始化
        std::cout << "Default constructor called with empty stock_data" << std::endl;
    }

    GenericFeedsAggragator(
        std::shared_ptr<std::unordered_map<std::string, std::vector<PriceFeedData>>> stock_data,
        std::shared_ptr<bool> next_index_date_change_)
        : stock_data(stock_data), next_index_date_change_(next_index_date_change_) {
        std::cout << "Constructor with shared stock_data and index called" << std::endl;
    }

    // // Constructor accepting shared stock_data from PriceDataImpl
    // GenericFeedsAggragator(
    //     std::shared_ptr<std::unordered_map<std::string, std::vector<PriceFeedData>>> stock_data,
    //     std::shared_ptr<bool> next_index_date_change_)
    //     : stock_data(stock_data), next_index_date_change_(next_index_date_change_) {
    //     std::cout << "Constructor with shared stock_data called" << std::endl;
    // }

    const auto &datas() const { return *stock_data; }
    const auto &datas_valid() const { return datas_valid_; }
    bool data_valid(int feed) const { return datas_valid_.coeff(feed); }

    const auto &data(int i) const { return data_[i]; }
    const auto &latest_data() const { return stock_data.end; }

    const auto feed(int i) const { return feeds_[i]; }

    void add_feed(const PriceData &feed);

    void init();
    void reset();
    auto finished() const { return finished_; }
    bool read();

    auto data_ptr() { return &data_; }
    const auto &time() const { return time_; }

    // int get_index() const { return *index; }

    std::shared_ptr<bool> get_next_index_date_change() const { return next_index_date_change_; }

    // void set_window(int src, int window) { stock_data[src].set_window(window); };

    // void set_window(int src, int window) {
    //     for (auto &datas : stock_data) {
    //         for(PriceFeedData &data : datas.second) {
    //             data.set_window(window);
    //         }
    //     }
    //  };

    // If two aggragators have different dates, then align them.
    template <typename T1, typename T2, typename T3>
    void align_with(const GenericFeedsAggragator<T1, T2, T3> &target);

    GenericFeedsAggragator clone();

  private:
    std::vector<ptime> times_;
    std::shared_ptr<bool> next_index_date_change_ = std::make_shared<bool>(false);
    std::vector<const PriceFeedData *> next_;
    std::vector<PriceData> feeds_;
    std::vector<BufferT> data_;
    // Shared stock_data between PriceDataImpl and GenericFeedsAggragator
    std::shared_ptr<std::unordered_map<std::string, std::vector<PriceFeedData>>> stock_data;

    VecArrXb datas_valid_;

    std::vector<State> status_;
    bool finished_ = false;
    ptime time_ = boost::gregorian::min_date_time;
};

using PriceFeedAggragator =
    GenericFeedsAggragator<PriceFeedData, BasePriceDataFeed, PriceFeedDataBuffer>;
using CommonFeedAggragator =
    GenericFeedsAggragator<CommonFeedData, BaseCommonDataFeed, CommonFeedDataBuffer>;

struct TimeConverter {
    //"20100202"
    static std::string non_delimt_date(const std::string &date_str) {
        std::string res_date = "0000-00-00 00:00:00";
        res_date.replace(0, 4, std::string_view{date_str.data(), 4});
        res_date.replace(5, 2, std::string_view{date_str.data() + 4, 2});
        res_date.replace(8, 2, std::string_view{date_str.data() + 6, 2});
        return res_date;
    }
};

//---------------------------------------------------------------

std::vector<std::string> CSVRowParaser::parse_row(const std::string &row) {
    boost::tokenizer<boost::escaped_list_separator<char>> tok(row, esc_list_sep);
    std::vector<std::string> res;
    for (auto beg = tok.begin(); beg != tok.end(); ++beg) {
        res.emplace_back(*beg);
    }
    return res;
}
double CSVRowParaser::parse_double(const std::string &ele) {
    double res = std::numeric_limits<double>::quiet_NaN();
    std::string s = boost::algorithm::trim_copy(ele);
    try {
        res = boost::lexical_cast<double>(s);
    } catch (...) {
        util::cout("Bad cast: {}\n", ele);
    }
    return res;
}

std::shared_ptr<BasePriceDataImpl> BasePriceDataImpl::clone() {
    return std::make_shared<BasePriceDataImpl>(*this);
}

inline CSVTabDataImpl::CSVTabDataImpl(const std::string &raw_data_file,
                                      TimeStrConv::func_type time_converter)
    : BasePriceDataImpl(time_converter), raw_data_file_name_(raw_data_file),
      adj_data_file_name_(raw_data_file), raw_data_file_(raw_data_file),
      adj_data_file_(raw_data_file) {
    util::check_path_exists(raw_data_file);
    init();
}

CSVTabDataImpl::CSVTabDataImpl(const std::string &raw_data_file,
                               const std::string &adjusted_data_file,
                               TimeStrConv::func_type time_converter)
    : BasePriceDataImpl(time_converter) {
    util::check_path_exists(raw_data_file);
    util::check_path_exists(adjusted_data_file);

    raw_data_file_name_ = raw_data_file;
    adj_data_file_name_ = raw_data_file;
    init();
}

inline CSVTabDataImpl::CSVTabDataImpl(const CSVTabDataImpl &impl_) : BasePriceDataImpl(impl_) {
    raw_data_file_name_ = impl_.raw_data_file_name_;
    adj_data_file_name_ = impl_.adj_data_file_name_;
    init();
}

inline std::shared_ptr<BasePriceDataImpl> CSVTabDataImpl::clone() {
    return std::make_shared<CSVTabDataImpl>(*this);
}

inline void CSVTabDataImpl::init() {

    raw_data_file_ = std::ifstream(raw_data_file_name_);
    adj_data_file_ = std::ifstream(adj_data_file_name_);

    // Read header
    std::string header;
    std::getline(raw_data_file_, header);
    std::getline(adj_data_file_, header);

    // Read one line and detect assets.
    auto row_string = CSVRowParaser::parse_row(header);
    assets_ = row_string.size() - 1;
    next_.resize(assets_);

    // Set codes.
    for (int i = 1; i < row_string.size(); ++i) {
        codes_.emplace_back(std::move(row_string[i]));
    }

    BasePriceDataImpl::init();
}

inline void CSVTabDataImpl::cast_ohlc_data_(std::vector<std::string> &row_string, OHLCData &dest) {
    for (int i = 0; i < row_string.size(); ++i) {
        // dest.open.coeffRef(i) = std::numeric_limits<double>::quiet_NaN();
        dest.open = 0;
        auto &s = row_string[i + 1];
        try {
            boost::algorithm::trim(s);
            dest.open = boost::lexical_cast<double>(s);
        } catch (...) {
            util::cout("Bad cast: {}\n", s);
        }
    }
    dest.high = dest.low = dest.close = dest.open;
};

inline bool CSVTabDataImpl::read() {
    std::getline(raw_data_file_, next_row_);
    if (raw_data_file_.eof()) {
        finished_ = true;
        return false;
    }
    auto row_string = CSVRowParaser::parse_row(next_row_);
    next_.time = boost::posix_time::time_from_string(time_converter_(row_string[0]));
    cast_ohlc_data_(row_string, next_.data);

    std::getline(adj_data_file_, adj_next_row_);
    row_string = CSVRowParaser::parse_row(adj_next_row_);
    cast_ohlc_data_(row_string, next_.adj_data);

    // Set volume to very large.
    next_.volume.setConstant(1e12);
    next_.validate_assets();

    return true;
}

template <typename PriceFeedData, typename PriceData, typename BufferT>
inline bool GenericFeedsAggragator<PriceFeedData, PriceData, BufferT>::read() {
    bool all_finished = true;

    // 遍历所有的 feeds_
    for (int i = 0; i < feeds_.size(); ++i) {
        // 如果当前 feed 的状态是 Invalid，尝试读取新的数据
        if (status_[i] == Invalid) {
            bool _get = feeds_[i].sp->read(); // 从 feed 中读取数据
            if (_get) {
                // 成功读取到数据，更新时间和状态
                times_[i] = feeds_[i].time(); // 更新 feed 的时间
                status_[i] = Valid;           // 设置状态为 Valid
                all_finished = false;         // 至少有一个 feed 还没结束
            } else {
                // 如果读取失败，设置为 Finished 状态并将时间设置为 max_date_time
                status_[i] = Finished;
                times_[i] = boost::posix_time::max_date_time; // 将时间设置为最大值
            }
        }
    }

    // 如果所有的 feeds_ 都读取完毕，返回 false
    if (all_finished) {
        return false;
    }

    // 找到所有 feed 中最早的时间，作为下一步的时间
    auto p = std::min_element(times_.begin(), times_.end()); // 查找最小时间
    time_ = *p;                                              // 将 time_ 设置为最早的时间

    // 遍历 feeds_，处理数据
    for (int i = 0; i < feeds_.size(); ++i) {
        if (times_[i] == time_) {
            // 如果 feed 的时间与最小时间相同，读取数据并设置状态为 Invalid
            data_[i].push_back(feeds_[i].sp->next()); // 添加数据
            status_[i] = Invalid;   // 将状态设置为 Invalid，准备下一轮读取
            datas_valid_[i] = true; // 标记该 feed 的数据为有效
        } else {
            // 否则，插入一个空数据，并标记为无效
            data_[i].push_back_();   // 插入一个空数据
            datas_valid_[i] = false; // 标记该 feed 的数据为无效
        }
    }

    // 如果有任何一个 feed 的数据是有效的，返回 true，否则返回 false
    bool success = datas_valid_.any();
    finished_ = !success; // 如果所有数据都是无效的，标记为 finished
    return success;
}

// inline const std::vector<FullAssetData> &FeedsAggragator::get_and_read() {
//     read();
//     return data_;
// }
template <typename PriceFeedData, typename PriceData, typename BufferT>
inline void
GenericFeedsAggragator<PriceFeedData, PriceData, BufferT>::add_feed(const PriceData &feed) {
    status_.emplace_back(Invalid);
    times_.emplace_back();

    feeds_.push_back(feed);
    next_.push_back(&(feed.sp->next()));
    data_.emplace_back(feed.sp->next());
    datas_valid_.resize(feeds_.size());
}

template <typename PriceFeedData, typename PriceData, typename BufferT>
GenericFeedsAggragator<PriceFeedData, PriceData, BufferT>
GenericFeedsAggragator<PriceFeedData, PriceData, BufferT>::clone() {
    GenericFeedsAggragator feed_agg_ = *this;
    for (int i = 0; i < feeds_.size(); ++i) {
        feed_agg_.feeds_[i] = feeds_[i].clone();
    }
    return feed_agg_;
}

template <typename PriceFeedData, typename PriceData, typename BufferT>
void GenericFeedsAggragator<PriceFeedData, PriceData, BufferT>::reset() {
    for (auto &ele : status_) {
        ele = Invalid;
    }
    time_ = boost::gregorian::min_date_time;
    finished_ = false;
    for (auto &f : feeds_) {
        f.reset();
    }
}

inline CSVDirDataImpl::CSVDirDataImpl(const std::string &raw_data_dir, std::array<int, 5> tohlc_map,
                                      TimeStrConv::func_type time_converter)
    : BasePriceDataImpl(time_converter), raw_data_dir(raw_data_dir), adj_data_dir(raw_data_dir),
      tohlc_map(tohlc_map) {
    init();
}

inline CSVDirDataImpl::CSVDirDataImpl(const std::string &raw_data_dir,
                                      const std::string &adj_data_dir, std::array<int, 5> tohlc_map,
                                      TimeStrConv::func_type time_converter)
    : BasePriceDataImpl(time_converter), raw_data_dir(raw_data_dir), adj_data_dir(adj_data_dir),
      tohlc_map(tohlc_map) {
    init();
}

CSVDirDataImpl::CSVDirDataImpl(const CSVDirDataImpl &impl_) : BasePriceDataImpl(impl_) {
    raw_data_dir = impl_.raw_data_dir;
    adj_data_dir = impl_.adj_data_dir;
    init();
}

inline void CSVDirDataImpl::init() {
    print(fg(fmt::color::yellow), "Reading asset pricing data in directory {}\n", raw_data_dir);
    codes_.clear();
    for (const auto &entry : std::filesystem::directory_iterator(raw_data_dir)) {
        auto file_path = entry.path().filename();
        auto adj_file_path = std::filesystem::path(adj_data_dir) / file_path;
        if (!exists(adj_file_path)) {
            print(fg(fmt::color::red), "Adjust data file {} doesn't exist. Ignore asset {}.\n",
                  adj_file_path.string(), file_path.string());
        } else {
            raw_files.emplace_back(entry.path());
            adj_files.emplace_back(adj_file_path);
            try {
                raw_data_filenames.emplace_back(entry.path().string());

            } catch (const std::exception &e) {
                std::cerr << "錯誤:在處理文件路徑時出現問題 - " << e.what() << '\n';
                std::cerr << "原始數據文件路徑: " << entry.path().string() << '\n';
                std::cerr << "調整後數據文件路徑: " << adj_file_path.string() << '\n';
            }
            adj_data_filenames.emplace_back(adj_file_path.string());
            ++assets_;
            codes_.emplace_back(file_path.string());
        }
    }

    next_.resize(assets_);
    times.resize(assets_);
    status.resize(assets_, Invalid);
    raw_line_buffer.resize(assets_);
    adj_line_buffer.resize(assets_);
    raw_parsed_buffer.resize(assets_);
    adj_parsed_buffer.resize(assets_);
    raw_parsed_double_buffer.resize(assets_);
    adj_parsed_double_buffer.resize(assets_);

    // Read header.
#pragma omp parallel for
    for (int i = 0; i < assets_; ++i) {
        std::getline(raw_files[i], raw_line_buffer[i]);
        std::getline(adj_files[i], adj_line_buffer[i]);
    }

    // init_extra_data();

    BasePriceDataImpl::init();
}

#define UNWRAP(...) __VA_ARGS__
#define BK_CSVDirectoryDataImpl_extra_col(name, init)                                              \
    inline CSVDirDataImpl &CSVDirDataImpl::extra_##name##_col(                                     \
        const std::vector<std::pair<int, std::string>> &cols) {                                    \
        for (const auto &col : cols) {                                                             \
            extra_##name##_col_.emplace_back(col.first);                                           \
                                                                                                   \
            const auto &name_ = col.second;                                                        \
            extra_##name##_col_names_.emplace_back(name_);                                         \
            next_.name##_data_[name_] = init;                                                      \
            std::cout << name_ << " size " << next_.name##_data_[name_].size() << std::endl;       \
            extra_##name##_data_ref_.emplace_back(next_.name##_data_[name_]);                      \
        }                                                                                          \
        std::cout << extra_num_data_ref_[0].get().size() << std::endl;                             \
        return *this;                                                                              \
    }
BK_CSVDirectoryDataImpl_extra_col(num, UNWRAP(VecArrXd::Zero(assets_)));
BK_CSVDirectoryDataImpl_extra_col(str, UNWRAP(std::vector<std::string>(assets_)));
#undef BK_CSVDirectoryDataImpl_extra_col
#undef UNWRAP

bool CSVDirDataImpl::read() {

    next_.reset();
    // read data.
    for (int i = 0; i < assets_; ++i) {
        switch (status[i]) {
        case Finished:
            break;
        case Invalid:
            // Try read data from file.
            if (!raw_files[i].eof()) {
                std::getline(raw_files[i], raw_line_buffer[i]);
                raw_parsed_buffer[i] = CSVRowParaser::parse_row(raw_line_buffer[i]);
                if (raw_parsed_buffer[i].empty()) {
                    status[i] = Finished;
                    break;
                }
                std::getline(adj_files[i], adj_line_buffer[i]);
                adj_parsed_buffer[i] = CSVRowParaser::parse_row(adj_line_buffer[i]);
                const auto &[t1, t2] = std::make_tuple(raw_parsed_buffer[i][tohlc_map[0]],
                                                       adj_parsed_buffer[i][tohlc_map[0]]);
                if (t1 != t2) {
                    print(fg(fmt::color::red),
                          "data in raw data file {} and adjusted data file {} have different "
                          "dates: {} "
                          "and {}. Please check data. Now abort...\n",
                          raw_data_filenames[i], adj_data_filenames[i], t1, t2);
                    std::abort();
                }
                times[i] = boost::posix_time::time_from_string(time_converter_(t1));

                status[i] = Valid; // After reading data, set to valid.
            } else {
                status[i] = Finished;
            }
            break;
        case Valid:
            break;
        default:
            break;
        }
    }
    if (std::ranges::all_of(status, [](const auto &e) { return e == Finished; })) {
        return false;
    }
    // Find smallest time.
    auto it = std::ranges::min_element(times);
    if (it != times.end()) {
        next_.time = *it;
        // Read data.
#pragma omp parallel for
        for (int i = 0; i < assets_; ++i) {
            if ((status[i] == Valid) && (times[i] == *it)) {
                for (int j = 0; j < 4; ++j) {
                    // Fill ohlc data.
                    raw_parsed_double_buffer[i][j] =
                        CSVRowParaser::parse_double(raw_parsed_buffer[i][tohlc_map[j + 1]]);
                    adj_parsed_double_buffer[i][j] =
                        CSVRowParaser::parse_double(adj_parsed_buffer[i][tohlc_map[j + 1]]);
                }
                next_.data.open = raw_parsed_double_buffer[i][0];
                next_.data.high = raw_parsed_double_buffer[i][1];
                next_.data.low = raw_parsed_double_buffer[i][2];
                next_.data.close = raw_parsed_double_buffer[i][3];

                next_.adj_data.open = adj_parsed_double_buffer[i][0];
                next_.adj_data.high = adj_parsed_double_buffer[i][1];
                next_.adj_data.low = adj_parsed_double_buffer[i][2];
                next_.adj_data.close = adj_parsed_double_buffer[i][3];

                // Fill extra data
                for (int j = 0; j < extra_num_col_.size(); ++j) {
                    extra_num_data_ref_[j].get().coeffRef(i) =
                        CSVRowParaser::parse_double(raw_parsed_buffer[i][extra_num_col_[j]]);
                }
                for (int j = 0; j < extra_str_col_.size(); ++j) {
                    extra_str_data_ref_[j].get()[i] = raw_parsed_buffer[i][extra_str_col_[j]];
                }

                status[i] = Invalid; // After read data, set to invalid.
            }
        }
        next_.validate_assets();
        return true;
    } else {
        return false;
    }
}

std::shared_ptr<BasePriceDataImpl> CSVDirDataImpl::clone() {
    auto res =
        std::make_shared<CSVDirDataImpl>(raw_data_dir, adj_data_dir, tohlc_map, time_converter_);
    res->extra_num_col_ = extra_num_col_;
    res->extra_str_col_ = extra_str_col_;
    res->extra_num_col_names_ = extra_num_col_names_;
    res->extra_str_col_names_ = extra_str_col_names_;

    return res;
}

CSVCommonDataImpl ::CSVCommonDataImpl(const std::string &file,
                                      TimeStrConv::func_type time_converter,
                                      const std::vector<int> str_cols)
    : BaseCommonDataFeedImpl(time_converter), file(file),
      str_cols_(str_cols.begin(), str_cols.end()) {
    util::check_path_exists(file);

    init();
}

CSVCommonDataImpl::CSVCommonDataImpl(const CSVCommonDataImpl &impl_)
    : BaseCommonDataFeedImpl(impl_) {
    str_cols_ = impl_.str_cols_;
    file = impl_.file;
    init();
}

bool CSVCommonDataImpl::read() {
    std::getline(data_file_, next_row_);
    if (data_file_.eof()) {
        finished_ = true;
        return false;
    }
    auto row_string = CSVRowParaser::parse_row(next_row_);
    auto &ts = row_string[0];
    if (time_converter_) {
        ts = time_converter_(ts);
    }
    next_.time = boost::posix_time::time_from_string(ts);

    for (int i = 1; i < row_string.size(); ++i) {
        if (!str_cols_.contains(i)) {
            next_.num_data_[col_names_[i - 1]] = CSVRowParaser::parse_double(row_string[i]);
        } else {
            next_.str_data_[col_names_[i - 1]] = row_string[i];
        }
    }
    // std::cout << util::format_map(next_.num_data_) << std::endl;

    return true;
}

std::shared_ptr<BaseCommonDataFeedImpl> CSVCommonDataImpl::clone() {
    auto res = std::make_shared<CSVCommonDataImpl>(*this);
    return res;
}
void CSVCommonDataImpl::init() { // Read header

    data_file_ = std::ifstream(file);
    std::string header;
    std::getline(data_file_, header);

    // Read one line and set col_names.
    auto row_string = CSVRowParaser::parse_row(header);
    for (int i = 1; i < row_string.size(); ++i) {
        boost::algorithm::trim(row_string[i]);
        col_names_.emplace_back(std::move(row_string[i]));
    }
}

} // namespace feeds
} // namespace backtradercpp
