const APPS = {
    list: [],
    registry: {},
    register(app) {
        this.registry[app.id] = app;
        if (!this.list.find(a => a.id === app.id)) {
            this.list.push(app);
        }
    },
    get(id) { return this.registry[id]; },
    getDefaultApps() {
        return [
            { id: 'store', name: 'Loja', icon: '🛒', desc: 'Instalar apps', url: '/apps/store.html' },
            { id: 'gpio', name: 'GPIO', icon: '💡', desc: 'Controlar pinos', url: '/apps/gpio.html' },
            { id: 'editor', name: 'Editor', icon: '✏️', desc: 'Editor de texto', url: '/apps/editor.html' },
            { id: 'proxy', name: 'Proxy', icon: '🌐', desc: 'Proxy HTTP', url: '/apps/proxy.html' },
            { id: 'monitor', name: 'Monitor', icon: '📊', desc: 'Monitor do sistema', url: '/apps/monitor.html' }
        ];
    },
    load() {
        const defaults = this.getDefaultApps();
        defaults.forEach(a => this.register(a));
        this.loadInstalled();
        return this.list;
    },
    async loadInstalled() {
        try {
            const data = await API.apps();
            if (data && data.apps) {
                data.apps.forEach(a => {
                    if (!this.registry[a.id]) {
                        this.register({
                            id: a.id,
                            name: a.id.charAt(0).toUpperCase() + a.id.slice(1),
                            icon: '📦',
                            desc: 'App instalado (' + a.size + ' B)',
                            url: a.path.startsWith('/') ? a.path : '/apps/' + a.path
                        });
                    }
                });
            }
        } catch (e) {
            console.warn('[Apps] Failed to load installed:', e);
        }
    },
    launch(id, desktop) {
        const app = this.get(id);
        if (!app) { console.warn('[Apps] App not found:', id); return; }
        if (desktop) {
            desktop.openWindow(app.name, app.url, app.icon);
        }
    }
};

APPS.load();
