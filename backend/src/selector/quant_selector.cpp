#include "selector/quant_selector.h"
#include <algorithm>
#include <cmath>
#include <thread>
#include <atomic>

namespace quant {

// ==================== 统一选股引擎实现 ====================

QuantSelector::QuantSelector(std::shared_ptr<DataProvider> provider)
    : provider_(std::move(provider)), period_(SelectorPeriod::MidTerm) {
    // 默认权重配置
    config_.value_weight = 15.0;
    config_.growth_weight = 25.0;
    config_.quality_weight = 20.0;
    config_.momentum_weight = 20.0;
    config_.reversal_weight = 10.0;
    config_.capital_weight = 10.0;

    // 风控默认开启
    config_.filter_st = true;
    config_.filter_new_stock = true;
    config_.filter_low_liquidity = true;
    config_.filter_high_risk = true;
}

void QuantSelector::setPeriod(SelectorPeriod period) {
    period_ = period;

    // 根据周期调整默认权重
    switch (period) {
        case SelectorPeriod::LongTerm:
            config_.value_weight = 25.0;
            config_.growth_weight = 15.0;
            config_.quality_weight = 25.0;
            config_.momentum_weight = 10.0;
            config_.reversal_weight = 5.0;
            config_.capital_weight = 20.0;
            break;

        case SelectorPeriod::MidTerm:
            config_.value_weight = 15.0;
            config_.growth_weight = 25.0;
            config_.quality_weight = 20.0;
            config_.momentum_weight = 20.0;
            config_.reversal_weight = 10.0;
            config_.capital_weight = 10.0;
            break;

        case SelectorPeriod::ShortTerm:
            config_.value_weight = 10.0;
            config_.growth_weight = 15.0;
            config_.quality_weight = 10.0;
            config_.momentum_weight = 30.0;
            config_.reversal_weight = 25.0;
            config_.capital_weight = 10.0;
            break;

        case SelectorPeriod::UltraShortTerm:
            config_.value_weight = 5.0;
            config_.growth_weight = 10.0;
            config_.quality_weight = 5.0;
            config_.momentum_weight = 25.0;
            config_.reversal_weight = 30.0;
            config_.capital_weight = 25.0;
            break;
    }
}

void QuantSelector::setFactorWeights(double value, double growth, double quality,
                                     double momentum, double reversal, double capital) {
    double total = value + growth + quality + momentum + reversal + capital;
    if (total > 0) {
        config_.value_weight = value / total * 100.0;
        config_.growth_weight = growth / total * 100.0;
        config_.quality_weight = quality / total * 100.0;
        config_.momentum_weight = momentum / total * 100.0;
        config_.reversal_weight = reversal / total * 100.0;
        config_.capital_weight = capital / total * 100.0;
    }
}

void QuantSelector::setRiskFilter(bool filterSt, bool filterNewStock,
                                 bool filterLowLiq, bool filterHighRisk) {
    config_.filter_st = filterSt;
    config_.filter_new_stock = filterNewStock;
    config_.filter_low_liquidity = filterLowLiq;
    config_.filter_high_risk = filterHighRisk;
}

bool QuantSelector::select(const std::vector<std::string>& codes,
                          int topN,
                          std::vector<SelectorResult>& results) {
    results.clear();

    // 1. 数据获取
    std::vector<StockFullData> allData;
    if (!loadStockData(codes, allData)) {
        return false;
    }

    // 2. 收集因子值用于截面标准化
    peValues_.clear();
    roeValues_.clear();
    growthValues_.clear();
    volumeValues_.clear();
    mcapValues_.clear();

    for (const auto& data : allData) {
        if (data.quote.pe > 0) peValues_.push_back(data.quote.pe);
        if (data.fundamental.roe > 0) roeValues_.push_back(data.fundamental.roe);
        if (data.fundamental.revenue_growth > -100) growthValues_.push_back(data.fundamental.revenue_growth);
        if (data.quote.amount > 0) volumeValues_.push_back(data.quote.amount);
        if (data.quote.float_mcap > 0) mcapValues_.push_back(data.quote.float_mcap);
    }

    // 3. 因子计算与筛选
    for (const auto& data : allData) {
        // 数据清洗
        StockFullData cleanedData = dataCleaner_.clean(data);

        // 风控过滤
        std::vector<std::string> riskReasons;
        if (!riskManager_.preRiskFilter(cleanedData, riskReasons)) {
            continue;
        }

        // 计算因子得分
        FactorScore fs = calculateFactorScore(cleanedData);

        // 构建结果
        SelectorResult result;
        result.code = data.code;
        result.name = data.name;
        result.price = data.quote.price;
        result.change_pct = data.quote.change_pct;
        result.industry = data.fundamental.industry;
        result.factor_scores = fs;
        result.score = fs.total_score;
        result.risk_warnings = riskReasons;

        results.push_back(result);
    }

    // 4. 排序
    std::sort(results.begin(), results.end(),
              [](const SelectorResult& a, const SelectorResult& b) {
                  return a.score > b.score;
              });

    // 5. 取前N
    if (topN > 0 && (int)results.size() > topN) {
        results.resize(topN);
    }

    return !results.empty();
}

FactorScore QuantSelector::calculateFactorScore(const StockFullData& data) {
    FactorScore fs;

    // 各因子得分
    fs.value_score = calcValueScore(data);
    fs.growth_score = calcGrowthScore(data);
    fs.quality_score = calcQualityScore(data);
    fs.momentum_score = calcMomentumScore(data);
    fs.reversal_score = calcReversalScore(data);
    fs.capital_score = calcCapitalScore(data);

    // 综合得分（加权）
    fs.total_score =
        fs.value_score * (config_.value_weight / 100.0) +
        fs.growth_score * (config_.growth_weight / 100.0) +
        fs.quality_score * (config_.quality_weight / 100.0) +
        fs.momentum_score * (config_.momentum_weight / 100.0) +
        fs.reversal_score * (config_.reversal_weight / 100.0) +
        fs.capital_score * (config_.capital_weight / 100.0);

    // 因子明细
    fs.details["PE"] = data.quote.pe;
    fs.details["PB"] = data.quote.pb;
    fs.details["ROE"] = data.fundamental.roe;
    fs.details["营收增速"] = data.fundamental.revenue_growth;
    fs.details["主力净流入"] = data.fund_flow.main_inflow;
    fs.details["换手率"] = data.technical.turnover_rate;

    return fs;
}

double QuantSelector::calcValueScore(const StockFullData& data) {
    double score = 0.0;
    const auto& fund = data.fundamental;
    const auto& quote = data.quote;

    // PE评分（越低越好，但负值无效）
    if (quote.pe > 0 && quote.pe < 100) {
        double peScore = linearScore(quote.pe, 5.0, 30.0, 30.0, true);
        score += peScore;
    }

    // PB评分（越低越好）
    if (quote.pb > 0 && quote.pb < 10) {
        double pbScore = linearScore(quote.pb, 1.0, 5.0, 20.0, true);
        score += pbScore;
    }

    // 股息率评分（越高越好）
    if (fund.dividend_yield > 0) {
        double divScore = linearScore(fund.dividend_yield, 1.0, 5.0, 25.0);
        score += divScore;
    }

    // PE分位数（低位好）
    if (fund.pe_percentile > 0 && fund.pe_percentile < 100) {
        double pctScore = linearScore(fund.pe_percentile, 10.0, 50.0, 25.0, true);
        score += pctScore;
    }

    return score;
}

double QuantSelector::calcGrowthScore(const StockFullData& data) {
    double score = 0.0;
    const auto& fund = data.fundamental;

    // ROE评分
    if (fund.roe > 0) {
        double roeScore = linearScore(fund.roe, 5.0, 20.0, 25.0);
        score += roeScore;
    }

    // 营收增速
    if (fund.revenue_growth > -50 && fund.revenue_growth < 100) {
        double revScore = linearScore(fund.revenue_growth, -10.0, 30.0, 25.0);
        score += revScore;
    }

    // 利润增速
    if (fund.profit_growth > -50 && fund.profit_growth < 100) {
        double profScore = linearScore(fund.profit_growth, -10.0, 30.0, 20.0);
        score += profScore;
    }

    // 5年平均ROE
    if (fund.roe_5y_avg > 0) {
        double roeAvgScore = linearScore(fund.roe_5y_avg, 8.0, 20.0, 20.0);
        score += roeAvgScore;
    }

    // 毛利率趋势（持续改善好）
    if (fund.gross_margin_trend.size() >= 2) {
        double trend = fund.gross_margin_trend.back() - fund.gross_margin_trend.front();
        if (trend > 0) {
            score += 10.0;
        }
    }

    return score;
}

double QuantSelector::calcQualityScore(const StockFullData& data) {
    double score = 0.0;
    const auto& fund = data.fundamental;

    // 毛利率
    if (fund.gross_margin > 0) {
        double gmScore = linearScore(fund.gross_margin, 10.0, 40.0, 20.0);
        score += gmScore;
    }

    // 净利率
    if (fund.net_margin > 0) {
        double nmScore = linearScore(fund.net_margin, 5.0, 20.0, 15.0);
        score += nmScore;
    }

    // 资产负债率（越低越好，但要适度）
    if (fund.debt_ratio > 0 && fund.debt_ratio < 95) {
        // 30-70%是合理区间
        double debtScore = linearScore(fund.debt_ratio, 30.0, 70.0, 20.0, true);
        score += debtScore;
    }

    // 经营现金流/净利润（越高越好，正值表示现金流健康）
    if (fund.operating_cf_ratio > 0) {
        double cfScore = linearScore(fund.operating_cf_ratio, 0.5, 1.5, 20.0);
        score += cfScore;
    }

    // 商誉占比（越低越好）
    if (fund.goodwill_ratio >= 0 && fund.goodwill_ratio < 0.5) {
        double gwScore = linearScore(fund.goodwill_ratio, 0.0, 0.3, 25.0, true);
        score += gwScore;
    }

    return score;
}

double QuantSelector::calcMomentumScore(const StockFullData& data) {
    double score = 0.0;
    const auto& tech = data.technical;

    // 均线多头排列
    double maScore = 0.0;
    if (tech.ma_bullish_monthly) {
        maScore = 30.0;
    } else if (tech.ma_bullish_weekly) {
        maScore = 20.0;
    } else if (tech.ma_bullish_daily) {
        maScore = 10.0;
    }
    score += maScore;

    // 20日动量（中期趋势）
    if (tech.momentum_20d > -20 && tech.momentum_20d < 50) {
        double momScore = linearScore(tech.momentum_20d, -10.0, 20.0, 25.0);
        score += momScore;
    }

    // 相对强弱
    if (tech.relative_strength > 0) {
        double rsScore = linearScore(tech.relative_strength, 40.0, 70.0, 20.0);
        score += rsScore;
    }

    // 距250日高点（不能太高）
    if (tech.price_250d_high_pct > 0 && tech.price_250d_high_pct < 100) {
        double posScore = linearScore(tech.price_250d_high_pct, 50.0, 85.0, 25.0, true);
        score += posScore;
    }

    return score;
}

double QuantSelector::calcReversalScore(const StockFullData& data) {
    double score = 0.0;
    const auto& tech = data.technical;

    // 5日反转（超跌反弹）
    if (tech.reversal_5d < 0 && tech.reversal_5d > -20) {
        double rev5Score = linearScore(tech.reversal_5d, -15.0, 0.0, 35.0, true);
        score += rev5Score;
    }

    // 20日反转
    if (tech.reversal_20d < 0 && tech.reversal_20d > -30) {
        double rev20Score = linearScore(tech.reversal_20d, -20.0, 0.0, 25.0, true);
        score += rev20Score;
    }

    // 量比（温和放量好）
    if (tech.volume_ratio > 0) {
        double volScore = linearScore(tech.volume_ratio, 0.8, 2.5, 20.0);
        score += volScore;
    }

    // 振幅（适中好，僵尸股排除）
    if (tech.amplitude > 0.5 && tech.amplitude < 15) {
        double ampScore = linearScore(tech.amplitude, 2.0, 8.0, 20.0);
        score += ampScore;
    }

    return score;
}

double QuantSelector::calcCapitalScore(const StockFullData& data) {
    double score = 0.0;
    const auto& flow = data.fund_flow;
    const auto& quote = data.quote;

    // 主力净流入（占比越高越好）
    if (flow.main_inflow_ratio > -10 && flow.main_inflow_ratio < 20) {
        double mainScore = linearScore(flow.main_inflow_ratio, -5.0, 10.0, 25.0);
        score += mainScore;
    }

    // 北向持股占比
    if (flow.northbound_ratio > 0) {
        double nbScore = linearScore(flow.northbound_ratio, 1.0, 8.0, 20.0);
        score += nbScore;
    }

    // 北向持股变化
    if (flow.northbound_change > 0) {
        double nbChangeScore = linearScore(flow.northbound_change, 0.0, 20.0, 15.0);
        score += nbChangeScore;
    }

    // 换手率（适度活跃）
    if (quote.turnover_rate > 0.5 && quote.turnover_rate < 20) {
        double toScore = linearScore(quote.turnover_rate, 1.0, 8.0, 20.0);
        score += toScore;
    }

    // 筹码集中度提升
    if (flow.chip_concentration > 0) {
        score += 20.0;
    }

    return score;
}

double QuantSelector::linearScore(double value, double minVal, double maxVal,
                                 double maxScore, bool reverse) {
    if (maxVal <= minVal) return 0;
    double clamped = std::max(minVal, std::min(maxVal, value));
    double ratio = (clamped - minVal) / (maxVal - minVal);
    if (reverse) ratio = 1.0 - ratio;
    return ratio * maxScore;
}

double QuantSelector::percentileScore(double value, const std::vector<double>& allValues,
                                     double maxScore, bool reverse) {
    if (allValues.empty()) return maxScore / 2;

    int count = 0;
    for (double v : allValues) {
        if (v < value) count++;
    }
    double percentile = (count * 100.0) / allValues.size();
    double ratio = percentile / 100.0;
    if (reverse) ratio = 1.0 - ratio;
    return ratio * maxScore;
}

bool QuantSelector::loadStockData(const std::vector<std::string>& codes,
                                 std::vector<StockFullData>& allData) {
    // 预筛选：按成交额排序，取前500只（选股只需要头部流动性好的）
    const int maxPreFilter = 500;
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

    // 多线程加载数据
    allData.resize(workCodes.size());
    std::vector<bool> dataOk(workCodes.size(), false);
    std::atomic<int> nextIdx(0);

    int numThreads = 12;
    std::vector<std::thread> threads;
    for (int t = 0; t < numThreads; t++) {
        threads.emplace_back([&]() {
            while (true) {
                int idx = nextIdx.fetch_add(1);
                if (idx >= (int)workCodes.size()) break;

                StockFullData data;
                if (provider_->getStockScreenData(workCodes[idx], data)) {
                    allData[idx] = data;
                    dataOk[idx] = true;
                }
            }
        });
    }

    for (auto& th : threads) {
        if (th.joinable()) th.join();
    }

    // 移除无效数据
    std::vector<StockFullData> validData;
    for (size_t i = 0; i < allData.size(); i++) {
        if (dataOk[i]) {
            validData.push_back(allData[i]);
        }
    }
    allData = validData;

    return !allData.empty();
}

// ==================== 简化版选股器实现 ====================

SimpleSelector::SimpleSelector(std::shared_ptr<DataProvider> provider)
    : provider_(std::move(provider)) {}

double SimpleSelector::linearScore(double value, double minVal, double maxVal,
                                  double maxScore, bool reverse) {
    if (maxVal <= minVal) return 0;
    double clamped = std::max(minVal, std::min(maxVal, value));
    double ratio = (clamped - minVal) / (maxVal - minVal);
    if (reverse) ratio = 1.0 - ratio;
    return ratio * maxScore;
}

bool SimpleSelector::select(const std::vector<std::string>& codes,
                           int topN,
                           std::vector<SelectorResult>& results) {
    results.clear();

    const int maxPreFilter = 500;
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

    // 多线程加载
    std::vector<StockFullData> allData(workCodes.size());
    std::vector<bool> dataOk(workCodes.size(), false);
    std::atomic<int> nextIdx(0);

    int numThreads = 12;
    std::vector<std::thread> threads;
    for (int t = 0; t < numThreads; t++) {
        threads.emplace_back([&]() {
            while (true) {
                int idx = nextIdx.fetch_add(1);
                if (idx >= (int)workCodes.size()) break;

                StockFullData data;
                if (provider_->getStockScreenData(workCodes[idx], data)) {
                    allData[idx] = data;
                    dataOk[idx] = true;
                }
            }
        });
    }

    for (auto& th : threads) {
        if (th.joinable()) th.join();
    }

    // 筛选与评分
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

        FactorScore fs = score(data);
        result.factor_scores = fs;
        result.score = fs.total_score;

        results.push_back(result);
    }

