const { app, BrowserWindow, ipcMain } = require('electron');
const path = require('path');
const { spawn } = require('child_process');
const Store = require('electron-store');

const store = new Store();

let mainWindow = null;
let backendProcess = null;
const BACKEND_PORT = 8888;

function getBackendPath() {
  if (app.isPackaged) {
    return path.join(process.resourcesPath, 'quant-server.exe');
  }
  return path.join(__dirname, '..', 'backend', 'build', 'Release', 'quant-server.exe');
}

function startBackend() {
  const backendPath = getBackendPath();
  console.log('Starting backend from:', backendPath);

  try {
    backendProcess = spawn(backendPath, [`--port=${BACKEND_PORT}`], {
      stdio: ['ignore', 'pipe', 'pipe'],
      windowsHide: true
    });

    backendProcess.stdout.on('data', (data) => {
      console.log('[Backend]', data.toString().trim());
    });

    backendProcess.stderr.on('data', (data) => {
      console.error('[Backend Error]', data.toString().trim());
    });

    backendProcess.on('close', (code) => {
      console.log(`Backend exited with code ${code}`);
      backendProcess = null;
    });

    backendProcess.on('error', (err) => {
      console.error('Failed to start backend:', err.message);
    });
  } catch (err) {
    console.error('Failed to spawn backend:', err.message);
  }
}

function stopBackend() {
  if (backendProcess) {
    try {
      backendProcess.kill('SIGTERM');
    } catch (e) {
      console.error('Error killing backend:', e);
    }
    backendProcess = null;
  }
}

function createWindow() {
  mainWindow = new BrowserWindow({
    width: 1400,
    height: 900,
    minWidth: 1024,
    minHeight: 680,
    title: '量化选股工具',
    webPreferences: {
      preload: path.join(__dirname, 'preload.js'),
      contextIsolation: true,
      nodeIntegration: false
    }
  });

  mainWindow.loadFile(path.join(__dirname, 'src', 'index.html'));

  mainWindow.on('closed', () => {
    mainWindow = null;
  });
}

ipcMain.handle('get-backend-port', () => {
  return BACKEND_PORT;
});

ipcMain.handle('get-favorites', () => {
  return store.get('favorites', []);
});

ipcMain.handle('add-favorite', (_, code, name) => {
  const favorites = store.get('favorites', []);
  if (!favorites.find(f => f.code === code)) {
    favorites.push({ code, name });
    store.set('favorites', favorites);
  }
  return true;
});

ipcMain.handle('remove-favorite', (_, code) => {
  let favorites = store.get('favorites', []);
  favorites = favorites.filter(f => f.code !== code);
  store.set('favorites', favorites);
  return true;
});

ipcMain.handle('is-favorite', (_, code) => {
  const favorites = store.get('favorites', []);
  return favorites.some(f => f.code === code);
});

app.whenReady().then(() => {
  startBackend();
  createWindow();

  app.on('activate', () => {
    if (BrowserWindow.getAllWindows().length === 0) {
      createWindow();
    }
  });
});

app.on('window-all-closed', () => {
  stopBackend();
  if (process.platform !== 'darwin') {
    app.quit();
  }
});

app.on('before-quit', () => {
  stopBackend();
});

process.on('exit', () => {
  stopBackend();
});
