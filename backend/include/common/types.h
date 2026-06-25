#pragma once

#include <string>
#include <vector>
#include <map>

namespace quant {

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
    double volume = 0.0;
    double amount = 0.0;
    double turnover_rate = 0.0;
    double pe = 0.0;
    double pb = 0.0;
    double total_mcap = 0.0;
    double float_mcap = 0.0;
    std::string industry;
};

struct FundamentalData {
    std::string code;
    // === 质量/盈利因子 ===
    double roe_5y_avg = 0.0;           // 5年平均ROE
    double roe = 0.0;                   // 单季度ROE
    double roa = 0.0;                   // 总资产收益率
    double gross_margin = 0.0;           // 毛利率
    double net_margin = 0.0;             // 净利率
    double operating_cf_ratio = 0.0;    // 经营现金流/净利润
    
    // === 价值因子 ===
    double pe = 0.0;                    // 市盈率
    double pb = 0.0;                    // 市净率
    double ps = 0.0;                    // 市销率
    double pcfe = 0.0;                  // 市现率（经营现金流）
    double dividend_yield = 0.0;         // 股息率
    double pe_percentile = 0.0;          // PE分位数
    
    // === 成长因子 ===
    double revenue_growth = 0.0;        // 营收增速
    double profit_growth = 0.0;         // 净利润增速
    double roe_growth = 0.0;            // ROE同比变化
    
    // === 风险因子 ===
    double debt_ratio = 0.0;            // 资产负债率
    double goodwill_ratio = 0.0;        // 商誉占净资产比例
    double pledge_ratio = 0.0;          // 质押比例
    double holder_num_change = 0.0;     // 股东户数变化率
    
    // === 辅助数据 ===
    double eps = 0.0;                   // 每股收益
    double total_share = 0.0;           // 总股本
    double float_share = 0.0;           // 流通股本
    std::vector<double> deduct_profit_3y;    // 近3年扣非净利润
    std::vector<double> operating_cashflow; // 近几年经营现金流
    std::vector<double> gross_margin_trend; // 毛利率趋势
    std::string industry;
};

struct FundFlowData {
    std::string code;
    double main_inflow = 0.0;
    double main_inflow_ratio = 0.0;
    double super_inflow = 0.0;
    double big_inflow = 0.0;
    double mid_inflow = 0.0;
    double small_inflow = 0.0;
    double northbound_shares = 0.0;
    double northbound_ratio = 0.0;
    double northbound_change = 0.0;
    double chip_concentration = 0.0;
    double share_holder_count = 0.0;
    bool has_longhubang = false;
    bool is_institution_trade = false;
};

struct TechnicalData {
    std::string code;
    std::vector<KlineData> day_klines;
    std::vector<KlineData> week_klines;
    std::vector<double> ma5;
    std::vector<double> ma10;
    std::vector<double> ma20;
    std::vector<double> ma60;
    std::vector<double> ma120;
    std::vector<double> ma250;
    
    // === 价格位置 ===
    double price_250d_high_pct = 0.0;  // 股价距250日高点百分比
    
    // === 均线信号 ===
    bool ma_bullish_monthly = false;    // 月线多头
    bool ma_bullish_weekly = false;     // 周线多头
    bool ma_bullish_daily = false;      // 日线多头
    
    // === 动量因子 ===
    double momentum_5d = 0.0;           // 5日动量
    double momentum_20d = 0.0;          // 20日动量
    double momentum_60d = 0.0;           // 60日动量
    double momentum_120d = 0.0;         // 120日动量
    
    // === 量价因子 ===
    double volume_ratio = 0.0;          // 量比（5日均量/20日均量）
    double turnover_rate = 0.0;         // 换手率
    double amplitude = 0.0;             // 振幅
    
    // === 波动率因子 ===
    double volatility_20d = 0.0;        // 20日波动率
    double volatility_60d = 0.0;         // 60日波动率
    double beta = 0.0;                  // Beta值
    
    // === 反转因子 ===
    double reversal_5d = 0.0;          // 5日反转（超跌信号）
    double reversal_20d = 0.0;          // 20日反转
};

struct StockFullData {
    std::string code;
    std::string name;
    RealtimeQuote quote;
    FundamentalData fundamental;
    FundFlowData fund_flow;
    TechnicalData technical;
    bool is_st = false;
    bool has_data = false;
};

struct SelectorResult {
    std::string code;
    std::string name;
    double score = 0.0;
    double price = 0.0;
    double change_pct = 0.0;
    std::string industry;
    std::map<std::string, double> score_details;
    std::vector<std::string> filter_reasons;
    bool passed_filter = true;
};

struct SelectorConfig {
    std::string period;
    int top_n = 10;
    std::map<std::string, double> weights;
};

enum class SelectorPeriod {
    LongTerm,
    MidTerm,
    ShortTerm,
    UltraShortTerm
};

}
