#pragma once
/*ZhouYao at 2022-09-10*/

#include "Common.hpp"
#include "DataFeeds.hpp"
#include "AyalysisUtil.hpp"
#include <list>
#include <fmt/core.h>

// #include "BaseBrokerImpl.hpp"

namespace backtradercpp {
class Cerebro;
namespace broker {
struct GenericCommission {
    GenericCommission() = default;
    GenericCommission(double long_rate, double short_rate)
        : long_commission_rate(long_rate), short_commission_rate(short_rate) {}

    virtual double cal_commission(double price, int volume) const {
        return (volume >= 0) ? price * volume * long_commission_rate
                             : -price * volume * short_commission_rate;
    }
    // virtual VecArrXd cal_commission(VecArrXd price, VecArrXi volume) const;
    double long_commission_rate = 0, short_commission_rate = 0;
};
struct GenericTax {
    GenericTax() = default;
    GenericTax(double long_rate, double short_rate)
        : long_tax_rate(long_rate), short_tax_rate(short_rate) {};

    virtual double cal_tax(double price, int volume) const {
        return (volume >= 0) ? price * volume * long_tax_rate : price * volume * short_tax_rate;
    }
    // virtual VecArrXd cal_tax(VecArrXd price, VecArrXi volume) const;
    double long_tax_rate = 0, short_tax_rate = 0;
};
class BrokerAggragator;
class BaseBroker;

struct BaseBrokerImplLogUtil {
    BaseBrokerImplLogUtil() = default;
    BaseBrokerImplLogUtil(const BaseBrokerImplLogUtil &util_)
        : log(util_.log), log_dir(util_.log_dir), name(util_.name) {
        if (log) {
        }
    }
    void set_log_dir(const std::string &dir, const std::string &name);
    void write_transaction(const std::string &log_) {
        if (log)
            transaction_file_ << log << std::endl;
    }
    void write_position(const std::string &log_) {
        if (log)
            position_file_ << log << std::endl;
    }

    bool log = false;
    std::string log_dir;
    std::string name;
    std::ofstream transaction_file_, position_file_;
};

class BaseBrokerImpl {

  public:
    BaseBrokerImpl(double cash = 10000, double long_rate = 0, double short_rate = 0,
                   double long_tax_rate = 0, double short_tax_rate = 0);
    // BaseBrokerImpl(const BaseBrokerImpl &impl_);

    void process(Order &order, std::unordered_map<std::string, std::vector<PriceFeedData>> datas);
    void process_old_orders(std::unordered_map<std::string, std::vector<PriceFeedData>> datas);
    virtual void process_trems() {} // Deal with terms, e.g. XRD.
    void update_info(int asset);
    const analysis::TotalValueAnalyzer &analyzer() const { return analyzer_; }

    std::unordered_map<int, int> assets() const { return positions_; }

    auto cash() const { return portfolio_.cash; }
    auto total_value() const { return portfolio_.total_value; }
    std::unordered_map<int, int> positions() const { return portfolio_.positions(); }
    VecArrXd values() const { return portfolio_.values(feed_.assets()); }
    VecArrXd profits() const { return portfolio_.profits(feed_.assets()); }
    VecArrXd adj_profits() const { return portfolio_.adj_profits(feed_.assets()); }

    void set_allow_short(bool flag) { allow_short_ = flag; }
    void set_commission_rate(double long_rate, double short_rate) {
        commission_ = std::make_shared<GenericCommission>(long_rate, short_rate);
    }

    void set_log_dir(const std::string &dir, int index);

    void resize(int n);
    void set_feed(feeds::PriceData data);
    // void set_df_feed(feeds::BasePriceDataFeed data);
    auto feed() { return feed_; }

    void set_data_ptr(std::string asset_id, PriceFeedData *data) {
        // std::string key = std::to_string(asset_id);  // 將 asset_id 轉換為 string 鍵
        current_map_[asset_id] = data; // 直接通過鍵來更新或插入值
    }

    inline void update_current_map();

    void add_order(const Order &order);

    void reset() {
        unprocessed.clear();
        analyzer_.reset();
        portfolio_.reset();
    }

    const auto &name() const { return feed_.name(); }
    const auto name(int index) const {
        const auto &name_ = name();
        return name_.empty() ? std::to_string(index) : name_;
    }

    virtual std::shared_ptr<BaseBrokerImpl> clone() {
        return std::make_shared<BaseBrokerImpl>(*this);
    }

  protected:
    friend class BrokerAggragator;
    friend class Cerebro;
    friend struct BaseBroker;

    feeds::BasePriceDataFeed feed_;
    feeds::BasePriceDataFeed df_feed_;
    std::shared_ptr<GenericCommission> commission_;
    std::shared_ptr<GenericTax> tax_;
    // 使用 map 來儲存多檔股票的數據，key 是資產ID或股票代號
    std::unordered_map<std::string, PriceFeedData *> current_map_;
    std::unordered_map<int, int> positions_; // 記錄每個資產的持倉 (資產ID -> 持倉量)

    double total_position_value = 0;

    // 方法：根據資產ID獲取對應的數據指針
    PriceFeedData *get_price_feed_data(int asset) {
        auto it = current_map_.find(std::to_string(asset));
        if (it != current_map_.end()) {
            return it->second;
        }
        return nullptr; // 找不到資產時返回 nullptr
    }

