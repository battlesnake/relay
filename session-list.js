const wildcard_to_regexp = require('./wildcard_to_regexp');

module.exports = SessionList;

const wildcard_rx = /[*?]/;

function SessionList() {
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
	};
	/* Add new client */
	const add = client => {
		const name = client.getName();
		if (name === null) {
			throw new Error('Attempted to register client with no name');
		}
		if (wildcard_rx.test(name)) {
			throw new Error('Attempted to register client with wildcards in name');
		}
		if (!lists.has(name)) {
			lists.set(name, new Set([client]));
		} else {
			lists.get(name).add(client);
		}
		client.on('close', () => remove(client));
	};
	this.get = get;
	this.add = add;
	this.remove = remove;
}
