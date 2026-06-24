#include "selector/short_term.h"
#include <algorithm>

namespace quant {

ShortTermSelector::ShortTermSelector(std::shared_ptr<DataProvider> provider)
    : StockSelector(std::move(provider)) {
    config_.period = "short_term";
    config_.top_n = 10;
    config_.weights = {
        {"technical", 60.0},
        {"capital", 30.0},
        {"fundamental", 10.0}
    };
}

bool ShortTermSelector::filter(const StockFullData& data,
                               std::vector<std::string>& reasons) {
    if (data.is_st) {
        reasons.push_back("ST股");
        return false;
    }

    const auto& tech = data.technical;
    if (tech.price_250d_high_pct > 95.0 && tech.volume_ratio > 3.0) {
        reasons.push_back("高位天量见顶");
        return false;
    }

    return true;
}

double ShortTermSelector::score(const StockFullData& data,
                                std::map<std::string, double>& details) {
    double technical = calcTechnicalScore(data);
    double capital = calcCapitalScore(data);
    double fundamental = calcFundamentalScore(data);

    details["technical"] = technical;
    details["capital"] = capital;
    details["fundamental"] = fundamental;

    double total = technical * 0.6 + capital * 0.3 + fundamental * 0.1;
    return total;
}

double ShortTermSelector::calcTechnicalScore(const StockFullData& data) {
    const auto& tech = data.technical;
    double score = 0.0;

    double trendScore = 0.0;
    if (tech.ma_bullish_weekly) {
        trendScore = 30.0;
    } else if (tech.ma_bullish_daily) {
        trendScore = 20.0;
    } else {
        trendScore = 10.0;
    }
    score += trendScore;

    double volumeScore = linearScore(tech.volume_ratio, 0.5, 2.5, 20.0);
    score += volumeScore;

    score += 15.0;

    return score;
}

double ShortTermSelector::calcCapitalScore(const StockFullData& data) {
    const auto& flow = data.fund_flow;
    double score = 0.0;

    double inflowScore = 0.0;
    if (flow.main_inflow > 0) {
        inflowScore = linearScore(flow.main_inflow / 100000000.0, 0.0, 5.0, 25.0);
    }
    score += inflowScore;

    return score;
}

double ShortTermSelector::calcFundamentalScore(const StockFullData& data) {
    const auto& fund = data.fundamental;
    double score = 0.0;

    if (fund.roe > 0) {
        score = 10.0;
    }

    return score;
}

}
