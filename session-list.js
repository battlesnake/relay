module.exports = SessionList;

function SessionList() {
	const lists = new Map();
	const get = name => {
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
