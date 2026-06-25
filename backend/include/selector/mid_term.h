#pragma once

#include "selector/stock_selector.h"

namespace quant {

class MidTermSelector : public StockSelector {
public:
    explicit MidTermSelector(std::shared_ptr<DataProvider> provider);

    std::string name() const override { return "中线TOP10"; }
    SelectorPeriod period() const override { return SelectorPeriod::MidTerm; }

protected:
    bool filter(const StockFullData& data,
                std::vector<std::string>& reasons) override;

    double score(const StockFullData& data,
                 std::map<std::string, double>& details) override;

    //double calcFundamentalScore(const StockFullData& data) override;
    double calcValuationScore(const StockFullData& data) override;
    double calcTechnicalScore(const StockFullData& data) override;
    double calcCapitalScore(const StockFullData& data) override;
    double calcGrowthScore(const StockFullData& data) override;
    double calcQualityScore(const StockFullData& data) override;
};

}
