#pragma once

/*ZhouYao at 2022-09-10*/

#include "Common.hpp"
#include "DataFeeds.hpp"
#include "Broker.hpp"
#include "util.hpp"

#include <boost/serialization/unordered_map.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/date_time/posix_time/time_serialize.hpp>

#include "3rd_party/boost_serialization_helper/save_load_eigen.h"

#define STRATEGYDUMPUTIL_SETGET_IMPL(name, type, ret_type)                                         \
    inline void StrategyDumpUtil::set_##name(const std::string &key, type v) { name[key] = v; }    \
    inline std::optional<ret_type> StrategyDumpUtil::get_##name(const std::string &key) {          \
        auto it = name.find(key);                                                                  \
        if (it != name.end()) {                                                                    \
            return it->second;                                                                     \
        } else {                                                                                   \
            return {};                                                                             \
        }                                                                                          \
    }                                                                                              \
    inline void StrategyDumpUtil::set_timed_##name(const ptime &t, const std::string &key,         \
                                                   type v) {                                       \
        timed_##name[t][key] = v;                                                                  \
    }                                                                                              \
    inline std::optional<ret_type> StrategyDumpUtil::get_timed_##name(const ptime &t,              \
                                                                      const std::string &key) {    \
        auto it1 = timed_##name.find(t);                                                           \
        if (it1 != timed_##name.end()) {                                                           \
            auto it2 = it1->second.find(key);                                                      \
            if (it2 != it1->second.end()) {                                                        \
                return it2->second;                                                                \
            } else {                                                                               \
                return {};                                                                         \
            }                                                                                      \
        } else {                                                                                   \
            return {};                                                                             \
        }                                                                                          \
    }
namespace backtradercpp {
namespace strategy {
class Cerebro;

class StrategyDumpUtil {
  public:
    void set_var(const std::string &key, double v);
    void set_vars(const std::unordered_map<std::string, double> &m) { util::update_map(var, m); };
    void set_vec(const std::string &key, const VecArrXd &v);
    void set_mat(const std::string &key, const RowArrayXd &v);

    std::optional<double> get_var(const std::string &key);
    std::optional<VecArrXd> get_vec(const std::string &key);
    std::optional<RowArrayXd> get_mat(const std::string &key);

    void set_timed_var(const ptime &t, const std::string &key, double v);
    void set_timed_vec(const ptime &t, const std::string &key, const VecArrXd &v);
    void set_timed_mat(const ptime &t, const std::string &key, const RowArrayXd &v);

    std::optional<double> get_timed_var(const ptime &t, const std::string &key);
    std::optional<VecArrXd> get_timed_vec(const ptime &t, const std::string &key);
    std::optional<RowArrayXd> get_timed_mat(const ptime &t, const std::string &key);

    template <class Archive> void serialize(Archive &ar, const unsigned int version) {
        ar & var;
        ar & vec;
        ar & mat;

        ar & timed_var;
        ar & timed_vec;
        ar & timed_mat;
    }

    void load() {
        if (std::filesystem::exists(std::filesystem::path(dump_file_name_))) {
            std::ifstream file(dump_file_name_);
            boost::archive::binary_iarchive i(file);
            i >> (*this);
        }
    }
    void save() {
        std::ofstream file(dump_file_name_);
        boost::archive::binary_oarchive o(file);
        o << *this;
    };
    void set_dump_file(const std::string &file, bool read_dump) {
        dump_file_name_ = file;
        read_dump_ = read_dump;
        save_dump_ = true;
        if (read_dump)
            load();
    }
    void finish() {
        if (save_dump_)
            save();
    }

  private:
    // std::unordered_map<std::string, int> int_var;
    std::unordered_map<std::string, double> var;
    std::unordered_map<std::string, VecArrXd> vec;
    std::unordered_map<std::string, RowArrayXd> mat;

    std::unordered_map<ptime, std::unordered_map<std::string, int>> timed_var;
    std::unordered_map<ptime, std::unordered_map<std::string, VecArrXd>> timed_vec;
    std::unordered_map<ptime, std::unordered_map<std::string, RowArrayXd>> timed_mat;

