#include "selector/short_term.h"
#include <algorithm>
#include <cmath>

namespace quant {

ShortTermSelector::ShortTermSelector(std::shared_ptr<DataProvider> provider)
    : StockSelector(std::move(provider)) {
    config_.period = "short_term";
    config_.top_n = 10;
    config_.weights = {
        {"technical", 50.0},      // 技术/趋势（核心）
        {"capital", 25.0},       // 资金（核心）
        {"momentum", 15.0},      // 动量
        {"reversal", 10.0}      // 反转
    };
}

bool ShortTermSelector::filter(const StockFullData& data,
                              std::vector<std::string>& reasons) {
    if (data.is_st) {
        reasons.push_back("ST/*ST股");
        return false;
    }

    const auto& tech = data.technical;
    const auto& quote = data.quote;
    
    // 高位天量见顶
    if (tech.price_250d_high_pct > 95.0 && tech.volume_ratio > 3.0) {
        reasons.push_back("高位天量见顶");
        return false;
    }

    // 流动性过滤
    if (quote.amount < 3000000.0) {
        reasons.push_back("流动性极差");
        return false;
    }

    // 涨停板过滤
    if (quote.change_pct >= 9.8) {
        reasons.push_back("涨停板");
        return false;
    }

    return true;
}

double ShortTermSelector::score(const StockFullData& data,
                              std::map<std::string, double>& details) {
    double technical = calcTechnicalScore(data);
    double capital = calcCapitalScore(data);
    double momentum = calcMomentumScore(data);
    double reversal = calcReversalScore(data);

    details["technical"] = technical;
    details["capital"] = capital;
    details["momentum"] = momentum;
    details["reversal"] = reversal;

    // 权重：技术50% + 资金25% + 动量15% + 反转10%
    double total = technical * 0.50 + capital * 0.25 + momentum * 0.15 + reversal * 0.10;
    return total;
}

// === 技术因子 ===
double ShortTermSelector::calcTechnicalScore(const StockFullData& data) {
    const auto& tech = data.technical;
    double score = 0.0;

    // 均线信号
    double maScore = 0.0;
    if (tech.ma_bullish_weekly) maScore = 25.0;
    else if (tech.ma_bullish_daily) maScore = 15.0;
    score += maScore;

    // 量比
    if (tech.volume_ratio >= 1.2 && tech.volume_ratio <= 3.0) {
        score += 15.0;
    } else if (tech.volume_ratio > 0.8) {
        score += 8.0;
    }

    // 振幅（适度振幅=活跃）
    if (tech.amplitude >= 2.0 && tech.amplitude <= 8.0) {
        score += 10.0;
    } else if (tech.amplitude > 0.5 && tech.amplitude < 10.0) {
        score += 5.0;
    }

    return score;
}

// === 资金因子 ===
double ShortTermSelector::calcCapitalScore(const StockFullData& data) {
    const auto& flow = data.fund_flow;
    const auto& tech = data.technical;
    double score = 0.0;

    // 主力资金净流入
    if (flow.main_inflow > 0) {
        double inflowScore = linearScore(flow.main_inflow / 100000000.0, 0.0, 5.0, 25.0);
        score += inflowScore;
    }

    // 主力净流入占比
    if (flow.main_inflow_ratio > 0 && flow.main_inflow_ratio < 20) {
        score += linearScore(flow.main_inflow_ratio, 0.0, 15.0, 15.0);
    }

    // 超大单资金
    if (flow.super_inflow > 0) {
        score += 5.0;
    }

    return score;
}

// === 动量因子 ===
double ShortTermSelector::calcMomentumScore(const StockFullData& data) {
    const auto& tech = data.technical;
    double score = 0.0;

    // 5日动量
    if (tech.momentum_5d > -10 && tech.momentum_5d < 30) {
        if (tech.momentum_5d > 0) {
            score += linearScore(tech.momentum_5d, 0.0, 15.0, 20.0);
        } else {
            // 适度负动量可接受
            score += linearScore(tech.momentum_5d, -10.0, 0.0, 10.0);
        }
    }

    // 20日动量
    if (tech.momentum_20d > -20 && tech.momentum_20d < 50) {
        score += linearScore(tech.momentum_20d, -10.0, 20.0, 15.0);
    }

    // 换手率（适度高换手=活跃）
    if (tech.turnover_rate >= 3.0 && tech.turnover_rate <= 15.0) {
        score += 10.0;
    } else if (tech.turnover_rate > 1.0) {
        score += 5.0;
    }

    return score;
}

// === 反转因子 ===
double ShortTermSelector::calcReversalScore(const StockFullData& data) {
    const auto& tech = data.technical;
    double score = 0.0;

    // 5日反转（超跌反弹机会）
    if (tech.reversal_5d > 3.0 && tech.reversal_5d < 25.0) {
        score += 15.0;
    } else if (tech.reversal_5d > 0) {
        score += 8.0;
    }

    // 20日反转（中期超跌）
    if (tech.reversal_20d > 5.0 && tech.reversal_20d < 40.0) {
        score += 10.0;
    }

    // 波动率适中（不太高不太低）
    if (tech.volatility_20d > 15.0 && tech.volatility_20d < 50.0) {
        score += 5.0;
    }

    return score;
}

double ShortTermSelector::calcFundamentalScore(const StockFullData& data) {
    // 短线不需要基本面，空实现
    return 5.0;
}

double ShortTermSelector::calcValuationScore(const StockFullData& data) {
    // 短线不需要估值，空实现
    return 5.0;
}

}
