#include "selector/ultra_short_term.h"
#include <algorithm>
#include <cmath>

namespace quant {

UltraShortTermSelector::UltraShortTermSelector(std::shared_ptr<DataProvider> provider)
    : StockSelector(std::move(provider)) {
    config_.period = "ultra_short_term";
    config_.top_n = 10;
    config_.weights = {
        {"capital", 55.0},      // 资金（核心）
        {"technical", 30.0},   // 技术
        {"momentum", 10.0},     // 动量
        {"reversal", 5.0}       // 反转
    };
}

bool UltraShortTermSelector::filter(const StockFullData& data,
                                    std::vector<std::string>& reasons) {
    if (data.is_st) {
        reasons.push_back("ST/*ST股");
        return false;
    }

    const auto& tech = data.technical;
    const auto& quote = data.quote;
    
    // 高位翻倍个股
    if (tech.price_250d_high_pct > 95.0) {
        reasons.push_back("高位翻倍个股");
        return false;
    }

    // 流动性极差
    if (quote.amount < 2000000.0) {
        reasons.push_back("流动性极差");
        return false;
    }

    // 涨停板
    if (quote.change_pct >= 9.8) {
        reasons.push_back("涨停板");
        return false;
    }

    return true;
}

double UltraShortTermSelector::score(const StockFullData& data,
                                     std::map<std::string, double>& details) {
    double capital = calcCapitalScore(data);
    double technical = calcTechnicalScore(data);
    double momentum = calcMomentumScore(data);
    double reversal = calcReversalScore(data);

    details["capital"] = capital;
    details["technical"] = technical;
    details["momentum"] = momentum;
    details["reversal"] = reversal;

    // 权重：资金55% + 技术30% + 动量10% + 反转5%
    double total = capital * 0.55 + technical * 0.30 + momentum * 0.10 + reversal * 0.05;
    return total;
}

// === 资金因子（核心） ===
double UltraShortTermSelector::calcCapitalScore(const StockFullData& data) {
    const auto& flow = data.fund_flow;
    double score = 0.0;

    // 主力资金净流入（最重要）
    if (flow.main_inflow > 0) {
        double inflowScore = linearScore(flow.main_inflow / 100000000.0, 0.0, 3.0, 25.0);
        score += inflowScore;
    }

    // 主力净流入占比
    if (flow.main_inflow_ratio > 0 && flow.main_inflow_ratio < 20) {
        score += linearScore(flow.main_inflow_ratio, 0.0, 15.0, 20.0);
    }

    // 超大单资金（机构信号）
    if (flow.super_inflow > 0) {
        score += linearScore(flow.super_inflow / 100000000.0, 0.0, 2.0, 15.0);
    }

    // 大单资金
    if (flow.big_inflow > 0) {
        score += 5.0;
    }

    return score;
}

// === 技术因子 ===
double UltraShortTermSelector::calcTechnicalScore(const StockFullData& data) {
    const auto& tech = data.technical;
    double score = 0.0;

    // 日线均线多头
    double maScore = 0.0;
    if (tech.ma_bullish_daily) {
        maScore = 25.0;
    }
    score += maScore;

    // 换手率（流动性指标）
    double tr = tech.turnover_rate;
    if (tr >= 3.0 && tr <= 12.0) {
        score += 20.0;
    } else if (tr > 0 && tr < 3.0) {
        score += tr / 3.0 * 15.0;
    } else if (tr > 12.0 && tr <= 20.0) {
        score += 15.0 * (20.0 - tr) / 8.0;
    } else if (tr > 20.0) {
        score += 5.0;  // 过高换手可能是出货
    }

    // 量比
    if (tech.volume_ratio >= 1.5 && tech.volume_ratio <= 4.0) {
        score += 10.0;
    } else if (tech.volume_ratio > 0.8) {
        score += 5.0;
    }

    // 振幅
    if (tech.amplitude >= 3.0 && tech.amplitude <= 10.0) {
        score += 5.0;
    }

    return score;
}

// === 动量因子 ===
double UltraShortTermSelector::calcMomentumScore(const StockFullData& data) {
    const auto& tech = data.technical;
    double score = 0.0;

    // 5日动量
    if (tech.momentum_5d > -8 && tech.momentum_5d < 25.0) {
        if (tech.momentum_5d > 0) {
            score += linearScore(tech.momentum_5d, 0.0, 15.0, 20.0);
        } else {
            score += linearScore(tech.momentum_5d, -8.0, 0.0, 10.0);
        }
    }

    return score;
}

// === 反转因子 ===
double UltraShortTermSelector::calcReversalScore(const StockFullData& data) {
    const auto& tech = data.technical;
    double score = 0.0;

    // 5日反转（超跌反弹机会）
    if (tech.reversal_5d > 5.0 && tech.reversal_5d < 30.0) {
        score += 15.0;
    } else if (tech.reversal_5d > 0) {
        score += 8.0;
    }

    // 20日反转
    if (tech.reversal_20d > 8.0 && tech.reversal_20d < 50.0) {
        score += 10.0;
    }

    return score;
}

double UltraShortTermSelector::calcFundamentalScore(const StockFullData& data) {
    // 超短线不需要基本面
    return 0.0;
}

double UltraShortTermSelector::calcValuationScore(const StockFullData& data) {
    // 超短线不需要估值
    return 0.0;
}

}