    bool save_dump_ = false;
    bool read_dump_ = false;
    std::string dump_file_name_;
};

class GenericStrategy {
    friend class Cerebro;

  public:
    int time_index_ = -1, id_ = 0; // id_ is used for multiple strategies support
    const auto &time() const { return price_feed_agg_->time(); }
    int time_index() const { return time_index_; }

    auto &datas() const { return price_feed_agg_->datas(); }
    const auto &stock_data(const std::string &broker) const {
        return (datas()).at(broker); // 使用 std::string 來查找
    }
    const auto &data(const std::string &broker_name) const {
        std::string broker_id =
            std::to_string(broker_agg_->broker_id(broker_name)); // 假設 broker_id 是 string
        return (datas()).at(broker_id);                          // 使用 at() 來查找鍵
    }

    const auto &common_datas() const { return common_feed_agg_->datas(); }
    const auto &common_data(int i) const {
        const auto &map = common_datas(); // 獲取 unordered_map
        const auto &keys = get_keys();    // 假設這是一個存有鍵的向量

        if (i < 0 || i >= keys.size()) {
            throw std::out_of_range("Index out of range");
        }

        const auto &key = keys[i]; // 根據 i 獲取對應的鍵
        auto it = map.find(key);   // 在 unordered_map 中查找對應的鍵

        if (it == map.end()) {
            throw std::runtime_error("Key not found in common_datas()");
        }

        return it->second; // 返回對應鍵的 vector<CommonFeedData>
    }
    std::vector<std::string> get_keys() const {
        const auto &map = common_datas(); // 假設 common_datas() 返回 unordered_map
        std::vector<std::string> keys;
        keys.reserve(map.size()); // 儲存所有鍵的空間

        // 遍歷 unordered_map，提取所有的鍵
        for (const auto &pair : map) {
            keys.push_back(pair.first); // 將每個鍵存入 keys 向量
        }

        return keys; // 返回鍵的向量
    }

    bool data_valid(int feed) const { return price_feed_agg_->data_valid(feed); }

    virtual void init() {};
    virtual void init_strategy(feeds::PriceFeedAggragator *feed_agg,
                               feeds::CommonFeedAggragator *common_feed_agg,
                               broker::BrokerAggragator *broker_agg);

    const Order &buy(int broker_id, int asset, double price, int volume, int time_index,
                     date_duration til_start_d = days(0), time_duration til_start_t = hours(0),
                     date_duration start_to_end_d = days(0),
                     time_duration start_to_end_t = hours(23));
    const Order &buy(const std::string &broker_name, int asset, double price, int volume,
                     int time_index, date_duration til_start_d = days(0),
                     time_duration til_start_t = hours(0), date_duration start_to_end_d = days(0),
                     time_duration start_to_end_t = hours(23)) {
        // std::cout << "buy stock" << asset << " with price " << price << std::endl;
         buy(broker_id(broker_name), asset, price, volume, time_index, til_start_d,
                   til_start_t, start_to_end_d, start_to_end_t);
    }
    const Order &buy(int broker_id, int asset, std::shared_ptr<GenericPriceEvaluator> price_eval,
                     int volume, int time_index, date_duration til_start_d = days(0),
                     time_duration til_start_t = hours(0), date_duration start_to_end_d = days(0),
                     time_duration start_to_end_t = hours(23));
    const Order &buy(const std::string &broker_name, int asset,
                     std::shared_ptr<GenericPriceEvaluator> price_eval, int volume, int time_index,
                     date_duration til_start_d = days(0), time_duration til_start_t = hours(0),
                     date_duration start_to_end_d = days(0),
                     time_duration start_to_end_t = hours(23)) {
        buy(broker_id(broker_name), asset, price_eval, volume, time_index, til_start_d, til_start_t,
            start_to_end_d, start_to_end_t);
    }
    const Order &delayed_buy(int broker_id, int asset, double price, int volume, int time_index,
                             date_duration til_start_d = days(1),
                             time_duration til_start_t = hours(0),
                             date_duration start_to_end_d = days(1),
                             time_duration start_to_end_t = hours(0));
    const Order &delayed_buy(const std::string &broker_name, int asset, double price, int volume,
                             int time_index, date_duration til_start_d = days(1),
                             time_duration til_start_t = hours(0),
                             date_duration start_to_end_d = days(1),
                             time_duration start_to_end_t = hours(0)) {
        delayed_buy(broker_id(broker_name), asset, price, volume, time_index, til_start_d,
                    til_start_t, start_to_end_d, start_to_end_t);
    }
    const Order &delayed_buy(int broker_id, int asset,
                             std::shared_ptr<GenericPriceEvaluator> price_eval, int volume,
                             int time_index, date_duration til_start_d = days(1),
                             time_duration til_start_t = hours(0),
                             date_duration start_to_end_d = days(1),
                             time_duration start_to_end_t = hours(0));
    const Order &delayed_buy(const std::string &broker_name, int asset,
                             std::shared_ptr<GenericPriceEvaluator> price_eval, int volume,
                             int time_index, date_duration til_start_d = days(1),
                             time_duration til_start_t = hours(0),
                             date_duration start_to_end_d = days(1),
                             time_duration start_to_end_t = hours(0)) {
        delayed_buy(broker_id(broker_name), asset, price_eval, volume, time_index, til_start_d,
                    til_start_t, start_to_end_d, start_to_end_t);
    }
    Order delayed_buy(int broker_id, int asset, int price, int volume, int time_index, ptime start,
                      ptime end);

