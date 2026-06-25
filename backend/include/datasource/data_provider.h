#pragma once

#include "common/types.h"
#include <string>
#include <vector>

namespace quant {

class DataProvider {
public:
    virtual ~DataProvider() = default;

    virtual bool getRealtimeQuotes(const std::vector<std::string>& codes,
                                   std::vector<RealtimeQuote>& quotes) = 0;

    virtual bool getKlineData(const std::string& code,
                              const std::string& type,
                              int count,
                              std::vector<KlineData>& klines) = 0;

    virtual bool getStockList(std::vector<RealtimeQuote>& stocks) = 0;

    virtual bool getFundamentalData(const std::string& code,
                                    FundamentalData& data) = 0;

    virtual bool getFundFlow(const std::string& code,
                             FundFlowData& data) = 0;

    virtual bool getStockFullData(const std::string& code,
                                  StockFullData& data) = 0;

    virtual bool getStockScreenData(const std::string& code,
                                    StockFullData& data) = 0;
};

}
