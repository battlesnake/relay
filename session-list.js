const Component = require('component');
const Session = require('./session');

const wildcard_to_regexp = require('./wildcard_to_regexp');

module.exports = SessionList;

const wildcard_rx = /[*?]/;

SessionList.prototype = new Component();
function SessionList() {
	Component.call(this, 'Session list', true);

	const lists = new Map();
	/* Get by regular expression */
	const get_rx = pattern => [...lists.keys()]
			.filter(key => pattern.test(key))
			.map(key => lists.get(key))
			.reduce((xs, x) => new Set([...xs, ...x]), new Set());
	/* Get by name */
	const get_name = name => lists.has(name) ? lists.get(name) : new Set();
	/* Get by name or by wildcard */
	const get = name => wildcard_rx.test(name) ? get_rx(wildcard_to_regexp(name)) : get_name(name);
	/* Remove client */
	const remove = client => {
		const name = client.getName();
		if (name === null || !lists.has(name)) {
			return;
		}
		const list = lists.get(name);
		list.delete(client);
		if (list.size === 0) {
			lists.delete(name);
		}
		client.close();
	};
	/* Called when a client is ready */
	const on_client_ready = client => {
		const name = client.getName();
		if (name === null) {
			this.warn({ msg: 'Attempted to register client with no name' });
			client.close();
			return;
		}
		if (wildcard_rx.test(name)) {
			this.warn({ msg: 'Attempted to register client with wildcards in name' });
			client.close();
			return;
		}
		if (!lists.has(name)) {
			lists.set(name, new Set([client]));
		} else {
			lists.get(name).add(client);
		}
		this.$on(client, 'close', () => remove(client));
		this.info({ msg: `Registering client ${name} at ${client.getAddr()}` });
	};
	/* Called when a client closes */
	const on_client_close = client => {
		if (client.getName()) {
			this.info({ msg: `Unregistering client ${client.getName()} at ${client.getAddr()}` });
		} else {
			this.info({ msg: `Unregistered connection ${client.getAddr()} terminated by remote` });
		}
	};
	/* Bind a client but do not add to list */
	const create = (socket, opts) => {
		const client = new Session(socket, opts);
		this.bind(client, true);
		this.$on(client, 'close', () => on_client_close(client));
		client.wait_for_ready().then(() => on_client_ready(client));
		return client;
	};

	this.create = create;
	this.get = get;
	this.remove = remove;
	this.$on(this, 'close', () => lists.clear());
}