    const Order &close(int broker_id, int asset, double price, int time_index);
    const Order &close(const std::string &broker_name, int asset, double price, int time_index) {
        // std::cout << "close stock" << asset << " with price " << price << std::endl;
         close(broker_id(broker_name), asset, price, time_index);
    }
    const Order &close(int broker_id, int asset, std::shared_ptr<GenericPriceEvaluator> price_eval,
                       int time_index);
    const Order &close(const std::string &broker_name, int asset,
                       std::shared_ptr<GenericPriceEvaluator> price_eval, int time_index) {
        close(broker_id(broker_name), asset, price_eval, time_index);
    }

    // Use today's open as target price. Only TOTAL_FRACTION of total total_wealth will be allocated
    // to reserving for fee.
    template <int UNIT = 1>
    void adjust_to_weight_target(int broker_id, const VecArrXd &w, const VecArrXd &p = VecArrXd(),
                                 double TOTAL_FRACTION = 0.99);
    void adjust_to_weight_target(const std::string &broker_name, const VecArrXd &w,
                                 const VecArrXd &p = VecArrXd(), double TOTAL_FRACTION = 0.99) {
        adjust_to_weight_target(broker_id(broker_name), w, p, TOTAL_FRACTION);
    };

    void adjust_to_volume_target(int broker_id, const VecArrXi &target_volume, const double &p = 0,
                                 double TOTAL_FRACTION = 0.99);
    void adjust_to_volume_target(const std::string &broker_name, const VecArrXi &target_volume,
                                 const double &p = 0, double TOTAL_FRACTION = 0.99) {
        adjust_to_volume_target(broker_id(broker_name), target_volume, p, TOTAL_FRACTION);
    };

    void pre_execute() {
        order_pool_.orders.clear();
        // ++time_index_;
    }

    const auto &execute() {
        pre_execute();
        run();
        // std::cout << "order_pool_ size: " << order_pool_.orders.size() << std::endl;
        return order_pool_;
    }

    virtual void run() = 0;
    virtual void reset() { time_index_ = -1; }

    std::unordered_map<int, int> assets(int broker) const { return broker_agg_->assets(broker); }
    std::unordered_map<int, int> assets(const std::string &broker_name) const {
        return broker_agg_->assets(broker_name);
    }

    double wealth(int broker) const { return broker_agg_->wealth(broker); }
    double cash(int broker) const { return broker_agg_->cash(broker); };

    const std::unordered_map<int, int> &positions(int broker) const;
    double position(int broker, int asset) const;

