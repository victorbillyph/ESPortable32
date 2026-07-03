const WS = {
    socket: null,
    handlers: {},
    connect() {
        const proto = location.protocol === 'https:' ? 'wss:' : 'ws:';
        const url = proto + '//' + location.host + '/ws';
        this.socket = new WebSocket(url);
        this.socket.onopen = () => { console.log('[WS] Connected'); };
        this.socket.onclose = () => {
            console.log('[WS] Disconnected, reconnecting...');
            setTimeout(() => WS.connect(), 2000);
        };
        this.socket.onmessage = (e) => {
            try {
                const msg = JSON.parse(e.data);
                if (msg.type && this.handlers[msg.type]) {
                    this.handlers[msg.type](msg.data);
                }
            } catch (err) {
                console.warn('[WS] Parse error:', err);
            }
        };
    },
    on(type, fn) { this.handlers[type] = fn; },
    send(data) {
        if (this.socket && this.socket.readyState === WebSocket.OPEN) {
            this.socket.send(JSON.stringify(data));
        }
    }
};