    // // 更新資料的方法
    // void set_price_feed_data(int asset, PriceFeedData* data) {
    //     current_map_[std::to_string(asset)] = data; // 將數據指針與資產ID對應
    // }

    // double cash_ = 10000;
    Portfolio portfolio_;
    // VecArrXi position_;
    // VecArrXd investments_;
    //  FullAssetData *data_;
    std::list<Order> unprocessed;

    bool allow_short_ = false, allow_default_ = false;

    std::vector<std::string> codes_;

    analysis::TotalValueAnalyzer analyzer_;

    BaseBrokerImplLogUtil log_util_;

    void _write_position();
};

struct XRDSetting {
    double bonus, addition, dividen;
};
struct XRDTable {
    std::vector<date> record_date, execute_date;
    std::vector<XRDSetting> setting;

    void read(const std::string &file, const std::vector<int> &columns);
};

struct XRDRecord {
    int code;
    int volume;
    double cash;
    ptime time;
};
// All XRD info will be read into memory.
class StockBrokerImpl : public BaseBrokerImpl {
  public:
    using BaseBrokerImpl::BaseBrokerImpl;

    void process_trems(int asset);
    // Columns: set column for record_date, execute_date, bonus嚗?,
    // addition嚗蓮憓?, dividen嚗?蝥ｇ?
    void set_xrd_dir(const std::string &dir, const std::vector<int> &columns,
                     std::function<std::string(const std::string &)> code_extract_func_ = nullptr);

  private:
    std::unordered_map<int, XRDTable> xrd_info_;

    std::unordered_multimap<date, XRDRecord> xrd_record_;

    std::function<std::string(const std::string &)> code_extract_func_ = nullptr;
};

class BaseBroker {
  protected:
    std::shared_ptr<BaseBrokerImpl> sp = nullptr;

  public:
    BaseBroker(double cash = 10000, double long_rate = 0, double short_rate = 0,
               double long_tax_rate = 0, double short_tax_rate = 0)
        : sp(std::make_shared<BaseBrokerImpl>(cash, long_rate, short_rate, long_tax_rate,
                                              short_tax_rate)) {
        // std::cout << "BaseBroker init.." << std::endl;
    }

    virtual BaseBroker &commission_eval(std::shared_ptr<GenericCommission> commission_) {
        sp->commission_ = commission_;
        return *this;
    }

    virtual BaseBroker &set_commission_rate(double long_rate, double short_rate) {
        sp->set_commission_rate(long_rate, short_rate);
        return *this;
    }

    virtual BaseBroker &tax(std::shared_ptr<GenericTax> tax_) {
        sp->tax_ = tax_;
        return *this;
    }

    virtual BaseBroker &allow_short() {
        sp->allow_short_ = true;
        return *this;
    }
    virtual BaseBroker &allow_default() {
        sp->allow_default_ = true;
        return *this;
    }

    virtual BaseBroker &reset() {
        sp->reset();
        return *this;
    }
    auto &portfolio() const { return sp->portfolio_; };

    void process(Order &order, std::unordered_map<std::string, std::vector<PriceFeedData>> datas) {
        sp->process(order, datas);
    }
    void process_old_orders(std::unordered_map<std::string, std::vector<PriceFeedData>> datas_) {
        sp->process_old_orders(datas_);
    }
    void process_terms() { sp->process_trems(); }
    void update_current_map() { sp->update_current_map(); }
    void update_info(int asset_id) { sp->update_info(asset_id); }
    const analysis::TotalValueAnalyzer &analyzer() const { return sp->analyzer(); }
    void set_log_dir(const std::string &dir, int index);

    const auto data_ptr() const { return sp->current_map_; }

    std::unordered_map<int, int> assets() const { return sp->assets(); }
    auto cash() const { return sp->cash(); }
    auto total_value() const { return sp->total_value(); }

    std::unordered_map<int, int> positions() const { return sp->positions(); }
    std::unordered_map<int, int> positions_() const { return sp->assets(); }
    VecArrXd values(int asset_id) const { return sp->values(); }
    VecArrXd profits() const { return sp->profits(); }
    VecArrXd adj_profits() const { return sp->adj_profits(); };

    void resize(int n) { sp->resize(n); };

    virtual BaseBroker &set_feed(feeds::PriceData data) {
        std::cout << "test set_feed started ..." << std::endl;
        sp->set_feed(data);
        return *this;
    }

    // virtual BaseBroker &set_df_feed(feeds::BasePriceDataFeed data) {
    //     std::cout << "test set_df_feed started ..." << std::endl;
    //     sp->set_df_feed(data);
    //     std::cout << "test set_df_feed finished ..." << std::endl;
    //     return *this;
    // }
    auto feed() const { return sp->feed(); }

    void set_data_ptr(int asset_id, PriceFeedData *data) {
        std::string key = std::to_string(asset_id);
        sp->current_map_[key] = data; // 將 PriceFeedData 指針與資產ID綁定
    }
    void add_order(const Order &order) { sp->add_order(order); }

    const ptime &time() const {
        if (sp->current_map_.empty()) {
            throw std::runtime_error("current_map_ is empty");
        }

        // 使用 std::prev 获取 unordered_map 的最后一个有效元素
        auto it = std::prev(sp->current_map_.end()); // 获取最后一个元素的迭代器

        // 确保指向的元素不为 nullptr
        if (it->second != nullptr) {
            return it->second->time; // 返回最后一个元素的时间
        } else {
            throw std::runtime_error("Last element in current_map_ is null");
        }
    }

