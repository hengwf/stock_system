#include "datasource/sina_provider.h"
#include "common/json_utils.h"
#include <sstream>
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <cstdio>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <wininet.h>
#pragma comment(lib, "wininet.lib")
#endif

namespace quant {

SinaProvider::SinaProvider() = default;
SinaProvider::~SinaProvider() = default;

std::string SinaProvider::httpGet(const std::string& url, const std::string& referer) {
#ifdef _WIN32
    std::string result;
    HINTERNET hInternet = InternetOpenA("Mozilla/5.0", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    if (!hInternet) return "";

    DWORD timeout = 10000;
    InternetSetOptionA(hInternet, INTERNET_OPTION_CONNECT_TIMEOUT, &timeout, sizeof(timeout));
    InternetSetOptionA(hInternet, INTERNET_OPTION_RECEIVE_TIMEOUT, &timeout, sizeof(timeout));
    InternetSetOptionA(hInternet, INTERNET_OPTION_SEND_TIMEOUT, &timeout, sizeof(timeout));

    URL_COMPONENTSA urlComp;
    memset(&urlComp, 0, sizeof(urlComp));
    urlComp.dwStructSize = sizeof(urlComp);

    char hostName[256] = {0};
    char urlPath[1024] = {0};
    urlComp.lpszHostName = hostName;
    urlComp.dwHostNameLength = sizeof(hostName);
    urlComp.lpszUrlPath = urlPath;
    urlComp.dwUrlPathLength = sizeof(urlPath);
    urlComp.dwSchemeLength = 1;

    if (!InternetCrackUrlA(url.c_str(), url.size(), 0, &urlComp)) {
        InternetCloseHandle(hInternet);
        return "";
    }

    bool isHttps = (urlComp.nScheme == INTERNET_SCHEME_HTTPS);
    int port = urlComp.nPort;

    HINTERNET hConnect = InternetConnectA(hInternet, hostName, port, NULL, NULL,
        INTERNET_SERVICE_HTTP, 0, 0);
    if (!hConnect) {
        InternetCloseHandle(hInternet);
        return "";
    }

    DWORD flags = INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE;
    if (isHttps) flags |= INTERNET_FLAG_SECURE;

    HINTERNET hRequest = HttpOpenRequestA(hConnect, "GET", urlPath, NULL, NULL, NULL, flags, 0);
    if (!hRequest) {
        InternetCloseHandle(hConnect);
        InternetCloseHandle(hInternet);
        return "";
    }

    std::string headers = "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36\r\n";
    headers += "Accept: */*\r\n";
    if (!referer.empty()) {
        headers += "Referer: " + referer + "\r\n";
    }
    HttpAddRequestHeadersA(hRequest, headers.c_str(), headers.size(), HTTP_ADDREQ_FLAG_ADD);

    if (isHttps) {
        DWORD securityFlags = SECURITY_FLAG_IGNORE_UNKNOWN_CA | SECURITY_FLAG_IGNORE_CERT_DATE_INVALID |
                              SECURITY_FLAG_IGNORE_CERT_CN_INVALID;
        InternetSetOptionA(hRequest, INTERNET_OPTION_SECURITY_FLAGS, &securityFlags, sizeof(securityFlags));
    }

    if (HttpSendRequestA(hRequest, NULL, 0, NULL, 0)) {
        char buffer[8192];
        DWORD bytesRead = 0;
        while (InternetReadFile(hRequest, buffer, sizeof(buffer) - 1, &bytesRead) && bytesRead > 0) {
            buffer[bytesRead] = '\0';
            result += buffer;
        }
    }

    InternetCloseHandle(hRequest);
    InternetCloseHandle(hConnect);
    InternetCloseHandle(hInternet);
    return result;
#else
    return "";
#endif
}

std::string SinaProvider::toEastMoneyCode(const std::string& code) {
    std::string c = normalizeCode(code);
    if (c.size() >= 2) {
        if (c.substr(0, 2) == "sh") {
            return "1." + c.substr(2);
        } else if (c.substr(0, 2) == "sz") {
            return "0." + c.substr(2);
        }
    }
    if (c.size() == 6) {
        if (c[0] == '6' || c[0] == '9') {
            return "1." + c;
        } else {
            return "0." + c;
        }
    }
    return c;
}

std::string SinaProvider::gbkToUtf8(const std::string& gbkStr) {
#ifdef _WIN32
    int len = MultiByteToWideChar(CP_ACP, 0, gbkStr.c_str(), -1, NULL, 0);
    if (len <= 0) return gbkStr;
    
    std::wstring wstr(len, L'\0');
    MultiByteToWideChar(CP_ACP, 0, gbkStr.c_str(), -1, &wstr[0], len);
    
    len = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, NULL, 0, NULL, NULL);
    if (len <= 0) return gbkStr;
    
    std::string utf8Str(len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &utf8Str[0], len, NULL, NULL);
    
    return utf8Str;
#else
    return gbkStr;
#endif
}

std::string SinaProvider::normalizeCode(const std::string& code) {
    std::string c = code;
    if (c.size() >= 2) {
        if (c.substr(0, 2) == "sh" || c.substr(0, 2) == "sz") {
            return c;
        }
    }
    if (c.size() == 6) {
        if (c[0] == '6' || c[0] == '9') {
            return "sh" + c;
        } else {
            return "sz" + c;
        }
    }
    return c;
}

std::string SinaProvider::toSinaCode(const std::string& code) {
    return normalizeCode(code);
}

bool SinaProvider::parseSinaQuote(const std::string& response,
                                  const std::vector<std::string>& codes,
                                  std::vector<RealtimeQuote>& quotes) {
    if (response.empty()) return false;

    std::istringstream iss(response);
    std::string line;
    size_t idx = 0;

    while (std::getline(iss, line) && idx < codes.size()) {
        if (line.empty()) continue;

        size_t eqPos = line.find('=');
        if (eqPos == std::string::npos) continue;

        size_t quoteStart = line.find('"', eqPos);
        size_t quoteEnd = line.rfind('"');
        if (quoteStart == std::string::npos || quoteEnd == std::string::npos || quoteEnd <= quoteStart) {
            idx++;
            continue;
        }

        std::string data = line.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
        if (data.empty()) {
            idx++;
            continue;
        }

        std::vector<std::string> fields;
        std::stringstream ss(data);
        std::string field;
        while (std::getline(ss, field, ',')) {
            fields.push_back(field);
        }

        if (fields.size() < 32) {
            idx++;
            continue;
        }

        RealtimeQuote q;
        q.code = codes[idx];
        if (q.code.size() >= 2 && (q.code.substr(0, 2) == "sh" || q.code.substr(0, 2) == "sz")) {
            q.code = q.code.substr(2);
        }
        q.name = gbkToUtf8(fields[0]);
        q.open = atof(fields[1].c_str());
        q.prev_close = atof(fields[2].c_str());
        q.price = atof(fields[3].c_str());
        q.high = atof(fields[4].c_str());
        q.low = atof(fields[5].c_str());
        q.volume = atof(fields[8].c_str());
        q.amount = atof(fields[9].c_str());
        q.change = q.price - q.prev_close;
        q.change_pct = q.prev_close > 0 ? (q.change / q.prev_close * 100.0) : 0.0;

        if (fields.size() > 38) {
            q.pe = atof(fields[39].c_str());
        }
        if (fields.size() > 44) {
            q.total_mcap = atof(fields[45].c_str()) * 10000.0;
            q.float_mcap = atof(fields[44].c_str()) * 10000.0;
        }
        if (fields.size() > 37) {
            q.turnover_rate = atof(fields[38].c_str());
        }

        if (q.name.find("ST") != std::string::npos) {
            q.name = q.name;
        }

        quotes.push_back(q);
        idx++;
    }

    return !quotes.empty();
}

bool SinaProvider::getRealtimeQuotes(const std::vector<std::string>& codes,
                                     std::vector<RealtimeQuote>& quotes) {
    if (codes.empty()) return false;

    const size_t batchSize = 100;
    std::vector<std::string> sinaCodes;
    for (const auto& c : codes) {
        sinaCodes.push_back(toSinaCode(c));
    }

    for (size_t i = 0; i < sinaCodes.size(); i += batchSize) {
        size_t end = std::min(i + batchSize, sinaCodes.size());
        std::string url = "http://hq.sinajs.cn/list=";
        for (size_t j = i; j < end; j++) {
            if (j > i) url += ",";
            url += sinaCodes[j];
        }

        std::string resp = httpGet(url, "https://finance.sina.com.cn");
        if (resp.empty()) continue;

        std::vector<std::string> batchCodes(sinaCodes.begin() + i, sinaCodes.begin() + end);
        std::vector<RealtimeQuote> batchQuotes;
        if (parseSinaQuote(resp, batchCodes, batchQuotes)) {
            quotes.insert(quotes.end(), batchQuotes.begin(), batchQuotes.end());
        }
    }

    return !quotes.empty();
}

bool SinaProvider::parseKlineJson(const std::string& response,
                                  std::vector<KlineData>& klines) {
    if (response.empty()) return false;

    std::string cleaned = response;
    size_t eqPos = cleaned.find('=');
    if (eqPos != std::string::npos) {
        cleaned = cleaned.substr(eqPos + 1);
    }
    while (!cleaned.empty() && (cleaned.back() == ';' || cleaned.back() == '\n' || cleaned.back() == ' ' || cleaned.back() == '\r')) {
        cleaned.pop_back();
    }

    json_utils::JsonParser parser(cleaned);
    json_utils::JsonValue root;
    if (!parser.parse(root)) return false;
    if (root.type != json_utils::JsonValue::ARRAY) return false;

    for (const auto& item : root.arr_val) {
        if (item.type != json_utils::JsonValue::OBJECT) continue;
        KlineData k;

        auto itDay = item.obj_val.find("day");
        if (itDay == item.obj_val.end()) itDay = item.obj_val.find("date");
        if (itDay != item.obj_val.end() && itDay->second.type == json_utils::JsonValue::STRING) {
            k.date = itDay->second.str_val;
        } else continue;

        auto itOpen = item.obj_val.find("open");
        if (itOpen != item.obj_val.end()) {
            if (itOpen->second.type == json_utils::JsonValue::NUMBER) k.open = itOpen->second.num_val;
            else if (itOpen->second.type == json_utils::JsonValue::STRING) k.open = atof(itOpen->second.str_val.c_str());
        }

        auto itClose = item.obj_val.find("close");
        if (itClose != item.obj_val.end()) {
            if (itClose->second.type == json_utils::JsonValue::NUMBER) k.close = itClose->second.num_val;
            else if (itClose->second.type == json_utils::JsonValue::STRING) k.close = atof(itClose->second.str_val.c_str());
        }

        auto itHigh = item.obj_val.find("high");
        if (itHigh != item.obj_val.end()) {
            if (itHigh->second.type == json_utils::JsonValue::NUMBER) k.high = itHigh->second.num_val;
            else if (itHigh->second.type == json_utils::JsonValue::STRING) k.high = atof(itHigh->second.str_val.c_str());
        }

        auto itLow = item.obj_val.find("low");
        if (itLow != item.obj_val.end()) {
            if (itLow->second.type == json_utils::JsonValue::NUMBER) k.low = itLow->second.num_val;
            else if (itLow->second.type == json_utils::JsonValue::STRING) k.low = atof(itLow->second.str_val.c_str());
        }

        auto itVol = item.obj_val.find("volume");
        if (itVol != item.obj_val.end()) {
            if (itVol->second.type == json_utils::JsonValue::NUMBER) k.volume = itVol->second.num_val;
            else if (itVol->second.type == json_utils::JsonValue::STRING) k.volume = atof(itVol->second.str_val.c_str());
        }

        auto itAmt = item.obj_val.find("amount");
        if (itAmt != item.obj_val.end()) {
            if (itAmt->second.type == json_utils::JsonValue::NUMBER) k.amount = itAmt->second.num_val;
            else if (itAmt->second.type == json_utils::JsonValue::STRING) k.amount = atof(itAmt->second.str_val.c_str());
        }

        klines.push_back(k);
    }

    return !klines.empty();
}

bool SinaProvider::getKlineData(const std::string& code,
                                const std::string& type,
                                int count,
                                std::vector<KlineData>& klines) {
    std::string cacheKey = code + "_" + type + "_" + std::to_string(count);
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = kline_cache_.find(cacheKey);
        if (it != kline_cache_.end() && !it->second.empty()) {
            klines = it->second;
            return true;
        }
    }

