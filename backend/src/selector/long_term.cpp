#include "selector/long_term.h"
#include <algorithm>
#include <cmath>

namespace quant {

LongTermSelector::LongTermSelector(std::shared_ptr<DataProvider> provider)
    : StockSelector(std::move(provider)) {
    config_.period = "long_term";
    config_.top_n = 10;
    config_.weights = {
        {"fundamental", 35.0},   // 质量/盈利
        {"valuation", 25.0},      // 估值
        {"growth", 20.0},         // 成长
        {"risk_ctrl", 10.0},      // 风险控制
        {"capital", 5.0},         // 资金
        {"technical", 5.0}        // 技术趋势
    };
}

bool LongTermSelector::filter(const StockFullData& data,
                              std::vector<std::string>& reasons) {
    const auto& fund = data.fundamental;
    const auto& tech = data.technical;
    const auto& flow = data.fund_flow;
    const auto& quote = data.quote;

    // === 数据清洗：ST/退市/停牌过滤 ===
    if (data.is_st) {
        reasons.push_back("ST/*ST股");
        return false;
    }
    if (quote.price <= 0 || quote.prev_close <= 0) {
        reasons.push_back("停牌/无成交");
        return false;
    }
    // 涨跌停过滤（长线不追涨停）
    if (quote.change_pct >= 9.8) {
        reasons.push_back("涨停板");
        return false;
    }

    // === 风险因子硬过滤 ===

    // 1. 扣非净利润过滤：近3年下滑或亏损
    if (fund.deduct_profit_3y.size() >= 3) {
        bool declining = true;
        for (size_t i = 1; i < fund.deduct_profit_3y.size(); i++) {
            if (fund.deduct_profit_3y[i] > fund.deduct_profit_3y[i-1]) {
                declining = false;
                break;
            }
        }
        if (declining) {
            reasons.push_back("近3年扣非净利润下滑/亏损");
            return false;
        }
        if (fund.deduct_profit_3y.back() < 0) {
            reasons.push_back("扣非净利润亏损");
            return false;
        }
    } else if (fund.deduct_profit_3y.size() >= 1 && fund.deduct_profit_3y.back() < 0) {
        reasons.push_back("扣非净利润亏损");
        return false;
    }

    // 2. 现金流过滤：连续经营现金流为负
    if (fund.operating_cashflow.size() >= 2) {
        bool allNegative = true;
        int negativeCount = 0;
        for (double cf : fund.operating_cashflow) {
            if (cf > 0) {
                allNegative = false;
                break;
            }
            negativeCount++;
        }
        if (allNegative && negativeCount >= 2) {
            reasons.push_back("连续经营现金流为负");
            return false;
        }
    }

    // 3. 商誉过滤：商誉占净资产>30%
    if (fund.goodwill_ratio > 30.0) {
        reasons.push_back("商誉占净资产>30%");
        return false;
    }

    // 4. 资产负债率过滤：消费/科技>70%，其他>85%
    bool isConsumerTech = (fund.industry.find("消费") != std::string::npos) ||
                          (fund.industry.find("科技") != std::string::npos) ||
                          (fund.industry.find("电子") != std::string::npos) ||
                          (fund.industry.find("传媒") != std::string::npos);
    if (isConsumerTech && fund.debt_ratio > 70.0) {
        reasons.push_back("资产负债率>70%（消费/科技）");
        return false;
    }
    if (fund.debt_ratio > 85.0) {
        reasons.push_back("资产负债率>85%");
        return false;
    }

    // 5. PE分位数过滤：PE近5年分位>80%（高估）
    if (fund.pe_percentile > 80.0 && fund.pe > 0) {
        reasons.push_back("PE近5年分位>80%（高估）");
        return false;
    }

    // 6. 质押比例过滤：高质押风险
    if (fund.pledge_ratio > 50.0) {
        reasons.push_back("质押比例>50%");
        return false;
    }

    // 7. 股东户数大幅增加（筹码分散）
    if (fund.holder_num_change > 50.0) {
        reasons.push_back("股东户数大幅增加");
        return false;
    }

    // 8. 流动性过滤：日均成交额<1000万
    if (quote.amount < 10000000.0) {
        reasons.push_back("流动性枯竭");
        return false;
    }

    // 9. 波动率过高（风险过大）
    if (tech.volatility_60d > 80.0) {
        reasons.push_back("波动率过高");
        return false;
    }

    return true;
}

double LongTermSelector::score(const StockFullData& data,
                               std::map<std::string, double>& details) {
    double fundamental = calcFundamentalScore(data);   // 质量/盈利
    double valuation = calcValuationScore(data);       // 估值
    double growth = calcGrowthScore(data);            // 成长
    double risk_ctrl = calcRiskScore(data);           // 风险控制
    double capital = calcCapitalScore(data);           // 资金
    double technical = calcTechnicalScore(data);       // 技术

    details["quality"] = fundamental;     // 质量/盈利
    details["valuation"] = valuation;       // 估值
    details["growth"] = growth;            // 成长
    details["risk_ctrl"] = risk_ctrl;      // 风险
    details["capital"] = capital;          // 资金
    details["technical"] = technical;      // 技术

    // 权重：质量35% + 估值25% + 成长20% + 风险10% + 资金5% + 技术5%
    double total = fundamental * 0.35 + valuation * 0.25 + growth * 0.20 +
                   risk_ctrl * 0.10 + capital * 0.05 + technical * 0.05;
    return total;
}

// === 质量/盈利因子（核心） ===
double LongTermSelector::calcFundamentalScore(const StockFullData& data) {
    const auto& fund = data.fundamental;
    const auto& quote = data.quote;
    double score = 0.0;

    // ROE评分（最重要）
    double roeScore = 0.0;
    if (fund.roe_5y_avg > 0) {
        // 5年平均ROE优先
        if (fund.roe_5y_avg >= 20.0) roeScore = 30.0;
        else if (fund.roe_5y_avg >= 15.0) roeScore = 25.0;
        else if (fund.roe_5y_avg >= 10.0) roeScore = 18.0;
        else if (fund.roe_5y_avg >= 5.0) roeScore = 10.0;
        else roeScore = 5.0;
    } else if (fund.roe > 0) {
        // 单季度ROE作为备用
        if (fund.roe >= 15.0) roeScore = 20.0;
        else if (fund.roe >= 10.0) roeScore = 15.0;
        else if (fund.roe >= 5.0) roeScore = 8.0;
        else roeScore = 4.0;
    }
    score += roeScore;

    // ROA（总资产收益率）
    double roaScore = 0.0;
    if (fund.roa > 0) {
        if (fund.roa >= 10.0) roaScore = 15.0;
        else if (fund.roa >= 5.0) roaScore = 10.0;
        else if (fund.roa >= 2.0) roaScore = 5.0;
    }
    score += roaScore;

    // 毛利率
    double marginScore = 0.0;
    if (fund.gross_margin > 0) {
        if (fund.gross_margin >= 40.0) marginScore = 15.0;
        else if (fund.gross_margin >= 30.0) marginScore = 12.0;
        else if (fund.gross_margin >= 20.0) marginScore = 8.0;
        else if (fund.gross_margin >= 10.0) marginScore = 5.0;
    } else {
        // 备用：大市值公司通常毛利率更稳定
        if (quote.total_mcap > 100000000000.0) marginScore = 8.0;
        else if (quote.total_mcap > 50000000000.0) marginScore = 5.0;
    }
    score += marginScore;

    // 净利率
    double netScore = 0.0;
    if (fund.net_margin > 0) {
        if (fund.net_margin >= 15.0) netScore = 10.0;
        else if (fund.net_margin >= 10.0) netScore = 7.0;
        else if (fund.net_margin >= 5.0) netScore = 4.0;
        else netScore = 2.0;
    }
    score += netScore;

    // 经营现金流/净利润（盈利质量）
    double cfScore = 0.0;
    if (fund.operating_cf_ratio > 0) {
        if (fund.operating_cf_ratio >= 1.0) cfScore = 10.0;
        else if (fund.operating_cf_ratio >= 0.5) cfScore = 7.0;
        else if (fund.operating_cf_ratio >= 0.2) cfScore = 4.0;
    }
    score += cfScore;

    return score;
}

// === 估值因子 ===
double LongTermSelector::calcValuationScore(const StockFullData& data) {
    const auto& fund = data.fundamental;
    const auto& quote = data.quote;
    double score = 0.0;

    // PE评分
    double peScore = 0.0;
    if (fund.pe > 0 && fund.pe < 100) {
        if (fund.pe <= 10) peScore = 20.0;
        else if (fund.pe <= 15) peScore = 18.0;
        else if (fund.pe <= 20) peScore = 15.0;
        else if (fund.pe <= 30) peScore = 10.0;
        else if (fund.pe <= 50) peScore = 5.0;
    }
    score += peScore;

    // PB评分
    double pbScore = 0.0;
    if (fund.pb > 0 && fund.pb < 15) {
        if (fund.pb <= 1.5) pbScore = 15.0;
        else if (fund.pb <= 2.5) pbScore = 12.0;
        else if (fund.pb <= 4.0) pbScore = 8.0;
        else pbScore = 4.0;
    }
    score += pbScore;

    // 股息率评分
    double divScore = linearScore(fund.dividend_yield, 0.0, 5.0, 15.0);
    score += divScore;

    return score;
}

// === 成长因子 ===
double LongTermSelector::calcGrowthScore(const StockFullData& data) {
    const auto& fund = data.fundamental;
    const auto& tech = data.technical;
    double score = 0.0;

    // 营收增速
    double revScore = 0.0;
    if (fund.revenue_growth > 0) {
        if (fund.revenue_growth >= 30.0) revScore = 15.0;
        else if (fund.revenue_growth >= 20.0) revScore = 12.0;
        else if (fund.revenue_growth >= 10.0) revScore = 8.0;
        else if (fund.revenue_growth >= 5.0) revScore = 5.0;
        else revScore = 2.0;
    } else if (tech.momentum_60d > 0) {
        // 备用：用中期动量判断"隐形"成长
        revScore = linearScore(tech.momentum_60d, 0.0, 30.0, 10.0);
    }
    score += revScore;

    // 净利润增速
    double profitScore = 0.0;
    if (fund.profit_growth > 0) {
        if (fund.profit_growth >= 30.0) profitScore = 15.0;
        else if (fund.profit_growth >= 20.0) profitScore = 12.0;
        else if (fund.profit_growth >= 10.0) profitScore = 8.0;
        else profitScore = 4.0;
    }
    score += profitScore;

    // ROE同比变化（是否在改善）
    double roeGrowthScore = 0.0;
    if (fund.roe_growth > 0) {
        if (fund.roe_growth >= 5.0) roeGrowthScore = 10.0;
        else if (fund.roe_growth >= 2.0) roeGrowthScore = 7.0;
        else roeGrowthScore = 4.0;
    }
    score += roeGrowthScore;

    return score;
}

// === 风险控制因子 ===
double LongTermSelector::calcRiskScore(const StockFullData& data) {
    const auto& fund = data.fundamental;
    const auto& tech = data.technical;
    double score = 50.0;  // 基础分50

    // 资产负债率（越低越好）
    if (fund.debt_ratio > 60.0) score -= 15;
    else if (fund.debt_ratio > 40.0) score -= 8;
    else if (fund.debt_ratio < 30.0) score += 10;

    // 商誉占比（越低越好）
    if (fund.goodwill_ratio > 20.0) score -= 15;
    else if (fund.goodwill_ratio > 10.0) score -= 8;
    else if (fund.goodwill_ratio < 5.0) score += 10;

    // 波动率（适中最好，太高不好）
    if (tech.volatility_60d > 60.0) score -= 15;
    else if (tech.volatility_60d > 40.0) score -= 8;
    else if (tech.volatility_60d > 15.0 && tech.volatility_60d < 35.0) score += 8;

    // 质押比例
    if (fund.pledge_ratio > 30.0) score -= 10;
    else if (fund.pledge_ratio < 10.0) score += 5;

    return std::max(0.0, std::min(50.0, score));
}

// === 资金因子 ===
double LongTermSelector::calcCapitalScore(const StockFullData& data) {
    const auto& flow = data.fund_flow;
    double score = 0.0;

    // 北向资金持仓比例
    double northScore = linearScore(flow.northbound_ratio, 0.0, 10.0, 25.0);
    score += northScore;

    // 北向资金变化
    if (flow.northbound_change > 0) {
        score += linearScore(flow.northbound_change, 0.0, 5.0, 15.0);
    }

    // 筹码集中度（股东户数下降=集中）
    if (flow.chip_concentration > 0.8) {
        score += 10.0;
    } else if (flow.chip_concentration > 0.6) {
        score += 5.0;
    }

    return score;
}

// === 技术因子 ===
double LongTermSelector::calcTechnicalScore(const StockFullData& data) {
    const auto& tech = data.technical;
    double score = 0.0;

    // 均线多头排列（长期趋势确认）
    double maScore = 0.0;
    if (tech.ma_bullish_monthly) maScore = 30.0;
    else if (tech.ma_bullish_weekly) maScore = 20.0;
    else if (tech.ma_bullish_daily) maScore = 10.0;
    score += maScore;

    // 120日动量（长期趋势）
    if (tech.momentum_120d > 0 && tech.momentum_120d < 100) {
        score += linearScore(tech.momentum_120d, 0.0, 50.0, 20.0);
    }

    return score;
}

}
