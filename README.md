# 量化选股工具

一款基于 **Electron 前端 + C++ 后端的量化选股桌面工具，接入新浪财经真实行情数据，实现四大周期（长线/中线/短线/超短线）多因子 TOP10 选股功能。

## 功能特性

- 全市场行情列表（沪深A股）
- 个股详情（K线图 + MACD/KDJ/BOLL 技术指标）
- 四大周期多因子选股（长线/中线/短线/超短线）
- 自选股管理
- 实时行情刷新

## 技术架构

### 前端
- Electron 28 + 原生 HTML/CSS/JS
- ECharts 5 图表库
- electron-store 本地存储

### 后端
- C++17，零第三方库依赖
- WinINet HTTP客户端（请求新浪财经接口）
- WinSock HTTP服务端
- 自研 JSON 解析/生成

## 项目结构

```
testPro/
├── frontend/          # Electron 前端
│   ├── main.js           # Electron 主进程
│   ├── preload.js      # 预加载脚本
│   ├── package.json
│   └── src/
│       ├── index.html  # 主页面
│       ├── css/style.css
│       └── js/
│           ├── api.js
│           └── renderer.js
└── backend/           # C++ 后端
    ├── CMakeLists.txt
    ├── include/          # 头文件
    │   ├── common/       # 公共类型
    │   ├── datasource/ # 数据获取层
    │   ├── selector/   # 选股策略层
    │   ├── cache/      # 缓存层
    │   └── server/     # HTTP服务层
    └── src/            # 实现文件
```

## 快速开始

### 前置条件

- Node.js >= 16+
- CMake >= 3.15+
- Visual Studio 2019/2022 (MSVC)

### 编译后端

```bash
cd backend
mkdir build
cd build
cmake ..
cmake --build . --config Release
```

编译完成后，可执行文件在 `build/Release/quant-server.exe`

### 运行前端

```bash
cd frontend
npm install
npm start
```

### 打包发布

```bash
cd frontend
npm run build:win
```

## API 接口

| 接口 | 方法 | 说明 |
|--------|------|------|
| `/api/health` | GET | 健康检查 |
| `/api/market/list` | GET | 全市场行情列表 |
| `/api/stock/realtime?codes=xxx,yyy` | GET | 批量实时行情 |
| `/api/stock/detail?code=xxx` | GET | 个股详情 |
| `/api/stock/kline?code=xxx&type=day&count=200` | GET | K线数据 |
| `/api/stock/fund_flow?code=xxx` | GET | 资金流向 |
| `/api/selector/run` | POST | 执行选股 |
| `/api/selector/result?period=long_term` | GET | 选股结果 |

### 选股周期参数

- `long_term` - 长线TOP10
- `mid_term` - 中线TOP10
- `short_term` - 短线TOP10
- `ultra_short_term` - 超短线TOP10

## 选股策略

### 长线 TOP10（持有≥1年）
- 权重：基本面60% + 估值25% + 行业10% + 资金5%
- 淘汰条件：扣非净利润下滑、经营现金流负、资产负债率过高、PE分位过高、商誉过高
- 打分维度：ROE、毛利率、股息率、PE分位、北向资金、行业景气

### 中线 TOP10（1~3个月）
- 权重：基本面30% + 估值20% + 技术趋势35% + 资金15%
- 打分维度：营收增速、PEG、均线多头、量价配合、机构持仓、无大额解禁

### 短线 TOP10（5~20交易日）
- 权重：技术60% + 短期资金30% + 基本面兜底10%
- 打分维度：周线趋势、资金净流入、成交量、无利空、业绩兜底

### 超短线 TOP10（1~5交易日）
- 权重：量价技术80% + 短期资金20%
- 打分维度：日线多头发散、换手率、主力资金、非高位泡沫

## 数据来源

新浪财经公开行情接口
