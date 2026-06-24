const App = {
  state: {
    currentPage: 'market',
    currentPeriod: 'long_term',
    marketData: [],
    filteredData: [],
    marketPage: 1,
    pageSize: 100,
    sortField: 'change_pct',
    sortOrder: 'desc',
    searchKeyword: '',
    autoRefreshInterval: 10,
    autoRefreshTimer: null,
    favorites: [],
    selectedStock: null,
    currentKtype: 'day',
    klineChart: null,
    macdChart: null,
    kdjChart: null,
    bollChart: null
  },

  async init() {
    await API.init();
    this.bindEvents();
    this.loadFavorites();
    this.switchPage('market');
    this.loadMarketData();
    this.startAutoRefresh();
  },

  bindEvents() {
    document.querySelectorAll('.nav-tab').forEach(tab => {
      tab.addEventListener('click', () => {
        this.switchPage(tab.dataset.page);
      });
    });

    document.querySelectorAll('.sub-tab').forEach(tab => {
      tab.addEventListener('click', () => {
        this.switchPeriod(tab.dataset.period);
      });
    });

    document.getElementById('searchInput').addEventListener('input', (e) => {
      this.state.searchKeyword = e.target.value.trim().toLowerCase();
      this.filterMarketData();
    });

    document.getElementById('refreshMarketBtn').addEventListener('click', () => {
      this.loadMarketData();
    });

    document.getElementById('refreshFavBtn').addEventListener('click', () => {
      this.loadFavoritesRealtime();
    });

    document.getElementById('autoRefreshSelect').addEventListener('change', (e) => {
      this.state.autoRefreshInterval = parseInt(e.target.value);
      this.startAutoRefresh();
    });

    document.getElementById('pageSizeSelect').addEventListener('change', (e) => {
      this.state.pageSize = parseInt(e.target.value);
      this.state.marketPage = 1;
      this.renderMarketTable();
    });

    document.getElementById('prevPage').addEventListener('click', () => {
      if (this.state.marketPage > 1) {
        this.state.marketPage--;
        this.renderMarketTable();
      }
    });

    document.getElementById('nextPage').addEventListener('click', () => {
      const totalPages = Math.ceil(this.state.filteredData.length / this.state.pageSize);
      if (this.state.marketPage < totalPages) {
        this.state.marketPage++;
        this.renderMarketTable();
      }
    });

    document.querySelectorAll('#marketTable th.sortable').forEach(th => {
      th.addEventListener('click', () => {
        const field = th.dataset.sort;
        if (this.state.sortField === field) {
          this.state.sortOrder = this.state.sortOrder === 'desc' ? 'asc' : 'desc';
        } else {
          this.state.sortField = field;
          this.state.sortOrder = 'desc';
        }
        this.updateSortHeaders();
        this.sortMarketData();
        this.renderMarketTable();
      });
    });

    document.getElementById('runSelectorBtn').addEventListener('click', () => {
      this.runSelector();
    });

    document.getElementById('backBtn').addEventListener('click', () => {
      this.goBack();
    });

    document.getElementById('detailStarBtn').addEventListener('click', () => {
      this.toggleDetailFavorite();
    });

    document.querySelectorAll('.kline-btn').forEach(btn => {
      btn.addEventListener('click', () => {
        document.querySelectorAll('.kline-btn').forEach(b => b.classList.remove('active'));
        btn.classList.add('active');
        this.state.currentKtype = btn.dataset.ktype;
        this.loadKlineData();
      });
    });
  },

  switchPage(page) {
    this.state.currentPage = page;
    document.querySelectorAll('.nav-tab').forEach(tab => {
      tab.classList.toggle('active', tab.dataset.page === page);
    });
    document.querySelectorAll('.page').forEach(p => {
      p.classList.remove('active');
    });
    document.getElementById(`page-${page}`).classList.add('active');

    if (page === 'selector') {
      this.loadSelectorResult();
    } else if (page === 'favorites') {
      this.loadFavoritesRealtime();
    }
  },

  switchPeriod(period) {
    this.state.currentPeriod = period;
    document.querySelectorAll('.sub-tab').forEach(tab => {
      tab.classList.toggle('active', tab.dataset.period === period);
    });
    this.updateSelectorTip();
    this.loadSelectorResult();
  },

  updateSelectorTip() {
    const tips = {
      long_term: '<strong>长线策略</strong>：持有≥1年，价值基本面优先。权重：基本面60% + 估值25% + 行业10% + 资金5%',
      mid_term: '<strong>中线策略</strong>：1~3个月，成长+趋势。权重：基本面30% + 估值20% + 技术趋势35% + 资金15%',
      short_term: '<strong>短线策略</strong>：5~20交易日，波段资金驱动。权重：技术60% + 短期资金30% + 基本面兜底10%',
      ultra_short_term: '<strong>超短线策略</strong>：1~5交易日，情绪+量价。权重：量价技术80% + 短期资金20%'
    };
    const titles = {
      long_term: '长线 TOP10 选股结果',
      mid_term: '中线 TOP10 选股结果',
      short_term: '短线 TOP10 选股结果',
      ultra_short_term: '超短线 TOP10 选股结果'
    };
    document.getElementById('selectorTip').innerHTML = tips[this.state.currentPeriod];
    document.getElementById('selectorTitle').textContent = titles[this.state.currentPeriod];
  },

  async loadMarketData() {
    try {
      const data = await API.getMarketList();
      this.state.marketData = data.stocks || [];
      this.filterMarketData();
      document.getElementById('marketCount').textContent = `(共 ${this.state.marketData.length} 只)`;
    } catch (err) {
      console.error('Load market data failed:', err);
    }
  },

  filterMarketData() {
    const keyword = this.state.searchKeyword;
    if (!keyword) {
      this.state.filteredData = [...this.state.marketData];
    } else {
      this.state.filteredData = this.state.marketData.filter(s =>
        s.code.toLowerCase().includes(keyword) ||
        s.name.toLowerCase().includes(keyword)
      );
    }
    this.sortMarketData();
    this.state.marketPage = 1;
    this.renderMarketTable();
  },

  sortMarketData() {
    const { sortField, sortOrder, filteredData } = this.state;
    filteredData.sort((a, b) => {
      let va = a[sortField];
      let vb = b[sortField];
      if (typeof va === 'string') va = parseFloat(va) || 0;
      if (typeof vb === 'string') vb = parseFloat(vb) || 0;
      return sortOrder === 'desc' ? vb - va : va - vb;
    });
  },

  updateSortHeaders() {
    document.querySelectorAll('#marketTable th.sortable').forEach(th => {
      th.classList.remove('sorted-asc', 'sorted-desc');
      if (th.dataset.sort === this.state.sortField) {
        th.classList.add(this.state.sortOrder === 'desc' ? 'sorted-desc' : 'sorted-asc');
      }
    });
  },

  renderMarketTable() {
    const { filteredData, marketPage, pageSize } = this.state;
    const start = (marketPage - 1) * pageSize;
    const end = start + pageSize;
    const pageData = filteredData.slice(start, end);
    const totalPages = Math.max(1, Math.ceil(filteredData.length / pageSize));

    const tbody = document.getElementById('marketTbody');
    tbody.innerHTML = pageData.map((s, i) => this.renderMarketRow(s, start + i + 1)).join('');

    document.getElementById('pageInfo').textContent = `第 ${marketPage} / ${totalPages} 页`;
    document.getElementById('prevPage').disabled = marketPage <= 1;
    document.getElementById('nextPage').disabled = marketPage >= totalPages;

    tbody.querySelectorAll('tr').forEach((tr, idx) => {
      tr.addEventListener('click', (e) => {
        if (e.target.closest('.star-btn')) return;
        this.openStockDetail(pageData[idx]);
      });
      tr.querySelector('.star-btn').addEventListener('click', (e) => {
        e.stopPropagation();
        this.toggleFavorite(pageData[idx]);
      });
    });
  },

  renderMarketRow(s) {
    const changeClass = s.change_pct > 0 ? 'up' : s.change_pct < 0 ? 'down' : 'flat';
    const changeSign = s.change_pct > 0 ? '+' : '';
    const isFav = this.state.favorites.some(f => f.code === s.code);
    return `
      <tr data-code="${s.code}">
        <td class="code">${s.code}</td>
        <td class="name">${s.name}${isFav ? '<span class="fav-badge">自选</span>' : ''}</td>
        <td class="${changeClass}">${s.price.toFixed(2)}</td>
        <td class="${changeClass}">${changeSign}${s.change_pct.toFixed(2)}%</td>
        <td class="${changeClass}">${changeSign}${s.change.toFixed(2)}</td>
        <td>${this.formatVolume(s.volume)}</td>
        <td>${this.formatAmount(s.amount)}</td>
        <td class="up">${s.high.toFixed(2)}</td>
        <td class="down">${s.low.toFixed(2)}</td>
        <td>${s.open.toFixed(2)}</td>
        <td>${s.prev_close.toFixed(2)}</td>
        <td><button class="star-btn ${isFav ? 'active' : ''}">${isFav ? '★' : '☆'}</button></td>
      </tr>
    `;
  },

  startAutoRefresh() {
    if (this.state.autoRefreshTimer) {
      clearInterval(this.state.autoRefreshTimer);
      this.state.autoRefreshTimer = null;
    }
    if (this.state.autoRefreshInterval > 0) {
      this.state.autoRefreshTimer = setInterval(() => {
        if (this.state.currentPage === 'market') {
          this.loadMarketData();
        } else if (this.state.currentPage === 'favorites') {
          this.loadFavoritesRealtime();
        }
      }, this.state.autoRefreshInterval * 1000);
    }
  },

  async loadFavorites() {
    try {
      this.state.favorites = await window.electronAPI.getFavorites();
    } catch (err) {
      console.error('Load favorites failed:', err);
    }
  },

  async toggleFavorite(stock) {
    const isFav = this.state.favorites.some(f => f.code === stock.code);
    if (isFav) {
      await window.electronAPI.removeFavorite(stock.code);
      this.state.favorites = this.state.favorites.filter(f => f.code !== stock.code);
    } else {
      await window.electronAPI.addFavorite(stock.code, stock.name);
      this.state.favorites.push({ code: stock.code, name: stock.name });
    }
    if (this.state.currentPage === 'market') {
      this.renderMarketTable();
    } else if (this.state.currentPage === 'favorites') {
      this.loadFavoritesRealtime();
    }
  },

  async loadFavoritesRealtime() {
    const favs = this.state.favorites;
    document.getElementById('favCount').textContent = `(共 ${favs.length} 只)`;

    if (favs.length === 0) {
      document.getElementById('favTbody').innerHTML = '';
      document.getElementById('favEmpty').style.display = 'flex';
      return;
    }

    document.getElementById('favEmpty').style.display = 'none';

    try {
      const codes = favs.map(f => f.code);
      const data = await API.getStockRealtime(codes);
      const stocks = data.stocks || [];

      const tbody = document.getElementById('favTbody');
      tbody.innerHTML = stocks.map(s => this.renderFavRow(s)).join('');

      tbody.querySelectorAll('tr').forEach((tr, idx) => {
        tr.addEventListener('click', (e) => {
          if (e.target.closest('.star-btn')) return;
          this.openStockDetail(stocks[idx]);
        });
        tr.querySelector('.star-btn').addEventListener('click', (e) => {
          e.stopPropagation();
          this.toggleFavorite(stocks[idx]);
        });
      });
    } catch (err) {
      console.error('Load favorites realtime failed:', err);
    }
  },

  renderFavRow(s) {
    const changeClass = s.change_pct > 0 ? 'up' : s.change_pct < 0 ? 'down' : 'flat';
    const changeSign = s.change_pct > 0 ? '+' : '';
    return `
      <tr data-code="${s.code}">
        <td class="code">${s.code}</td>
        <td class="name">${s.name}</td>
        <td class="${changeClass}">${s.price.toFixed(2)}</td>
        <td class="${changeClass}">${changeSign}${s.change_pct.toFixed(2)}%</td>
        <td class="${changeClass}">${changeSign}${s.change.toFixed(2)}</td>
        <td>${this.formatVolume(s.volume)}</td>
        <td>${this.formatAmount(s.amount)}</td>
        <td class="up">${s.high.toFixed(2)}</td>
        <td class="down">${s.low.toFixed(2)}</td>
        <td><button class="star-btn active">★</button></td>
      </tr>
    `;
  },

  async runSelector() {
    const btn = document.getElementById('runSelectorBtn');
    btn.disabled = true;
    btn.textContent = '选股中...';
    try {
      const data = await API.runSelector(this.state.currentPeriod);
      this.renderSelectorResult(data);
    } catch (err) {
      console.error('Run selector failed:', err);
      alert('选股失败：' + err.message);
    } finally {
      btn.disabled = false;
      btn.textContent = '重新选股';
    }
  },

  async loadSelectorResult() {
    try {
      const data = await API.getSelectorResult(this.state.currentPeriod);
      this.renderSelectorResult(data);
    } catch (err) {
      console.error('Load selector result failed:', err);
      document.getElementById('selectorTbody').innerHTML =
        '<tr><td colspan="11" style="text-align:center;color:#8b95a7;padding:40px;">暂无选股结果，点击"重新选股"开始计算</td></tr>';
    }
  },

  renderSelectorResult(data) {
    const stocks = data.stocks || [];
    document.getElementById('selectorTime').textContent = data.time ? `选股时间：${data.time}` : '';

    const tbody = document.getElementById('selectorTbody');
    if (stocks.length === 0) {
      tbody.innerHTML = '<tr><td colspan="11" style="text-align:center;color:#8b95a7;padding:40px;">暂无选股结果</td></tr>';
      return;
    }

    tbody.innerHTML = stocks.map((s, i) => this.renderSelectorRow(s, i + 1)).join('');

    tbody.querySelectorAll('tr').forEach((tr, idx) => {
      tr.addEventListener('click', () => {
        this.showSelectorDetail(stocks[idx]);
      });
    });
  },

  renderSelectorRow(s, rank) {
    const changeClass = s.change_pct > 0 ? 'up' : s.change_pct < 0 ? 'down' : 'flat';
    const changeSign = s.change_pct > 0 ? '+' : '';
    const details = s.score_details || {};
    const rankBg = rank <= 3 ? `style="color:#ff6b35;font-weight:bold;"` : '';
    return `
      <tr data-code="${s.code}">
        <td ${rankBg}>${rank}</td>
        <td class="code">${s.code}</td>
        <td class="name">${s.name}</td>
        <td style="font-weight:bold;color:#ffc53d;">${s.score.toFixed(1)}</td>
        <td>${details.fundamental !== undefined ? details.fundamental.toFixed(1) : '-'}</td>
        <td>${details.valuation !== undefined ? details.valuation.toFixed(1) : '-'}</td>
        <td>${details.technical !== undefined ? details.technical.toFixed(1) : '-'}</td>
        <td>${details.capital !== undefined ? details.capital.toFixed(1) : '-'}</td>
        <td>${s.industry || '-'}</td>
        <td class="${changeClass}">${s.price.toFixed(2)}</td>
        <td><button class="btn btn-secondary" onclick="event.stopPropagation();App.openStockDetailByCode('${s.code}','${s.name}')">详情</button></td>
      </tr>
    `;
  },

  showSelectorDetail(stock) {
    const panel = document.getElementById('selectorDetail');
    const grid = document.getElementById('selectorDetailGrid');
    document.getElementById('selectorDetailTitle').textContent = `${stock.name}(${stock.code}) - 得分明细`;

    const details = stock.score_details || {};
    const items = Object.entries(details).map(([key, val]) => {
      const labels = {
        roe: 'ROE近5年均值',
        gross_margin: '毛利率趋势',
        dividend: '股息率',
        pe_valuation: 'PE估值分位',
        northbound: '北向资金',
        industry_boom: '行业景气度',
        revenue_growth: '营收利润增速',
        peg: 'PEG',
        ma_bullish: '均线多头排列',
        volume_pattern: '量价配合',
        institutional: '机构持仓',
        no_unlock: '无大额解禁',
        weekly_trend: '周线趋势',
        fund_inflow: '资金净流入',
        volume_expand: '成交量放大',
        no_negative: '无利空',
        profit_floor: '业绩兜底',
        daily_bullish: '日线多头发散',
        turnover: '换手率',
        main_fund: '主力资金',
        no_bubble: '非高位泡沫',
        fundamental: '基本面总分',
        valuation: '估值总分',
        technical: '技术趋势总分',
        capital: '资金总分'
      };
      const label = labels[key] || key;
      return `
        <div class="detail-item">
          <div class="detail-label">${label}</div>
          <div class="score-bar">
            <div class="score-bar-bg">
              <div class="score-bar-fill" style="width:${Math.min(100, Math.max(0, val * 4))}%"></div>
            </div>
            <span class="score-text">${val.toFixed(1)}</span>
          </div>
        </div>
      `;
    }).join('');

    grid.innerHTML = items;
    panel.style.display = 'block';
  },

  async openStockDetail(stock) {
    this.state.selectedStock = stock;
    document.getElementById('detailName').textContent = stock.name;
    document.getElementById('detailCode').textContent = stock.code;
    this.updateDetailPrice(stock);
    this.updateDetailStarBtn();
    this.switchPage('detail');
    this.initCharts();
    this.loadStockDetailData(stock.code);
    this.loadKlineData();
  },

  async openStockDetailByCode(code, name) {
    const stock = { code, name, price: 0, change_pct: 0, change: 0 };
    try {
      const data = await API.getStockRealtime([code]);
      if (data.stocks && data.stocks.length > 0) {
        Object.assign(stock, data.stocks[0]);
      }
    } catch (e) {}
    this.openStockDetail(stock);
  },

  updateDetailPrice(stock) {
    const changeClass = stock.change_pct > 0 ? 'up' : stock.change_pct < 0 ? 'down' : 'flat';
    const changeSign = stock.change_pct > 0 ? '+' : '';
    const priceEl = document.getElementById('detailPrice');
    const changeEl = document.getElementById('detailChange');
    priceEl.textContent = stock.price.toFixed(2);
    priceEl.className = 'stock-price ' + changeClass;
    changeEl.textContent = `${changeSign}${stock.change.toFixed(2)} (${changeSign}${stock.change_pct.toFixed(2)}%)`;
    changeEl.className = 'stock-change ' + changeClass;
  },

  updateDetailStarBtn() {
    if (!this.state.selectedStock) return;
    const isFav = this.state.favorites.some(f => f.code === this.state.selectedStock.code);
    const btn = document.getElementById('detailStarBtn');
    btn.textContent = isFav ? '★' : '☆';
    btn.className = 'star-btn' + (isFav ? ' active' : '');
    btn.title = isFav ? '取消自选' : '加入自选';
  },

  async toggleDetailFavorite() {
    if (!this.state.selectedStock) return;
    await this.toggleFavorite(this.state.selectedStock);
    this.updateDetailStarBtn();
  },

  goBack() {
    this.switchPage('market');
    this.state.selectedStock = null;
    this.disposeCharts();
  },

  initCharts() {
    if (!this.state.klineChart) {
      this.state.klineChart = echarts.init(document.getElementById('klineChart'), 'dark');
      this.state.macdChart = echarts.init(document.getElementById('macdChart'), 'dark');
      this.state.kdjChart = echarts.init(document.getElementById('kdjChart'), 'dark');
      this.state.bollChart = echarts.init(document.getElementById('bollChart'), 'dark');
    }
  },

  disposeCharts() {
    ['klineChart', 'macdChart', 'kdjChart', 'bollChart'].forEach(name => {
      if (this.state[name]) {
        this.state[name].dispose();
        this.state[name] = null;
      }
    });
  },

  async loadStockDetailData(code) {
    try {
      const detail = await API.getStockDetail(code);
      this.updateBasicInfo(detail);
      this.updateFundFlow(detail.fund_flow || {});
    } catch (err) {
      console.error('Load stock detail failed:', err);
    }
  },

  updateBasicInfo(detail) {
    document.getElementById('infoOpen').textContent = detail.open ? detail.open.toFixed(2) : '--';
    document.getElementById('infoHigh').textContent = detail.high ? detail.high.toFixed(2) : '--';
    document.getElementById('infoLow').textContent = detail.low ? detail.low.toFixed(2) : '--';
    document.getElementById('infoPrev').textContent = detail.prev_close ? detail.prev_close.toFixed(2) : '--';
    document.getElementById('infoVolume').textContent = detail.volume ? this.formatVolume(detail.volume) : '--';
    document.getElementById('infoAmount').textContent = detail.amount ? this.formatAmount(detail.amount) + '万' : '--';
    document.getElementById('infoTurnover').textContent = detail.turnover_rate ? detail.turnover_rate.toFixed(2) + '%' : '--';
    document.getElementById('infoPe').textContent = detail.pe ? detail.pe.toFixed(2) : '--';

    const basicGrid = document.getElementById('basicInfoGrid');
    const items = [
      { label: '所属行业', value: detail.industry || '--' },
      { label: '总股本', value: detail.total_share ? (detail.total_share / 100000000).toFixed(2) + '亿' : '--' },
      { label: '流通股本', value: detail.float_share ? (detail.float_share / 100000000).toFixed(2) + '亿' : '--' },
      { label: '总市值', value: detail.total_mcap ? (detail.total_mcap / 100000000).toFixed(2) + '亿' : '--' },
      { label: '流通市值', value: detail.float_mcap ? (detail.float_mcap / 100000000).toFixed(2) + '亿' : '--' },
      { label: 'ROE', value: detail.roe ? detail.roe.toFixed(2) + '%' : '--' },
      { label: '毛利率', value: detail.gross_margin ? detail.gross_margin.toFixed(2) + '%' : '--' },
      { label: '资产负债率', value: detail.debt_ratio ? detail.debt_ratio.toFixed(2) + '%' : '--' }
    ];
    basicGrid.innerHTML = items.map(item => `
      <div class="detail-item">
        <div class="detail-label">${item.label}</div>
        <div class="detail-value">${item.value}</div>
      </div>
    `).join('');
  },

  updateFundFlow(fundFlow) {
    const grid = document.getElementById('fundFlowGrid');
    const items = [
      { label: '主力净流入', value: fundFlow.main_inflow ? (fundFlow.main_inflow / 10000).toFixed(2) + '万' : '--', color: fundFlow.main_inflow > 0 ? 'up' : fundFlow.main_inflow < 0 ? 'down' : '' },
      { label: '超大单净流入', value: fundFlow.super_inflow ? (fundFlow.super_inflow / 10000).toFixed(2) + '万' : '--', color: fundFlow.super_inflow > 0 ? 'up' : fundFlow.super_inflow < 0 ? 'down' : '' },
      { label: '大单净流入', value: fundFlow.big_inflow ? (fundFlow.big_inflow / 10000).toFixed(2) + '万' : '--', color: fundFlow.big_inflow > 0 ? 'up' : fundFlow.big_inflow < 0 ? 'down' : '' },
      { label: '中单净流入', value: fundFlow.mid_inflow ? (fundFlow.mid_inflow / 10000).toFixed(2) + '万' : '--', color: fundFlow.mid_inflow > 0 ? 'up' : fundFlow.mid_inflow < 0 ? 'down' : '' },
      { label: '小单净流入', value: fundFlow.small_inflow ? (fundFlow.small_inflow / 10000).toFixed(2) + '万' : '--', color: fundFlow.small_inflow > 0 ? 'up' : fundFlow.small_inflow < 0 ? 'down' : '' },
      { label: '北向持股', value: fundFlow.northbound_shares ? (fundFlow.northbound_shares / 10000).toFixed(2) + '万股' : '--' },
      { label: '北向占比', value: fundFlow.northbound_ratio ? fundFlow.northbound_ratio.toFixed(2) + '%' : '--' },
      { label: '龙虎榜', value: fundFlow.has_longhubang ? '有' : '无' }
    ];
    grid.innerHTML = items.map(item => `
      <div class="detail-item">
        <div class="detail-label">${item.label}</div>
        <div class="detail-value ${item.color || ''}">${item.value}</div>
      </div>
    `).join('');
  },

  async loadKlineData() {
    if (!this.state.selectedStock) return;
    try {
      const data = await API.getKline(this.state.selectedStock.code, this.state.currentKtype, 200);
      const klines = data.klines || [];
      this.renderKlineChart(klines);
      this.renderMacdChart(klines);
      this.renderKdjChart(klines);
      this.renderBollChart(klines);
    } catch (err) {
      console.error('Load kline failed:', err);
    }
  },

  renderKlineChart(klines) {
    const dates = klines.map(k => k.date);
    const ohlc = klines.map(k => [k.open, k.close, k.low, k.high]);
    const volumes = klines.map(k => k.volume);

    const ma5 = this.calcMA(klines, 5);
    const ma10 = this.calcMA(klines, 10);
    const ma20 = this.calcMA(klines, 20);
    const ma60 = this.calcMA(klines, 60);

    this.state.klineChart.setOption({
      backgroundColor: 'transparent',
      tooltip: { trigger: 'axis', axisPointer: { type: 'cross' } },
      legend: { data: ['K线', 'MA5', 'MA10', 'MA20', 'MA60'], textStyle: { color: '#8b95a7' } },
      grid: [{ left: '8%', right: '3%', top: '12%', height: '55%' }, { left: '8%', right: '3%', top: '72%', height: '18%' }],
      xAxis: [
        { type: 'category', data: dates, gridIndex: 0, axisLine: { lineStyle: { color: '#2a3040' } }, axisLabel: { color: '#8b95a7' } },
        { type: 'category', data: dates, gridIndex: 1, axisLine: { lineStyle: { color: '#2a3040' } }, axisLabel: { show: false } }
      ],
      yAxis: [
        { scale: true, gridIndex: 0, splitLine: { lineStyle: { color: '#1a1f2e' } }, axisLine: { lineStyle: { color: '#2a3040' } }, axisLabel: { color: '#8b95a7' } },
        { scale: true, gridIndex: 1, splitLine: { lineStyle: { color: '#1a1f2e' } }, axisLine: { lineStyle: { color: '#2a3040' } }, axisLabel: { color: '#8b95a7' } }
      ],
      dataZoom: [
        { type: 'inside', xAxisIndex: [0, 1], start: 50, end: 100 }
      ],
      series: [
        {
          name: 'K线',
          type: 'candlestick',
          data: ohlc,
          xAxisIndex: 0,
          yAxisIndex: 0,
          itemStyle: { color: '#f53f3f', color0: '#00b42a', borderColor: '#f53f3f', borderColor0: '#00b42a' }
        },
        { name: 'MA5', type: 'line', data: ma5, xAxisIndex: 0, yAxisIndex: 0, smooth: true, lineStyle: { width: 1, color: '#ffc53d' }, symbol: 'none' },
        { name: 'MA10', type: 'line', data: ma10, xAxisIndex: 0, yAxisIndex: 0, smooth: true, lineStyle: { width: 1, color: '#3491fa' }, symbol: 'none' },
        { name: 'MA20', type: 'line', data: ma20, xAxisIndex: 0, yAxisIndex: 0, smooth: true, lineStyle: { width: 1, color: '#b71ec5' }, symbol: 'none' },
        { name: 'MA60', type: 'line', data: ma60, xAxisIndex: 0, yAxisIndex: 0, smooth: true, lineStyle: { width: 1, color: '#00b42a' }, symbol: 'none' },
        {
          name: '成交量',
          type: 'bar',
          data: volumes.map((v, i) => ({
            value: v,
            itemStyle: { color: klines[i].close >= klines[i].open ? '#f53f3f' : '#00b42a' }
          })),
          xAxisIndex: 1,
          yAxisIndex: 1
        }
      ]
    });
  },

  renderMacdChart(klines) {
    const macdData = this.calcMACD(klines, 12, 26, 9);
    const dates = klines.map(k => k.date);

    this.state.macdChart.setOption({
      backgroundColor: 'transparent',
      tooltip: { trigger: 'axis' },
      legend: { data: ['DIF', 'DEA', 'MACD'], textStyle: { color: '#8b95a7' } },
      grid: { left: '10%', right: '5%', top: '18%', bottom: '12%' },
      xAxis: { type: 'category', data: dates, axisLine: { lineStyle: { color: '#2a3040' } }, axisLabel: { color: '#8b95a7' } },
      yAxis: { scale: true, splitLine: { lineStyle: { color: '#1a1f2e' } }, axisLine: { lineStyle: { color: '#2a3040' } }, axisLabel: { color: '#8b95a7' } },
      series: [
        { name: 'DIF', type: 'line', data: macdData.dif, lineStyle: { width: 1.5, color: '#ffc53d' }, symbol: 'none' },
        { name: 'DEA', type: 'line', data: macdData.dea, lineStyle: { width: 1.5, color: '#3491fa' }, symbol: 'none' },
        {
          name: 'MACD',
          type: 'bar',
          data: macdData.macd.map(v => ({
            value: v,
            itemStyle: { color: v >= 0 ? '#f53f3f' : '#00b42a' }
          }))
        }
      ]
    });
  },

  renderKdjChart(klines) {
    const kdjData = this.calcKDJ(klines, 9, 3, 3);
    const dates = klines.map(k => k.date);

    this.state.kdjChart.setOption({
      backgroundColor: 'transparent',
      tooltip: { trigger: 'axis' },
      legend: { data: ['K', 'D', 'J'], textStyle: { color: '#8b95a7' } },
      grid: { left: '10%', right: '5%', top: '18%', bottom: '12%' },
      xAxis: { type: 'category', data: dates, axisLine: { lineStyle: { color: '#2a3040' } }, axisLabel: { color: '#8b95a7' } },
      yAxis: { min: 0, max: 100, splitLine: { lineStyle: { color: '#1a1f2e' } }, axisLine: { lineStyle: { color: '#2a3040' } }, axisLabel: { color: '#8b95a7' } },
      series: [
        { name: 'K', type: 'line', data: kdjData.k, lineStyle: { width: 1.5, color: '#ffc53d' }, symbol: 'none' },
        { name: 'D', type: 'line', data: kdjData.d, lineStyle: { width: 1.5, color: '#3491fa' }, symbol: 'none' },
        { name: 'J', type: 'line', data: kdjData.j, lineStyle: { width: 1.5, color: '#b71ec5' }, symbol: 'none' }
      ]
    });
  },

  renderBollChart(klines) {
    const bollData = this.calcBOLL(klines, 20, 2);
    const dates = klines.map(k => k.date);
    const ohlc = klines.map(k => [k.open, k.close, k.low, k.high]);

    this.state.bollChart.setOption({
      backgroundColor: 'transparent',
      tooltip: { trigger: 'axis', axisPointer: { type: 'cross' } },
      legend: { data: ['K线', '上轨', '中轨', '下轨'], textStyle: { color: '#8b95a7' } },
      grid: { left: '8%', right: '3%', top: '12%', bottom: '10%' },
      xAxis: { type: 'category', data: dates, axisLine: { lineStyle: { color: '#2a3040' } }, axisLabel: { color: '#8b95a7' } },
      yAxis: { scale: true, splitLine: { lineStyle: { color: '#1a1f2e' } }, axisLine: { lineStyle: { color: '#2a3040' } }, axisLabel: { color: '#8b95a7' } },
      dataZoom: [{ type: 'inside', start: 50, end: 100 }],
      series: [
        {
          name: 'K线',
          type: 'candlestick',
          data: ohlc,
          itemStyle: { color: '#f53f3f', color0: '#00b42a', borderColor: '#f53f3f', borderColor0: '#00b42a' }
        },
        { name: '上轨', type: 'line', data: bollData.upper, lineStyle: { width: 1, color: '#b71ec5' }, symbol: 'none' },
        { name: '中轨', type: 'line', data: bollData.middle, lineStyle: { width: 1, color: '#ffc53d' }, symbol: 'none' },
        { name: '下轨', type: 'line', data: bollData.lower, lineStyle: { width: 1, color: '#3491fa' }, symbol: 'none' }
      ]
    });
  },

  calcMA(klines, period) {
    const result = [];
    for (let i = 0; i < klines.length; i++) {
      if (i < period - 1) {
        result.push(null);
      } else {
        let sum = 0;
        for (let j = 0; j < period; j++) {
          sum += klines[i - j].close;
        }
        result.push(parseFloat((sum / period).toFixed(2)));
      }
    }
    return result;
  },

  calcEMA(data, period) {
    const result = [];
    let ema = data[0];
    const k = 2 / (period + 1);
    for (let i = 0; i < data.length; i++) {
      if (i === 0) {
        ema = data[i];
      } else {
        ema = data[i] * k + ema * (1 - k);
      }
      result.push(parseFloat(ema.toFixed(4)));
    }
    return result;
  },

  calcMACD(klines, fast, slow, signal) {
    const closes = klines.map(k => k.close);
    const emaFast = this.calcEMA(closes, fast);
    const emaSlow = this.calcEMA(closes, slow);
    const dif = emaFast.map((v, i) => parseFloat((v - emaSlow[i]).toFixed(4)));
    const dea = this.calcEMA(dif, signal);
    const macd = dif.map((v, i) => parseFloat(((v - dea[i]) * 2).toFixed(4)));
    return { dif, dea, macd };
  },

  calcKDJ(klines, n, m1, m2) {
    const k = [], d = [], j = [];
    let rsvArr = [];

    for (let i = 0; i < klines.length; i++) {
      if (i < n - 1) {
        k.push(50);
        d.push(50);
        j.push(50);
        continue;
      }
      let low = Infinity, high = -Infinity;
      for (let t = 0; t < n; t++) {
        low = Math.min(low, klines[i - t].low);
        high = Math.max(high, klines[i - t].high);
      }
      const rsv = high === low ? 50 : ((klines[i].close - low) / (high - low)) * 100;
      rsvArr.push(rsv);
      const kVal = (rsv + (m1 - 1) * k[i - 1]) / m1;
      const dVal = (kVal + (m2 - 1) * d[i - 1]) / m2;
      const jVal = 3 * kVal - 2 * dVal;
      k.push(parseFloat(kVal.toFixed(2)));
      d.push(parseFloat(dVal.toFixed(2)));
      j.push(parseFloat(jVal.toFixed(2)));
    }
    return { k, d, j };
  },

  calcBOLL(klines, period, k) {
    const middle = this.calcMA(klines, period);
    const upper = [], lower = [];
    for (let i = 0; i < klines.length; i++) {
      if (middle[i] === null) {
        upper.push(null);
        lower.push(null);
        continue;
      }
      let sum = 0;
      for (let j = 0; j < period; j++) {
        sum += Math.pow(klines[i - j].close - middle[i], 2);
      }
      const std = Math.sqrt(sum / period);
      upper.push(parseFloat((middle[i] + k * std).toFixed(2)));
      lower.push(parseFloat((middle[i] - k * std).toFixed(2)));
    }
    return { upper, middle, lower };
  },

  formatVolume(vol) {
    if (!vol) return '0';
    if (vol >= 100000000) return (vol / 100000000).toFixed(2) + '亿';
    if (vol >= 10000) return (vol / 10000).toFixed(2) + '万';
    return vol.toFixed(0);
  },

  formatAmount(amt) {
    if (!amt) return '0';
    if (amt >= 100000000) return (amt / 100000000).toFixed(2) + '亿';
    if (amt >= 10000) return (amt / 10000).toFixed(2) + '万';
    return amt.toFixed(0);
  }
};

document.addEventListener('DOMContentLoaded', () => {
  App.init();
});
