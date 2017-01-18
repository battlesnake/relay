const EventEmitter = require('eventemitter');

const packetFormat = require('./packet-format');

const NAME_TIMEOUT = 5000;

module.exports = Session;

Session.STATE_CONNECTING = 0;
Session.STATE_OPEN = 1;
Session.STATE_CLOSED = 2;
Session.prototype = new EventEmitter();
function Session(socket, opts) {
	EventEmitter.call(this);

	let state = Session.STATE_CONNECTING;
	let name = null;

	const reader = new packetFormat.Reader();
	const writer = new packetFormat.Writer();

	let authTimer;

	const close = () => {
		const closing = state !== Session.STATE_CLOSED;
		if (!closing) {
			return;
		}
		state = Session.STATE_CLOSED;
		socket.destroy();
		this.emit('close');
	};

	const authTimeout = () => {
		this.emit('error', new Error('Authentication timeout'));
		close();
	};

	const authFailed = () => {
		this.emit('error', new Error('Authentication failed'));
		close();
	};

	const authCompleted = _name => {
		name = _name;
		state = 1;
		clearTimeout(authTimer);
		authTimer = null;
		this.emit('open');
	};

	const login = packet => {
		if (packet.type !== 'AUTH' || packet.remote.length) {
			this.emit('error', 'Invalid authentication packet');
			return authFailed();
		}
		const _name = packet.data.toString('ascii');
		if (_name !== packet.local) {
			this.emit('error', `Name does not match local: "${_name}" != "${packet.local}"`);
			return authFailed();
		}
		if (!opts.nameValidator(_name)) {
			this.emit('error', `Invalid name: "${_name}"`);
			return authFailed();
		}
		return authCompleted(_name);
	};

	const dispatch = packet => this.emit('data', packet);

	const onData = packet => {
		switch (state) {
		case Session.STATE_CONNECTING: return login(packet);
		case Session.STATE_OPEN: return dispatch(packet);
		}
		return null;
	};

	authTimer = setTimeout(authTimeout, NAME_TIMEOUT);

	socket.on('data', buf => reader.write(buf));
	reader.on('data', onData);

	this.send = packet => writer.write(packet);
	writer.on('data', buf => socket.write(buf));

	socket.on('close', close);
	socket.on('error', err => {
		this.emit('error', err);
		close();
	});

	this.close = close;

	this.getName = () => name;
	this.getState = () => ['connecting', 'open', 'closed'][state];
}