    // 排序
    std::sort(results.begin(), results.end(),
              [](const SelectorResult& a, const SelectorResult& b) {
                  return a.score > b.score;
              });

    if (topN > 0 && (int)results.size() > topN) {
        results.resize(topN);
    }

    return !results.empty();
}

// ==================== 长线选股器 ====================

LongTermSelector::LongTermSelector(std::shared_ptr<DataProvider> provider)
    : SimpleSelector(std::move(provider)) {
    config_.period = "long_term";
    config_.top_n = 10;
}

bool LongTermSelector::filter(const StockFullData& data,
                             std::vector<std::string>& reasons) {
    const auto& fund = data.fundamental;

    // ROE过滤
    if (fund.roe <= 0) {
        reasons.push_back("ROE为负");
        return false;
    }
    if (fund.roe < 5) {
        reasons.push_back("ROE过低");
    }

    // 资产负债率
    if (fund.debt_ratio > 85) {
        reasons.push_back("资产负债率过高");
        return false;
    }

    // 股价位置
    if (data.technical.price_250d_high_pct > 90) {
        reasons.push_back("股价接近一年高位");
    }

    return true;
}

FactorScore LongTermSelector::score(const StockFullData& data) {
    FactorScore fs;
    fs.value_score = calcValueScore(data);
    fs.quality_score = calcQualityScore(data);
    fs.growth_score = calcGrowthScore(data);
    fs.momentum_score = calcMomentumScore(data);
    fs.capital_score = calcCapitalScore(data);

    fs.details["fundamental"] = fs.quality_score * 0.6 + fs.growth_score * 0.4;
    fs.details["valuation"] = fs.value_score;
    fs.details["technical"] = fs.momentum_score * 0.3;
    fs.details["capital"] = fs.capital_score * 0.2;

    fs.total_score = fs.details["fundamental"] * 0.50 +
                    fs.details["valuation"] * 0.30 +
                    fs.details["technical"] * 0.12 +
                    fs.details["capital"] * 0.08;

    return fs;
}

