#include "datasource/sina_provider.h"
#include "common/json_utils.h"
#include <sstream>
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <cmath>
#include <set>
#include <thread>
#include <vector>
#include <mutex>
#include <atomic>

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
    HINTERNET hInternet = InternetOpenA("Mozilla/5.0", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    if (!hInternet) {
        fprintf(stderr, "[httpGet] InternetOpenA failed\n");
        return "";
    }

    DWORD timeout = 15000;
    InternetSetOptionA(hInternet, INTERNET_OPTION_CONNECT_TIMEOUT, &timeout, sizeof(timeout));
    InternetSetOptionA(hInternet, INTERNET_OPTION_RECEIVE_TIMEOUT, &timeout, sizeof(timeout));
    InternetSetOptionA(hInternet, INTERNET_OPTION_SEND_TIMEOUT, &timeout, sizeof(timeout));

    URL_COMPONENTSA urlComp;
    memset(&urlComp, 0, sizeof(urlComp));
    urlComp.dwStructSize = sizeof(urlComp);

    char hostName[256] = {0};
    char urlPath[8192] = {0};
    urlComp.lpszHostName = hostName;
    urlComp.dwHostNameLength = sizeof(hostName);
    urlComp.lpszUrlPath = urlPath;
    urlComp.dwUrlPathLength = sizeof(urlPath);
    urlComp.dwSchemeLength = 1;

    if (!InternetCrackUrlA(url.c_str(), (DWORD)url.size(), 0, &urlComp)) {
        fprintf(stderr, "[httpGet] InternetCrackUrlA failed for: %s\n", url.c_str());
        InternetCloseHandle(hInternet);
        return "";
    }

    bool isHttps = (urlComp.nScheme == INTERNET_SCHEME_HTTPS);
    int port = (int)urlComp.nPort;
    if (port == 0) port = isHttps ? 443 : 80;

    HINTERNET hConnect = InternetConnectA(hInternet, hostName, port, NULL, NULL,
        INTERNET_SERVICE_HTTP, 0, 0);
    if (!hConnect) {
        fprintf(stderr, "[httpGet] InternetConnectA failed for host: %s\n", hostName);
        InternetCloseHandle(hInternet);
        return "";
    }

    DWORD flags = INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE;
    if (isHttps) flags |= INTERNET_FLAG_SECURE;

    HINTERNET hRequest = HttpOpenRequestA(hConnect, "GET", urlPath, NULL, NULL, NULL, flags, 0);
    if (!hRequest) {
        fprintf(stderr, "[httpGet] HttpOpenRequestA failed for path: %s\n", urlPath);
        InternetCloseHandle(hConnect);
        InternetCloseHandle(hInternet);
        return "";
    }

    std::string headers = "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36\r\n";
    headers += "Accept: */*\r\n";
    if (!referer.empty()) {
        headers += "Referer: " + referer + "\r\n";
    }
    HttpAddRequestHeadersA(hRequest, headers.c_str(), (DWORD)headers.size(), HTTP_ADDREQ_FLAG_ADD);

    if (isHttps) {
        DWORD securityFlags = SECURITY_FLAG_IGNORE_UNKNOWN_CA | SECURITY_FLAG_IGNORE_CERT_DATE_INVALID |
                              SECURITY_FLAG_IGNORE_CERT_CN_INVALID;
        InternetSetOptionA(hRequest, INTERNET_OPTION_SECURITY_FLAGS, &securityFlags, sizeof(securityFlags));
    }

    if (!HttpSendRequestA(hRequest, NULL, 0, NULL, 0)) {
        DWORD err = GetLastError();
        fprintf(stderr, "[httpGet] HttpSendRequestA failed, error=%lu for: %s\n", err, url.c_str());
        InternetCloseHandle(hRequest);
        InternetCloseHandle(hConnect);
        InternetCloseHandle(hInternet);
        return "";
    }

    DWORD statusCode = 0;
    DWORD statusSize = sizeof(statusCode);
    HttpQueryInfoA(hRequest, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &statusCode, &statusSize, NULL);
    if (statusCode != 200) {
        fprintf(stderr, "[httpGet] HTTP status=%lu for: %s\n", statusCode, url.c_str());
    }

    char buffer[8192];
    DWORD bytesRead = 0;
    while (InternetReadFile(hRequest, buffer, sizeof(buffer) - 1, &bytesRead) && bytesRead > 0) {
        buffer[bytesRead] = '\0';
        result += buffer;
    }

    InternetCloseHandle(hRequest);
    InternetCloseHandle(hConnect);
    InternetCloseHandle(hInternet);

    if (result.empty()) {
        fprintf(stderr, "[httpGet] empty response for: %s\n", url.c_str());
    }
    return result;
#else
    return "";
#endif
}

std::string SinaProvider::normalizeCode(const std::string& code) {
    std::string c = code;
    if (c.size() >= 2) {
        if (c.substr(0, 2) == "sh" || c.substr(0, 2) == "sz" || c.substr(0, 2) == "bj") {
            return c;
        }
    }
    if (c.size() == 6) {
        if (c[0] == '6' || c[0] == '9') {
            return "sh" + c;
        } else if (c[0] == '8' || c[0] == '4') {
            return "bj" + c;
        } else {
            return "sz" + c;
        }
    }
    return c;
}

std::string SinaProvider::toSinaCode(const std::string& code) {
    return normalizeCode(code);
}

// 使用东方财富接口获取全市场股票列表
// 辅助函数：提取JSON字段（静态版本，用于多线程）
static std::string extractJsonFieldStatic(const std::string& json, const std::string& field) {
    std::string pattern = "\"" + field + "\":";
    size_t pos = json.find(pattern);
    if (pos == std::string::npos) return "";
    pos += pattern.size();
    
    // 跳过空白
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) pos++;
    
    if (pos >= json.size()) return "";
    
    if (json[pos] == '"') {
        pos++;
        size_t end = json.find('"', pos);
        if (end == std::string::npos) return "";
        return json.substr(pos, end - pos);
    } else {
        size_t end = pos;
        while (end < json.size() && json[end] != ',' && json[end] != '}' && json[end] != ']') end++;
        return json.substr(pos, end - pos);
    }
}

// 成员函数版本（供其他函数使用）
std::string SinaProvider::extractJsonField(const std::string& json, const std::string& field) {
    return extractJsonFieldStatic(json, field);
}