    const VecArrXd &values(int broker) const;
    double value(int broker, int asset) const;

    const VecArrXd &profits(int broker) const;
    double profit(int broker, int asset) const;
    const VecArrXd &adj_profits(int broker) const;
    double adj_profit(int broker, int asset) const;

    const auto &portfolio(int broker) const;
    const auto &portfolio_items(int broker) const;

    const std::vector<std::string> &codes(int broker) const {
        auto feed = price_feed_agg_->feed(broker); // 确保 feed 返回正确的对象
        return feed.codes();                       // 调用 .codes() 方法
    }

    int broker_id(const std::string &name) const { return broker_agg_->broker_id(name); }

    // void set_int_vars(const std::unordered_map<std::string, int> &vars) { int_vars_ = vars; }
    // void set_int_var(const std::string &name, int val) { int_vars_[name] = val; }
    // auto get_int_var(const std::string &name) { return int_vars_[name]; }
    // void set_vars(const std::unordered_map<std::string, double> &vars) { double_vars_ = vars; }
    // void set_var(const std::string &name, double val) { double_vars_[name] = val; }
    // auto get_var(const std::string &name) { return double_vars_[name]; }

    // Set read_dump to false if only want to store dump.
    void set_dump_file(const std::string &file, bool read_dump = true) {
        dump_.set_dump_file(file, read_dump);
    }

    void set_var(const std::string &key, double v) { return dump_.set_var(key, v); }
    void set_vars(const std::unordered_map<std::string, double> &m) { dump_.set_vars(m); }
    void set_vec(const std::string &key, const VecArrXd &v) { dump_.set_vec(key, v); }
    void set_mat(const std::string &key, const RowArrayXd &v) { dump_.set_mat(key, v); }

    std::optional<double> get_var(const std::string &key) { return dump_.get_var(key); }
    std::optional<VecArrXd> get_vec(const std::string &key) { return dump_.get_vec(key); }
    std::optional<RowArrayXd> get_mat(const std::string &key) { return dump_.get_mat(key); }

    void set_timed_var(const std::string &key, double v) { dump_.set_timed_var(time(), key, v); }
    void set_timed_vec(const std::string &key, const VecArrXd &v) {
        dump_.set_timed_vec(time(), key, v);
    }
    void set_timed_mat(const std::string &key, const RowArrayXd &v) {
        dump_.set_timed_mat(time(), key, v);
    };

    std::optional<double> get_timed_var(const std::string &key) {
        return dump_.get_timed_var(time(), key);
    }
    std::optional<VecArrXd> get_timed_vec(const std::string &key) {
        return dump_.get_timed_vec(time(), key);
    }
    std::optional<RowArrayXd> get_timed_mat(const std::string &key) {
        return dump_.get_timed_mat(time(), key);
    }

    virtual void finish() {};
    void finish_all() {
        try {
            dump_.save();
        } catch (const std::exception &e) {
            // std::cerr << "Error: - " << e.what() << '\n';
        }
    }

    virtual ~GenericStrategy() = default;

  private:
    Cerebro *cerebro_;
    feeds::PriceFeedAggragator *price_feed_agg_;
    feeds::CommonFeedAggragator *common_feed_agg_;

    broker::BrokerAggragator *broker_agg_;

    OrderPool order_pool_;

