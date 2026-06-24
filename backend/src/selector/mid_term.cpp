#include "selector/mid_term.h"
#include <algorithm>
#include <cmath>

namespace quant {

MidTermSelector::MidTermSelector(std::shared_ptr<DataProvider> provider)
    : StockSelector(std::move(provider)) {
    config_.period = "mid_term";
    config_.top_n = 10;
    config_.weights = {
        {"fundamental", 30.0},
        {"valuation", 20.0},
        {"technical", 35.0},
        {"capital", 15.0}
    };
}

bool MidTermSelector::filter(const StockFullData& data,
                             std::vector<std::string>& reasons) {
    const auto& tech = data.technical;
    const auto& fund = data.fundamental;

    if (fund.deduct_profit_3y.size() >= 2) {
        double lastGrowth = 0.0;
        if (fund.deduct_profit_3y[fund.deduct_profit_3y.size() - 2] > 0) {
            lastGrowth = (fund.deduct_profit_3y.back() -
                          fund.deduct_profit_3y[fund.deduct_profit_3y.size() - 2]) /
                         std::abs(fund.deduct_profit_3y[fund.deduct_profit_3y.size() - 2]) * 100.0;
        }
        if (lastGrowth < -30.0) {
            reasons.push_back("单季度净利润大幅下滑");
            return false;
        }
    }

    if (tech.price_250d_high_pct > 85.0) {
        reasons.push_back("股价位于近1年高位85%以上");
        return false;
    }

    return true;
}

double MidTermSelector::score(const StockFullData& data,
                              std::map<std::string, double>& details) {
    double fundamental = calcFundamentalScore(data);
    double valuation = calcValuationScore(data);
    double technical = calcTechnicalScore(data);
    double capital = calcCapitalScore(data);

    details["fundamental"] = fundamental;
    details["valuation"] = valuation;
    details["technical"] = technical;
    details["capital"] = capital;

    double total = fundamental * 0.3 + valuation * 0.2 + technical * 0.35 + capital * 0.15;
    return total;
}

double MidTermSelector::calcFundamentalScore(const StockFullData& data) {
    const auto& fund = data.fundamental;
    double score = 0.0;

    double growthScore = linearScore(fund.roe, 0.0, 30.0, 20.0);
    score += growthScore;

    return score;
}

double MidTermSelector::calcValuationScore(const StockFullData& data) {
    const auto& fund = data.fundamental;
    double score = 0.0;

    double peg = 1.0;
    if (fund.roe > 0 && data.quote.pe > 0) {
        peg = data.quote.pe / fund.roe;
    }
    if (peg < 1.0) {
        score += 15.0;
    } else if (peg < 1.5) {
        score += 10.0;
    } else if (peg < 2.0) {
        score += 5.0;
    }

    double peScore = linearScore(fund.pe_percentile, 0.0, 80.0, 5.0, true);
    score += peScore;

    return score;
}

double MidTermSelector::calcTechnicalScore(const StockFullData& data) {
    const auto& tech = data.technical;
    double score = 0.0;

    double maScore = 0.0;
    if (tech.ma_bullish_monthly) {
        maScore = 25.0;
    } else if (tech.ma_bullish_weekly) {
        maScore = 15.0;
    } else if (tech.ma_bullish_daily) {
        maScore = 8.0;
    }
    score += maScore;

    double volumeScore = linearScore(tech.volume_ratio, 0.8, 2.0, 10.0);
    score += volumeScore;

    return score;
}

double MidTermSelector::calcCapitalScore(const StockFullData& data) {
    const auto& flow = data.fund_flow;
    double score = 0.0;

    double instScore = linearScore(flow.northbound_ratio, 0.0, 8.0, 15.0);
    score += instScore;

    score += 15.0;

    return score;
}

}
