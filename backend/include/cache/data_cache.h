#pragma once

#include "common/types.h"
#include <string>
#include <map>
#include <mutex>
#include <chrono>
#include <vector>

namespace quant {

class DataCache {
public:
    DataCache();
    ~DataCache();

    void setRealtimeQuote(const std::string& code, const RealtimeQuote& quote, int ttl_seconds = 5);
    bool getRealtimeQuote(const std::string& code, RealtimeQuote& quote);

    void setKlineData(const std::string& code, const std::string& type,
                      const std::vector<KlineData>& klines, int ttl_seconds = 300);
    bool getKlineData(const std::string& code, const std::string& type,
                      std::vector<KlineData>& klines);

    void setSelectorResult(const std::string& period,
                           const std::vector<SelectorResult>& results,
                           const std::string& time);
    bool getSelectorResult(const std::string& period,
                           std::vector<SelectorResult>& results,
                           std::string& time);

    void clearExpired();

private:
    template<typename T>
    struct CacheItem {
        T data;
        std::chrono::steady_clock::time_point expire_at;
    };

    std::mutex mutex_;
    std::map<std::string, CacheItem<RealtimeQuote>> quote_cache_;
    std::map<std::string, CacheItem<std::vector<KlineData>>> kline_cache_;

    struct SelectorCache {
        std::vector<SelectorResult> results;
        std::string time;
    };
    std::map<std::string, SelectorCache> selector_cache_;
};

}
