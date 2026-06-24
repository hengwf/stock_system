const { contextBridge, ipcRenderer } = require('electron');

contextBridge.exposeInMainWorld('electronAPI', {
  getBackendPort: () => ipcRenderer.invoke('get-backend-port'),
  getFavorites: () => ipcRenderer.invoke('get-favorites'),
  addFavorite: (code, name) => ipcRenderer.invoke('add-favorite', code, name),
  removeFavorite: (code) => ipcRenderer.invoke('remove-favorite', code),
  isFavorite: (code) => ipcRenderer.invoke('is-favorite', code)
});
