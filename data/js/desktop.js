let windows = [];
let windowZIndex = 100;
let nextWindowId = 1;

function updateClock() {
    const now = new Date();
    document.getElementById('clock').textContent =
        now.getHours().toString().padStart(2,'0') + ':' +
        now.getMinutes().toString().padStart(2,'0');
}
setInterval(updateClock, 10000);
updateClock();

function toggleMenu() {
    document.getElementById('startMenu').classList.toggle('open');
    document.getElementById('menuOverlay').classList.toggle('open');
}

function renderAppList() {
    const list = document.getElementById('appList');
    APPS.list.forEach(app => {
        const div = document.createElement('div');
        div.className = 'app-item';
        div.innerHTML = `<div class="icon">${app.icon || '📦'}</div>
            <div><div class="name">${app.name}</div>
            <div class="desc">${app.desc || ''}</div></div>`;
        div.onclick = () => { toggleMenu(); APPS.launch(app.id, DESKTOP); };
        list.appendChild(div);
    });
}

function renderDesktopIcons() {
    const desktop = document.getElementById('desktop');
    const container = document.createElement('div');
    container.style.cssText = 'display:flex;flex-wrap:wrap;gap:8px;padding:16px;align-content:flex-start';
    APPS.list.forEach(app => {
        const div = document.createElement('div');
        div.className = 'desktop-icon';
        div.innerHTML = `<div class="icon">${app.icon || '📦'}</div>
            <div class="label">${app.name}</div>`;
        div.onclick = () => APPS.launch(app.id, DESKTOP);
        container.appendChild(div);
    });
    desktop.insertBefore(container, desktop.firstChild);
}

const DESKTOP = {
    openWindow(title, url, icon) {
        const id = 'win_' + (nextWindowId++);
        const win = document.createElement('div');
        win.className = 'window';
        win.id = id;
        win.style.zIndex = ++windowZIndex;
        win.style.width = Math.min(800, window.innerWidth - 40) + 'px';
        win.style.height = Math.min(500, window.innerHeight - 80) + 'px';
        win.style.top = (30 + (windows.length * 20) % 100) + 'px';
        win.style.left = (30 + (windows.length * 20) % 100) + 'px';

        win.innerHTML = `
            <div class="window-header">
                <div class="window-title">${icon || ''} ${title}</div>
                <div class="window-controls">
                    <button onclick="DESKTOP.minimizeWindow('${id}')">─</button>
                    <button onclick="DESKTOP.maximizeWindow('${id}')">□</button>
                    <button class="close" onclick="DESKTOP.closeWindow('${id}')">✕</button>
                </div>
            </div>
            <div class="window-content"></div>
            <div class="window-statusbar"><span>${title}</span></div>
        `;

        document.getElementById('windowContainer').appendChild(win);
        this.makeDraggable(win);

        const iframe = document.createElement('iframe');
        iframe.src = url;
        win.querySelector('.window-content').appendChild(iframe);

        const taskBtn = document.createElement('button');
        taskBtn.className = 'taskbar-app active';
        taskBtn.textContent = title;
        taskBtn.id = 'task_' + id;
        taskBtn.onclick = () => this.toggleWindow(id);

        document.getElementById('taskbar-center').appendChild(taskBtn);

        windows.push({ id, title, taskBtn, win, minimized: false });

        this.focusWindow(id);

        // Handle messages from iframe (SDK)
        window.addEventListener('message', (e) => {
            const msg = e.data;
            if (!msg || !msg._esp) return;
            if (msg._type === 'init') {
                e.source.postMessage({ _esp: true, _type: 'ready' }, e.origin);
            }
            if (msg._type === 'close') {
                this.closeWindow(id);
            }
            if (msg._type === 'setTitle') {
                win.querySelector('.window-title').textContent = msg.title;
                taskBtn.textContent = msg.title;
            }
        });

        return id;
    },

    closeWindow(id) {
        const idx = windows.findIndex(w => w.id === id);
        if (idx === -1) return;
        const w = windows[idx];
        w.win.remove();
        w.taskBtn.remove();
        windows.splice(idx, 1);
    },

    minimizeWindow(id) {
        const w = windows.find(w => w.id === id);
        if (!w) return;
        w.minimized = !w.minimized;
        w.win.classList.toggle('minimized');
        w.taskBtn.classList.toggle('active');
    },

    maximizeWindow(id) {
        const w = windows.find(w => w.id === id);
        if (!w) return;
        w.win.classList.toggle('maximized');
    },

    toggleWindow(id) {
        const w = windows.find(w => w.id === id);
        if (!w) return;
        if (w.minimized) {
            w.minimized = false;
            w.win.classList.remove('minimized');
            w.taskBtn.classList.add('active');
            this.focusWindow(id);
        } else if (w.win.classList.contains('minimized')) {
            w.win.classList.remove('minimized');
            w.taskBtn.classList.add('active');
        } else {
            this.minimizeWindow(id);
        }
    },

    focusWindow(id) {
        const w = windows.find(w => w.id === id);
        if (!w) return;
        w.win.style.zIndex = ++windowZIndex;
    },

    makeDraggable(win) {
        const header = win.querySelector('.window-header');
        let dragging = false, startX, startY, startLeft, startTop;

        header.addEventListener('mousedown', (e) => {
            if (e.target.tagName === 'BUTTON') return;
            this.focusWindow(win.id);
            dragging = true;
            const rect = win.getBoundingClientRect();
            startX = e.clientX;
            startY = e.clientY;
            startLeft = rect.left;
            startTop = rect.top;
            header.classList.add('dragging');
            e.preventDefault();
        });

        document.addEventListener('mousemove', (e) => {
            if (!dragging) return;
            win.style.left = (startLeft + e.clientX - startX) + 'px';
            win.style.top = (startTop + e.clientY - startY) + 'px';
        });

        document.addEventListener('mouseup', () => {
            if (dragging) {
                dragging = false;
                header.classList.remove('dragging');
            }
        });
    }
};

// Check if locked
async function checkLock() {
    try {
        const status = await API.status();
        if (status.locked) {
            document.getElementById('lockScreen').style.display = 'flex';
        } else {
            document.getElementById('lockScreen').style.display = 'none';
            renderAppList();
            renderDesktopIcons();
            WS.connect();
            WS.on('status', (data) => {
                console.log('[WS] Status update:', data);
            });
        }
    } catch (e) {
        console.warn('Status check failed:', e);
        setTimeout(checkLock, 2000);
    }
}

async function unlock() {
    const pin = document.getElementById('lockPin').value;
    try {
        const r = await API.unlock(pin);
        if (r.status === 'ok') {
            document.getElementById('lockScreen').style.display = 'none';
            renderAppList();
            renderDesktopIcons();
            WS.connect();
        } else {
            document.getElementById('lockError').textContent = 'PIN incorreto';
        }
    } catch (e) {
        document.getElementById('lockError').textContent = 'Erro de conexão';
    }
}

document.getElementById('lockPin').addEventListener('keydown', (e) => {
    if (e.key === 'Enter') unlock();
});

checkLock();
