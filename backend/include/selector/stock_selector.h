#pragma once

#include "common/types.h"
#include "datasource/data_provider.h"
#include <string>
#include <vector>
#include <memory>

namespace quant {

class StockSelector {
public:
    StockSelector(std::shared_ptr<DataProvider> provider);
    virtual ~StockSelector() = default;

    virtual std::string name() const = 0;
    virtual SelectorPeriod period() const = 0;

    bool select(const std::vector<std::string>& codes,
                int top_n,
                std::vector<SelectorResult>& results);

    const SelectorConfig& config() const { return config_; }

protected:
    virtual bool filter(const StockFullData& data,
                        std::vector<std::string>& reasons) = 0;

    virtual double score(const StockFullData& data,
                         std::map<std::string, double>& details) = 0;

    virtual double calcFundamentalScore(const StockFullData& data) { return 0; }
    virtual double calcValuationScore(const StockFullData& data) { return 0; }
    virtual double calcTechnicalScore(const StockFullData& data) { return 0; }
    virtual double calcCapitalScore(const StockFullData& data) { return 0; }
    virtual double calcIndustryScore(const StockFullData& data) { return 0; }

    double linearScore(double value, double min_val, double max_val,
                       double max_score, bool reverse = false);

    std::shared_ptr<DataProvider> provider_;
    SelectorConfig config_;
};

}
