#pragma once

#include "selector/stock_selector.h"

namespace quant {

class LongTermSelector : public StockSelector {
public:
    explicit LongTermSelector(std::shared_ptr<DataProvider> provider);

    std::string name() const override { return "长线TOP10"; }
    SelectorPeriod period() const override { return SelectorPeriod::LongTerm; }

protected:
    bool filter(const StockFullData& data,
                std::vector<std::string>& reasons) override;

    double score(const StockFullData& data,
                 std::map<std::string, double>& details) override;

    double calcFundamentalScore(const StockFullData& data) override;
    double calcValuationScore(const StockFullData& data) override;
    double calcCapitalScore(const StockFullData& data) override;
    double calcIndustryScore(const StockFullData& data) override;
};

}
