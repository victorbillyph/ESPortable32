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
    async restart() { return this.post('/api/restart'); }
};
