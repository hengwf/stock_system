#include "cache/data_cache.h"

namespace quant {

DataCache::DataCache() = default;
DataCache::~DataCache() = default;

void DataCache::setRealtimeQuote(const std::string& code, const RealtimeQuote& quote, int ttl_seconds) {
    std::lock_guard<std::mutex> lock(mutex_);
    CacheItem<RealtimeQuote> item;
    item.data = quote;
    item.expire_at = std::chrono::steady_clock::now() + std::chrono::seconds(ttl_seconds);
    quote_cache_[code] = item;
}

bool DataCache::getRealtimeQuote(const std::string& code, RealtimeQuote& quote) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = quote_cache_.find(code);
    if (it == quote_cache_.end()) return false;
    if (std::chrono::steady_clock::now() > it->second.expire_at) {
        quote_cache_.erase(it);
        return false;
    }
    quote = it->second.data;
    return true;
}

void DataCache::setKlineData(const std::string& code, const std::string& type,
                             const std::vector<KlineData>& klines, int ttl_seconds) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string key = code + "_" + type;
    CacheItem<std::vector<KlineData>> item;
    item.data = klines;
    item.expire_at = std::chrono::steady_clock::now() + std::chrono::seconds(ttl_seconds);
    kline_cache_[key] = item;
}

bool DataCache::getKlineData(const std::string& code, const std::string& type,
                             std::vector<KlineData>& klines) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string key = code + "_" + type;
    auto it = kline_cache_.find(key);
    if (it == kline_cache_.end()) return false;
    if (std::chrono::steady_clock::now() > it->second.expire_at) {
        kline_cache_.erase(it);
        return false;
    }
    klines = it->second.data;
    return true;
}

void DataCache::setSelectorResult(const std::string& period,
                                  const std::vector<SelectorResult>& results,
                                  const std::string& time) {
    std::lock_guard<std::mutex> lock(mutex_);
    SelectorCache item;
    item.results = results;
    item.time = time;
    selector_cache_[period] = item;
}

bool DataCache::getSelectorResult(const std::string& period,
                                  std::vector<SelectorResult>& results,
                                  std::string& time) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = selector_cache_.find(period);
    if (it == selector_cache_.end()) return false;
    results = it->second.results;
    time = it->second.time;
    return true;
}

void DataCache::clearExpired() {
    std::lock_guard<std::mutex> lock(mutex_);
    auto now = std::chrono::steady_clock::now();
    for (auto it = quote_cache_.begin(); it != quote_cache_.end(); ) {
        if (now > it->second.expire_at) {
            it = quote_cache_.erase(it);
        } else {
            ++it;
        }
    }
    for (auto it = kline_cache_.begin(); it != kline_cache_.end(); ) {
        if (now > it->second.expire_at) {
            it = kline_cache_.erase(it);
        } else {
            ++it;
        }
    }
}

}