    StrategyDumpUtil dump_;
};

//--------------------------------------------------------------------------------------------------------------
STRATEGYDUMPUTIL_SETGET_IMPL(var, double, double)
STRATEGYDUMPUTIL_SETGET_IMPL(vec, const VecArrXd &, VecArrXd)
STRATEGYDUMPUTIL_SETGET_IMPL(mat, const RowArrayXd &, RowArrayXd)

inline void GenericStrategy::init_strategy(feeds::PriceFeedAggragator *feed_agg,
                                           feeds::CommonFeedAggragator *common_feed_agg,
                                           broker::BrokerAggragator *broker_agg) {
    price_feed_agg_ = feed_agg;
    common_feed_agg_ = common_feed_agg;

    broker_agg_ = broker_agg;

    init();
}

#define BK_STRATEGY_PORTFOLIO_MEMBER_ACESSOR(name, type, vectype)                                  \
    inline const vectype &GenericStrategy::name##s(int broker) const {                             \
        return broker_agg_->name##s(broker);                                                       \
    }                                                                                              \
    inline type GenericStrategy::name(int broker, int asset) const {                               \
        return broker_agg_->name##s(broker).coeff(asset);                                          \
    }
// BK_STRATEGY_PORTFOLIO_MEMBER_ACESSOR(position, int, VecArrXi)
BK_STRATEGY_PORTFOLIO_MEMBER_ACESSOR(profit, double, VecArrXd)
BK_STRATEGY_PORTFOLIO_MEMBER_ACESSOR(adj_profit, double, VecArrXd)
#undef BK_STRATEGY_PORTFOLIO_MEMBER_ACESSOR

// 單個資產的獲取
#define BK_STRATEGY_PORTFOLIO_MEMBER_ACCESSOR_SINGLE(name, type)                                   \
    inline type GenericStrategy::name(int broker, int asset) const {                               \
        const auto &map = broker_agg_->name##s(broker);                                            \
        auto it = map.find(asset);                                                                 \
        return it != map.end() ? it->second : 0;                                                   \
    }

// 獲取整個資產集合的映射
#define BK_STRATEGY_PORTFOLIO_MEMBER_ACCESSOR_MAP(name, type)                                      \
    inline const std::unordered_map<int, type> &GenericStrategy::name##s(int broker) const {       \
        return broker_agg_->name##s(broker);                                                       \
    }

// 使用宏創建 position 獲取函數
BK_STRATEGY_PORTFOLIO_MEMBER_ACCESSOR_SINGLE(position, double)
BK_STRATEGY_PORTFOLIO_MEMBER_ACCESSOR_MAP(position, int)

// 移除宏定義
#undef BK_STRATEGY_PORTFOLIO_MEMBER_ACCESSOR_SINGLE
#undef BK_STRATEGY_PORTFOLIO_MEMBER_ACCESSOR_MAP

const auto &GenericStrategy::portfolio(int broker) const { return broker_agg_->portfolio(broker); }

const auto &GenericStrategy::portfolio_items(int broker) const {
    return broker_agg_->portfolio(broker).portfolio_items;
}

const Order &GenericStrategy::buy(int broker_id, int asset, double price, int volume,
                                  int time_index, date_duration til_start_d,
                                  time_duration til_start_t, date_duration start_to_end_d,
                                  time_duration start_to_end_t) {
    Order order{
        .broker_id = broker_id,
        .asset = asset,
        .price = price,
        .volume = volume,
        .time_index = time_index,
        .valid_from = time() + til_start_d + til_start_t,
        .valid_until = time() + til_start_d + til_start_t + start_to_end_d + start_to_end_t,
    }; // 設置 time_index

    // std::cout << "buy stock " << asset << " with price " << price << " and volume "<< volume<< std::endl;
    
    order_pool_.orders.emplace_back(std::move(order));
    return order_pool_.orders.back();
}

const Order &GenericStrategy::buy(int broker_id, int asset,
                                  std::shared_ptr<GenericPriceEvaluator> price_eval, int volume,
                                  int time_index_, date_duration til_start_d,
                                  time_duration til_start_t, date_duration start_to_end_d,
                                  time_duration start_to_end_t) {
    Order order{.broker_id = broker_id,
                .asset = asset,
                .volume = volume,
                .price_eval = price_eval,
                .created_at = time(),
                .valid_from = time() + til_start_d + til_start_t,
                .valid_until =
                    time() + til_start_d + til_start_t + start_to_end_d + start_to_end_t};
    order_pool_.orders.emplace_back(std::move(order));
    return order_pool_.orders.back();
}

const Order &GenericStrategy::delayed_buy(int broker_id, int asset, double price, int volume,
                                          int time_index, date_duration til_start_d,
                                          time_duration til_start_t, date_duration start_to_end_d,
                                          time_duration start_to_end_t) {
    return buy(broker_id, asset, price, volume, time_index_, til_start_d, til_start_t,
               start_to_end_d, start_to_end_t);
}

