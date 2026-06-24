#pragma once

#include "datasource/data_provider.h"
#include <string>
#include <vector>
#include <map>
#include <mutex>

namespace quant {

class SinaProvider : public DataProvider {
public:
    SinaProvider();
    ~SinaProvider() override;

    bool getRealtimeQuotes(const std::vector<std::string>& codes,
                           std::vector<RealtimeQuote>& quotes) override;

    bool getKlineData(const std::string& code,
                      const std::string& type,
                      int count,
                      std::vector<KlineData>& klines) override;

    bool getStockList(std::vector<RealtimeQuote>& stocks) override;

    bool getFundamentalData(const std::string& code,
                            FundamentalData& data) override;

    bool getFundFlow(const std::string& code,
                     FundFlowData& data) override;

    bool getStockFullData(const std::string& code,
                          StockFullData& data) override;

private:
    std::string httpGet(const std::string& url,
                        const std::string& referer = "");

    bool parseSinaQuote(const std::string& response,
                        const std::vector<std::string>& codes,
                        std::vector<RealtimeQuote>& quotes);

    bool parseKlineJson(const std::string& response,
                        std::vector<KlineData>& klines);

    std::string normalizeCode(const std::string& code);

    std::string toSinaCode(const std::string& code);

    std::string gbkToUtf8(const std::string& gbkStr);

    bool calculateTechnicalIndicators(TechnicalData& tech);

    std::mutex mutex_;
    std::map<std::string, RealtimeQuote> stock_list_cache_;
    bool stock_list_loaded_ = false;

    std::map<std::string, std::vector<KlineData>> kline_cache_;
    std::map<std::string, TechnicalData> tech_cache_;
    std::map<std::string, FundamentalData> fund_cache_;
    std::map<std::string, FundFlowData> flow_cache_;
};

}
