#pragma once

#include "selector/stock_selector.h"

namespace quant {

class UltraShortTermSelector : public StockSelector {
public:
    explicit UltraShortTermSelector(std::shared_ptr<DataProvider> provider);

    std::string name() const override { return "超短线TOP10"; }
    SelectorPeriod period() const override { return SelectorPeriod::UltraShortTerm; }

protected:
    bool filter(const StockFullData& data,
                std::vector<std::string>& reasons) override;

    double score(const StockFullData& data,
                 std::map<std::string, double>& details) override;

    double calcTechnicalScore(const StockFullData& data) override;
    double calcCapitalScore(const StockFullData& data) override;
};

}
