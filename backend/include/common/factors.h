#pragma once

#include <string>
#include <vector>
#include <map>

namespace quant {

// ==================== 行情数据 ====================
struct KlineData {
    std::string date;
    double open = 0.0;
    double close = 0.0;
    double high = 0.0;
    double low = 0.0;
    double volume = 0.0;
    double amount = 0.0;
};

struct RealtimeQuote {
    std::string code;
    std::string name;
    double price = 0.0;
    double open = 0.0;
    double high = 0.0;
    double low = 0.0;
    double prev_close = 0.0;
    double change = 0.0;
    double change_pct = 0.0;
    double volume = 0.0;           // 成交量
    double amount = 0.0;           // 成交额
    double turnover_rate = 0.0;     // 换手率
    double pe = 0.0;               // 市盈率
    double pb = 0.0;               // 市净率
    double total_mcap = 0.0;       // 总市值
    double float_mcap = 0.0;       // 流通市值
    std::string industry;

    // 额外计算字段
    double daily_turnover = 0.0;   // 日均成交额
    double amplitude = 0.0;        // 振幅
};

// ==================== 基本面数据（价值+成长+质量因子） ====================
struct FundamentalData {
    std::string code;

    // === 价值因子 ===
    double pe = 0.0;               // 市盈率
    double pb = 0.0;               // 市净率
    double ps = 0.0;              // 市销率
    double pcf = 0.0;             // 现金流倍率
    double dividend_yield = 0.0;   // 股息率
    double ev_ebitda = 0.0;       // EV/EBITDA

    // === 成长因子 ===
    double roe = 0.0;             // 净资产收益率
    double roe_5y_avg = 0.0;      // 5年平均ROE
    double roa = 0.0;             // 总资产收益率
    double gross_margin = 0.0;     // 毛利率
    double net_margin = 0.0;       // 净利率
    double revenue_growth = 0.0;   // 营收增速
    double profit_growth = 0.0;    // 利润增速
    double eps = 0.0;             // 每股收益

    // === 质量因子 ===
    double debt_ratio = 0.0;       // 资产负债率
    double goodwill_ratio = 0.0;    // 商誉占比
    double current_ratio = 0.0;    // 流动比率
    double quick_ratio = 0.0;     // 速动比率
    double operating_cf_ratio = 0.0; // 经营现金流/净利润
    double receivables_turnover = 0.0; // 应收账款周转率

    // === 规模因子 ===
    double total_share = 0.0;     // 总股本
    double float_share = 0.0;     // 流通股本
    double free_float_ratio = 0.0; // 自由流通比例

    // === 历史趋势数据 ===
    std::vector<double> deduct_profit_3y;      // 扣非净利润3年
    std::vector<double> operating_cashflow;     // 经营现金流3年
    std::vector<double> gross_margin_trend;      // 毛利率趋势

    // === 基本信息 ===
    std::string industry;
    double pe_percentile = 50.0;  // PE分位数

    // 财务健康度评分
    double financial_health_score = 0.0;
};

// ==================== 资金流向数据（资金行为因子） ====================
struct FundFlowData {
    std::string code;

    // 主力资金流向
    double main_inflow = 0.0;      // 主力净流入
    double main_inflow_ratio = 0.0; // 主力净流入占比
    double super_inflow = 0.0;     // 超大单净流入
    double big_inflow = 0.0;      // 大单净流入
    double mid_inflow = 0.0;       // 中单净流入
    double small_inflow = 0.0;     // 小单净流入

    // 北向资金
    double northbound_shares = 0.0;  // 北向持股数量
    double northbound_ratio = 0.0;   // 北向持股占比
    double northbound_change = 0.0;  // 北向持股变动

    // 融资融券
    double margin_balance = 0.0;   // 融资余额
    double margin_balance_change = 0.0; // 融资余额变动
    double short_balance = 0.0;    // 融券余额

    // 筹码集中度
    double chip_concentration = 0.0;
    double share_holder_count = 0.0; // 股东户数
    double share_holder_change = 0.0; // 股东户数变动

    // 龙虎榜
    bool has_longhubang = false;
    bool is_institution_trade = false; // 机构交易

    // 主力资金评分
    double capital_score = 0.0;
};

// ==================== 技术指标数据（动量+反转+风险因子） ====================
struct TechnicalData {
    std::string code;
    std::vector<KlineData> day_klines;
    std::vector<KlineData> week_klines;

    // 均线系统
    std::vector<double> ma5;
    std::vector<double> ma10;
    std::vector<double> ma20;
    std::vector<double> ma60;
    std::vector<double> ma120;
    std::vector<double> ma250;

