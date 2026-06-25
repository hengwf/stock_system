#pragma once

#include "common/factors.h"
#include "common/risk_control.h"
#include "datasource/data_provider.h"
#include <string>
#include <vector>
#include <map>
#include <memory>

namespace quant {

// ==================== 统一选股引擎 ====================
class QuantSelector {
public:
    explicit QuantSelector(std::shared_ptr<DataProvider> provider);
    ~QuantSelector() = default;

    // 设置选股周期
    void setPeriod(SelectorPeriod period);

    // 设置因子权重
    void setFactorWeights(double value, double growth, double quality,
                         double momentum, double reversal, double capital);

    // 风控设置
    void setRiskFilter(bool filterSt, bool filterNewStock,
                      bool filterLowLiq, bool filterHighRisk);

    // 选股主函数
    bool select(const std::vector<std::string>& codes,
                int topN,
                std::vector<SelectorResult>& results);

    // 获取因子得分
    FactorScore calculateFactorScore(const StockFullData& data);

private:
    // === 因子计算 ===
    double calcValueScore(const StockFullData& data);
    double calcGrowthScore(const StockFullData& data);
    double calcQualityScore(const StockFullData& data);
    double calcMomentumScore(const StockFullData& data);
    double calcReversalScore(const StockFullData& data);
    double calcCapitalScore(const StockFullData& data);

    // === 辅助函数 ===
    double linearScore(double value, double minVal, double maxVal,
                      double maxScore, bool reverse = false);
    double percentileScore(double value, const std::vector<double>& allValues,
                          double maxScore, bool reverse = false);

    // === 数据获取 ===
    bool loadStockData(const std::vector<std::string>& codes,
                      std::vector<StockFullData>& allData);

    std::shared_ptr<DataProvider> provider_;
    RiskManager riskManager_;
    DataCleaner dataCleaner_;

    SelectorConfig config_;
    SelectorPeriod period_;

    // 因子缓存（用于截面标准化）
    std::vector<double> peValues_;
    std::vector<double> roeValues_;
    std::vector<double> growthValues_;
    std::vector<double> volumeValues_;
    std::vector<double> mcapValues_;
};

// ==================== 简化版选股器（保持向后兼容） ====================
class SimpleSelector {
public:
    SimpleSelector(std::shared_ptr<DataProvider> provider);
    virtual ~SimpleSelector() = default;

    virtual std::string name() const = 0;
    virtual SelectorPeriod period() const = 0;

    bool select(const std::vector<std::string>& codes,
                int topN,
                std::vector<SelectorResult>& results);

protected:
    virtual bool filter(const StockFullData& data,
                       std::vector<std::string>& reasons) = 0;

    virtual FactorScore score(const StockFullData& data) = 0;

    double linearScore(double value, double minVal, double maxVal,
                      double maxScore, bool reverse = false);

    std::shared_ptr<DataProvider> provider_;
    SelectorConfig config_;
};

// 四个周期选股器
class LongTermSelector : public SimpleSelector {
public:
    explicit LongTermSelector(std::shared_ptr<DataProvider> provider);

    std::string name() const override { return "长线价值"; }
    SelectorPeriod period() const override { return SelectorPeriod::LongTerm; }

protected:
    bool filter(const StockFullData& data,
               std::vector<std::string>& reasons) override;

    FactorScore score(const StockFullData& data) override;

    double calcValueScore(const StockFullData& data);
    double calcQualityScore(const StockFullData& data);
    double calcGrowthScore(const StockFullData& data);
    double calcMomentumScore(const StockFullData& data);
    double calcCapitalScore(const StockFullData& data);
};

class MidTermSelector : public SimpleSelector {
public:
    explicit MidTermSelector(std::shared_ptr<DataProvider> provider);

    std::string name() const override { return "中线成长"; }
    SelectorPeriod period() const override { return SelectorPeriod::MidTerm; }

protected:
    bool filter(const StockFullData& data,
               std::vector<std::string>& reasons) override;

    FactorScore score(const StockFullData& data) override;

    double calcGrowthScore(const StockFullData& data);
    double calcMomentumScore(const StockFullData& data);
    double calcValueScore(const StockFullData& data);
    double calcQualityScore(const StockFullData& data);
    double calcCapitalScore(const StockFullData& data);
};

class ShortTermSelector : public SimpleSelector {
public:
    explicit ShortTermSelector(std::shared_ptr<DataProvider> provider);

    std::string name() const override { return "短线动量"; }
    SelectorPeriod period() const override { return SelectorPeriod::ShortTerm; }

protected:
    bool filter(const StockFullData& data,
               std::vector<std::string>& reasons) override;

    FactorScore score(const StockFullData& data) override;

    double calcMomentumScore(const StockFullData& data);
    double calcReversalScore(const StockFullData& data);
    double calcCapitalScore(const StockFullData& data);
    double calcValueScore(const StockFullData& data);
    double calcQualityScore(const StockFullData& data);
};

class UltraShortTermSelector : public SimpleSelector {
public:
    explicit UltraShortTermSelector(std::shared_ptr<DataProvider> provider);

    std::string name() const override { return "超短资金"; }
    SelectorPeriod period() const override { return SelectorPeriod::UltraShortTerm; }

protected:
    bool filter(const StockFullData& data,
               std::vector<std::string>& reasons) override;

    FactorScore score(const StockFullData& data) override;

    double calcCapitalScore(const StockFullData& data);
    double calcReversalScore(const StockFullData& data);
    double calcMomentumScore(const StockFullData& data);
    double calcValueScore(const StockFullData& data);
};

}