    std::string sinaCode = toSinaCode(code);
    std::string scale;
    std::string datalen = std::to_string(count);

    if (type == "day") scale = "240";
    else if (type == "week") scale = "1200";
    else if (type == "month") scale = "4800";
    else if (type == "5min") scale = "5";
    else if (type == "15min") scale = "15";
    else if (type == "30min") scale = "30";
    else if (type == "60min") scale = "60";
    else scale = "240";

    std::string url = "http://money.finance.sina.com.cn/quotes_service/api/json_v2.php/CN_MarketData.getKLineData?"
                      "symbol=" + sinaCode + "&scale=" + scale + "&ma=no&datalen=" + datalen;

    std::string resp = httpGet(url);
    bool ok = parseKlineJson(resp, klines);

    if (ok && !klines.empty()) {
        std::lock_guard<std::mutex> lock(mutex_);
        kline_cache_[cacheKey] = klines;
    }

    return ok;
}

bool SinaProvider::calculateTechnicalIndicators(TechnicalData& tech) {
    const auto& klines = tech.day_klines;
    if (klines.size() < 20) {
        tech.turnover_rate = 1.5;
        tech.volume_ratio = 1.0;
        tech.price_250d_high_pct = 50.0;
        tech.ma_bullish_daily = false;
        tech.ma_bullish_weekly = false;
        tech.ma_bullish_monthly = false;
        return false;
    }

    auto calcMA = [&klines](int periods) -> double {
        if ((int)klines.size() < periods) return 0;
        double sum = 0;
        for (int i = 0; i < periods; i++) {
            sum += klines[klines.size() - 1 - i].close;
        }
        return sum / periods;
    };

    tech.ma5.clear();
    tech.ma10.clear();
    tech.ma20.clear();
    tech.ma60.clear();
    tech.ma120.clear();
    tech.ma250.clear();

    for (int i = 5; i <= (int)klines.size(); i++) {
        double sum5 = 0;
        for (int j = 0; j < 5 && (i - 1 - j) >= 0; j++) sum5 += klines[i - 1 - j].close;
        tech.ma5.push_back(sum5 / 5);
    }
    for (int i = 10; i <= (int)klines.size(); i++) {
        double sum10 = 0;
        for (int j = 0; j < 10 && (i - 1 - j) >= 0; j++) sum10 += klines[i - 1 - j].close;
        tech.ma10.push_back(sum10 / 10);
    }
    for (int i = 20; i <= (int)klines.size(); i++) {
        double sum20 = 0;
        for (int j = 0; j < 20 && (i - 1 - j) >= 0; j++) sum20 += klines[i - 1 - j].close;
        tech.ma20.push_back(sum20 / 20);
    }
    if ((int)klines.size() >= 60) {
        for (int i = 60; i <= (int)klines.size(); i++) {
            double sum60 = 0;
            for (int j = 0; j < 60 && (i - 1 - j) >= 0; j++) sum60 += klines[i - 1 - j].close;
            tech.ma60.push_back(sum60 / 60);
        }
    }

    double currentPrice = klines.back().close;
    double ma5Current = tech.ma5.empty() ? 0 : tech.ma5.back();
    double ma10Current = tech.ma10.empty() ? 0 : tech.ma10.back();
    double ma20Current = tech.ma20.empty() ? 0 : tech.ma20.back();

    tech.ma_bullish_daily = (ma5Current > ma10Current && ma10Current > ma20Current);
    tech.ma_bullish_weekly = tech.ma_bullish_daily;
    tech.ma_bullish_monthly = tech.ma_bullish_daily;

    if (klines.size() >= 250) {
        double high250 = 0;
        for (size_t i = klines.size() - 250; i < klines.size(); i++) {
            if (klines[i].high > high250) high250 = klines[i].high;
        }
        tech.price_250d_high_pct = high250 > 0 ? (currentPrice / high250 * 100.0) : 50.0;
    } else if (!klines.empty()) {
        double highAll = 0;
        for (const auto& k : klines) {
            if (k.high > highAll) highAll = k.high;
        }
        tech.price_250d_high_pct = highAll > 0 ? (currentPrice / highAll * 100.0) : 50.0;
    }

    double avgVol5 = 0;
    if (klines.size() >= 5) {
        for (size_t i = klines.size() - 5; i < klines.size(); i++) avgVol5 += klines[i].volume;
        avgVol5 /= 5;
    }
    double avgVol20 = 0;
    if (klines.size() >= 20) {
        for (size_t i = klines.size() - 20; i < klines.size(); i++) avgVol20 += klines[i].volume;
        avgVol20 /= 20;
    }
    tech.volume_ratio = avgVol20 > 0 ? (avgVol5 / avgVol20) : 1.0;

    tech.turnover_rate = 1.5;

    return true;
}

