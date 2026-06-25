#include "selector/mid_term.h"
#include <algorithm>
#include <cmath>

namespace quant {

MidTermSelector::MidTermSelector(std::shared_ptr<DataProvider> provider)
    : StockSelector(std::move(provider)) {
    config_.period = "mid_term";
    config_.top_n = 10;
    config_.weights = {
        {"technical", 35.0},      // 技术/趋势（核心）
        {"growth", 25.0},         // 成长
        {"quality", 20.0},        // 质量/盈利
        {"valuation", 10.0},       // 估值
        {"capital", 10.0}         // 资金
    };
}

bool MidTermSelector::filter(const StockFullData& data,
                             std::vector<std::string>& reasons) {
    const auto& tech = data.technical;
    const auto& fund = data.fundamental;
    const auto& quote = data.quote;

    // === 数据清洗 ===
    if (data.is_st) {
        reasons.push_back("ST/*ST股");
        return false;
    }
    if (quote.price <= 0 || quote.prev_close <= 0) {
        reasons.push_back("停牌/无成交");
        return false;
    }
    if (quote.change_pct >= 9.8) {
        reasons.push_back("涨停板");
        return false;
    }

    // === 成长性过滤：单季度净利润大幅下滑 ===
    if (fund.deduct_profit_3y.size() >= 2) {
        double lastGrowth = 0.0;
        if (fund.deduct_profit_3y[fund.deduct_profit_3y.size() - 2] > 0) {
            lastGrowth = (fund.deduct_profit_3y.back() -
                          fund.deduct_profit_3y[fund.deduct_profit_3y.size() - 2]) /
                         std::abs(fund.deduct_profit_3y[fund.deduct_profit_3y.size() - 2]) * 100.0;
        }
        if (lastGrowth < -50.0) {
            reasons.push_back("单季度净利润大幅下滑");
            return false;
        }
    }

    // === 高位风险过滤 ===
    if (tech.price_250d_high_pct > 90.0) {
        reasons.push_back("股价位于近1年高位90%以上");
        return false;
    }

    // === 流动性过滤 ===
    if (quote.amount < 5000000.0) {
        reasons.push_back("流动性枯竭");
        return false;
    }

    // === 波动率过滤 ===
    if (tech.volatility_60d > 100.0) {
        reasons.push_back("波动率过高");
        return false;
    }

    return true;
}

double MidTermSelector::score(const StockFullData& data,
                              std::map<std::string, double>& details) {
    double technical = calcTechnicalScore(data);
    double growth = calcGrowthScore(data);
    double quality = calcQualityScore(data);
    double valuation = calcValuationScore(data);
    double capital = calcCapitalScore(data);

    details["technical"] = technical;
    details["growth"] = growth;
    details["quality"] = quality;
    details["valuation"] = valuation;
    details["capital"] = capital;

    // 权重：技术35% + 成长25% + 质量20% + 估值10% + 资金10%
    double total = technical * 0.35 + growth * 0.25 + quality * 0.20 +
                   valuation * 0.10 + capital * 0.10;
    return total;
}

// === 技术/趋势因子（核心） ===
double MidTermSelector::calcTechnicalScore(const StockFullData& data) {
    const auto& tech = data.technical;
    double score = 0.0;

    // 均线多头排列（趋势确认）
    double maScore = 0.0;
    if (tech.ma_bullish_monthly) maScore = 25.0;
    else if (tech.ma_bullish_weekly) maScore = 18.0;
    else if (tech.ma_bullish_daily) maScore = 10.0;
    score += maScore;

    // 20日动量（中期趋势）
    if (tech.momentum_20d > -20 && tech.momentum_20d < 60) {
        if (tech.momentum_20d > 0) {
            score += linearScore(tech.momentum_20d, 0.0, 30.0, 20.0);
        } else {
            // 适度负动量可接受（反弹机会）
            score += linearScore(tech.momentum_20d, -20.0, 0.0, 10.0);
        }
    }

    // 60日动量（长期趋势辅助）
    if (tech.momentum_60d > 0 && tech.momentum_60d < 80) {
        score += linearScore(tech.momentum_60d, 0.0, 40.0, 15.0);
    }

    // 量比
    double volScore = linearScore(tech.volume_ratio, 0.5, 2.5, 15.0);
    score += volScore;

    return score;
}

// === 成长因子 ===
double MidTermSelector::calcGrowthScore(const StockFullData& data) {
    const auto& fund = data.fundamental;
    const auto& tech = data.technical;
    double score = 0.0;

    // ROE（成长性指标）
    double growthScore = 0.0;
    if (fund.roe > 0) {
        if (fund.roe >= 20.0) growthScore = 20.0;
        else if (fund.roe >= 15.0) growthScore = 16.0;
        else if (fund.roe >= 10.0) growthScore = 12.0;
        else if (fund.roe >= 5.0) growthScore = 8.0;
        else growthScore = 4.0;
    }
    score += growthScore;

    // 营收增速
    double revScore = 0.0;
    if (fund.revenue_growth > 0) {
        if (fund.revenue_growth >= 30.0) revScore = 15.0;
        else if (fund.revenue_growth >= 20.0) revScore = 12.0;
        else if (fund.revenue_growth >= 10.0) revScore = 8.0;
        else revScore = 4.0;
    } else if (tech.momentum_60d > 10) {
        // 备用：用中期动量
        revScore = linearScore(tech.momentum_60d, 0.0, 30.0, 10.0);
    }
    score += revScore;

    // 净利润增速
    if (fund.profit_growth > 0) {
        if (fund.profit_growth >= 30.0) score += 15.0;
        else if (fund.profit_growth >= 20.0) score += 10.0;
        else score += 5.0;
    }

    return score;
}

// === 质量/盈利因子 ===
double MidTermSelector::calcQualityScore(const StockFullData& data) {
    const auto& fund = data.fundamental;
    const auto& quote = data.quote;
    double score = 0.0;

    // 毛利率
    double marginScore = 0.0;
    if (fund.gross_margin > 0) {
        if (fund.gross_margin >= 40.0) marginScore = 15.0;
        else if (fund.gross_margin >= 30.0) marginScore = 12.0;
        else if (fund.gross_margin >= 20.0) marginScore = 8.0;
        else marginScore = 4.0;
    } else if (quote.total_mcap > 50000000000.0) {
        marginScore = 8.0;
    }
    score += marginScore;

    // 净利率
    if (fund.net_margin > 0) {
        if (fund.net_margin >= 15.0) score += 10.0;
        else if (fund.net_margin >= 10.0) score += 7.0;
        else if (fund.net_margin >= 5.0) score += 4.0;
    }

    // 资产负债率
    if (fund.debt_ratio < 60.0) {
        score += 5.0;
    } else if (fund.debt_ratio > 80.0) {
        score -= 5.0;
    }

    return std::max(0.0, score);
}

// === 估值因子 ===
double MidTermSelector::calcValuationScore(const StockFullData& data) {
    const auto& fund = data.fundamental;
    const auto& quote = data.quote;
    double score = 0.0;

    // PEG（市盈率/增长率）
    double peg = 1.0;
    if (fund.roe > 0 && quote.pe > 0) {
        peg = quote.pe / fund.roe;
    }
    if (peg < 0.8) {
        score += 20.0;
    } else if (peg < 1.0) {
        score += 15.0;
    } else if (peg < 1.5) {
        score += 10.0;
    } else if (peg < 2.0) {
        score += 5.0;
    }

    // PE分位数
    double peScore = linearScore(fund.pe_percentile, 0.0, 80.0, 10.0, true);
    score += peScore;

    return score;
}

// === 资金因子 ===
double MidTermSelector::calcCapitalScore(const StockFullData& data) {
    const auto& flow = data.fund_flow;
    const auto& tech = data.technical;
    double score = 0.0;

    // 北向资金
    double northScore = linearScore(flow.northbound_ratio, 0.0, 8.0, 15.0);
    score += northScore;

    // 主力资金净流入
    if (flow.main_inflow_ratio > -5 && flow.main_inflow_ratio < 15) {
        score += linearScore(flow.main_inflow_ratio, -3.0, 10.0, 15.0);
    }

    // 筹码集中度
    if (flow.chip_concentration > 0.7) {
        score += 5.0;
    }

    return score;
}

}