    const auto &name() const { return sp->name(); }
    const auto &name(int index) const { return sp->name(index); }

    virtual ~BaseBroker() = default;
};

class StockBroker : public BaseBroker {
  protected:
    std::shared_ptr<StockBrokerImpl> sp = nullptr;

  public:
    StockBroker(double cash = 10000, double long_rate = 0, double short_rate = 0,
                double long_tax_rate = 0, double short_tax_rate = 0)
        : sp(std::make_shared<StockBrokerImpl>(cash, long_rate, short_rate, long_tax_rate,
                                               short_tax_rate)) {
        BaseBroker::sp = sp;
    }

    StockBroker &commission_eval(std::shared_ptr<GenericCommission> commission_) override {
        BaseBroker::commission_eval(commission_);
        return *this;
    }

    StockBroker &set_commission_rate(double long_rate, double short_rate = 0) override {
        BaseBroker::set_commission_rate(long_rate, short_rate);
        return *this;
    }

    StockBroker &tax(std::shared_ptr<GenericTax> tax_) override {
        BaseBroker::tax(tax_);
        return *this;
    }

    StockBroker &allow_short() override {
        BaseBroker::allow_short();
        return *this;
    }
    StockBroker &allow_default() override {
        BaseBroker::allow_default();
        return *this;
    }

    StockBroker &set_feed(feeds::PriceData data) {
        BaseBroker::set_feed(data);
        return *this;
    }

    //     StockBroker &set_df_feed(feeds::BasePriceDataFeed data) {
    //     BaseBroker::set_df_feed(data);
    //     return *this;
    // }

    StockBroker &
    set_xrd_dir(const std::string &dir, const std::vector<int> &columns,
                std::function<std::string(const std::string &)> code_extract_func_ = nullptr) {
        sp->set_xrd_dir(dir, columns, code_extract_func_);
        return *this;
    }
};

struct BrokerAggragatorLogUtil {};
class BrokerAggragator {
  public:
    BrokerAggragator(
        std::shared_ptr<std::unordered_map<std::string, std::vector<PriceFeedData>>> datas_)

    {
        std::string filename = "my wealth file";
        wealth_file_ = std::make_shared<std::ofstream>(filename, std::ios::out | std::ios::app);
        if (!wealth_file_->is_open()) {
            throw std::runtime_error("Failed to open wealth file: " + filename);
        }

        // 初始化 positions_ 為每個資產代碼分配空的持倉
        // initialize_positions(datas_);
    }
    BrokerAggragator(
        const std::string &filename,
        std::shared_ptr<std::unordered_map<std::string, std::vector<PriceFeedData>>> datas_) {
        wealth_file_ = std::make_shared<std::ofstream>(filename, std::ios::out | std::ios::app);
        if (!wealth_file_->is_open()) {
            throw std::runtime_error("Failed to open wealth file: " + filename);
        }
        // 初始化 positions_ 為每個資產代碼分配空的持倉
        // initialize_positions(datas_);
    }

    // void initialize_positions(
    //     std::shared_ptr<std::unordered_map<std::string, std::vector<PriceFeedData>>> datas_) {
    //     for (const auto &pair : *datas_) {
    //         const std::string &asset_id = pair.first;
    //         positions_[0].emplace(std::stoi(asset_id), 0.0); // 插入持倉量為 0.0
    //     }
    // }

    int position(int broker_id, int asset_id) const {
        // std::cout << "position called" << std::endl;
        const auto &positions_map = brokers_[broker_id].positions_();
        // std::cout << "positions_map called" << std::endl;
        auto it = positions_map.find(asset_id);
        // std::cout << "it called" << std::endl;
        return it != positions_map.end() ? it->second : 100;
    }
    void process(OrderPool &pool,
                 std::unordered_map<std::string, std::vector<PriceFeedData>> datas_);
    void process_old_orders(std::unordered_map<std::string, std::vector<PriceFeedData>> datas_);
    void process_terms();
    void update_current_map();
    void update_info(); // Calculate welath daily.
    void set_log_dir(const std::string &dir);

    void add_broker(const BaseBroker &broker);

    // const auto &positions() const { return positions_; }
    // const auto &positions(int broker) const { return positions_[broker]; }

    const auto &values() const { return values_; }
    const auto &values(int broker) const { return values_[broker]; }

    const auto &profits() const { return profits_; }
    const auto &profits(int broker) const { return profits_[broker]; }
    const auto &adj_profits() const { return adj_profits_; }
    const auto &adj_profits(int broker) const { return adj_profits_[broker]; }

    const std::vector<std::unordered_map<int, int>> &positions_() {
        return {brokers_[0].positions_()};
    }

    // const VecArrXi &positions(int broker) const { return positions_[broker]; }
    const std::unordered_map<int, int> &positions(int broker) const {
        return {brokers_[broker].positions_()};
    }

    const auto &portfolio(int broker) const { return brokers_[broker].portfolio(); }

    std::unordered_map<int, int> assets(int broker) const { return brokers_[broker].assets(); }
    std::unordered_map<int, int> assets(const std::string &broker_name) const {
        return brokers_[broker_name_map_.at(broker_name)].assets();
    }

