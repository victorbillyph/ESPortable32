const APPS = {
    list: [],
    registry: {},
    register(app) {
        this.registry[app.id] = app;
        this.list.push(app);
    },
    get(id) { return this.registry[id]; },
    getDefaultApps() {
        return [
            { id: 'store', name: 'Loja', icon: '🛒', desc: 'Instalar apps', url: '/apps/store.html' },
            { id: 'editor', name: 'Editor', icon: '✏️', desc: 'Editor de texto', url: '/apps/editor.html' },
            { id: 'proxy', name: 'Proxy', icon: '🌐', desc: 'Proxy HTTP', url: '/apps/proxy.html' }
        ];
    },
    load() {
        const defaults = this.getDefaultApps();
        defaults.forEach(a => this.register(a));
        return this.list;
    },
    launch(id, desktop) {
        const app = this.get(id);
        if (!app) { console.warn('App not found:', id); return; }
        if (desktop) {
            desktop.openWindow(app.name, app.url, app.icon);
        }
    }
};

APPS.load();