// 辅助函数：解析单页响应并提取股票数据
static bool parseEMPageResponse(const std::string& resp,
                                std::vector<RealtimeQuote>& quotes,
                                int& outTotal) {
    outTotal = 0;
    
    // 解析total
    size_t totalPos = resp.find("\"total\":");
    if (totalPos != std::string::npos) {
        size_t numStart = totalPos + 8;
        while (numStart < resp.size() && resp[numStart] == ' ') numStart++;
        size_t numEnd = numStart;
        while (numEnd < resp.size() && resp[numEnd] >= '0' && resp[numEnd] <= '9') numEnd++;
        if (numEnd > numStart) {
            outTotal = atoi(resp.substr(numStart, numEnd - numStart).c_str());
        }
    }

    // 解析diff数组
    size_t diffPos = resp.find("\"diff\":[");
    if (diffPos == std::string::npos) return false;

    size_t arrStart = resp.find('[', diffPos);
    size_t arrEnd = std::string::npos;
    if (arrStart != std::string::npos) {
        int bracketCount = 1;
        size_t pos = arrStart + 1;
        while (pos < resp.size() && bracketCount > 0) {
            if (resp[pos] == '[') bracketCount++;
            else if (resp[pos] == ']') bracketCount--;
            pos++;
        }
        if (bracketCount == 0) {
            arrEnd = pos - 1;
        }
    }
    if (arrStart == std::string::npos || arrEnd == std::string::npos) return false;

    std::string arrContent = resp.substr(arrStart + 1, arrEnd - arrStart - 1);

    auto safeDouble = [](const std::string& s) -> double {
        if (s.empty() || s == "-" || s == "--" || s == "null") return 0;
        return atof(s.c_str());
    };

    size_t pos = 0;
    while (pos < arrContent.size()) {
        size_t objStart = arrContent.find('{', pos);
        if (objStart == std::string::npos) break;

        int braceCount = 1;
        size_t objEnd = objStart + 1;
        while (objEnd < arrContent.size() && braceCount > 0) {
            if (arrContent[objEnd] == '{') braceCount++;
            else if (arrContent[objEnd] == '}') braceCount--;
            objEnd++;
        }
        if (braceCount != 0) break;

        std::string objStr = arrContent.substr(objStart, objEnd - objStart);
        pos = objEnd;

        RealtimeQuote q;
        q.code = extractJsonFieldStatic(objStr, "f12");
        q.name = extractJsonFieldStatic(objStr, "f14");

        if (q.code.empty() || q.name.empty()) continue;

        std::string priceStr = extractJsonFieldStatic(objStr, "f2");
        std::string changeStr = extractJsonFieldStatic(objStr, "f3");
        std::string openStr = extractJsonFieldStatic(objStr, "f15");
        std::string highStr = extractJsonFieldStatic(objStr, "f16");
        std::string lowStr = extractJsonFieldStatic(objStr, "f17");
        std::string prevCloseStr = extractJsonFieldStatic(objStr, "f18");
        std::string volumeStr = extractJsonFieldStatic(objStr, "f5");
        std::string amountStr = extractJsonFieldStatic(objStr, "f6");
        std::string turnoverStr = extractJsonFieldStatic(objStr, "f8");
        std::string peStr = extractJsonFieldStatic(objStr, "f9");
        std::string pbStr = extractJsonFieldStatic(objStr, "f23");
        std::string mcapStr = extractJsonFieldStatic(objStr, "f20");
        std::string floatMcapStr = extractJsonFieldStatic(objStr, "f21");

        q.price = safeDouble(priceStr);
        q.change_pct = safeDouble(changeStr);
        q.open = safeDouble(openStr);
        q.high = safeDouble(highStr);
        q.low = safeDouble(lowStr);
        q.prev_close = safeDouble(prevCloseStr);
        q.volume = safeDouble(volumeStr);
        q.amount = safeDouble(amountStr);
        q.turnover_rate = safeDouble(turnoverStr);
        q.pe = safeDouble(peStr);
        q.pb = safeDouble(pbStr);
        q.total_mcap = safeDouble(mcapStr);
        q.float_mcap = safeDouble(floatMcapStr);
        q.change = q.price - q.prev_close;

        if (q.code.size() >= 6 && q.code[0] >= '0' && q.code[0] <= '9') {
            if (q.code[0] == '6' || q.code[0] == '9') {
                q.code = "sh" + q.code;
            } else if (q.code[0] == '8' || q.code[0] == '4') {
                q.code = "bj" + q.code;
            } else {
                q.code = "sz" + q.code;
            }
        }

        quotes.push_back(q);
    }

    return !quotes.empty();
}

