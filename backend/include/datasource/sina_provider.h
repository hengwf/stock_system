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

    bool getStockScreenData(const std::string& code,
                            StockFullData& data) override;

    // 东方财富接口
    bool getMarketListFromEM(std::vector<RealtimeQuote>& quotes);

private:
    std::string httpGet(const std::string& url,
                        const std::string& referer = "");

    bool parseKlineJson(const std::string& response,
                        std::vector<KlineData>& klines);

    std::string extractJsonField(const std::string& json, const std::string& field);

    std::string normalizeCode(const std::string& code);

    std::string toSinaCode(const std::string& code);

    bool calculateTechnicalIndicators(TechnicalData& tech);

    // 获取单个市场的股票列表（用于多线程）
    void fetchMarketPage(const std::string& market, int page, int pageSize,
                         std::vector<RealtimeQuote>& result, std::mutex& resultMutex);

    std::mutex mutex_;
    std::map<std::string, RealtimeQuote> stock_list_cache_;
    bool stock_list_loaded_ = false;

    std::map<std::string, std::vector<KlineData>> kline_cache_;
    std::map<std::string, TechnicalData> tech_cache_;
    std::map<std::string, FundamentalData> fund_cache_;
    std::map<std::string, FundFlowData> flow_cache_;
};

}
