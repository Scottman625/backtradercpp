#pragma once
/*ZhouYao at 2022-09-10*/

#include "DataFeeds.hpp"
#include "Broker.hpp"
#include "Strategy.hpp"
#define SPDLOG_FMT_EXTERNAL
#include <spdlog/stopwatch.h>

// #include<chrono>
namespace backtradercpp {

enum class VerboseLevel { None, OnlySummary, AllInfo };
class Cerebro {
  public:
    Cerebro(feeds::PriceData &priceData)
        : price_feeds_agg_(priceData.sp->get_stock_data(),
                           priceData.sp->get_next_index_date_change()),
          broker_agg_(priceData.sp->get_stock_data()) {}
    // window is for strategy. DataFeed and Broker doesn't store history data.
    void add_broker(broker::BaseBroker broker, int window = 1);
    void add_common_data(feeds::BaseCommonDataFeed data, int window);

    // void init_feeds_aggrator_();
    void add_strategy(std::shared_ptr<strategy::GenericStrategy> strategy);
    void init_strategy();
    void set_range(const date &start, const date &end = date(boost::date_time::max_date_time));
    // Set a directory for logging.
    void set_log_dir(const std::string &dir);
    void set_verbose(VerboseLevel v) { verbose_ = v; };

    void run();
    void reset();

    auto broker(int broker);
    auto broker(const std::string &broker_name);

    // const std::vector<PriceFeedDataBuffer> &datas() const {
    //     static std::vector<PriceFeedDataBuffer> data_buffers;

    //     data_buffers.clear();                       // 確保結果是空的
    //     const auto &map = price_feeds_agg_->datas(); // 假設這返回 unordered_map

    //     // 遍歷 unordered_map 並將其轉換為 PriceFeedDataBuffer
    //     for (const auto &pair : map) {
    //         for (const auto &price_feed_data : pair.second) {
    //             PriceFeedDataBuffer buffer;
    //             buffer.load_from_price_feed_data(price_feed_data); // 假設有這樣的函數
    //             data_buffers.push_back(buffer);
    //         }
    //     }

    //     return data_buffers;
    // }

    // const std::unordered_map<std::string, std::vector<PriceFeedData>> &stock_datas() const {
    //    return *price_data_impl->get_stock_data();
    // }

    const auto &performance() const { return broker_agg_.performance(); }

    Cerebro clone();

  private:
    // std::vector<int> asset_broker_map_
    // std::shared_ptr<feeds::PriceDataImpl> price_data_impl; // 變更為 shared_ptr 來指向同一個資料
    feeds::PriceFeedAggragator price_feeds_agg_;
    feeds::CommonFeedAggragator common_feeds_agg_;

    broker::BrokerAggragator broker_agg_;
    std::shared_ptr<strategy::GenericStrategy> strategy_;
    // strategy::FullAssetData data_;
    ptime start_{boost::posix_time::min_date_time}, end_{boost::posix_time::max_date_time};