double LongTermSelector::calcValueScore(const StockFullData& data) {
    double score = 0.0;
    const auto& quote = data.quote;
    const auto& fund = data.fundamental;

    if (quote.pe > 0 && quote.pe < 50) {
        score += linearScore(quote.pe, 5.0, 25.0, 30.0, true);
    }
    if (quote.pb > 0 && quote.pb < 8) {
        score += linearScore(quote.pb, 1.0, 4.0, 25.0, true);
    }
    if (fund.dividend_yield > 0) {
        score += linearScore(fund.dividend_yield, 1.0, 5.0, 25.0);
    }
    if (fund.pe_percentile > 0 && fund.pe_percentile < 60) {
        score += linearScore(fund.pe_percentile, 20.0, 50.0, 20.0, true);
    }

    return score;
}

double LongTermSelector::calcQualityScore(const StockFullData& data) {
    double score = 0.0;
    const auto& fund = data.fundamental;

    if (fund.roe > 0) {
        score += linearScore(fund.roe, 8.0, 25.0, 30.0);
    }
    if (fund.roe_5y_avg > 0) {
        score += linearScore(fund.roe_5y_avg, 10.0, 20.0, 25.0);
    }
    if (fund.gross_margin > 0) {
        score += linearScore(fund.gross_margin, 15.0, 40.0, 20.0);
    }
    if (fund.operating_cf_ratio > 0) {
        score += linearScore(fund.operating_cf_ratio, 0.5, 1.5, 25.0);
    }

    return score;
}

