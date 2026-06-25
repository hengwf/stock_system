#pragma once

#include "common/factors.h"
#include <vector>
#include <map>

namespace quant {

// ==================== 风控管理器 ====================
class RiskManager {
public:
    RiskManager();
    ~RiskManager() = default;

    // 事前风控：过滤高风险股票
    bool preRiskFilter(const StockFullData& data,
                       std::vector<std::string>& reasons);

    // 事中风控：检查持仓约束
    bool checkHoldingConstraints(const StockFullData& data,
                                 int currentIndustryCount,
                                 const std::map<std::string, int>& industryCounts);

    // 事后风控：计算风险评分
    double calculateRiskScore(const StockFullData& data);

    // 组合风控：计算整体风险暴露
    void analyzePortfolioRisk(const std::vector<SelectorResult>& holdings,
                             double& maxDrawdown,
                             double& volatility,
                             std::map<std::string, double>& styleExposure);

    // 更新风控参数
    void setRiskParams(double maxPositionSize,
                       double maxIndustryWeight,
                       double maxSingleStockWeight);

private:
    // === 财务排雷 ===
    bool checkFinancialRisk(const StockFullData& data, std::vector<std::string>& reasons);

    // === 流动性检查 ===
    bool checkLiquidityRisk(const StockFullData& data, std::vector<std::string>& reasons);

    // === 风险名单检查 ===
    bool checkRiskList(const StockFullData& data, std::vector<std::string>& reasons);

    // === 解禁减持检查 ===
    bool checkUnlockRisk(const StockFullData& data, std::vector<std::string>& reasons);

    // 风控参数
    double max_position_size_ = 0.05;        // 单只最大持仓5%
    double max_industry_weight_ = 0.30;      // 单行业最大权重30%
    double min_daily_amount_ = 10000000.0;    // 最小日成交额1000万
    int min_listing_days_ = 60;               // 最小上市天数（次新股过滤）
    double max_goodwill_ratio_ = 0.30;        // 最大商誉占比30%
    double max_debt_ratio_ = 0.85;            // 最大资产负债率85%
};

// ==================== 数据清洗模块 ====================
class DataCleaner {
public:
    DataCleaner();
    ~DataCleaner() = default;

    // 数据清洗主函数
    StockFullData clean(const StockFullData& raw);

    // 剔除ST股票
    bool isValidStock(const StockFullData& data);

    // 计算因子分位数
    double calculatePercentile(double value,
                              const std::vector<double>& allValues);

    // 去极值（Winzorize）
    double winsorize(double value, double lower, double upper);

    // 行业中性化
    double industryNeutralize(double value,
                             const std::string& industry,
                             const std::map<std::string, std::vector<double>>& industryValues);

    // 市值中性化
    double sizeNeutralize(double value, double mcap, double medianMcap);

    // 截面标准化
    double crossSectionalZScore(double value,
                                const std::vector<double>& allValues);

private:
    // 计算对数市值
    double calculateLogMarketCap(double mcap);
};

}
