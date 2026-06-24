#include "selector/ultra_short_term.h"
#include <algorithm>

namespace quant {

UltraShortTermSelector::UltraShortTermSelector(std::shared_ptr<DataProvider> provider)
    : StockSelector(std::move(provider)) {
    config_.period = "ultra_short_term";
    config_.top_n = 10;
    config_.weights = {
        {"technical", 80.0},
        {"capital", 20.0}
    };
}

bool UltraShortTermSelector::filter(const StockFullData& data,
                                    std::vector<std::string>& reasons) {
    if (data.is_st) {
        reasons.push_back("ST股");
        return false;
    }

    const auto& tech = data.technical;
    if (tech.price_250d_high_pct > 95.0) {
        reasons.push_back("高位翻倍个股");
        return false;
    }

    if (tech.turnover_rate < 0.1) {
        reasons.push_back("流动性极差");
        return false;
    }

    return true;
}

double UltraShortTermSelector::score(const StockFullData& data,
                                     std::map<std::string, double>& details) {
    double technical = calcTechnicalScore(data);
    double capital = calcCapitalScore(data);

    details["technical"] = technical;
    details["capital"] = capital;

    double total = technical * 0.8 + capital * 0.2;
    return total;
}

double UltraShortTermSelector::calcTechnicalScore(const StockFullData& data) {
    const auto& tech = data.technical;
    double score = 0.0;

    double bullishScore = 0.0;
    if (tech.ma_bullish_daily) {
        bullishScore = 35.0;
    } else {
        bullishScore = 15.0;
    }
    score += bullishScore;

    double turnoverScore = 0.0;
    double tr = tech.turnover_rate;
    if (tr >= 3.0 && tr <= 12.0) {
        turnoverScore = 30.0;
    } else if (tr > 0 && tr < 3.0) {
        turnoverScore = tr / 3.0 * 20.0;
    } else if (tr > 12.0 && tr <= 20.0) {
        turnoverScore = 30.0 * (20.0 - tr) / 8.0;
    } else {
        turnoverScore = 5.0;
    }
    score += turnoverScore;

    double volumeScore = linearScore(tech.volume_ratio, 0.5, 3.0, 15.0);
    score += volumeScore;

    return score;
}

double UltraShortTermSelector::calcCapitalScore(const StockFullData& data) {
    const auto& flow = data.fund_flow;
    double score = 0.0;

    if (flow.main_inflow > 0) {
        score = linearScore(flow.main_inflow / 100000000.0, 0.0, 3.0, 20.0);
    }

    return score;
}

}