double LongTermSelector::calcGrowthScore(const StockFullData& data) {
    double score = 0.0;
    const auto& fund = data.fundamental;

    if (fund.revenue_growth > -20 && fund.revenue_growth < 50) {
        score += linearScore(fund.revenue_growth, 0.0, 20.0, 50.0);
    }
    if (fund.profit_growth > -30 && fund.profit_growth < 50) {
        score += linearScore(fund.profit_growth, -10.0, 30.0, 50.0);
    }

    return score;
}

double LongTermSelector::calcMomentumScore(const StockFullData& data) {
    double score = 0.0;
    const auto& tech = data.technical;

    if (tech.ma_bullish_monthly) {
        score += 30.0;
    } else if (tech.ma_bullish_weekly) {
        score += 20.0;
    } else if (tech.ma_bullish_daily) {
        score += 10.0;
    }

    if (tech.momentum_20d > -20 && tech.momentum_20d < 50) {
        score += linearScore(tech.momentum_20d, -10.0, 20.0, 25.0);
    }

    if (tech.price_250d_high_pct > 0 && tech.price_250d_high_pct < 100) {
        score += linearScore(tech.price_250d_high_pct, 50.0, 85.0, 25.0, true);
    }

    if (tech.relative_strength > 0) {
        score += linearScore(tech.relative_strength, 40.0, 70.0, 20.0);
    }

    return score;
}