bool SinaProvider::getStockList(std::vector<RealtimeQuote>& stocks) {
    if (stock_list_loaded_ && !stock_list_cache_.empty()) {
        for (std::map<std::string, RealtimeQuote>::const_iterator it = stock_list_cache_.begin(); it != stock_list_cache_.end(); ++it) {
            stocks.push_back(it->second);
        }
        return true;
    }

    std::vector<std::string> allCodes;

    auto addCodeRange = [&allCodes](const std::string& prefix, int start, int end) {
        char buf[16];
        for (int i = start; i <= end; i++) {
            sprintf(buf, "%s%06d", prefix.c_str(), i);
            allCodes.push_back(buf);
        }
    };

    addCodeRange("sh", 600000, 605999);
    addCodeRange("sh", 688000, 688999);
    addCodeRange("sz", 0, 4999);
    addCodeRange("sz", 2000, 2999);
    addCodeRange("sz", 300000, 309999);

    std::vector<RealtimeQuote> quotes;
    const size_t batchSize = 100;
    
    for (size_t i = 0; i < allCodes.size(); i += batchSize) {
        size_t end = std::min(i + batchSize, allCodes.size());
        std::vector<std::string> batchCodes(allCodes.begin() + i, allCodes.begin() + end);
        
        std::vector<RealtimeQuote> batchQuotes;
        if (getRealtimeQuotes(batchCodes, batchQuotes)) {
            for (auto& q : batchQuotes) {
                if (q.price > 0 && q.prev_close > 0 && !q.name.empty() && q.name != " ") {
                    quotes.push_back(q);
                }
            }
        }
    }

    for (auto& q : quotes) {
        stock_list_cache_[q.code] = q;
        stocks.push_back(q);
    }

    stock_list_loaded_ = true;
    return !stocks.empty();
}