    double total_wealth() const { return wealth_; }
    double wealth(int broker) const { return total_values_.coeff(broker); }
    const auto &wealth_history() const { return total_value_analyzer_.total_value_history(); }
    double cash(int broker) const { return brokers_[broker].portfolio().cash; }
    double total_cash() const {
        double cash = 0;
        for (const auto &b : brokers_)
            cash += b.portfolio().cash;
        return cash;
    }
    // void init();
    void summary();
    void cal_metrics();
    const auto &performance() const { return metric_analyzer_.performance(); }

    auto broker(int b) { return brokers_[b]; }
    auto broker(const std::string &b_name) { return brokers_[broker_name_map_.at(b_name)]; }
    auto broker_id(const std::string &b_name) { return broker_name_map_.at(b_name); }

    void reset() {
        times_.clear();
        total_value_analyzer_.reset();
        metric_analyzer_.reset();
        total_values_.resize(brokers_.size());

        for (auto &b : brokers_) {
            b.reset();
        }

        _collect_portfolio();
    }

    void sync_feed_agg(const feeds::PriceFeedAggragator &feed_agg_);

  private:
    // std::shared_ptr<feeds::FeedsAggragator> feed_agg_;
    std::vector<ptime> times_;
    std::vector<BaseBroker> brokers_;
    std::unordered_map<std::string, int> broker_name_map_; // Map name to index.

    // std::vector<VecArrXi> positions_;
    // std::vector<std::unordered_map<int, int>> positions_ ;

    std::vector<VecArrXd> values_, profits_, adj_profits_;

    VecArrXd total_values_; // Current total values of each broker.
    // VecArrXd total_value_history_; // History of total values.
    double wealth_ = 0;

    bool log_ = false;
    std::string log_dir_;

    std::shared_ptr<std::ofstream> wealth_file_;
    analysis::TotalValueAnalyzer total_value_analyzer_;
    analysis::MetricAnalyzer metric_analyzer_;

    // std::vector<analysis::PerformanceMetric> performance_;