double LongTermSelector::calcCapitalScore(const StockFullData& data) {
    double score = 0.0;
    const auto& flow = data.fund_flow;
    const auto& quote = data.quote;

    if (flow.main_inflow_ratio > -10 && flow.main_inflow_ratio < 20) {
        score += linearScore(flow.main_inflow_ratio, -5.0, 10.0, 25.0);
    }

    if (flow.northbound_ratio > 0) {
        score += linearScore(flow.northbound_ratio, 1.0, 8.0, 20.0);
    }

    if (quote.turnover_rate > 0.5 && quote.turnover_rate < 20) {
        score += linearScore(quote.turnover_rate, 1.0, 8.0, 20.0);
    }

    if (flow.northbound_change > 0) {
        score += linearScore(flow.northbound_change, 0.0, 20.0, 15.0);
    }

    if (flow.chip_concentration > 0) {
        score += 20.0;
    }

    return score;
}

// ==================== 中线选股器 ====================

MidTermSelector::MidTermSelector(std::shared_ptr<DataProvider> provider)
    : SimpleSelector(std::move(provider)) {
    config_.period = "mid_term";
    config_.top_n = 10;
}

bool MidTermSelector::filter(const StockFullData& data,
                            std::vector<std::string>& reasons) {
    const auto& tech = data.technical;

    // 股价位置
    if (tech.price_250d_high_pct > 85) {
        reasons.push_back("股价位于近1年高位85%以上");
    }

    // 扣非净利润下滑
    if (data.fundamental.deduct_profit_3y.size() >= 2) {
        double lastGrowth = 0.0;
        if (data.fundamental.deduct_profit_3y[data.fundamental.deduct_profit_3y.size() - 2] > 0) {
            lastGrowth = (data.fundamental.deduct_profit_3y.back() -
                         data.fundamental.deduct_profit_3y[data.fundamental.deduct_profit_3y.size() - 2]) /
                         std::abs(data.fundamental.deduct_profit_3y[data.fundamental.deduct_profit_3y.size() - 2]) * 100.0;
        }
        if (lastGrowth < -30.0) {
            reasons.push_back("单季度净利润大幅下滑");
        }
    }

    return true;
}