const Order &GenericStrategy::delayed_buy(int broker_id, int asset,
                                          std::shared_ptr<GenericPriceEvaluator> price_eval,
                                          int volume, int time_index, date_duration til_start_d,
                                          time_duration til_start_t, date_duration start_to_end_d,
                                          time_duration start_to_end_t) {
    return buy(broker_id, asset, price_eval, volume, time_index, til_start_d, til_start_t,
               start_to_end_d, start_to_end_t);
}

inline const Order &GenericStrategy::close(int broker_id, int asset, double price, int time_index) {
    // std::cout << "close stock " << asset << " with price " << price << std::endl;
    int volume = -(broker_agg_->position(broker_id, asset));
    // std::cout << "sell volume: " << volume << std::endl;
    // std::cout << "time_index: " << time_index << std::endl;
    return delayed_buy(broker_id, asset, price, volume, time_index);
}

inline const backtradercpp::Order &backtradercpp::strategy::GenericStrategy::close(
    int broker_id, int asset, std::shared_ptr<GenericPriceEvaluator> price_eval, int time_index) {
    return delayed_buy(broker_id, asset, price_eval, -(broker_agg_->position(broker_id, asset)), time_index);
}

// template <int UNIT>
// void GenericStrategy::adjust_to_weight_target(int broker_id, const VecArrXd &w, const VecArrXd
// &p,
//                                               double TOTAL_FRACTION) {
//     // 假設我們需要處理多檔股票，遍歷所有的資產（股票代號）
//     for (const auto &price_feed_data : data(std::to_string(broker_id))) {
//         // 獲取每檔股票的開盤價
//         VecArrXd target_prices = price_feed_data.data.open;

//         // 如果 p 有指定價格，使用 p 替代
//         if (p.size() != 0) {
//             target_prices = p;
//         }

//         // 計算每檔股票的目標價值
//         VecArrXd target_value = wealth(broker_id) * TOTAL_FRACTION * w;

//         // 計算每檔股票的目標持倉
//         VecArrXi target_volume = (target_value / (target_prices * UNIT)).cast<int>();

//         // 計算目標持倉與當前持倉的差異
//         VecArrXi volume_diff = target_volume - positions(broker_id);

//         for (int i = 0; i < volume_diff.size(); ++i) {
//             // 確認當前資產有效且存在需要交易的數量
//             if (price_feed_data.valid.coeff(i) && (volume_diff.coeff(i) != 0)) {
//                 // 執行交易，根據差異進行買入或賣出
//                 buy(broker_id, i, target_prices.coeff(i), volume_diff.coeff(i), time_index_);
//             }
//         }
//     }
// }

void GenericStrategy::adjust_to_volume_target(int broker_id, const VecArrXi &target_volume,
                                              const double &p, double TOTAL_FRACTION) {
    // 遍歷每個資產（股票代號）
    for (const auto &price_feed_data : data(std::to_string(broker_id))) {
        // 獲取該資產的開盤價格資料
        double target_prices = price_feed_data.data.open;

        // 如果 p 有指定價格，則使用 p 替代開盤價格
        if (p != 0) {
            target_prices = p;
        }

        // 獲取當前持倉
        auto position_it = positions(broker_id).find(stoi(price_feed_data.ticker_));
        int current_position =
            (position_it != positions(broker_id).end()) ? position_it->second : 0;

        // 計算持倉差異
        VecArrXi volume_diff = target_volume - current_position;

        // 執行交易
        for (int i = 0; i < volume_diff.size(); ++i) {
            // 當前資產有效，且需要調整持倉
            if (price_feed_data.valid.coeff(i) && (volume_diff.coeff(i) != 0)) {
                // 買入或賣出以達到目標持倉量
                buy(broker_id, i, target_prices, volume_diff.coeff(i), time_index_);
            }
        }
    }
}

} // namespace strategy
} // namespace backtradercpp