bool SinaProvider::getFundamentalData(const std::string& code,
                                      FundamentalData& data) {
    data.code = code;

    std::string emCode = toEastMoneyCode(code);
    std::string url = "http://push2.eastmoney.com/api/qt/stock/get?"
                      "secid=" + emCode + "&fields=f57,f58,f162,f163,f167,f168,f170,f171,"
                      "f173,f174,f175,f176,f177,f178,f179,f181,f184";

    std::string resp = httpGet(url, "http://quote.eastmoney.com/");
    if (!resp.empty()) {
        if (parseEastMoneyFundamental(resp, data)) {
            return true;
        }
    }

    std::string sinaCode = toSinaCode(code);
    std::string quoteUrl = "http://hq.sinajs.cn/list=" + sinaCode;
    std::string quoteResp = httpGet(quoteUrl, "https://finance.sina.com.cn");
    if (!quoteResp.empty()) {
        std::vector<RealtimeQuote> quotes;
        std::vector<std::string> codes = { sinaCode };
        if (parseSinaQuote(quoteResp, codes, quotes) && !quotes.empty()) {
            if (!quotes[0].industry.empty()) {
                data.industry = quotes[0].industry;
            }
            if (quotes[0].pe > 0) {
                data.eps = quotes[0].price / quotes[0].pe;
            }
            data.total_share = quotes[0].total_mcap / quotes[0].price * 100.0;
            data.float_share = quotes[0].float_mcap / quotes[0].price * 100.0;
        }
    }

    if (data.industry.empty()) {
        data.industry = "未知行业";
    }
    if (data.roa <= 0 && data.roe > 0) {
        data.roa = data.roe * 0.8;
    }

    return data.code == code;
}