FactorScore MidTermSelector::score(const StockFullData& data) {
    FactorScore fs;
    fs.growth_score = calcGrowthScore(data);
    fs.momentum_score = calcMomentumScore(data);
    fs.value_score = calcValueScore(data);
    fs.quality_score = calcQualityScore(data);
    fs.capital_score = calcCapitalScore(data);

    fs.details["fundamental"] = fs.quality_score * 0.4 + fs.growth_score * 0.6;
    fs.details["valuation"] = fs.value_score;
    fs.details["technical"] = fs.momentum_score;
    fs.details["capital"] = fs.capital_score * 0.5;

    fs.total_score = fs.details["fundamental"] * 0.30 +
                    fs.details["valuation"] * 0.20 +
                    fs.details["technical"] * 0.35 +
                    fs.details["capital"] * 0.15;

    return fs;
}

double MidTermSelector::calcGrowthScore(const StockFullData& data) {
    double score = 0.0;
    const auto& fund = data.fundamental;

    if (fund.roe > 0) {
        score += linearScore(fund.roe, 5.0, 30.0, 30.0);
    }
    if (fund.revenue_growth > -20 && fund.revenue_growth < 80) {
        score += linearScore(fund.revenue_growth, 0.0, 30.0, 35.0);
    }
    if (fund.profit_growth > -30 && fund.profit_growth < 80) {
        score += linearScore(fund.profit_growth, -10.0, 40.0, 35.0);
    }

    return score;
}

double MidTermSelector::calcMomentumScore(const StockFullData& data) {
    double score = 0.0;
    const auto& tech = data.technical;

    if (tech.ma_bullish_monthly) {
        score += 30.0;
    } else if (tech.ma_bullish_weekly) {
        score += 20.0;
    } else if (tech.ma_bullish_daily) {
        score += 10.0;
    }

    if (tech.volume_ratio > 0) {
        score += linearScore(tech.volume_ratio, 0.8, 2.5, 25.0);
    }

    if (tech.momentum_20d > -20 && tech.momentum_20d < 50) {
        score += linearScore(tech.momentum_20d, -10.0, 25.0, 25.0);
    }

    return score;
}

double MidTermSelector::calcValueScore(const StockFullData& data) {
    double score = 0.0;
    const auto& quote = data.quote;
    const auto& fund = data.fundamental;

    double peg = 1.0;
    if (fund.roe > 0 && quote.pe > 0) {
        peg = quote.pe / fund.roe;
    }
    if (peg < 1.0) {
        score += 25.0;
    } else if (peg < 1.5) {
        score += 15.0;
    } else if (peg < 2.5) {
        score += 5.0;
    }

    if (fund.pe_percentile > 0) {
        score += linearScore(fund.pe_percentile, 10.0, 60.0, 25.0, true);
    }

    return score;
}

double MidTermSelector::calcQualityScore(const StockFullData& data) {
    double score = 0.0;
    const auto& fund = data.fundamental;

    if (fund.gross_margin > 0) {
        score += linearScore(fund.gross_margin, 10.0, 40.0, 20.0);
    }

    if (fund.net_margin > 0) {
        score += linearScore(fund.net_margin, 5.0, 20.0, 15.0);
    }

    if (fund.debt_ratio > 0 && fund.debt_ratio < 95) {
        score += linearScore(fund.debt_ratio, 30.0, 70.0, 20.0, true);
    }

    if (fund.operating_cf_ratio > 0) {
        score += linearScore(fund.operating_cf_ratio, 0.5, 1.5, 20.0);
    }

    if (fund.goodwill_ratio >= 0 && fund.goodwill_ratio < 0.5) {
        score += linearScore(fund.goodwill_ratio, 0.0, 0.3, 25.0, true);
    }

    return score;
}

