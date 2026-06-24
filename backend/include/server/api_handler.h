#pragma once

#include "datasource/data_provider.h"
#include "cache/data_cache.h"
#include "selector/stock_selector.h"
#include <memory>
#include <string>

namespace quant {

class ApiHandler {
public:
    ApiHandler(std::shared_ptr<DataProvider> provider,
               std::shared_ptr<DataCache> cache);

    std::string handleHealth();
    std::string handleMarketList(const std::string& params);
    std::string handleStockRealtime(const std::string& codes);
    std::string handleStockDetail(const std::string& code);
    std::string handleKline(const std::string& code,
                            const std::string& type,
                            int count);
    std::string handleFundFlow(const std::string& code);
    std::string handleRunSelector(const std::string& body);
    std::string handleSelectorResult(const std::string& period);

private:
    std::shared_ptr<DataProvider> provider_;
    std::shared_ptr<DataCache> cache_;

    std::string errorResponse(int code, const std::string& message);
    std::string successResponse(const std::string& data_json);

    std::string buildSelectorResult(const std::vector<SelectorResult>& results,
                                    const std::string& period,
                                    const std::string& time);

    std::unique_ptr<StockSelector> createSelector(const std::string& period);
};

}
