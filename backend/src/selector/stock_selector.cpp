#include "selector/stock_selector.h"
#include <algorithm>
#include <cmath>
#include <thread>
#include <vector>
#include <mutex>
#include <atomic>

namespace quant {

StockSelector::StockSelector(std::shared_ptr<DataProvider> provider)
    : provider_(std::move(provider)) {}

double StockSelector::linearScore(double value, double min_val, double max_val,
                                  double max_score, bool reverse) {
    if (max_val <= min_val) return 0;
    double clamped = std::max(min_val, std::min(max_val, value));
    double ratio = (clamped - min_val) / (max_val - min_val);
    if (reverse) ratio = 1.0 - ratio;
    return ratio * max_score;
}

bool StockSelector::select(const std::vector<std::string>& codes,
                           int top_n,
                           std::vector<SelectorResult>& results) {
    results.clear();

    const int maxPreFilter = 800;
    std::vector<std::string> workCodes;
    
    if ((int)codes.size() <= maxPreFilter) {
        workCodes = codes;
    } else {
        std::vector<RealtimeQuote> quotes;
        provider_->getRealtimeQuotes(codes, quotes);
        std::sort(quotes.begin(), quotes.end(),
            [](const RealtimeQuote& a, const RealtimeQuote& b) {
                return a.amount > b.amount;
            });
        int take = std::min(maxPreFilter, (int)quotes.size());
        for (int i = 0; i < take; i++) {
            workCodes.push_back(quotes[i].code);
        }
    }

    std::vector<StockFullData> allData(workCodes.size());
    std::vector<bool> dataOk(workCodes.size(), false);
    std::atomic<int> nextIdx(0);

    int numThreads = 4;
    std::vector<std::thread> threads;
    for (int t = 0; t < numThreads; t++) {
        threads.emplace_back([&]() {
            while (true) {
                int idx = nextIdx.fetch_add(1);
                if (idx >= (int)workCodes.size()) break;
                
                StockFullData data;
                if (provider_->getStockFullData(workCodes[idx], data)) {
                    allData[idx] = data;
                    dataOk[idx] = true;
                }
            }
        });
    }
    for (auto& th : threads) {
        if (th.joinable()) th.join();
    }

    for (size_t i = 0; i < workCodes.size(); i++) {
        if (!dataOk[i]) continue;
        const auto& data = allData[i];

        SelectorResult result;
        result.code = data.code;
        result.name = data.name;
        result.price = data.quote.price;
        result.change_pct = data.quote.change_pct;
        result.industry = data.fundamental.industry;

        std::vector<std::string> reasons;
        if (!filter(data, reasons)) {
            result.passed_filter = false;
            result.filter_reasons = reasons;
            continue;
        }

        result.score = score(data, result.score_details);
        results.push_back(result);
    }

    std::sort(results.begin(), results.end(),
              [](const SelectorResult& a, const SelectorResult& b) {
                  return a.score > b.score;
              });

    if (top_n > 0 && (int)results.size() > top_n) {
        results.resize(top_n);
    }

    return !results.empty();
}

}