double MidTermSelector::calcCapitalScore(const StockFullData& data) {
    double score = 0.0;
    const auto& flow = data.fund_flow;
    const auto& quote = data.quote;

    if (flow.main_inflow_ratio > -10 && flow.main_inflow_ratio < 20) {
        score += linearScore(flow.main_inflow_ratio, -5.0, 10.0, 25.0);
    }

    if (flow.northbound_ratio > 0) {
        score += linearScore(flow.northbound_ratio, 1.0, 8.0, 20.0);
    }

    if (quote.turnover_rate > 0.5 && quote.turnover_rate < 20) {
        score += linearScore(quote.turnover_rate, 1.0, 8.0, 20.0);
    }

    if (flow.northbound_change > 0) {
        score += linearScore(flow.northbound_change, 0.0, 20.0, 15.0);
    }

    if (flow.chip_concentration > 0) {
        score += 20.0;
    }

    return score;
}

// ==================== 短线选股器 ====================

ShortTermSelector::ShortTermSelector(std::shared_ptr<DataProvider> provider)
    : SimpleSelector(std::move(provider)) {
    config_.period = "short_term";
    config_.top_n = 15;
}

bool ShortTermSelector::filter(const StockFullData& data,
                              std::vector<std::string>& reasons) {
    const auto& quote = data.quote;

    // 流动性
    if (quote.amount < 10000000) {
        reasons.push_back("成交额过低");
        return false;
    }

    // 涨停板过滤
    if (quote.change_pct > 9.5) {
        reasons.push_back("涨停板");
    }

    return true;
}

FactorScore ShortTermSelector::score(const StockFullData& data) {
    FactorScore fs;
    fs.momentum_score = calcMomentumScore(data);
    fs.reversal_score = calcReversalScore(data);
    fs.capital_score = calcCapitalScore(data);
    fs.value_score = calcValueScore(data);
    fs.quality_score = calcQualityScore(data);

    fs.details["fundamental"] = (fs.value_score + fs.quality_score) * 0.3;
    fs.details["valuation"] = fs.value_score * 0.2;
    fs.details["technical"] = fs.momentum_score * 0.6 + fs.reversal_score * 0.4;
    fs.details["capital"] = fs.capital_score;

    fs.total_score = fs.details["fundamental"] * 0.10 +
                    fs.details["valuation"] * 0.05 +
                    fs.details["technical"] * 0.55 +
                    fs.details["capital"] * 0.30;

    return fs;
}

double ShortTermSelector::calcMomentumScore(const StockFullData& data) {
    double score = 0.0;
    const auto& tech = data.technical;

    if (tech.momentum_5d > -15 && tech.momentum_5d < 30) {
        score += linearScore(tech.momentum_5d, -5.0, 15.0, 35.0);
    }

    if (tech.relative_strength > 0) {
        score += linearScore(tech.relative_strength, 40.0, 75.0, 30.0);
    }

    if (tech.ma_bullish_daily) {
        score += 15.0;
    }

    if (tech.volume_ratio > 0.5) {
        score += linearScore(tech.volume_ratio, 1.0, 3.0, 20.0);
    }

    return score;
}

double ShortTermSelector::calcReversalScore(const StockFullData& data) {
    double score = 0.0;
    const auto& tech = data.technical;

    if (tech.reversal_5d < 0 && tech.reversal_5d > -15) {
        score += linearScore(tech.reversal_5d, -12.0, 0.0, 40.0, true);
    }

    if (tech.amplitude > 1 && tech.amplitude < 12) {
        score += linearScore(tech.amplitude, 3.0, 8.0, 30.0);
    }

    return score;
}

double ShortTermSelector::calcCapitalScore(const StockFullData& data) {
    double score = 0.0;
    const auto& flow = data.fund_flow;

    if (flow.main_inflow_ratio > 0) {
        score += linearScore(flow.main_inflow_ratio, 0.0, 8.0, 40.0);
    }

    if (flow.northbound_ratio > 0) {
        score += linearScore(flow.northbound_ratio, 1.0, 6.0, 30.0);
    }

    if (flow.northbound_change > 0) {
        score += 30.0;
    }

    return score;
}

double ShortTermSelector::calcValueScore(const StockFullData& data) {
    double score = 0.0;
    const auto& quote = data.quote;
    const auto& fund = data.fundamental;

    if (quote.pe > 0 && quote.pe < 50) {
        score += linearScore(quote.pe, 5.0, 25.0, 30.0, true);
    }
    if (quote.pb > 0 && quote.pb < 8) {
        score += linearScore(quote.pb, 1.0, 4.0, 25.0, true);
    }
    if (fund.pe_percentile > 0 && fund.pe_percentile < 60) {
        score += linearScore(fund.pe_percentile, 20.0, 50.0, 20.0, true);
    }

    return score;
}

