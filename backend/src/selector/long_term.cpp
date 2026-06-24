#include "selector/long_term.h"
#include <algorithm>

namespace quant {

LongTermSelector::LongTermSelector(std::shared_ptr<DataProvider> provider)
    : StockSelector(std::move(provider)) {
    config_.period = "long_term";
    config_.top_n = 10;
    config_.weights = {
        {"fundamental", 60.0},
        {"valuation", 25.0},
        {"industry", 10.0},
        {"capital", 5.0}
    };
}

bool LongTermSelector::filter(const StockFullData& data,
                              std::vector<std::string>& reasons) {
    const auto& fund = data.fundamental;

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
    }

    if (fund.operating_cashflow.size() >= 2) {
        bool negative = true;
        for (double cf : fund.operating_cashflow) {
            if (cf > 0) {
                negative = false;
                break;
            }
        }
        if (negative) {
            reasons.push_back("连续经营现金流为负");
            return false;
        }
    }

    bool isConsumerTech = (fund.industry.find("消费") != std::string::npos) ||
                          (fund.industry.find("科技") != std::string::npos) ||
                          (fund.industry.find("电子") != std::string::npos) ||
                          (fund.industry.find("传媒") != std::string::npos);
    if (isConsumerTech && fund.debt_ratio > 70.0) {
        reasons.push_back("资产负债率＞70%（消费/科技）");
        return false;
    }
    if (fund.debt_ratio > 85.0) {
        reasons.push_back("资产负债率过高");
        return false;
    }

    if (fund.pe_percentile > 80.0) {
        reasons.push_back("PE近5年分位＞80%（高估）");
        return false;
    }

    if (fund.goodwill_ratio > 30.0) {
        reasons.push_back("商誉占净资产＞30%");
        return false;
    }

    return true;
}

double LongTermSelector::score(const StockFullData& data,
                               std::map<std::string, double>& details) {
    double fundamental = calcFundamentalScore(data);
    double valuation = calcValuationScore(data);
    double capital = calcCapitalScore(data);
    double industry = calcIndustryScore(data);

    details["fundamental"] = fundamental;
    details["valuation"] = valuation;
    details["capital"] = capital;
    details["industry"] = industry;

    double total = fundamental * 0.6 + valuation * 0.25 + industry * 0.1 + capital * 0.05;
    return total;
}

double LongTermSelector::calcFundamentalScore(const StockFullData& data) {
    const auto& fund = data.fundamental;
    double score = 0.0;

    double roeScore = 0.0;
    if (fund.roe_5y_avg >= 15.0) roeScore = 25.0;
    else if (fund.roe_5y_avg >= 10.0) {
        roeScore = 25.0 * (fund.roe_5y_avg - 10.0) / 5.0;
    }
    score += roeScore;

    double marginScore = 0.0;
    int risingYears = 0;
    for (size_t i = 1; i < fund.gross_margin_trend.size(); i++) {
        if (fund.gross_margin_trend[i] > fund.gross_margin_trend[i-1]) {
            risingYears++;
        }
    }
    marginScore = std::min(15.0, risingYears * 5.0);
    score += marginScore;

    double dividendScore = linearScore(fund.dividend_yield, 0.0, 5.0, 10.0);
    score += dividendScore;

    return score;
}

double LongTermSelector::calcValuationScore(const StockFullData& data) {
    const auto& fund = data.fundamental;
    double peScore = linearScore(fund.pe_percentile, 0.0, 80.0, 20.0, true);
    return peScore;
}

double LongTermSelector::calcCapitalScore(const StockFullData& data) {
    const auto& flow = data.fund_flow;
    double northboundScore = linearScore(flow.northbound_ratio, 0.0, 10.0, 15.0);
    return northboundScore;
}

double LongTermSelector::calcIndustryScore(const StockFullData& data) {
    const auto& fund = data.fundamental;
    double score = 8.0;

    static const std::vector<std::string> boomIndustries = {
        "新能源", "半导体", "人工智能", "医药", "消费", "高端制造"
    };
    for (const auto& ind : boomIndustries) {
        if (fund.industry.find(ind) != std::string::npos) {
            score = 15.0;
            break;
        }
    }
    return score;
}

}