    VerboseLevel verbose_ = VerboseLevel::OnlySummary;
};

void Cerebro::add_broker(broker::BaseBroker broker, int window) {
    std::cout << "add_broker test started.." << std::endl;
    price_feeds_agg_.add_feed(broker.feed());
    // price_feeds_agg_.set_window(price_feeds_agg_.datas().size() - 1, window);
    std::cout << "add_broker test test02.." << std::endl;
    broker_agg_.add_broker(broker);
    std::cout << "add_broker test finished.." << std::endl;
}
void Cerebro::add_common_data(feeds::BaseCommonDataFeed data, int window) {
    common_feeds_agg_.add_feed(data);
    // common_feeds_agg_.set_window(common_feeds_agg_.datas().size() - 1, window);
};

void Cerebro::add_strategy(std::shared_ptr<strategy::GenericStrategy> strategy) {
    strategy_ = strategy;
}

void Cerebro::init_strategy() {
    strategy_->init_strategy(&price_feeds_agg_, &common_feeds_agg_, &broker_agg_);
}

inline void Cerebro::set_range(const date &start, const date &end) {
    start_ = ptime(start);
    end_ = ptime(end);
}

void Cerebro::run() {
    if (verbose_ == VerboseLevel::AllInfo)
        fmt::print(fmt::fg(fmt::color::yellow), "Running strategy with multiple stocks..\n");

    init_strategy(); // 初始化策略

    ptime previous_time = boost::posix_time::not_a_date_time; // 记录上一次的时间
    fmt::print("previous_time initialized 1: {}\n",
               boost::posix_time::to_simple_string(previous_time));

    while (!price_feeds_agg_.finished()) {
        // std::cout << "price_feeds_agg_.finished() is false" << std::endl;
        spdlog::stopwatch sw; // 用于记录时间
        if (price_feeds_agg_.time() > end_) {
            fmt::print("End of the range.\n");
            break;
        }

        if (!price_feeds_agg_.read()) {
            fmt::print("End of the range2.\n");
            break;
        }

        // 获取当前时间
        ptime current_time = price_feeds_agg_.time();
        // fmt::print("times_[i]: {}\n", boost::posix_time::to_simple_string(current_time));

        // common_feeds_agg_.read();

        bool next_index_date_change = *price_feeds_agg_.get_next_index_date_change();

        // 判断是否在指定时间范围内
        if (current_time >= start_) {
            try {

                // 如果日期发生变化，则增加 time_index_
                if (current_time.date() != previous_time.date()) {
                    fmt::print("Date changed from {} to {}. Incrementing time_index_\n",
                               boost::gregorian::to_iso_extended_string(previous_time.date()),
                               boost::gregorian::to_iso_extended_string(current_time.date()));

                    strategy_->time_index_++;     // 日期变化时增加时间步长
                    previous_time = current_time; // 更新上一次的时间
                }

                if (previous_time.is_not_a_date_time()) {
                    previous_time = current_time;
                    fmt::print("previous_time initialized 2: {}\n",
                               boost::posix_time::to_simple_string(previous_time));
                }

                broker_agg_.update_current_map();
                broker_agg_.process_old_orders(price_feeds_agg_.datas());
                if (next_index_date_change) {
                    auto order_pool = strategy_->execute();
                    if (!order_pool.orders.empty()) {
                        broker_agg_.process(order_pool, price_feeds_agg_.datas());
                    }
                    broker_agg_.process_terms();
                    broker_agg_.update_info();

                    if (verbose_ == VerboseLevel::AllInfo) {
                        // std::cout << "cash: " << broker_agg_.cash(0) << ",  total_wealth: "
                        //           << broker_agg_.total_wealth() << std::endl;
                        // fmt::print("cash: {:12.4f},  total_wealth: {:12.2f}\n",
                        //            broker_agg_.total_cash(), broker_agg_.total_wealth());
                        fmt::print("Using {} seconds.\n", util::sw_to_seconds(sw));
                    }
                }

            } catch (const std::exception &e) {
                std::cout << "錯誤: - " << e.what() << '\n';
            }
        }
    }

    // 打印總結信息
    if (verbose_ == VerboseLevel::OnlySummary || verbose_ == VerboseLevel::AllInfo)
        broker_agg_.summary();

    strategy_->finish_all();
}

void Cerebro::reset() {
    price_feeds_agg_.reset();
    common_feeds_agg_.reset();
    broker_agg_.reset();
    strategy_->reset();
}

auto Cerebro::broker(int broker) { return broker_agg_.broker(broker); }
auto Cerebro::broker(const std::string &broker_name) { return broker_agg_.broker(broker_name); }
void Cerebro::set_log_dir(const std::string &dir) {
    if (!std::filesystem::exists(dir)) {
        std::filesystem::create_directories(dir);
    }

    broker_agg_.set_log_dir(dir);
}

Cerebro Cerebro::clone() {
    Cerebro cerebro = *this;
    price_feeds_agg_ = price_feeds_agg_.clone();
    common_feeds_agg_ = common_feeds_agg_.clone();
    broker_agg_.sync_feed_agg(price_feeds_agg_);
    return cerebro;
}
}; // namespace backtradercpp