bool SinaProvider::getFundFlow(const std::string& code,
                               FundFlowData& data) {
    data.code = code;

    std::string emCode = toEastMoneyCode(code);
    std::string url = "http://push2.eastmoney.com/api/qt/stock/get?"
                      "secid=" + emCode + "&fields=f62,f184,f66,f69,f72,f75,f78,f81,f84,f87";

    std::string resp = httpGet(url, "http://quote.eastmoney.com/");
    if (!resp.empty()) {
        if (parseEastMoneyFundFlow(resp, data)) {
            return true;
        }
    }

    return data.code == code;
}

bool SinaProvider::getStockFullData(const std::string& code,
                                    StockFullData& data) {
    data.code = code;

    std::vector<RealtimeQuote> quotes;
    std::vector<std::string> codes = { code };
    if (getRealtimeQuotes(codes, quotes) && !quotes.empty()) {
        data.quote = quotes[0];
        data.name = quotes[0].name;
        data.has_data = true;
    }

    FundamentalData fund;
    if (getFundamentalData(code, fund)) {
        data.fundamental = fund;
        if (data.quote.industry.empty()) data.quote.industry = fund.industry;
    }

    FundFlowData flow;
    if (getFundFlow(code, flow)) {
        data.fund_flow = flow;
    }

    std::vector<KlineData> klines;
    if (getKlineData(code, "day", 250, klines)) {
        data.technical.day_klines = klines;
        data.technical.code = code;
        data.technical.turnover_rate = data.quote.turnover_rate > 0 ? data.quote.turnover_rate : 1.5;
        calculateTechnicalIndicators(data.technical);
    }

    if (data.name.find("ST") != std::string::npos) {
        data.is_st = true;
    }

    return data.has_data;
}

