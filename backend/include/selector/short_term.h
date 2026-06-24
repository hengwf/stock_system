#pragma once

#include "selector/stock_selector.h"

namespace quant {

class ShortTermSelector : public StockSelector {
public:
    explicit ShortTermSelector(std::shared_ptr<DataProvider> provider);

    std::string name() const override { return "短线TOP10"; }
    SelectorPeriod period() const override { return SelectorPeriod::ShortTerm; }

protected:
    bool filter(const StockFullData& data,
                std::vector<std::string>& reasons) override;

    double score(const StockFullData& data,
                 std::map<std::string, double>& details) override;

    double calcTechnicalScore(const StockFullData& data) override;
    double calcCapitalScore(const StockFullData& data) override;
    double calcFundamentalScore(const StockFullData& data) override;
};

}