    // === 动量因子 ===
    double momentum_5d = 0.0;      // 5日动量
    double momentum_20d = 0.0;     // 20日动量
    double momentum_60d = 0.0;     // 60日动量
    double momentum_120d = 0.0;    // 120日动量
    double relative_strength = 0.0; // 相对强弱

    // === 反转因子 ===
    double reversal_5d = 0.0;     // 5日反转
    double reversal_20d = 0.0;    // 20日反转

    // === 趋势因子 ===
    bool ma_bullish_monthly = false;
    bool ma_bullish_weekly = false;
    bool ma_bullish_daily = false;
    double trend_score = 0.0;      // 趋势评分

    // === 风险因子 ===
    double volatility_20d = 0.0;   // 20日波动率
    double volatility_60d = 0.0;   // 60日波动率
    double beta = 0.0;             // Beta值
    double max_drawdown = 0.0;     // 最大回撤

    // === 量价因子 ===
    double volume_ratio = 0.0;    // 量比
    double turnover_rate = 0.0;    // 换手率
    double amplitude = 0.0;       // 振幅

    // === 位置因子 ===
    double price_250d_high_pct = 0.0; // 距250日高点
    double price_250d_low_pct = 0.0;  // 距250日低点
    double price_near_high = 0.0;     // 接近年内高点

    // 技术面综合评分
    double technical_score = 0.0;
};

// ==================== 风控数据 ====================
struct RiskData {
    std::string code;

    // 财务风险
    bool is_st = false;           // ST标识
    bool is_risk_warning = false;  // 风险警示
    bool is_suspended = false;    // 停牌
    bool is_delisted = false;     // 退市风险

    // 财务雷区
    bool high_goodwill_risk = false;  // 高商誉风险
    bool high_debt_risk = false;      // 高负债风险
    bool negative_cf_risk = false;    // 现金流为负
    bool profit_warning_risk = false; // 业绩变脸

    // 减持解禁
    bool has_large_unlock = false; // 大额解禁
    bool has_reduction = false;    // 大股东减持

    // 流动性风险
    bool low_liquidity = false;    // 低流动性
    double min_daily_volume = 0.0; // 最小日成交量

    // 舆情风险
    bool negative_news = false;   // 负面新闻

    // 综合风险评分
    double risk_score = 0.0;
    std::vector<std::string> risk_reasons;
};

// ==================== 完整股票数据 ====================
struct StockFullData {
    std::string code;
    std::string name;
    RealtimeQuote quote;
    FundamentalData fundamental;
    FundFlowData fund_flow;
    TechnicalData technical;
    RiskData risk;

    // 上市时间（天）
    int listing_days = 0;

    bool has_data = false;
};

// ==================== 选股结果 ====================
struct FactorScore {
    // 因子得分
    double value_score = 0.0;      // 价值因子
    double growth_score = 0.0;     // 成长因子
    double quality_score = 0.0;    // 质量因子
    double momentum_score = 0.0;   // 动量因子
    double reversal_score = 0.0;   // 反转因子
    double capital_score = 0.0;    // 资金因子
    double risk_score = 0.0;       // 风险因子

    // 综合得分
    double total_score = 0.0;

    // 详细得分
    std::map<std::string, double> details;
};

struct SelectorResult {
    std::string code;
    std::string name;
    double score = 0.0;
    double price = 0.0;
    double change_pct = 0.0;
    std::string industry;

    // 因子得分明细
    FactorScore factor_scores;

    // 风控信息
    bool passed_filter = true;
    std::vector<std::string> filter_reasons;
    std::vector<std::string> risk_warnings;

    // 持仓建议
    int holding_days = 0;
    double target_weight = 0.0;
};

struct SelectorConfig {
    std::string period;            // 周期名称
    int top_n = 10;                // 选取数量

    // 因子权重配置
    double value_weight = 0.0;     // 价值因子权重
    double growth_weight = 0.0;     // 成长因子权重
    double quality_weight = 0.0;    // 质量因子权重
    double momentum_weight = 0.0;   // 动量因子权重
    double reversal_weight = 0.0;   // 反转因子权重
    double capital_weight = 0.0;    // 资金因子权重

    // 风控配置
    bool filter_st = true;          // 过滤ST
    bool filter_new_stock = true;  // 过滤次新股
    bool filter_low_liquidity = true; // 过滤低流动性
    bool filter_high_risk = true;   // 过滤高风险

    // 行业约束
    int max_industry_stocks = 3;    // 单行业最大持仓

    // 中性化配置
    bool size_neutral = false;      // 市值中性
    bool industry_neutral = false;  // 行业中性
};

enum class SelectorPeriod {
    LongTerm,      // 长线（价值+质量）
    MidTerm,       // 中线（成长+动量）
    ShortTerm,     // 短线（动量+反转）
    UltraShortTerm // 超短线（资金+情绪）
};

}