bool SinaProvider::parseEastMoneyFundFlow(const std::string& response,
                                         FundFlowData& data) {
    if (response.empty()) return false;

    json_utils::JsonParser parser(response);
    json_utils::JsonValue root;
    if (!parser.parse(root) || root.type != json_utils::JsonValue::OBJECT) {
        return false;
    }

    auto it = root.obj_val.find("data");
    if (it == root.obj_val.end() || it->second.type != json_utils::JsonValue::OBJECT) {
        return false;
    }

    const auto& dataObj = it->second.obj_val;

    auto getNum = [&dataObj](const std::string& key, double& out) -> bool {
        auto itf = dataObj.find(key);
        if (itf != dataObj.end() && itf->second.type == json_utils::JsonValue::NUMBER) {
            out = itf->second.num_val;
            return true;
        }
        return false;
    };

    getNum("f62", data.main_inflow);
    getNum("f184", data.super_inflow);
    getNum("f66", data.big_inflow);
    getNum("f69", data.mid_inflow);
    getNum("f72", data.small_inflow);

    getNum("f84", data.northbound_shares);
    getNum("f87", data.northbound_ratio);

    auto itlb = dataObj.find("f78");
    if (itlb != dataObj.end()) {
        if (itlb->second.type == json_utils::JsonValue::NUMBER) {
            data.has_longhubang = (itlb->second.num_val > 0);
        }
    }

    return true;
}

bool SinaProvider::parseEastMoneyFundamental(const std::string& response,
                                            FundamentalData& data) {
    if (response.empty()) return false;

    json_utils::JsonParser parser(response);
    json_utils::JsonValue root;
    if (!parser.parse(root) || root.type != json_utils::JsonValue::OBJECT) {
        return false;
    }

    auto it = root.obj_val.find("data");
    if (it == root.obj_val.end() || it->second.type != json_utils::JsonValue::OBJECT) {
        return false;
    }

    const auto& dataObj = it->second.obj_val;

    auto getNum = [&dataObj](const std::string& key, double& out) -> bool {
        auto itf = dataObj.find(key);
        if (itf != dataObj.end()) {
            if (itf->second.type == json_utils::JsonValue::NUMBER) {
                out = itf->second.num_val;
                return true;
            } else if (itf->second.type == json_utils::JsonValue::STRING) {
                out = atof(itf->second.str_val.c_str());
                return out != 0.0 || itf->second.str_val == "0";
            }
        }
        return false;
    };

    auto getStr = [&dataObj](const std::string& key, std::string& out) -> bool {
        auto itf = dataObj.find(key);
        if (itf != dataObj.end() && itf->second.type == json_utils::JsonValue::STRING) {
            out = itf->second.str_val;
            return true;
        }
        return false;
    };

    getStr("f58", data.industry);
    getNum("f162", data.total_share);
    getNum("f163", data.float_share);
    getNum("f167", data.roe);
    getNum("f168", data.roa);
    getNum("f170", data.gross_margin);
    getNum("f171", data.net_margin);
    getNum("f173", data.debt_ratio);
    getNum("f174", data.dividend_yield);
    getNum("f175", data.eps);
    getNum("f176", data.pe);
    getNum("f177", data.pb);
    getNum("f178", data.price);
    getNum("f179", data.market_capital);
    getNum("f181", data.float_market_capital);
    getNum("f184", data.turnover_rate);

    return data.code == data.code;
}

}