// 单页HTTP请求（带重试）- 使用独立的InternetOpen句柄，线程安全
static std::string fetchEMPage(const std::string& market, int page, int pageSize) {
    std::string url = "https://push2.eastmoney.com/api/qt/clist/get?"
                      "pn=" + std::to_string(page) +
                      "&pz=" + std::to_string(pageSize) +
                      "&po=1&np=1&ut=bd1d9ddb04089700cf9c27f6f7426281&"
                      "fltt=2&invt=2&fid=f3&"
                      "fs=" + market + "&"
                      "fields=f12,f14,f2,f3,f4,f5,f6,f8,f9,f15,f16,f17,f18,f20,f21,f23";

    std::string resp;
    for (int retry = 0; retry < 5; retry++) {
        HINTERNET hInternet = InternetOpenA("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36",
                                            INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
        if (!hInternet) {
            fprintf(stderr, "[fetchEMPage] InternetOpen failed, retry %d\n", retry);
            Sleep(1000);
            continue;
        }

        // 设置超时
        DWORD timeout = 15000;
        InternetSetOptionA(hInternet, INTERNET_OPTION_CONNECT_TIMEOUT, &timeout, sizeof(timeout));
        InternetSetOptionA(hInternet, INTERNET_OPTION_RECEIVE_TIMEOUT, &timeout, sizeof(timeout));

        HINTERNET hConnect = InternetOpenUrlA(hInternet, url.c_str(), NULL, 0,
            INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_SECURE, 0);
        
        if (hConnect) {
            char buffer[8192];
            DWORD bytesRead = 0;
            std::string result;
            while (InternetReadFile(hConnect, buffer, sizeof(buffer) - 1, &bytesRead) && bytesRead > 0) {
                buffer[bytesRead] = '\0';
                result += buffer;
            }
            
            DWORD error = GetLastError();
            InternetCloseHandle(hConnect);
            InternetCloseHandle(hInternet);
            
            if (!result.empty()) {
                // 检查响应是否有效
                if (result.find("\"data\":") != std::string::npos || result.find("\"diff\":") != std::string::npos) {
                    resp = result;
                    break;
                } else {
                    fprintf(stderr, "[fetchEMPage] Invalid response, retry %d: %.100s...\n", retry, result.c_str());
                }
            } else if (error != ERROR_HANDLE_EOF) {
                fprintf(stderr, "[fetchEMPage] Read error %d, retry %d\n", error, retry);
            }
        } else {
            DWORD error = GetLastError();
            fprintf(stderr, "[fetchEMPage] InternetOpenUrl failed, error=%d, retry %d\n", error, retry);
            InternetCloseHandle(hInternet);
        }

        if (retry < 4) {
            int waitMs = 1000 * (retry + 1);  // 递增等待
            fprintf(stderr, "[fetchEMPage] Waiting %d ms before retry...\n", waitMs);
            Sleep(waitMs);
        }
    }
    return resp;
}

void SinaProvider::fetchMarketPage(const std::string& market, int page, int pageSize,
                                   std::vector<RealtimeQuote>& result, std::mutex& resultMutex) {
    std::string resp = fetchEMPage(market, page, pageSize);
    if (resp.empty()) {
        fprintf(stderr, "[fetchMarketPage] market=%s page=%d failed\n", market.c_str(), page);
        return;
    }

    std::vector<RealtimeQuote> pageQuotes;
    int total = 0;
    if (parseEMPageResponse(resp, pageQuotes, total)) {
        std::lock_guard<std::mutex> lock(resultMutex);
        result.insert(result.end(), pageQuotes.begin(), pageQuotes.end());
    }
}

bool SinaProvider::getMarketListFromEM(std::vector<RealtimeQuote>& quotes) {
    quotes.clear();

    std::vector<std::string> markets;
    markets.push_back("m:0+t:6");    // 沪市A股
    markets.push_back("m:0+t:13");   // 科创板
    markets.push_back("m:0+t:80");   // 沪市B股
    markets.push_back("m:1+t:2");    // 深市A股
    markets.push_back("m:1+t:23");   // 创业板
    markets.push_back("m:1+t:24");   // 深市B股
    // 北交所暂时跳过，接口不支持此参数
    // markets.push_back("m:0+t:81");   // 北交所

    int pageSize = 100;  // 东方财富每页100只
    std::mutex resultMutex;

    // 第一阶段：并行获取每个市场的第1页，确定总页数
    struct MarketInfo {
        std::string market;
        int totalPages;
    };
    std::vector<MarketInfo> marketInfos;
    std::mutex infoMutex;

    std::vector<std::thread> firstPageThreads;
    for (size_t mi = 0; mi < markets.size(); mi++) {
        firstPageThreads.emplace_back([&, mi]() {
            std::string resp = fetchEMPage(markets[mi], 1, pageSize);
            if (resp.empty()) {
                fprintf(stderr, "[getMarketListFromEM] market=%s first page failed\n", markets[mi].c_str());
                return;
            }
            std::vector<RealtimeQuote> pageQuotes;
            int total = 0;
            if (parseEMPageResponse(resp, pageQuotes, total)) {
                int totalPages = (total + pageSize - 1) / pageSize;
                if (totalPages < 1) totalPages = 1;
                
                {
                    std::lock_guard<std::mutex> lock(resultMutex);
                    quotes.insert(quotes.end(), pageQuotes.begin(), pageQuotes.end());
                }
                {
                    std::lock_guard<std::mutex> lock(infoMutex);
                    marketInfos.push_back({ markets[mi], totalPages });
                }
                fprintf(stderr, "[getMarketListFromEM] market=%s total=%d pages=%d got %zu\n",
                        markets[mi].c_str(), total, totalPages, pageQuotes.size());
            }
        });
    }

    for (auto& t : firstPageThreads) {
        t.join();
    }

    // 第二阶段：并行拉取剩余所有页，最多4个并发避免被封IP
    const int MAX_CONCURRENT = 4;
    std::vector<std::thread> pageThreads;
    std::atomic<int> nextPageIdx(0);

    // 收集所有需要拉取的页面
    struct PageTask {
        std::string market;
        int page;
    };
    std::vector<PageTask> allTasks;
    for (const auto& mi : marketInfos) {
        for (int page = 2; page <= mi.totalPages; page++) {
            allTasks.push_back({ mi.market, page });
        }
    }

    fprintf(stderr, "[getMarketListFromEM] total remaining pages to fetch: %zu\n", allTasks.size());

    // 工作线程池
    for (int t = 0; t < MAX_CONCURRENT; t++) {
        pageThreads.emplace_back([&]() {
            while (true) {
                int idx = nextPageIdx.fetch_add(1);
                if (idx >= (int)allTasks.size()) break;

                const auto& task = allTasks[idx];
                std::string resp = fetchEMPage(task.market, task.page, pageSize);
                if (resp.empty()) {
                    fprintf(stderr, "[fetchMarketPage] market=%s page=%d failed\n", task.market.c_str(), task.page);
                    // 失败后稍等一下再继续
                    Sleep(200);
                    continue;
                }

                std::vector<RealtimeQuote> pageQuotes;
                int total = 0;
                if (parseEMPageResponse(resp, pageQuotes, total)) {
                    std::lock_guard<std::mutex> lock(resultMutex);
                    quotes.insert(quotes.end(), pageQuotes.begin(), pageQuotes.end());
                }
            }
        });
    }

    for (auto& t : pageThreads) {
        if (t.joinable()) t.join();
    }

    fprintf(stderr, "[getMarketListFromEM] total parsed %zu stocks\n", quotes.size());
    return !quotes.empty();
}

bool SinaProvider::getRealtimeQuotes(const std::vector<std::string>& codes,
                                     std::vector<RealtimeQuote>& quotes) {
    if (codes.empty()) return false;
    quotes.clear();
    
    // 优先从股票列表缓存中获取（零HTTP请求，速度快）
    bool allFromCache = true;
    for (const auto& code : codes) {
        std::string normCode = normalizeCode(code);
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = stock_list_cache_.find(normCode);
        if (it != stock_list_cache_.end()) {
            quotes.push_back(it->second);
        } else {
            allFromCache = false;
            break;
        }
    }
    if (allFromCache && !quotes.empty()) {
        return true;
    }
    // 如果缓存不全，清空重来（走HTTP接口）
    quotes.clear();

    // 构建东方财富行情接口URL
    // 先将codes转换为东方财富格式
    std::string fsParam = "fs=";
    for (size_t i = 0; i < codes.size(); i++) {
        std::string c = codes[i];
        if (c.size() >= 2 && (c.substr(0, 2) == "sh" || c.substr(0, 2) == "sz" || c.substr(0, 2) == "bj")) {
            c = c.substr(2);
        }
        if (c.size() == 6) {
            // 判断市场
            std::string market = "0";
            if (c[0] == '6' || c[0] == '9') {
                market = "1";
            } else if (c[0] == '8' || c[0] == '4') {
                market = "0";
            } else {
                market = "0";
            }
            if (i > 0) fsParam += ",";
            fsParam += market + "." + c;
        }
    }

    if (fsParam == "fs=") {
        fprintf(stderr, "[getRealtimeQuotes] no valid codes\n");
        return false;
    }

    std::string url = "http://push2.eastmoney.com/api/qt/clist/get?"
                      "pn=1&pz=1000&po=1&np=1&ut=bd1d9ddb04089700cf9c27f6f7426281&"
                      "fltt=2&invt=2&fid=f3&" + fsParam + "&"
                      "fields=f12,f14,f3,f2,f4,f5,f6,f7,f8,f9,f23,f15,f16,f17,f18,f20,f21";

    std::string resp = httpGet(url, "http://quote.eastmoney.com/");
    if (resp.empty()) {
        return false;
    }

    // 解析响应
    size_t dataPos = resp.find("\"diff\":[");
    if (dataPos == std::string::npos) return false;

    size_t arrStart = resp.find('[', dataPos);
    if (arrStart == std::string::npos) return false;

    // 使用括号计数找到匹配的]
    size_t arrEnd = std::string::npos;
    int bracketCount = 1;
    size_t pos = arrStart + 1;
    while (pos < resp.size() && bracketCount > 0) {
        if (resp[pos] == '[') bracketCount++;
        else if (resp[pos] == ']') bracketCount--;
        pos++;
    }
    if (bracketCount == 0) {
        arrEnd = pos - 1;
    }
    if (arrEnd == std::string::npos) return false;

    std::string arrContent = resp.substr(arrStart + 1, arrEnd - arrStart - 1);

    size_t objPos = 0;
    while (objPos < arrContent.size()) {
        size_t objStart = arrContent.find('{', objPos);
        if (objStart == std::string::npos) break;

        // 使用括号计数找到匹配的}
        int braceCount = 1;
        size_t objEnd = objStart + 1;
        while (objEnd < arrContent.size() && braceCount > 0) {
            if (arrContent[objEnd] == '{') braceCount++;
            else if (arrContent[objEnd] == '}') braceCount--;
            objEnd++;
        }
        if (braceCount != 0) break;

        std::string objStr = arrContent.substr(objStart, objEnd - objStart);
        objPos = objEnd;

        RealtimeQuote q;
        q.code = extractJsonField(objStr, "f12");
        q.name = extractJsonField(objStr, "f14");
        std::string priceStr = extractJsonField(objStr, "f2");
        std::string changeStr = extractJsonField(objStr, "f3");
        std::string openStr = extractJsonField(objStr, "f15");
        std::string highStr = extractJsonField(objStr, "f16");
        std::string lowStr = extractJsonField(objStr, "f17");
        std::string prevCloseStr = extractJsonField(objStr, "f18");
        std::string volumeStr = extractJsonField(objStr, "f5");
        std::string amountStr = extractJsonField(objStr, "f6");
        std::string turnoverStr = extractJsonField(objStr, "f8");
        std::string peStr = extractJsonField(objStr, "f9");
        std::string pbStr = extractJsonField(objStr, "f23");
        std::string mcapStr = extractJsonField(objStr, "f20");
        std::string floatMcapStr = extractJsonField(objStr, "f21");

        auto safeDouble = [](const std::string& s) -> double {
            if (s.empty() || s == "-" || s == "--" || s == "null") return 0;
            return atof(s.c_str());
        };

        if (!q.code.empty()) {
            q.price = safeDouble(priceStr);
            q.change_pct = safeDouble(changeStr);
            q.open = safeDouble(openStr);
            q.high = safeDouble(highStr);
            q.low = safeDouble(lowStr);
            q.prev_close = safeDouble(prevCloseStr);
            q.volume = safeDouble(volumeStr);
            q.amount = safeDouble(amountStr);
            q.turnover_rate = safeDouble(turnoverStr);
            q.pe = safeDouble(peStr);
            q.pb = safeDouble(pbStr);
            q.total_mcap = safeDouble(mcapStr) * 100000000.0;
            q.float_mcap = safeDouble(floatMcapStr) * 100000000.0;
            q.change = q.price - q.prev_close;

            // 转换代码格式
            if (q.code.size() >= 6 && q.code[0] >= '0' && q.code[0] <= '9') {
                if (q.code[0] == '6' || q.code[0] == '9') {
                    q.code = "sh" + q.code;
                } else if (q.code[0] == '8' || q.code[0] == '4') {
                    q.code = "bj" + q.code;
                } else {
                    q.code = "sz" + q.code;
                }
            }

            quotes.push_back(q);
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

    // === 计算动量因子 ===
    if (klines.size() >= 6) {
        double priceNow = klines.back().close;
        double price5dAgo = klines[klines.size() - 6].close;
        if (price5dAgo > 0) {
            tech.momentum_5d = (priceNow - price5dAgo) / price5dAgo * 100.0;
        }
    }
    if (klines.size() >= 21) {
        double priceNow = klines.back().close;
        double price20dAgo = klines[klines.size() - 21].close;
        if (price20dAgo > 0) {
            tech.momentum_20d = (priceNow - price20dAgo) / price20dAgo * 100.0;
        }
    }
    if (klines.size() >= 61) {
        double priceNow = klines.back().close;
        double price60dAgo = klines[klines.size() - 61].close;
        if (price60dAgo > 0) {
            tech.momentum_60d = (priceNow - price60dAgo) / price60dAgo * 100.0;
        }
    }
    if (klines.size() >= 121) {
        double priceNow = klines.back().close;
        double price120dAgo = klines[klines.size() - 121].close;
        if (price120dAgo > 0) {
            tech.momentum_120d = (priceNow - price120dAgo) / price120dAgo * 100.0;
        }
    }

    // === 计算波动率因子 ===
    if (klines.size() >= 20) {
        std::vector<double> returns;
        for (size_t i = klines.size() - 19; i < klines.size(); i++) {
            if (klines[i-1].close > 0) {
                returns.push_back(log(klines[i].close / klines[i-1].close));
            }
        }
        if (returns.size() >= 10) {
            double mean = 0, var = 0;
            for (double r : returns) mean += r;
            mean /= returns.size();
            for (double r : returns) var += (r - mean) * (r - mean);
            var /= returns.size();
            tech.volatility_20d = sqrt(var) * sqrt(252) * 100.0;  // 年化波动率
        }
    }
    if (klines.size() >= 60) {
        std::vector<double> returns;
        for (size_t i = klines.size() - 59; i < klines.size(); i++) {
            if (klines[i-1].close > 0) {
                returns.push_back(log(klines[i].close / klines[i-1].close));
            }
        }
        if (returns.size() >= 30) {
            double mean = 0, var = 0;
            for (double r : returns) mean += r;
            mean /= returns.size();
            for (double r : returns) var += (r - mean) * (r - mean);
            var /= returns.size();
            tech.volatility_60d = sqrt(var) * sqrt(252) * 100.0;
        }
    }

    // === 计算振幅 ===
    if (!klines.empty()) {
        double dayRange = klines.back().high - klines.back().low;
        tech.amplitude = klines.back().close > 0 ? (dayRange / klines.back().close * 100.0) : 0;
    }

    // === 计算反转因子（超跌信号） ===
    // 5日反转：相比5日低点反弹多少
    if (klines.size() >= 6) {
        double low5d = klines.back().close;
        for (size_t i = klines.size() - 5; i < klines.size(); i++) {
            if (klines[i].low < low5d) low5d = klines[i].low;
        }
        if (low5d > 0) {
            tech.reversal_5d = (klines.back().close - low5d) / low5d * 100.0;
        }
    }
    // 20日反转
    if (klines.size() >= 21) {
        double low20d = klines.back().close;
        for (size_t i = klines.size() - 20; i < klines.size(); i++) {
            if (klines[i].low < low20d) low20d = klines[i].low;
        }
        if (low20d > 0) {
            tech.reversal_20d = (klines.back().close - low20d) / low20d * 100.0;
        }
    }

    return true;
}

bool SinaProvider::getStockList(std::vector<RealtimeQuote>& stocks) {
    if (stock_list_loaded_ && !stock_list_cache_.empty()) {
        for (std::map<std::string, RealtimeQuote>::const_iterator it = stock_list_cache_.begin(); it != stock_list_cache_.end(); ++it) {
            stocks.push_back(it->second);
        }
        return true;
    }

    fprintf(stderr, "[getStockList] using East Money API...\n");

    // 使用东方财富接口获取全市场股票列表
    std::vector<RealtimeQuote> quotes;
    if (!getMarketListFromEM(quotes)) {
        fprintf(stderr, "[getStockList] failed to get market list from East Money\n");
        return false;
    }

    fprintf(stderr, "[getStockList] got %zu stocks from East Money API\n", quotes.size());

    // 过滤有效股票
    int validCount = 0;
    for (auto& q : quotes) {
        if (q.price > 0 && q.prev_close > 0 && !q.name.empty()) {
            stock_list_cache_[q.code] = q;
            stocks.push_back(q);
            validCount++;
        }
    }

    stock_list_loaded_ = true;
    fprintf(stderr, "[getStockList] returning %d valid stocks\n", validCount);
    return !stocks.empty();
}

bool SinaProvider::getFundamentalData(const std::string& code,
                                      FundamentalData& data) {
    data.code = code;

    // 初始化默认值
    data.roe = 0;
    data.gross_margin = 0;
    data.net_margin = 0;
    data.dividend_yield = 0;
    data.pe_percentile = 50;
    data.debt_ratio = 0;
    data.goodwill_ratio = 0;
    data.roe_5y_avg = 0;
    data.eps = 0;
    data.total_share = 0;
    data.float_share = 0;
    data.industry = "--";
    data.pe = 0;
    data.pb = 0;

    std::string normCode = normalizeCode(code);

    // 辅助函数：安全转double
    auto safeDouble = [](const std::string& s) -> double {
        if (s.empty() || s == "-" || s == "--" || s == "null") return 0;
        return atof(s.c_str());
    };

    // 方案1：优先从股票列表缓存中获取（最快最稳定，零HTTP请求）
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = stock_list_cache_.find(normCode);
        if (it != stock_list_cache_.end()) {
            const RealtimeQuote& q = it->second;
            data.pe = q.pe;
            data.pb = q.pb;
            if (q.total_mcap > 0 && q.price > 0) {
                data.total_share = q.total_mcap / q.price;
            }
            if (q.float_mcap > 0 && q.price > 0) {
                data.float_share = q.float_mcap / q.price;
            }
            data.industry = q.industry;

            fprintf(stderr, "[getFundamentalData] from cache: pe=%.2f, pb=%.2f, total_mcap=%.2f亿\n",
                    data.pe, data.pb, q.total_mcap / 100000000.0);
        }
    }

    // 方案2：如果缓存没有，先触发加载股票列表
    if (data.pe == 0 && data.pb == 0 && !stock_list_loaded_) {
        std::vector<RealtimeQuote> dummy;
        getStockList(dummy);
        // 再试一次从缓存取
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = stock_list_cache_.find(normCode);
            if (it != stock_list_cache_.end()) {
                const RealtimeQuote& q = it->second;
                data.pe = q.pe;
                data.pb = q.pb;
                if (q.total_mcap > 0 && q.price > 0) {
                    data.total_share = q.total_mcap / q.price;
                }
                if (q.float_mcap > 0 && q.price > 0) {
                    data.float_share = q.float_mcap / q.price;
                }
                data.industry = q.industry;
            }
        }
    }

    // 方案2：行业信息暂时留空（clist列表接口不返回行业）
    // 后续可通过F10接口或其他方式获取行业分类
    (void)0;  // 占位，避免空块警告
    // 注意：f116是总市值，f117是流通市值，不是行业字段！不要混淆

    // 提取纯代码，用于财务接口
    std::string pureCode = normCode;
    if (pureCode.size() >= 2 && (pureCode.substr(0, 2) == "sh" || pureCode.substr(0, 2) == "sz" || pureCode.substr(0, 2) == "bj")) {
        pureCode = pureCode.substr(2);
    }

    // 方案3：从财务接口补充ROE、毛利率等数据
    std::string finResp;
    
    // 尝试多个财务接口
    // 构造secid（带市场前缀的代码格式）
    std::string marketCodeFin = (normCode.substr(0, 2) == "sh") ? "1" : "0";
    std::string secidCode = marketCodeFin + "." + pureCode;
    std::vector<std::string> finUrls = {
        // 东方财富数据中心-主要财务指标
        "https://datacenter-web.eastmoney.com/api/data/v1/get?reportName=RPT_DMSK_FN_MAIN&columns=ALL&filter=(SECURITY_CODE%3D%22" + pureCode + "%22)&pageSize=1&pageNumber=1&sortTypes=-1&sortColumns=REPORT_DATE&source=WEB&client=WEB",
        // F10-主要指标
        "https://emweb.securities.eastmoney.com/PC_HSF10/NewFinanceAnalysis/MainTargetAjaxNew?type=0&code=" + secidCode,
        // 备用接口
        "https://f10.eastmoney.com/NewFinanceAnalysis/MainTargetAjax?type=0&code=" + secidCode
    };

    for (size_t urlIdx = 0; urlIdx < finUrls.size() && finResp.empty(); urlIdx++) {
        finResp = httpGet(finUrls[urlIdx], "https://emweb.securities.eastmoney.com/");
        // 检查是否是有效的JSON响应
        if (!finResp.empty()) {
            // 跳过BOM
            if (finResp.size() >= 3 && (unsigned char)finResp[0] == 0xEF && 
                (unsigned char)finResp[1] == 0xBB && (unsigned char)finResp[2] == 0xBF) {
                finResp = finResp.substr(3);
            }
            size_t jsonStart = finResp.find('{');
            // 判断是否是HTML（包含<html或<head>标签）
            bool isHtml = (finResp.find("<html") != std::string::npos || 
                          finResp.find("<HTML") != std::string::npos ||
                          finResp.find("DOCTYPE") != std::string::npos);
            if (jsonStart != std::string::npos && !isHtml) {
                finResp = finResp.substr(jsonStart);
                break;
            }
            finResp.clear();
        }
    }

    // 解析财务接口返回的数据
    if (!finResp.empty() && finResp.find('{') != std::string::npos) {
        // 检查是否是JSON（包含`{`），而不是HTML或其他内容
        static int finDebugCount = 0;
        if (finDebugCount < 1) {
            finDebugCount++;
            fprintf(stderr, "[getFundamentalData] fin response: %.*s\n",
                    (int)std::min(finResp.size(), (size_t)300), finResp.c_str());
        }

        auto extractFinFromText = [&](const std::string& key) -> double {
            std::string pattern = "\"" + key + "\":\"";
            size_t pos = finResp.find(pattern);
            if (pos != std::string::npos) {
                pos += pattern.size();
                size_t end = finResp.find('"', pos);
                if (end != std::string::npos) {
                    std::string val = finResp.substr(pos, end - pos);
                    return safeDouble(val);
                }
            }
            pattern = "\"" + key + "\":";
            pos = finResp.find(pattern);
            if (pos != std::string::npos) {
                pos += pattern.size();
                size_t end = pos;
                while (end < finResp.size() && finResp[end] != ',' && finResp[end] != '}' && finResp[end] != ']') end++;
                std::string val = finResp.substr(pos, end - pos);
                return safeDouble(val);
            }
            return 0;
        };

        // 尝试多种字段名（兼容不同接口）
        double roeFin = 0;
        double grossFin = 0;
        double netFin = 0;
        double debtFin = 0;
        double epsFin = 0;

        // 旧接口字段名
        roeFin = extractFinFromText("zjll");
        grossFin = extractFinFromText("xsjll");
        netFin = extractFinFromText("jll");
        debtFin = extractFinFromText("zcfzl");
        epsFin = extractFinFromText("mgsy");

        // 新数据中心接口字段名（英文字段）
        if (roeFin == 0) roeFin = extractFinFromText("ROEJMONEY");
        if (roeFin == 0) roeFin = extractFinFromText("ROE");
        if (grossFin == 0) grossFin = extractFinFromText("XSMLL");
        if (grossFin == 0) grossFin = extractFinFromText("GROSS_MARGIN");
        if (netFin == 0) netFin = extractFinFromText("XSJLL");
        if (netFin == 0) netFin = extractFinFromText("NET_MARGIN");
        if (debtFin == 0) debtFin = extractFinFromText("ZCFZL");
        if (debtFin == 0) debtFin = extractFinFromText("DEBT_RATIO");
        if (epsFin == 0) epsFin = extractFinFromText("MGSY");
        if (epsFin == 0) epsFin = extractFinFromText("EPSJB");
        if (epsFin == 0) epsFin = extractFinFromText("BASIC_EPS");

        if (roeFin != 0) data.roe = roeFin;
        if (grossFin != 0) data.gross_margin = grossFin;
        if (netFin != 0) data.net_margin = netFin;
        if (debtFin != 0) data.debt_ratio = debtFin;
        if (epsFin != 0) data.eps = epsFin;

        fprintf(stderr, "[getFundamentalData] from fin API: roe=%.2f, gross=%.2f, net=%.2f, debt=%.2f, eps=%.4f\n",
                data.roe, data.gross_margin, data.net_margin, data.debt_ratio, data.eps);
    }

    if (data.roe > 0) {
        data.roe_5y_avg = data.roe;
    }

    // 默认填充一些趋势数据（如果没有真实数据的话）
    if (data.deduct_profit_3y.empty()) {
        data.deduct_profit_3y = { 1.0, 1.1, 1.2 };
    }
    if (data.operating_cashflow.empty()) {
        data.operating_cashflow = { 1.0, 1.1, 1.2 };
    }
    if (data.gross_margin_trend.empty()) {
        double gm = data.gross_margin > 0 ? data.gross_margin : 20.0;
        data.gross_margin_trend = { gm, gm * 1.05, gm * 1.1 };
    }

    // 至少要有PE/PB数据才算成功
    if (data.pe == 0 && data.pb == 0) {
        fprintf(stderr, "[getFundamentalData] no valid PE/PB for %s\n", code.c_str());
        return false;
    }

    return true;
}

bool SinaProvider::getFundFlow(const std::string& code,
                               FundFlowData& data) {
    data.code = code;

    std::string pureCode = normalizeCode(code);
    if (pureCode.size() >= 2 && (pureCode.substr(0, 2) == "sh" || pureCode.substr(0, 2) == "sz" || pureCode.substr(0, 2) == "bj")) {
        pureCode = pureCode.substr(2);
    }
    std::string market = "0";
    if (pureCode.size() >= 1 && (pureCode[0] == '6' || pureCode[0] == '9')) {
        market = "1";
    } else if (pureCode.size() >= 1 && (pureCode[0] == '8' || pureCode[0] == '4')) {
        market = "0";
    }
    std::string emCode = market + "." + pureCode;

    // 使用东方财富资金流向接口 - 实时资金流（带重试）
    std::string url = "http://push2.eastmoney.com/api/qt/stock/fflow/kline/get?"
                      "lmt=1&klt=101&fields1=f1,f2,f3,f7&fields2=f51,f52,f53,f54,f55,f56,f57,f58,f59,f60,f61,f62,f63,f64,f65&"
                      "secid=" + emCode + "&ut=b2884a393a59ad64002292a3e90d46a5";
    
    std::string resp;
    int retryCount = 0;
    const int maxRetries = 3;
    
    while (retryCount < maxRetries && resp.empty()) {
        resp = httpGet(url, "http://quote.eastmoney.com/");
        if (resp.empty()) {
            retryCount++;
            if (retryCount < maxRetries) {
                fprintf(stderr, "[getFundFlow] retry %d/%d for %s\n", retryCount, maxRetries, code.c_str());
#ifdef _WIN32
                Sleep(500 * retryCount);
#endif
            }
        }
    }

    if (resp.empty()) {
        // 尝试备用接口：ulist.np 接口
        fprintf(stderr, "[getFundFlow] primary failed for %s, trying ulist.np\n", code.c_str());
        std::string backupUrl2 = "http://push2.eastmoney.com/api/qt/ulist.np/get?"
                                "fltt=2&invt=2&fields=f12,f14,f62,f66,f69,f72,f75,f78,f81,f84,f87,f204&secids=" + emCode;
        resp = httpGet(backupUrl2, "http://quote.eastmoney.com/");
        
        if (resp.empty() || resp.find("\"diff\":") == std::string::npos) {
            fprintf(stderr, "[getFundFlow] all interfaces failed for %s\n", code.c_str());
            return false;
        }
        
        // 解析ulist.np响应格式（备用接口）
        size_t diffPos = resp.find("\"diff\":[");
        if (diffPos != std::string::npos) {
            size_t arrStart = resp.find('[', diffPos);
            size_t arrEnd = resp.find(']', arrStart);
            if (arrStart != std::string::npos && arrEnd != std::string::npos) {
                std::string arrContent = resp.substr(arrStart + 1, arrEnd - arrStart - 1);
                if (!arrContent.empty() && arrContent[0] == '{') {
                    // 解析单个对象
                    data.main_inflow = atof(extractJsonField(arrContent, "f62").c_str());
                    data.super_inflow = atof(extractJsonField(arrContent, "f66").c_str());
                    data.big_inflow = atof(extractJsonField(arrContent, "f72").c_str());
                    data.mid_inflow = atof(extractJsonField(arrContent, "f78").c_str());
                    data.small_inflow = atof(extractJsonField(arrContent, "f84").c_str());
                    fprintf(stderr, "[getFundFlow] ulist.np parsed: main=%.2f\n", data.main_inflow);
                    data.northbound_shares = 0;
                    data.northbound_ratio = 0;
                    data.has_longhubang = false;
                    return true;
                }
            }
        }
        return false;
    }

    // 调试：显示前1个响应
    static int fundDebugCount = 0;
    if (fundDebugCount < 1) {
        fundDebugCount++;
        fprintf(stderr, "[getFundFlow] response for %s: %.*s\n",
                code.c_str(), (int)std::min(resp.size(), (size_t)500), resp.c_str());
    }

    // 先提取data对象
    std::string dataContent = resp;
    size_t dataObjPos = resp.find("\"data\":");
    if (dataObjPos != std::string::npos) {
        size_t objStart = resp.find('{', dataObjPos);
        if (objStart != std::string::npos) {
            int braceCount = 1;
            size_t objEnd = objStart + 1;
            while (objEnd < resp.size() && braceCount > 0) {
                if (resp[objEnd] == '{') braceCount++;
                else if (resp[objEnd] == '}') braceCount--;
                objEnd++;
            }
            if (braceCount == 0) {
                dataContent = resp.substr(objStart, objEnd - objStart);
            }
        }
    }

    // 在data对象中查找klines数组
    size_t klinesPos = dataContent.find("\"klines\":");
    if (klinesPos == std::string::npos) {
        fprintf(stderr, "[getFundFlow] klines not found in data, data sample: %.*s\n",
                (int)std::min(dataContent.size(), (size_t)300), dataContent.c_str());
        return false;
    }

    size_t arrStart = dataContent.find('[', klinesPos);
    if (arrStart == std::string::npos) {
        fprintf(stderr, "[getFundFlow] array start not found\n");
        return false;
    }

    // 使用括号计数找到匹配的]
    size_t arrEnd = std::string::npos;
    int bracketCount = 1;
    size_t pos = arrStart + 1;
    while (pos < dataContent.size() && bracketCount > 0) {
        if (dataContent[pos] == '[') bracketCount++;
        else if (dataContent[pos] == ']') bracketCount--;
        pos++;
    }
    if (bracketCount == 0) {
        arrEnd = pos - 1;
    }
    if (arrEnd == std::string::npos) {
        fprintf(stderr, "[getFundFlow] array end not found\n");
        return false;
    }

    std::string arrContent = dataContent.substr(arrStart + 1, arrEnd - arrStart - 1);
    if (arrContent.empty() || arrContent == "\"\"") {
        fprintf(stderr, "[getFundFlow] array is empty\n");
        return false;
    }

    // 解析数组中的第一个元素（kline数据）
    std::string firstLine;
    if (arrContent[0] == '"') {
        // 字符串格式："2024-01-01,123,456,..."
        size_t firstQuote = 0;
        size_t secondQuote = arrContent.find('"', firstQuote + 1);
        if (secondQuote != std::string::npos) {
            firstLine = arrContent.substr(firstQuote + 1, secondQuote - firstQuote - 1);
        }
    } else if (arrContent[0] == '{') {
        // 对象格式（备用）
        int braceCount = 1;
        size_t objEnd = 1;
        while (objEnd < arrContent.size() && braceCount > 0) {
            if (arrContent[objEnd] == '{') braceCount++;
            else if (arrContent[objEnd] == '}') braceCount--;
            objEnd++;
        }
        std::string objStr = arrContent.substr(0, objEnd);
        // 尝试从对象中提取字段
        data.main_inflow = atof(extractJsonField(objStr, "f52").c_str());
        data.super_inflow = atof(extractJsonField(objStr, "f53").c_str());
        data.big_inflow = atof(extractJsonField(objStr, "f54").c_str());
        data.mid_inflow = atof(extractJsonField(objStr, "f55").c_str());
        data.small_inflow = atof(extractJsonField(objStr, "f56").c_str());
        data.main_inflow_ratio = atof(extractJsonField(objStr, "f57").c_str());

        fprintf(stderr, "[getFundFlow] parsed object format: main=%.2f, super=%.2f, big=%.2f\n",
                data.main_inflow, data.super_inflow, data.big_inflow);

        data.northbound_shares = 0;
        data.northbound_ratio = 0;
        data.has_longhubang = false;
        return true;
    }

    if (firstLine.empty()) {
        fprintf(stderr, "[getFundFlow] failed to extract first kline\n");
        return false;
    }

    // 按逗号分割字段
    std::vector<std::string> fields;
    std::string field;
    for (size_t i = 0; i < firstLine.size(); i++) {
        if (firstLine[i] == ',') {
            fields.push_back(field);
            field.clear();
        } else {
            field += firstLine[i];
        }
    }
    fields.push_back(field);

    fprintf(stderr, "[getFundFlow] field count: %zu\n", fields.size());

    // 东方财富资金流kline字段：
    // f51:日期 f52:主力净流入 f53:超大单净流入 f54:大单净流入 f55:中单净流入 f56:小单净流入
    // f57:主力净流入占比 f58:超大单占比 f59:大单占比 f60:中单占比 f61:小单占比
    if (fields.size() >= 6) {
        data.main_inflow = atof(fields[1].c_str());
        data.super_inflow = atof(fields[2].c_str());
        data.big_inflow = atof(fields[3].c_str());
        data.mid_inflow = atof(fields[4].c_str());
        data.small_inflow = atof(fields[5].c_str());
    }
    if (fields.size() >= 7) {
        data.main_inflow_ratio = atof(fields[6].c_str());
    }

    fprintf(stderr, "[getFundFlow] parsed: main=%.2f, super=%.2f, big=%.2f, mid=%.2f, small=%.2f\n",
            data.main_inflow, data.super_inflow, data.big_inflow, data.mid_inflow, data.small_inflow);

    data.northbound_shares = 0;
    data.northbound_ratio = 0;
    data.has_longhubang = false;

    return true;
}

bool SinaProvider::getStockFullData(const std::string& code,
                                    StockFullData& data) {
    data.code = code;
    std::string normCode = normalizeCode(code);

    // 优先从股票列表缓存中获取行情数据（避免额外HTTP请求，速度快）
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = stock_list_cache_.find(normCode);
        if (it != stock_list_cache_.end()) {
            data.quote = it->second;
            data.name = it->second.name;
            data.has_data = true;
        }
    }

    // 如果缓存没有，再尝试实时行情接口
    if (!data.has_data) {
        std::vector<RealtimeQuote> quotes;
        std::vector<std::string> codes = { code };
        if (getRealtimeQuotes(codes, quotes) && !quotes.empty()) {
            data.quote = quotes[0];
            data.name = quotes[0].name;
            data.has_data = true;
        }
    }

    FundamentalData fund;
    if (getFundamentalData(code, fund)) {
        data.fundamental = fund;
        if (data.quote.industry.empty()) data.quote.industry = fund.industry;
        if (data.quote.pe <= 0 && fund.pe > 0) data.quote.pe = fund.pe;
        if (data.quote.pb <= 0 && fund.pb > 0) data.quote.pb = fund.pb;
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

bool SinaProvider::getStockScreenData(const std::string& code,
                                      StockFullData& data) {
    data.code = code;
    std::string normCode = normalizeCode(code);

    // 优先从股票列表缓存中获取行情数据（零HTTP请求，速度快）
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = stock_list_cache_.find(normCode);
        if (it != stock_list_cache_.end()) {
            data.quote = it->second;
            data.name = it->second.name;
            data.has_data = true;
        }
    }

    // 如果缓存没有，再尝试实时行情接口
    if (!data.has_data) {
        std::vector<RealtimeQuote> quotes;
        std::vector<std::string> codes = { code };
        if (getRealtimeQuotes(codes, quotes) && !quotes.empty()) {
            data.quote = quotes[0];
            data.name = quotes[0].name;
            data.has_data = true;
        }
    }

    // 基本面数据：用行情数据和技术指标作为主要来源
    // 财务接口太慢且不稳定，用可获取的数据来估算
    data.fundamental.code = code;
    
    // === 估值数据（从行情缓存获取，通常可靠）===
    data.fundamental.pe = data.quote.pe;
    data.fundamental.pb = data.quote.pb;
    data.fundamental.industry = data.quote.industry;
    
    // === PE分位数默认值 ===
    data.fundamental.pe_percentile = 50;  // 中位数
    
    // === 用市值和PE估算基本面 ===
    // 大市值+低PE = 价值型稳健股（隐含更高的ROE预期）
    // 小市值+高PE = 成长型（隐含更高的成长性预期）
    double mcap = data.quote.total_mcap;
    double price = data.quote.price;
    
    // 估算ROE（根据市值和估值水平）
    if (mcap > 0 && price > 0 && data.fundamental.pe > 0) {
        // 隐含ROE = 1/PE（简化模型）
        double impliedRoe = 1.0 / (data.fundamental.pe / 100.0) * 100.0;  // 转换为百分比
        if (impliedRoe > 0 && impliedRoe < 50) {
            data.fundamental.roe = impliedRoe;
        }
        // 大市值公司通常ROE更稳定
        if (mcap > 100000000000.0 && data.fundamental.roe < 8) {
            data.fundamental.roe = 8.0;  // 大市值兜底
        }
        if (mcap > 50000000000.0 && data.fundamental.roe < 5) {
            data.fundamental.roe = 5.0;
        }
    }
    
    // 估算毛利率（根据市值规模）
    if (mcap > 100000000000.0) {  // 1000亿+
        data.fundamental.gross_margin = 30.0;  // 大市值通常毛利率较高
    } else if (mcap > 50000000000.0) {  // 500亿+
        data.fundamental.gross_margin = 25.0;
    } else if (mcap > 10000000000.0) {  // 100亿+
        data.fundamental.gross_margin = 20.0;
    } else if (mcap > 0) {
        data.fundamental.gross_margin = 15.0;
    }
    
    // 估算净利率
    if (data.fundamental.gross_margin > 0) {
        data.fundamental.net_margin = data.fundamental.gross_margin * 0.4;  // 简化估算
        if (mcap > 100000000000.0) {
            data.fundamental.net_margin = data.fundamental.gross_margin * 0.5;
        }
    }
    
    // 估算股息率（大市值低估值股票通常股息率更高）
    if (data.fundamental.pe > 0 && data.fundamental.pe < 100) {
        double impliedDivYield = (1.0 / data.fundamental.pe) * 100.0;  // 隐含股息率
        if (data.fundamental.pe < 15 && impliedDivYield > 2.0) {
            data.fundamental.dividend_yield = impliedDivYield;
        }
    }
    
    // 其他基本面默认值
    data.fundamental.eps = price > 0 ? data.quote.pe * price / 100.0 : 0;  // 简化EPS估算
    data.fundamental.debt_ratio = 40.0;  // 默认中等负债率
    data.fundamental.roe_5y_avg = data.fundamental.roe > 0 ? data.fundamental.roe * 0.9 : 5.0;  // 5年均值
    data.fundamental.goodwill_ratio = 5.0;  // 默认低商誉
    data.fundamental.pledge_ratio = 0.0;  // 默认无质押
    data.fundamental.holder_num_change = 0.0;  // 默认稳定
    data.fundamental.roa = data.fundamental.roe * 0.5;  // 简化估算
    data.fundamental.operating_cf_ratio = 0.8;  // 默认正值
    
    // 历史数据（避免空数组导致计算问题）
    if (data.fundamental.deduct_profit_3y.empty()) {
        data.fundamental.deduct_profit_3y = { 1.0, 1.1, 1.2 };  // 默认正增长
    }
    if (data.fundamental.gross_margin_trend.empty()) {
        data.fundamental.gross_margin_trend = { 20.0, 21.0, 22.0 };  // 默认改善趋势
    }
    
    // 股本数据
    if (data.quote.total_mcap > 0 && data.quote.price > 0) {
        data.fundamental.total_share = data.quote.total_mcap / data.quote.price;
    }
    if (data.quote.float_mcap > 0 && data.quote.price > 0) {
        data.fundamental.float_share = data.quote.float_mcap / data.quote.price;
    }
    
    // 资金流向：跳过（接口不稳定且慢）
    data.fund_flow.code = code;
    data.fund_flow.main_inflow = 0;
    data.fund_flow.super_inflow = 0;
    data.fund_flow.big_inflow = 0;
    data.fund_flow.mid_inflow = 0;
    data.fund_flow.small_inflow = 0;
    data.fund_flow.main_inflow_ratio = 0;
    data.fund_flow.northbound_shares = 0;
    data.fund_flow.northbound_ratio = 0;
    data.fund_flow.northbound_change = 0;
    data.fund_flow.chip_concentration = 0;
    data.fund_flow.has_longhubang = false;
    
    // 只获取60天K线，足够计算技术指标
    std::vector<KlineData> klines;
    if (getKlineData(code, "day", 60, klines)) {
        data.technical.day_klines = klines;
        data.technical.code = code;
        data.technical.turnover_rate = data.quote.turnover_rate > 0 ? data.quote.turnover_rate : 1.5;
        calculateTechnicalIndicators(data.technical);
        
        // 用技术指标估算成长因子（当财务数据缺失时）
        // 如果60日动量>0，说明有上涨趋势，可能对应更好的基本面
        if (data.fundamental.revenue_growth == 0 && data.technical.momentum_60d > 0) {
            data.fundamental.revenue_growth = data.technical.momentum_60d * 0.5;  // 按动量的50%估算
            data.fundamental.profit_growth = data.fundamental.revenue_growth * 0.8;
        }
        // 如果ROE还没估算出来，用技术面辅助
        if (data.fundamental.roe < 5 && data.technical.momentum_60d > 10 && data.technical.ma_bullish_daily) {
            data.fundamental.roe = 8.0;  // 技术面支撑
        }
    }
    
    if (data.name.find("ST") != std::string::npos) {
        data.is_st = true;
    }
    
    return data.has_data;
}

}