double ShortTermSelector::calcQualityScore(const StockFullData& data) {
    double score = 0.0;
    const auto& fund = data.fundamental;

    if (fund.roe > 0) {
        score += linearScore(fund.roe, 8.0, 25.0, 30.0);
    }
    if (fund.gross_margin > 0) {
        score += linearScore(fund.gross_margin, 15.0, 40.0, 20.0);
    }
    if (fund.debt_ratio > 0 && fund.debt_ratio < 95) {
        score += linearScore(fund.debt_ratio, 30.0, 70.0, 20.0, true);
    }

    return score;
}

// ==================== 超短线选股器 ====================

UltraShortTermSelector::UltraShortTermSelector(std::shared_ptr<DataProvider> provider)
    : SimpleSelector(std::move(provider)) {
    config_.period = "ultra_short_term";
    config_.top_n = 20;
}

bool UltraShortTermSelector::filter(const StockFullData& data,
                                   std::vector<std::string>& reasons) {
    const auto& quote = data.quote;

    // 成交额要求
    if (quote.amount < 20000000) {
        reasons.push_back("成交额过低");
        return false;
    }

    // 排除一字涨停/跌停
    if (data.technical.amplitude < 0.5 && std::abs(quote.change_pct) > 9) {
        reasons.push_back("一字板");
        return false;
    }

    return true;
}

FactorScore UltraShortTermSelector::score(const StockFullData& data) {
    FactorScore fs;
    fs.capital_score = calcCapitalScore(data);
    fs.reversal_score = calcReversalScore(data);
    fs.momentum_score = calcMomentumScore(data);
    fs.value_score = calcValueScore(data);

    fs.details["fundamental"] = fs.value_score * 0.1;
    fs.details["valuation"] = fs.value_score * 0.1;
    fs.details["technical"] = fs.momentum_score * 0.3 + fs.reversal_score * 0.7;
    fs.details["capital"] = fs.capital_score;

    fs.total_score = fs.details["fundamental"] * 0.03 +
                    fs.details["valuation"] * 0.02 +
                    fs.details["technical"] * 0.40 +
                    fs.details["capital"] * 0.55;

    return fs;
}

double UltraShortTermSelector::calcCapitalScore(const StockFullData& data) {
    double score = 0.0;
    const auto& flow = data.fund_flow;
    const auto& quote = data.quote;

    // 主力净流入强度
    if (flow.main_inflow_ratio > -5 && flow.main_inflow_ratio < 15) {
        score += linearScore(flow.main_inflow_ratio, 0.0, 10.0, 35.0);
    }

    // 超大单流入
    if (flow.super_inflow > 0) {
        double ratio = flow.super_inflow / quote.amount * 100.0;
        if (ratio > 0 && ratio < 20) {
            score += linearScore(ratio, 2.0, 10.0, 25.0);
        }
    }

    // 北向资金
    if (flow.northbound_change > 5) {
        score += 20.0;
    }

    // 融资买入
    if (flow.margin_balance_change > 0) {
        score += 20.0;
    }

    return score;
}

double UltraShortTermSelector::calcReversalScore(const StockFullData& data) {
    double score = 0.0;
    const auto& tech = data.technical;

    // 温和调整后反弹
    if (tech.reversal_5d < -3 && tech.reversal_5d > -12) {
        score += linearScore(tech.reversal_5d, -10.0, -3.0, 40.0, true);
    }

    // 量比适中放大
    if (tech.volume_ratio > 1.2 && tech.volume_ratio < 4) {
        score += linearScore(tech.volume_ratio, 1.5, 3.0, 30.0);
    }

    // 振幅足够
    if (tech.amplitude > 2 && tech.amplitude < 10) {
        score += linearScore(tech.amplitude, 3.0, 7.0, 30.0);
    }

    return score;
}

double UltraShortTermSelector::calcMomentumScore(const StockFullData& data) {
    double score = 0.0;
    const auto& tech = data.technical;

    if (tech.momentum_5d > -15 && tech.momentum_5d < 30) {
        score += linearScore(tech.momentum_5d, -5.0, 15.0, 35.0);
    }

    if (tech.relative_strength > 0) {
        score += linearScore(tech.relative_strength, 40.0, 75.0, 30.0);
    }

    if (tech.ma_bullish_daily) {
        score += 15.0;
    }

    if (tech.volume_ratio > 0.5) {
        score += linearScore(tech.volume_ratio, 1.0, 3.0, 20.0);
    }

    return score;
}

double UltraShortTermSelector::calcValueScore(const StockFullData& data) {
    double score = 0.0;
    const auto& quote = data.quote;

    if (quote.pe > 0 && quote.pe < 100) {
        score += linearScore(quote.pe, 5.0, 30.0, 30.0, true);
    }
    if (quote.pb > 0 && quote.pb < 10) {
        score += linearScore(quote.pb, 1.0, 5.0, 20.0, true);
    }

    return score;
}

}
