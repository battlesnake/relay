const wildcard_to_regexp = require('./wildcard_to_regexp');

module.exports = SessionList;

const wildcard_rx = /\*\?/;

function SessionList() {
	const lists = new Map();
	/* Get by regular expression */
	const get_rx = pattern => [...lists.keys()]
			.filter(key => pattern.test(key))
			.map(key => lists.get(key))
			.reduce((xs, x) => new Set([...xs, ...x]), new Set());
	/* Get by name or by wildcard */
	const get = name => {
		if (wildcard_rx.test(name)) {
			return get_rx(wildcard_to_regexp(name));
		}
		if (lists.has(name)) {
			return lists.get(name);
		} else {
			const list = new Set();
			lists.set(name, list);
			return list;
		}
	};
	const add = client => {
		const name = client.getName();
		if (name === null) {
			throw new Error('Attempted to register client with no name');
		}
		if (wildcard_rx.test(name)) {
			throw new Error('Attempted to register client with wildcards in name');
		}
		this.get(name).add(client);
	};
	const remove = client => {
		const name = client.getName();
		if (name === null) {
			return;
		}
		this.get(name).delete(client);
	};
	this.get = get;
	this.add = add;
	this.remove = remove;
}