    void _write_log();
    void _collect_portfolio();
};

inline void backtradercpp::broker::BaseBrokerImplLogUtil::set_log_dir(const std::string &dir,
                                                                      const std::string &name) {
    log = true;
    log_dir = dir;
    this->name = name;
    transaction_file_ =
        std::ofstream(std::filesystem::path(dir) /
                      std::filesystem ::path(std::format("Transaction_{}.csv", name)));
    transaction_file_ << "Date, CashBefore,  ID, Code, PositionBefore,  Direction, Volume, Price, "
                         "Value, Fee, PositionAfter, CashAfter,  Info"
                      << std::endl;

    position_file_ = std::ofstream(std::filesystem::path(dir) /
                                   std::filesystem ::path(std::format("Position_{}.csv", name)));
    position_file_ << "Date, ID, Code, Position, Price, Value, State" << std::endl;
}

inline BaseBrokerImpl::BaseBrokerImpl(double cash, double long_commission_rate,
                                      double short_commission_rate, double long_tax_rate,
                                      double short_tax_rate)
    : portfolio_{cash},
      commission_(std::make_shared<GenericCommission>(long_commission_rate, short_commission_rate)),
      tax_(std::make_shared<GenericTax>(long_tax_rate, long_commission_rate)) {}

inline void
BaseBrokerImpl::process(Order &order,
                        std::unordered_map<std::string, std::vector<PriceFeedData>> datas_) {
    int asset = order.asset;
    // std::cout << "Processing order for asset: " << asset << std::endl;
    try {
        // 使用資產代碼在 datas 中查找對應的數據向量
        auto it = datas_.find(std::to_string(asset));
        if (it == datas_.end()) {
            // exit(0);
            throw std::runtime_error("Asset ID not found in datas map");
            // return;
        }

        // 獲取最新的 PriceFeedData（假設每支股票的數據是按時間排序的）
        PriceFeedData *current_data = &(it->second.back());

        // std::cout << "Price: " << order.price << ", Low: " << current_data->data.low
        //               << ", High: " << current_data->data.high << std::endl;

        auto time = current_data->time; // 使用當前資產的時間

        // std::cout << "Time: " << time << "order valid from" << order.valid_from << std::endl;

        if (time < order.valid_from) {
            order.state = OrderState::Waiting;
            return;
        }

        // std::cout << "time greater than valid_from" << std::endl;

        // 生成 PriceEvaluatorInput 以便後續檢查價格範圍
        PriceEvaluatorInput info{current_data->data.open, current_data->data.high,
                                 current_data->data.low, current_data->data.close};

        if (order.price_eval) {
            order.price = order.price_eval->price(info);
        }

        // 檢查訂單價格是否在範圍內
        if ((order.price >= info.low) && (order.price <= info.high)) {
            // std::cout << "Order price in bounds for asset: " << asset << std::endl;
            // order.value = order.volume * order.price;
            if (order.volume != 0) {
                order.value = order.price * order.volume;
                if (order.volume < 0) { // 如果是賣出訂單，取負
                    order.value = -std::abs(order.value);
                    std::cout << "ORDER VALUE: " << order.value << " ORDER PRICE: " << order.price
                              << " ORDER VOLUME: " << order.volume << std::endl;
                }
            }

            double commission = commission_->cal_commission(order.price, order.volume);
            double tax = tax_->cal_tax(order.price, order.volume);
            order.fee = commission + tax;
            double total_v = order.value + order.fee;
            bool order_valid = true;

            if (portfolio_.cash < total_v) {
                if (!allow_default_) {
                    order_valid = false;
                    // std::cout << "Insufficient funds for asset: " << asset << std::endl;
                }
            }

            if (portfolio_.position(asset) + order.volume < 0) {
                if (!allow_short_) {
                    order_valid = false;
                    std::cout << "Short not allowed for asset: " << asset << std::endl;
                }
            }

            // if (order_valid) {
            order.processed = true;
            order.processed_at = time;
            portfolio_.update(order, current_data->data.close);
            // update_positions(order,
            //                  current_data->data.close); // 使用對應的股票數據

            if (current_map_.find(std::to_string(asset)) != current_map_.end()) {
                // 確保這個資產的數據存在
                if (order.volume > 0) {
                    // 買入
                    positions_[asset] += order.volume;
                    // brokers_[0].cash -= order.volume * data.close;

                } else {
                    // 賣出
                    positions_[asset] += order.volume;
                }
            }
            order.state = OrderState::Success;
            std::cout << "Order processed successfully for asset: " << asset << std::endl;
            // }
        }
        // else {
        //     std::cout << "Order price out of bounds for asset: " << asset << std::endl;

        //     // exit(0);
        // }
    } catch (const std::exception &e) {
        std::cerr << "Error processing order for asset: " << asset << ": " << e.what() << '\n';
    } catch (...) {
        // std::cout << "Unknown error processing order for asset: " << asset << std::endl;
        // exit(0);
        std::cerr << "Unknown error processing order for asset: " << asset << std::endl;
    }
}

inline void BaseBrokerImpl::process_old_orders(
    std::unordered_map<std::string, std::vector<PriceFeedData>> datas_) {
    // Remove expired
    for (auto it = unprocessed.begin(); it != unprocessed.end();) {
        process(*it, datas_);
        if (it->state == OrderState::Waiting) {
            it++;
        } else {
            // If expired or success, remove it.
            it = unprocessed.erase(it);
        }
    }
}

inline void BaseBrokerImpl::update_current_map() {
    // 清空 previous data
    current_map_.clear();

    // 獲取當前回測日的數據
    feeds::PriceDataImpl *price_data_impl = static_cast<feeds::PriceDataImpl *>(feed_.sp.get());
    if (!price_data_impl) {
        throw std::runtime_error("Failed to cast BasePriceDataImpl to PriceDataImpl.");
    }
    // 取得當日stock data
    //  將當日數據存入 current_map_
    for (int i = 0; i < feed_.assets(); ++i) {
        std::string code = feed_.codes()[i];
        auto it = price_data_impl->get_stock_data()->find(code);
        PriceFeedData *daily_data =
            (it != price_data_impl->get_stock_data()->end() && !it->second.empty())
                ? &it->second.back()
                : nullptr;
        if (daily_data != nullptr) {
            set_data_ptr(code, daily_data);
        }
    }
    // std::cout << "current_map_ updated for current date with size: " << current_map_.size()
    //           << std::endl;
}

void BaseBrokerImpl::update_info(int asset) {
    try {

        auto it = current_map_.find(std::to_string(asset));
        if (it == current_map_.end()) {
            std::cerr << "Error: Asset not found in current_map_" << std::endl;
            return;
        }

        PriceFeedData *current_data = it->second;
        if (!current_data) {
            std::cerr << "Error: Null data pointer for asset: " << asset << std::endl;
            return;
        }

        if (current_data->time == boost::posix_time::not_a_date_time) {
            std::cerr << "Error: Invalid date-time for asset: " << asset << std::endl;
            return;
        }

        std::string time_str;
        try {
            time_str = util::to_string(current_data->time);
        } catch (const std::exception &e) {
            std::cerr << "Error converting time to string: " << e.what() << std::endl;
            return;
        }

        log_util_.write_position(std::format("{}, {}, {}, {}, {}, {}, {}", time_str, "", "Cash", "",
                                             "", portfolio_.cash, ""));
        // std::cout << "Portfolio cash: " << portfolio_.cash << std::endl;

        // 更新指定资产的持仓信息
        auto item_it = portfolio_.portfolio_items.find(asset);
        if (item_it != portfolio_.portfolio_items.end()) {
            auto &item = item_it->second;
            std::string state = "Valid";

            if (current_data->valid.size() > asset && current_data->valid.coeff(asset)) {
                item.update_value(current_data->time, current_data->data.close,
                                  current_data->data.close);
            } else {
                state = "Invalid";
            }
        }

        portfolio_.update_info();

        analyzer_.update_total_value(portfolio_.total_value);

    } catch (const std::exception &e) {
        std::cerr << "Exception in update_info for asset " << asset << ": " << e.what()
                  << std::endl;
    }
}

inline void BaseBrokerImpl::resize(int n) {}

void BaseBrokerImpl::set_feed(feeds::PriceData data) {
    std::cout << "Setting feed: " << data.name() << std::endl;
    feed_ = data;
    std::cout << "Setting feed: " << data.name() << std::endl;

    // 正确获取资产数量
    analyzer_.set_name(data.name());
    resize(data.assets()); // 现在能够正确获取 assets 数量
    codes_ = data.codes();

    std::cout << "CODES_SIZE: " << codes_.size() << std::endl;

    // // 转换 sp 到具体的 PriceDataImpl
    feeds::PriceDataImpl *price_data_impl = static_cast<feeds::PriceDataImpl *>(data.sp.get());
    if (!price_data_impl) {
        throw std::runtime_error("Failed to cast BasePriceDataImpl to PriceDataImpl.");
    }

    // 将数据存入 current_map_
    for (int i = 0; i < data.assets(); ++i) {
        std::string code = data.codes()[i]; // 获取每个资产的代码（股票代码）
        set_data_ptr(
            code, price_data_impl->data_ptr()); // 使用股票代码作为键将 data_ptr 存入 current_map_
    }

    // 确保数据正确被存入
    std::cout << "CODES_SIZE: " << codes_.size() << std::endl;
}

// void BaseBrokerImpl::set_df_feed(feeds::BasePriceDataFeed data) {
//     df_feed_ = data;

//     analyzer_.set_name(data.name());
//     resize(data.assets());
//     codes_ = data.codes();

//     // 將數據存入 current_map_
//     for (int i = 0; i < data.assets(); ++i) {
//         std::string code = data.codes()[i];  // 獲取每個資產的代碼（股票代號）
//         current_map_[i] = data.data_ptr(i);  // 將 data_ptr 存入 current_map_ 中
//     }
// }

inline void BaseBrokerImpl::add_order(const Order &order) { unprocessed.emplace_back(order); }

void StockBrokerImpl::process_trems(int asset) {
    try {

        auto it = current_map_.find(std::to_string(asset));
        date dt = it->second->time.date(); // 獲取當前資產的日期

        // 登記（Register）
        for (const auto &[cd, pos] : portfolio_.portfolio_items) {
            const auto info_it = xrd_info_.find(cd);
            if (info_it != xrd_info_.end()) {
                const auto &recordates = info_it->second.record_date;
                const auto date_it = std::ranges::find(recordates, dt);
                if (date_it != recordates.end()) {
                    int k = date_it - recordates.begin();
                    const auto &setting = info_it->second.setting[k];
                    xrd_record_.insert(std::make_pair(
                        info_it->second.execute_date[k],
                        XRDRecord{cd, int(pos.position / 10 * (setting.bonus + setting.addition)),
                                  pos.position / 10 * setting.dividen}));
                }
            }
        }

        // 執行（Execute）
        auto records = xrd_record_.equal_range(dt);
        for (auto it = records.first; it != records.second; ++it) {
            const auto &[cd, vol, cash, time] = it->second; // 使用 "." 來訪問 XRDRecord 成員

            int position_before = portfolio_.position(cd);
            double cash_before = portfolio_.cash;

            // 執行股票和現金的轉移操作
            portfolio_.transfer_stock(it->second.time, cd, vol); // 使用 "." 而不是 "->"
            portfolio_.transfer_cash(cash);

            int position_after = portfolio_.position(cd);
            double cash_after = portfolio_.cash;

            // 記錄這次交易
            log_util_.write_transaction(std::format(
                "{}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}", util::to_string(ptime(dt)),
                cash_before, cd, codes_[cd], position_before, "", vol, 0, cash, "", position_after,
                cash_after, "XRD"));
        }
    } catch (const std::exception &e) {
        std::cerr << "Exception in process_trems for asset " << asset << ": " << e.what()
                  << std::endl;
    }
}

void XRDTable::read(const std::string &file, const std::vector<int> &columns) {
    std::string row;
    std::ifstream f(file);

    std::getline(f, row);
    while (std::getline(f, row)) {
        if (row.size() > 0) {
            auto row_string = feeds::CSVRowParaser::parse_row(row);
            if (!row_string[columns[1]].empty()) {
                record_date.emplace_back(util::delimited_to_date(row_string[columns[0]]));
                execute_date.emplace_back(util::delimited_to_date(row_string[columns[1]]));
                setting.emplace_back(std::stod(row_string[columns[2]]),
                                     std::stod(row_string[columns[3]]),
                                     std::stod(row_string[columns[4]]));
            }
        }
    }
}
void StockBrokerImpl::set_xrd_dir(
    const std::string &dir, const std::vector<int> &columns,
    std::function<std::string(const std::string &)> code_extract_func_) {

    fmt::print(fmt::fg(fmt::color::yellow), "Reading XRD information.\n");

    // First read all codes in xrd_dir.
    util::check_path_exists(dir);
    std::unordered_map<std::string, std::string> code_file;
    for (const auto &entry : std::filesystem::directory_iterator(dir)) {
        if (entry.path().extension() == ".csv") {
            std::string code = entry.path().filename().stem().string();
            if (code_extract_func_ != nullptr) {
                code = code_extract_func_(code);
            }
            code_file[code] = entry.path().string();
        }
    }

    int ct = 0;
    for (int i = 0; i < codes_.size(); ++i) {
        const auto &cd = codes_[i];
        if (code_file.contains(cd)) {
            XRDTable tb;
            tb.read(code_file[cd], columns);
            xrd_info_[i] = std::move(tb);
            ++ct;
        }
    }

    fmt::print(fmt::fg(fmt::color::yellow), "Total {} codes.\n", ct);
}

inline void
BrokerAggragator::process(OrderPool &pool,
                          std::unordered_map<std::string, std::vector<PriceFeedData>> datas_) {
    // std::cout << "Starting process function" << std::endl;

    for (auto &order : pool.orders) {
        // std::cout << "Processing order with broker_id: " << order.broker_id << std::endl;

        if (order.broker_id >= brokers_.size()) {
            std::cerr << "Error: broker_id out of range" << std::endl;
            continue;
        }
        auto &broker = brokers_[order.broker_id]; // 注意这里应该是引用而不是拷贝
        // std::cout << "Processing order with broker_id: " << order.broker_id << std::endl;
        broker.process(order, datas_);

        if (order.state ==
            OrderState::Waiting) { // 如果訂單處於等待狀態，則將其添加到未處理的訂單列表中
            // std::cout << "Order is waiting" << std::endl;
            broker.add_order(order);
        }

        // std::cout << "Processed order with state: " << static_cast<int>(order.state) <<
        // std::endl;
    }

    // std::cout << "Finished processing orders" << std::endl;
}

inline void BrokerAggragator::process_old_orders(
    std::unordered_map<std::string, std::vector<PriceFeedData>> datas_) {
    for (auto &broker : brokers_) {
        broker.process_old_orders(datas_);
    }
}
inline void BrokerAggragator::process_terms() {
    for (auto &broker : brokers_) {
        broker.process_terms();
    }
}

inline void BrokerAggragator::update_current_map() {
    for (auto &broker : brokers_) {
        broker.update_current_map();
    }
}

inline void BrokerAggragator::update_info() {
    //  update portfolio value for all assets
    // std::cout << "Updating info for all brokers" << std::endl;

    try {
        for (int i = 0; i < brokers_.size(); ++i) {
            auto broker = brokers_[i];
            const auto &data_map =
                broker.data_ptr(); // 假設 data_ptr() 返回的是一個 std::unordered_map
            // 遍歷 data_map，處理每個資產
            for (const auto &pair : data_map) {
                const std::string &asset_id = pair.first; // 資產ID
                const auto &data = pair.second;           // PriceFeedData*

                // 確保 data 不為 nullptr，並使用其成員
                if (data != nullptr) {
                    const auto &time = data->time; // 使用指針來訪問 time 成員
                    broker.update_info(
                        std::stoi(asset_id)); // 將 asset_id 轉換為 int，並更新經紀人信息
                } else {
                    std::cerr << "Error: data is nullptr for asset ID: " << asset_id << std::endl;
                }
            }
            // std::cout <<"info updated for broker: " << i << std::endl;

            // 更新 portfolio 和利潤
            total_values_.coeffRef(i) = broker.total_value();
            profits_[i] = broker.profits();
            adj_profits_[i] = broker.adj_profits();
        }

        // positions_[0] = brokers_[0].positions_();

        // std::cout << "Cash: " << brokers_[0].cash() << std::endl;

        // 計算總財富
        // fmt::print(fmt::fg(fmt::color::green), "Calculating total wealth.\n");
        wealth_ = total_values_.sum();
        // fmt::print(fmt::fg(fmt::color::green), "Total wealth calculated.\n");
        total_value_analyzer_.update_total_value(wealth_);
        // fmt::print(fmt::fg(fmt::color::green), "Total value analyzer updated.\n");

        // 更新時間：使用所有 brokers 中最晚的時間作為最後的更新時間
        ptime t = brokers_[0].time();
        // fmt::print(fmt::fg(fmt::color::green), "Updating time.\n");
        for (int i = 1; i < brokers_.size(); ++i) {
            if (t < brokers_[i].time()) {
                t = brokers_[i].time();
            }
        }
        // fmt::print(fmt::fg(fmt::color::green), "Time updated.\n");
        times_.emplace_back(std::move(t));
    } catch (const std::exception &e) {
        std::cerr << "Error updating info: " << e.what() << std::endl;
    }
}

void BrokerAggragator::_write_log() {
    try {
        if (!wealth_file_ || !wealth_file_->is_open()) {
            std::cerr << "Error: wealth_file_ is not open or invalid" << std::endl;
            return;
        }

        // 确保 times_.back() 是有效的
        if (times_.empty()) {
            std::cerr << "Error: times_ is empty" << std::endl;
            return;
        }
        auto last_time = times_.back();

        // 确保 wealth_ 是有效的

        // 格式化字符串并输出调试信息
        std::string log_entry = std::format("{}, {}", util::to_string(last_time), wealth_);

        *wealth_file_ << log_entry;
        // std::cout << "write log format string finish ..." << std::endl;
    } catch (const std::exception &e) {
        std::cerr << "錯誤: - " << e.what() << '\n';
    }

    for (int i = 0; i < brokers_.size(); ++i) {
        double cash = brokers_[i].cash();
        double holding_value = values_[i].sum();

        // 格式化字符串并输出调试信息
        std::string broker_log_entry =
            std::format(", {}, {}, {}", cash, holding_value, cash + holding_value);

        *wealth_file_ << broker_log_entry;
    }
    *wealth_file_ << std::endl;
}

// void BrokerAggragator::_write_log() {
//     try{
//     std::cout << "write_log started ..." << std::endl;
//     *wealth_file_ << std::format("{}, {}", util::to_string(times_.back()), wealth_);
//     std::cout << "write log format string finish ..." << std::endl;
//     }catch (const std::exception& e) {
//                 std::cerr << "錯誤: - " << e.what() << '\n';

//             }
//     std::cout << "test 201" << std::endl;
//     for (int i = 0; i < brokers_.size(); ++i) {
//         std::cout << "test 202" << std::endl;
//         double cash = brokers_[i].cash();
//         std::cout << "test 203" << std::endl;
//         double holding_value = values_[i].sum();
//         std::cout << "test 204" << std::endl;
//         *wealth_file_ << std::format(", {}, {}, {}", cash, holding_value, cash + holding_value);
//         std::cout << "test 205" << std::endl;
//     }
//     *wealth_file_ << std::endl;
//     std::cout << "test 206" << std::endl;
// }
void backtradercpp::broker::BrokerAggragator::_collect_portfolio() {
    for (int i = 0; i < brokers_.size(); ++i) {
        auto broker = brokers_[i];
        // positions_[i] = broker.positions();
        // values_[i] = broker.values();
        profits_[i] = broker.profits();
        adj_profits_[i] = broker.adj_profits();
    }
}

inline void BaseBrokerImpl::set_log_dir(const std::string &dir, int index) {
    log_util_.set_log_dir(dir, name(index));
}

inline void BrokerAggragator::set_log_dir(const std::string &dir) {
    log_ = true;
    log_dir_ = dir;

    for (int i = 0; i < brokers_.size(); ++i) {
        brokers_[i].set_log_dir(dir, i);
    }

    log_ = true;

    wealth_file_ = std::make_shared<std::ofstream>(std::filesystem::path(dir) /
                                                   std::filesystem ::path("TotalValue.csv"));
    *wealth_file_ << "Date, TotalValue";
    for (int i = 0; i < brokers_.size(); ++i) {
        const auto &name_ = brokers_[i].name(i);
        *wealth_file_ << std::format(
            ", Broker_{}_Cash, Broker_{}_HoldingValue, Broker_{}_TotalValue", name_, name_, name_);
    }
    *wealth_file_ << std::endl;
}
inline void BrokerAggragator::add_broker(const BaseBroker &broker) {
    broker_name_map_[broker.feed().name()] = brokers_.size();
    brokers_.push_back(broker);
    // positions_.emplace_back(broker.positions(asset_id));
    // values_.emplace_back(broker.values(asset_id));
    profits_.emplace_back();
    adj_profits_.emplace_back();

    total_values_.conservativeResize(total_values_.size() + 1);
    total_values_.bottomRows(1) = broker.portfolio().cash;
}

inline void BrokerAggragator::sync_feed_agg(const feeds::PriceFeedAggragator &feed_agg_) {
    for (int i = 0; i < brokers_.size(); ++i) {
        const auto &base_feed = feed_agg_.feed(i); // 确保是左值
        const auto *price_feed =
            dynamic_cast<const feeds::PriceData *>(&base_feed); // 动态转换为 const feeds::PriceData
        if (!price_feed) {
            throw std::runtime_error("Failed to cast BasePriceDataFeed to PriceData");
        }
        brokers_[i].set_feed(*price_feed); // 注意 set_feed 应该接受 const PriceData
    }
}

// inline void BrokerAggragator::sync_feed_agg(const feeds::PriceFeedAggragator &feed_agg_) {
//     for (int i = 0; i < brokers_.size(); ++i) {
//         brokers_[i].set_df_feed(feed_agg_.feed(i));
//     }
// }

inline void BaseBroker::set_log_dir(const std::string &dir, int index) {

    sp->set_log_dir(dir, index);
}

void BrokerAggragator::cal_metrics() {
    metric_analyzer_.register_TotalValueAnalyzer(&total_value_analyzer_);
    if (brokers_.size() > 1) {
        for (const auto &b : brokers_) {
            metric_analyzer_.register_TotalValueAnalyzer(&(b.analyzer()));
        }
    }
    metric_analyzer_.cal_metrics();
}
inline void BrokerAggragator::summary() {
    util::cout("{:=^100}\nSummary\n", "");

    if (times_.empty()) {
        util::cout("No data...\n");
    } else {

        fmt::print("{: ^6} : {: ^21} —— {: ^21}, {: ^12} periods\n", "Time",
                   util::to_string(times_[0]), util::to_string(times_.back()), times_.size());
        double start_ = total_value_analyzer_.total_value_history().coeff(0);
        double end_ = total_value_analyzer_.total_value_history().coeff(
            total_value_analyzer_.total_value_history().size() - 1);
        double d = end_ - start_;

        if (d >= 0) {
            fmt::print("{: ^6} : {: ^21.4f} —— {: ^21.4f}, {: ^12.2f} profit\n", "Wealth", start_,
                       end_, d);
        } else {
            fmt::print("{: ^6} : {: ^21.4f} —— {: ^21.4f}, {: ^12.2f} loss\n", "Wealth", start_,
                       end_, d);
        }

        cal_metrics();
        metric_analyzer_.print_table();
        if (log_) {
            metric_analyzer_.write_table(
                (std::filesystem::path(log_dir_) / std::filesystem::path("Performance.csv"))
                    .string());
        }
    }

    util::cout("{:=^100}\n", "");
}
}; // namespace broker
}; // namespace backtradercpp
