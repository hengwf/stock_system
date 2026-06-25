#include "server/api_handler.h"
#include "selector/long_term.h"
#include "selector/mid_term.h"
#include "selector/short_term.h"
#include "selector/ultra_short_term.h"
#include "common/json_utils.h"
#include <sstream>
#include <algorithm>
#include <ctime>
#include <iomanip>
#include <cstdio>

using namespace quant::json_utils;

namespace quant {

ApiHandler::ApiHandler(std::shared_ptr<DataProvider> provider,
                       std::shared_ptr<DataCache> cache)
    : provider_(std::move(provider)), cache_(std::move(cache)) {}

std::string ApiHandler::errorResponse(int code, const std::string& message) {
    JsonWriter w;
    w.startObject();
    w.keyValue("code", code);
    w.keyValue("message", message);
    w.keyNull("data");
    w.endObject();
    return w.str();
}

std::string ApiHandler::handleHealth() {
    JsonWriter w;
    w.startObject();
    w.keyValue("code", 0);
    w.keyValue("message", "success");
    w.key("data");
    w.startObject();
    w.keyValue("status", "ok");
    w.keyValue("version", "1.0.0");
    w.endObject();
    w.endObject();
    return w.str();
}

std::string ApiHandler::handleMarketList(const std::string& params) {
    std::vector<RealtimeQuote> stocks;
    if (!provider_->getStockList(stocks)) {
        return errorResponse(-1, "获取行情列表失败");
    }

    JsonWriter w;
    w.startObject();
    w.keyValue("code", 0);
    w.keyValue("message", "success");
    w.key("data");
    w.startObject();
    w.keyValue("total", (int)stocks.size());
    w.key("stocks");
    w.startArray();
    for (const auto& s : stocks) {
        w.startObject();
        w.keyValue("code", s.code);
        w.keyValue("name", s.name);
        w.keyValue("price", s.price);
        w.keyValue("change_pct", s.change_pct);
        w.keyValue("change", s.change);
        w.keyValue("open", s.open);
        w.keyValue("high", s.high);
        w.keyValue("low", s.low);
        w.keyValue("prev_close", s.prev_close);
        w.keyValue("volume", s.volume);
        w.keyValue("amount", s.amount);
        w.keyValue("turnover_rate", s.turnover_rate);
        w.keyValue("pe", s.pe);
        w.endObject();
    }
    w.endArray();
    w.endObject();
    w.endObject();
    return w.str();
}

std::string ApiHandler::handleStockRealtime(const std::string& codes_str) {
    std::vector<std::string> codes;
    std::stringstream ss(codes_str);
    std::string code;
    while (std::getline(ss, code, ',')) {
        if (!code.empty()) codes.push_back(code);
    }

    if (codes.empty()) {
        return errorResponse(-1, "股票代码不能为空");
    }

    std::vector<RealtimeQuote> quotes;
    if (!provider_->getRealtimeQuotes(codes, quotes)) {
        return errorResponse(-1, "获取实时行情失败");
    }

    JsonWriter w;
    w.startObject();
    w.keyValue("code", 0);
    w.keyValue("message", "success");
    w.key("data");
    w.startObject();
    w.key("stocks");
    w.startArray();
    for (const auto& q : quotes) {
        w.startObject();
        w.keyValue("code", q.code);
        w.keyValue("name", q.name);
        w.keyValue("price", q.price);
        w.keyValue("change_pct", q.change_pct);
        w.keyValue("change", q.change);
        w.keyValue("open", q.open);
        w.keyValue("high", q.high);
        w.keyValue("low", q.low);
        w.keyValue("prev_close", q.prev_close);
        w.keyValue("volume", q.volume);
        w.keyValue("amount", q.amount);
        w.keyValue("turnover_rate", q.turnover_rate);
        w.keyValue("pe", q.pe);
        w.keyValue("total_mcap", q.total_mcap);
        w.keyValue("float_mcap", q.float_mcap);
        w.endObject();
    }
    w.endArray();
    w.endObject();
    w.endObject();
    return w.str();
}

std::string ApiHandler::handleStockDetail(const std::string& code) {
    if (code.empty()) {
        return errorResponse(-1, "股票代码不能为空");
    }

    fprintf(stderr, "[handleStockDetail] called with code: %s\n", code.c_str());

    StockFullData data;
    if (!provider_->getStockFullData(code, data)) {
        fprintf(stderr, "[handleStockDetail] getStockFullData failed for: %s\n", code.c_str());
        return errorResponse(-1, "获取股票详情失败");
    }

    fprintf(stderr, "[handleStockDetail] success - pe=%.2f, pb=%.2f, fund_flow.main=%.2f\n",
            data.quote.pe, data.quote.pb, data.fund_flow.main_inflow);

    JsonWriter w;
    w.startObject();
    w.keyValue("code", 0);
    w.keyValue("message", "success");
    w.key("data");
    w.startObject();
    w.keyValue("code", data.code);
    w.keyValue("name", data.name);
    w.keyValue("price", data.quote.price);
    w.keyValue("change_pct", data.quote.change_pct);
    w.keyValue("change", data.quote.change);
    w.keyValue("open", data.quote.open);
    w.keyValue("high", data.quote.high);
    w.keyValue("low", data.quote.low);
    w.keyValue("prev_close", data.quote.prev_close);
    w.keyValue("volume", data.quote.volume);
    w.keyValue("amount", data.quote.amount);
    w.keyValue("turnover_rate", data.quote.turnover_rate);
    w.keyValue("pe", data.quote.pe);
    w.keyValue("pb", data.quote.pb);
    w.keyValue("total_mcap", data.quote.total_mcap);
    w.keyValue("float_mcap", data.quote.float_mcap);
    w.keyValue("industry", data.fundamental.industry);
    w.keyValue("total_share", data.fundamental.total_share);
    w.keyValue("float_share", data.fundamental.float_share);
    w.keyValue("roe", data.fundamental.roe);
    w.keyValue("gross_margin", data.fundamental.gross_margin);
    w.keyValue("net_margin", data.fundamental.net_margin);
    w.keyValue("debt_ratio", data.fundamental.debt_ratio);
    w.keyValue("dividend_yield", data.fundamental.dividend_yield);
    w.key("fund_flow");
    w.startObject();
    w.keyValue("main_inflow", data.fund_flow.main_inflow);
    w.keyValue("super_inflow", data.fund_flow.super_inflow);
    w.keyValue("big_inflow", data.fund_flow.big_inflow);
    w.keyValue("mid_inflow", data.fund_flow.mid_inflow);
    w.keyValue("small_inflow", data.fund_flow.small_inflow);
    w.keyValue("northbound_shares", data.fund_flow.northbound_shares);
    w.keyValue("northbound_ratio", data.fund_flow.northbound_ratio);
    w.keyValue("has_longhubang", data.fund_flow.has_longhubang);
    w.endObject();
    w.endObject();
    w.endObject();
    return w.str();
}

std::string ApiHandler::handleKline(const std::string& code,
                                    const std::string& type,
                                    int count) {
    if (code.empty()) {
        return errorResponse(-1, "股票代码不能为空");
    }

    std::vector<KlineData> klines;
    if (!cache_->getKlineData(code, type, klines)) {
        if (!provider_->getKlineData(code, type, count, klines)) {
            return errorResponse(-1, "获取K线数据失败");
        }
        cache_->setKlineData(code, type, klines, 300);
    }

    JsonWriter w;
    w.startObject();
    w.keyValue("code", 0);
    w.keyValue("message", "success");
    w.key("data");
    w.startObject();
    w.key("klines");
    w.startArray();
    for (const auto& k : klines) {
        w.startObject();
        w.keyValue("date", k.date);
        w.keyValue("open", k.open);
        w.keyValue("close", k.close);
        w.keyValue("high", k.high);
        w.keyValue("low", k.low);
        w.keyValue("volume", k.volume);
        w.keyValue("amount", k.amount);
        w.endObject();
    }
    w.endArray();
    w.keyValue("count", (int)klines.size());
    w.endObject();
    w.endObject();
    return w.str();
}

std::string ApiHandler::handleFundFlow(const std::string& code) {
    if (code.empty()) {
        return errorResponse(-1, "股票代码不能为空");
    }

    FundFlowData flow;
    if (!provider_->getFundFlow(code, flow)) {
        return errorResponse(-1, "获取资金流向失败");
    }

    JsonWriter w;
    w.startObject();
    w.keyValue("code", 0);
    w.keyValue("message", "success");
    w.key("data");
    w.startObject();
    w.keyValue("code", flow.code);
    w.keyValue("main_inflow", flow.main_inflow);
    w.keyValue("super_inflow", flow.super_inflow);
    w.keyValue("big_inflow", flow.big_inflow);
    w.keyValue("mid_inflow", flow.mid_inflow);
    w.keyValue("small_inflow", flow.small_inflow);
    w.keyValue("northbound_shares", flow.northbound_shares);
    w.keyValue("northbound_ratio", flow.northbound_ratio);
    w.keyValue("has_longhubang", flow.has_longhubang);
    w.endObject();
    w.endObject();
    return w.str();
}

std::string ApiHandler::handleRunSelector(const std::string& body) {
    try {
        std::string period = "long_term";
        JsonParser parser(body);
        JsonValue root;
        if (parser.parse(root) && root.type == JsonValue::OBJECT) {
            auto it = root.obj_val.find("period");
            if (it != root.obj_val.end() && it->second.type == JsonValue::STRING) {
                period = it->second.str_val;
            }
        }

        auto selector = createSelector(period);
        if (!selector) {
            return errorResponse(-1, "无效的选股周期");
        }

        std::vector<RealtimeQuote> stockList;
        provider_->getStockList(stockList);

        std::vector<std::string> codes;
        for (const auto& s : stockList) {
            codes.push_back(s.code);
        }

        std::vector<SelectorResult> results;
        selector->select(codes, 10, results);

        std::time_t now = std::time(nullptr);
        char timeBuf[64];
        std::strftime(timeBuf, sizeof(timeBuf), "%Y-%m-%d %H:%M:%S", std::localtime(&now));
        std::string timeStr = timeBuf;

        cache_->setSelectorResult(period, results, timeStr);

        return buildSelectorResult(results, period, timeStr);
    } catch (const std::exception& e) {
        return errorResponse(-1, std::string("选股失败: ") + e.what());
    }
}

std::string ApiHandler::handleSelectorResult(const std::string& period) {
    std::vector<SelectorResult> results;
    std::string time;

    if (!cache_->getSelectorResult(period, results, time)) {
        return errorResponse(-2, "暂无选股结果，请先执行选股");
    }

    return buildSelectorResult(results, period, time);
}

std::string ApiHandler::buildSelectorResult(const std::vector<SelectorResult>& results,
                                            const std::string& period,
                                            const std::string& time) {
    JsonWriter w;
    w.startObject();
    w.keyValue("code", 0);
    w.keyValue("message", "success");
    w.key("data");
    w.startObject();
    w.keyValue("period", period);
    w.keyValue("total", (int)results.size());
    w.keyValue("time", time);
    w.key("stocks");
    w.startArray();
    for (const auto& r : results) {
        w.startObject();
        w.keyValue("code", r.code);
        w.keyValue("name", r.name);
        w.keyValue("score", r.score);
        w.keyValue("price", r.price);
        w.keyValue("change_pct", r.change_pct);
        w.keyValue("industry", r.industry);
        w.key("score_details");
        w.startObject();
        for (std::map<std::string, double>::const_iterator it = r.score_details.begin(); it != r.score_details.end(); ++it) {
            const std::string& k = it->first;
            double v = it->second;
            w.key(k);
            w.value(v);
        }
        w.endObject();
        w.endObject();
    }
    w.endArray();
    w.endObject();
    w.endObject();
    return w.str();
}

std::unique_ptr<StockSelector> ApiHandler::createSelector(const std::string& period) {
    if (period == "long_term") {
        return std::make_unique<LongTermSelector>(provider_);
    } else if (period == "mid_term") {
        return std::make_unique<MidTermSelector>(provider_);
    } else if (period == "short_term") {
        return std::make_unique<ShortTermSelector>(provider_);
    } else if (period == "ultra_short_term") {
        return std::make_unique<UltraShortTermSelector>(provider_);
    }
    return nullptr;
}

}
