const API = {
    base: '',
    async get(path) {
        const r = await fetch(this.base + path);
        if (!r.ok) throw new Error(r.statusText);
        return r.json();
    },
    async post(path, body) {
        const opts = { method: 'POST' };
        if (typeof body === 'string') {
            opts.headers = { 'Content-Type': 'application/x-www-form-urlencoded' };
            opts.body = body;
        } else if (body) {
            opts.headers = { 'Content-Type': 'application/json' };
            opts.body = JSON.stringify(body);
        }
        const r = await fetch(this.base + path, opts);
        return r.json();
    },
    async status() { return this.get('/api/status'); },
    async unlock(pin) { return this.post('/api/unlock', 'pin=' + encodeURIComponent(pin)); },
    async restart() { return this.post('/api/restart'); },
    async gpioList() { return this.get('/api/gpio'); },
    async gpioSet(pin, state) { return this.post('/api/gpio', { pin, state }); },
    async gpioSetMode(pin, mode) { return this.post('/api/gpio', { pin, mode }); },
    async fsList(path) { return this.get('/api/fs/list?path=' + encodeURIComponent(path || '/')); },
    async fsRead(path) { return this.get('/api/fs/read?path=' + encodeURIComponent(path)); },
    async fsWrite(path, content) { return this.post('/api/fs/write', { path, content }); },
    async fsDelete(path) { return fetch('/api/fs/delete?path=' + encodeURIComponent(path), { method: 'DELETE' }).then(r => r.json()); },
    async apps() { return this.get('/api/apps'); },
    async installApp(url, path) { return this.post('/api/apps', { url, path }); },
    async proxy(url) { return this.post('/api/proxy', { url }); }
};
