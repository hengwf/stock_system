const API = {
  baseUrl: '',

  async init() {
    const port = await window.electronAPI.getBackendPort();
    this.baseUrl = `http://127.0.0.1:${port}`;
  },

  async request(path, options = {}) {
    if (!this.baseUrl) {
      await this.init();
    }
    const url = this.baseUrl + path;
    try {
      const resp = await fetch(url, {
        headers: { 'Content-Type': 'application/json' },
        ...options
      });
      const data = await resp.json();
      if (data.code !== 0) {
        throw new Error(data.message || '请求失败');
      }
      return data.data;
    } catch (err) {
      console.error('API request failed:', url, err);
      throw err;
    }
  },

  async health() {
    return this.request('/api/health');
  },

  async getMarketList(params = {}) {
    const query = new URLSearchParams(params).toString();
    return this.request(`/api/market/list?${query}`);
  },

  async getStockRealtime(codes) {
    const query = new URLSearchParams({ codes: codes.join(',') }).toString();
    return this.request(`/api/stock/realtime?${query}`);
  },

  async getStockDetail(code) {
    return this.request(`/api/stock/detail?code=${code}`);
  },

  async getKline(code, type = 'day', count = 200) {
    return this.request(`/api/stock/kline?code=${code}&type=${type}&count=${count}`);
  },

  async getFundFlow(code) {
    return this.request(`/api/stock/fund_flow?code=${code}`);
  },

  async runSelector(period) {
    return this.request('/api/selector/run', {
      method: 'POST',
      body: JSON.stringify({ period })
    });
  },

  async getSelectorResult(period) {
    return this.request(`/api/selector/result?period=${period}`);
  }
};
