#include "common/risk_control.h"
#include <algorithm>
#include <cmath>

namespace quant {

RiskManager::RiskManager() {}

bool RiskManager::preRiskFilter(const StockFullData& data,
                                std::vector<std::string>& reasons) {
    reasons.clear();

    // 1. 财务风险检查
    if (!checkFinancialRisk(data, reasons)) {
        return false;
    }

    // 2. 流动性风险检查
    if (!checkLiquidityRisk(data, reasons)) {
        return false;
    }

    // 3. 风险名单检查
    if (!checkRiskList(data, reasons)) {
        return false;
    }

    // 4. 解禁减持检查
    if (!checkUnlockRisk(data, reasons)) {
        return false;
    }

    return reasons.empty();
}

bool RiskManager::checkFinancialRisk(const StockFullData& data,
                                     std::vector<std::string>& reasons) {
    const auto& fund = data.fundamental;
    const auto& risk = data.risk;

    // ST股票过滤
    if (data.risk.is_st) {
        reasons.push_back("ST/*ST股票");
        return false;
    }

    // 高商誉风险
    if (fund.goodwill_ratio > max_goodwill_ratio_) {
        reasons.push_back("商誉占比过高");
    }

    // 高负债风险
    if (fund.debt_ratio > max_debt_ratio_) {
        reasons.push_back("资产负债率过高");
    }

    // 持续亏损
    if (fund.deduct_profit_3y.size() >= 2) {
        bool all_negative = true;
        for (double p : fund.deduct_profit_3y) {
            if (p > 0) {
                all_negative = false;
                break;
            }
        }
        if (all_negative) {
            reasons.push_back("连续亏损");
        }
    }

    // 现金流为负
    if (fund.operating_cf_ratio < 0 && fund.operating_cf_ratio != 0) {
        reasons.push_back("经营现金流为负");
    }

    // 次新股过滤（上市不足60天）
    if (data.listing_days > 0 && data.listing_days < min_listing_days_) {
        reasons.push_back("次新股");
    }

    // 退市风险
    if (data.risk.is_delisted || data.risk.is_risk_warning) {
        reasons.push_back("退市风险");
        return false;
    }

    return true;
}

bool RiskManager::checkLiquidityRisk(const StockFullData& data,
                                     std::vector<std::string>& reasons) {
    // 停牌
    if (data.risk.is_suspended) {
        reasons.push_back("停牌中");
        return false;
    }

    // 低流动性
    if (data.quote.amount < min_daily_amount_) {
        reasons.push_back("流动性不足");
    }

    // 僵尸股（长期低成交）
    if (data.technical.turnover_rate < 0.1) {
        reasons.push_back("僵尸股");
    }

    return true;
}

bool RiskManager::checkRiskList(const StockFullData& data,
                                std::vector<std::string>& reasons) {
    // 舆情风险
    if (data.risk.negative_news) {
        reasons.push_back("负面舆情");
    }

    // 业绩变脸
    if (data.risk.profit_warning_risk) {
        reasons.push_back("业绩变脸");
    }

    return true;
}

bool RiskManager::checkUnlockRisk(const StockFullData& data,
                                 std::vector<std::string>& reasons) {
    // 大额解禁
    if (data.risk.has_large_unlock) {
        reasons.push_back("大额解禁");
    }

    // 大股东减持
    if (data.risk.has_reduction) {
        reasons.push_back("大股东减持");
    }

    return true;
}

bool RiskManager::checkHoldingConstraints(const StockFullData& data,
                                         int currentIndustryCount,
                                         const std::map<std::string, int>& industryCounts) {
    // 单只股票最大权重
    if (data.quote.total_mcap > 0) {
        double weight = (data.quote.float_mcap * data.quote.price) /
                       (data.quote.total_mcap * data.quote.price);
        if (weight > max_position_size_) {
            return false;
        }
    }

    // 单行业最大持仓
    if (currentIndustryCount >= max_industry_stocks_) {
        return false;
    }

    return true;
}

double RiskManager::calculateRiskScore(const StockFullData& data) {
    double score = 0.0;

    // 波动率风险
    if (data.technical.volatility_60d > 0) {
        double volScore = std::min(data.technical.volatility_60d / 50.0, 1.0) * 30.0;
        score += volScore;
    }

    // 流动性风险
    if (data.quote.amount > 0) {
        double liqScore = std::max(0.0, 1.0 - data.quote.amount / min_daily_amount_) * 20.0;
        score += liqScore;
    }

    // 财务风险
    if (data.fundamental.goodwill_ratio > 0) {
        double goodWillScore = std::min(data.fundamental.goodwill_ratio / 0.5, 1.0) * 20.0;
        score += goodWillScore;
    }

    if (data.fundamental.debt_ratio > 0) {
        double debtScore = std::min(data.fundamental.debt_ratio / 0.9, 1.0) * 15.0;
        score += debtScore;
    }

    // 风险标识
    if (data.risk.is_st || data.risk.is_risk_warning) {
        score += 15.0;
    }

    return std::min(score, 100.0);
}

void RiskManager::analyzePortfolioRisk(const std::vector<SelectorResult>& holdings,
                                       double& maxDrawdown,
                                       double& volatility,
                                       std::map<std::string, double>& styleExposure) {
    maxDrawdown = 0.0;
    volatility = 0.0;
    styleExposure.clear();

    // TODO: 实现组合风险分析
}

void RiskManager::setRiskParams(double maxPositionSize,
                                double maxIndustryWeight,
                                double maxSingleStockWeight) {
    max_position_size_ = maxSingleStockWeight;
    max_industry_weight_ = maxIndustryWeight;
}

// ==================== 数据清洗模块 ====================

DataCleaner::DataCleaner() {}

StockFullData DataCleaner::clean(const StockFullData& raw) {
    StockFullData cleaned = raw;

    // 异常值处理
    if (cleaned.quote.pe < 0 || cleaned.quote.pe > 1000) {
        cleaned.quote.pe = 0;
    }
    if (cleaned.quote.pb < 0 || cleaned.quote.pb > 50) {
        cleaned.quote.pb = 0;
    }

    // 去极值
    if (cleaned.technical.volume_ratio > 10) {
        cleaned.technical.volume_ratio = 10;
    }
    if (cleaned.technical.amplitude > 20) {
        cleaned.technical.amplitude = 20;
    }

    return cleaned;
}

bool DataCleaner::isValidStock(const StockFullData& data) {
    // ST股票
    if (data.risk.is_st || data.risk.is_risk_warning) {
        return false;
    }

    // 停牌
    if (data.risk.is_suspended) {
        return false;
    }

    // 退市
    if (data.risk.is_delisted) {
        return false;
    }

    // 无数据
    if (!data.has_data) {
        return false;
    }

    // 价格为0
    if (data.quote.price <= 0) {
        return false;
    }

    return true;
}

double DataCleaner::calculatePercentile(double value,
                                       const std::vector<double>& allValues) {
    if (allValues.empty()) return 50.0;

    int count = 0;
    for (double v : allValues) {
        if (v < value) count++;
    }
    return (count * 100.0) / allValues.size();
}

double DataCleaner::winsorize(double value, double lower, double upper) {
    if (value < lower) return lower;
    if (value > upper) return upper;
    return value;
}

double DataCleaner::industryNeutralize(double value,
                                      const std::string& industry,
                                      const std::map<std::string, std::vector<double>>& industryValues) {
    auto it = industryValues.find(industry);
    if (it == industryValues.end()) return value;

    const auto& values = it->second;
    if (values.empty()) return value;

    // 计算行业均值和标准差
    double sum = 0.0;
    for (double v : values) sum += v;
    double mean = sum / values.size();

    double varSum = 0.0;
    for (double v : values) varSum += (v - mean) * (v - mean);
    double std = std::sqrt(varSum / values.size());

    if (std < 0.0001) return 0.0;

    // 中性化：减去行业均值，除以行业标准差
    return (value - mean) / std;
}

double DataCleaner::sizeNeutralize(double value, double mcap, double medianMcap) {
    if (medianMcap <= 0) return value;

    double logValue = calculateLogMarketCap(value);
    double logMedian = calculateLogMarketCap(medianMcap);

    return logValue - logMedian;
}

double DataCleaner::crossSectionalZScore(double value,
                                        const std::vector<double>& allValues) {
    if (allValues.empty()) return 0.0;

    double sum = 0.0;
    for (double v : allValues) sum += v;
    double mean = sum / allValues.size();

    double varSum = 0.0;
    for (double v : allValues) varSum += (v - mean) * (v - mean);
    double std = std::sqrt(varSum / allValues.size());

    if (std < 0.0001) return 0.0;

    return (value - mean) / std;
}

double DataCleaner::calculateLogMarketCap(double mcap) {
    if (mcap <= 0) return 0.0;
    return std::log(mcap);
}

}
