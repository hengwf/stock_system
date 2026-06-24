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
    double roe_5y_avg = 0.0;
    double gross_margin = 0.0;
    double net_margin = 0.0;
    double dividend_yield = 0.0;
    double pe_percentile = 0.0;
    double debt_ratio = 0.0;
    double goodwill_ratio = 0.0;
    double roe = 0.0;
    double roa = 0.0;
    double eps = 0.0;
    double pe = 0.0;
    double pb = 0.0;
    double price = 0.0;
    double total_share = 0.0;
    double float_share = 0.0;
    double market_capital = 0.0;
    double float_market_capital = 0.0;
    double turnover_rate = 0.0;
    std::vector<double> deduct_profit_3y;
    std::vector<double> operating_cashflow;
    std::vector<double> gross_margin_trend;
    std::string industry;
};

struct FundFlowData {
    std::string code;
    double main_inflow = 0.0;
    double super_inflow = 0.0;
    double big_inflow = 0.0;
    double mid_inflow = 0.0;
    double small_inflow = 0.0;
    double northbound_shares = 0.0;
    double northbound_ratio = 0.0;
    bool has_longhubang = false;
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
    double price_250d_high_pct = 0.0;
    bool ma_bullish_monthly = false;
    bool ma_bullish_weekly = false;
    bool ma_bullish_daily = false;
    double volume_ratio = 0.0;
    double turnover_rate = 0.0;
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
