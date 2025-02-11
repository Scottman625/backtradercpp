﻿#include "../../include/backtradercpp/Cerebro.hpp"
using namespace backtradercpp;

struct EqualWeightStrategy : strategy::GenericStrategy {
    void run() override {
        // Re-adjust to equal weigh each 30 trading days.
        if (time_index() % 30 == 0) {
            adjust_to_weight_target<100>(
                0, VecArrXd::Constant(assets(0),
                                      1. / assets(0))); // 100 means target volumes will be 100*k.
        }
    }
};

int main() {
    Cerebro cerebro;
    cerebro.add_broker(
        broker::BaseBroker(1000000, 0.0005, 0.001)
            .set_feed(feeds::CSVTabPriceData("../../example_data/CSVTabular/djia.csv",
                                             feeds::TimeStrConv::non_delimited_date)),
        2); // 2 for window
    cerebro.add_strategy(std::make_shared<EqualWeightStrategy>());
    cerebro.run();
}
