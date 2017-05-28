const Component = require('component');
const PacketBuffer = require('./packet-buffer');
const packet_format = require('./packet-format');

/* How long to wait for login after connection accepted */
const NAME_TIMEOUT = 10000;

module.exports = Session;

Session.STATE_AUTHENTICATING = 0;
Session.STATE_OPENING = 1;
Session.STATE_OPEN = 2;
Session.STATE_CLOSED = 3;
Session.prototype = new Component();
function Session(socket, opts) {
	const addr = `${socket.remoteAddress}:${socket.remotePort}`;
	Component.call(this, `Session for ${addr}`, false);

	this.bind(socket);

	const reader = new packet_format.Reader();
	this.bind(reader);

	const writer = new packet_format.Writer();
	this.bind(writer);

	let state = Session.STATE_AUTHENTICATING;
	let name = null;

	let states;

	let authTimer;

	const set_state = value => {
		state = value;
		this.info({ msg: `${this.$component.name} -> ${states[state].name}` });
	};

	/* Cleanup */
	this.$on(this, 'close', () => {
		set_state(Session.STATE_CLOSED);
		socket.destroy();
	});

	const tx_queue = new PacketBuffer();
	this.bind(tx_queue, true);
	this.$on(tx_queue, 'flush', writer.write);

	const on_auth_timeout = () => {
		this.warn(new Error('Authentication timeout'));
		this.close();
	};

	const on_auth_failed = () => {
		this.warn(new Error('Authentication failed'));
		this.close();
	};

	const on_auth_completed = _name => {
		name = _name;
		clearTimeout(authTimer);
		authTimer = null;
		writer.write({ local: name, remote: '', type: 'AUTH', data: '' });
		set_state(Session.STATE_OPENING);
		this.info({ msg: `${addr} authenticated as "${name}"` });
		this.$component.rename(`Session for "${name}" @ ${addr}`);
		this.$component.ready();
		setTimeout(() => {
			this.emit('open');
			set_state(Session.STATE_OPEN);
			tx_queue.flush();
			tx_queue.close();
		}, 500);
	};

	const on_try_auth = packet => {
		if (packet.type !== 'AUTH' || packet.remote.length) {
			this.warn(new Error('Invalid authentication packet'));
			return on_auth_failed();
		}
		const _name = packet.data.toString('ascii');
		if (_name !== packet.local) {
			this.warn(new Error(`Name does not match local: "${_name}" != "${packet.local}"`));
			return on_auth_failed();
		}
		if (!opts.nameValidator(_name)) {
			this.warn(new Error(`Invalid name: "${_name}"`));
			return on_auth_failed();
		}
		return on_auth_completed(_name);
	};

	const emit_packet = packet => this.emit('data', packet);

	const drop_packet = type => packet => {
		this.info({ msg: `Dropping ${type} packet for "${name}"` });
		packet;
	};

	/* State operations */
	states = {
		[Session.STATE_AUTHENTICATING]: {
			name: 'authenticating',
			on_rx: on_try_auth,
			on_tx: tx_queue.push
		},
		[Session.STATE_OPENING]: {
			name: 'opening',
			on_rx: emit_packet,
			on_tx: tx_queue.push
		},
		[Session.STATE_OPEN]: {
			name: 'open',
			on_rx: emit_packet,
			on_tx: writer.write
		},
		[Session.STATE_CLOSED]: {
			name: 'closed',
			on_rx: drop_packet('rx'),
			on_tx: drop_packet('tx')
		}
	};

	authTimer = setTimeout(on_auth_timeout, NAME_TIMEOUT);

	/* socket -> reader -> (data) */
	this.$on(socket, 'data', buf => reader.write(buf));
	this.$on(reader, 'data', packet => states[state].on_rx(packet));

	/* (data) -> writer -> socket */
	this.$on(writer, 'data', buf => socket.write(buf));

	this.send = packet => states[state].on_tx(packet);

	this.getName = () => name;
	this.getState = () => states[state].name;
	this.getAddr = () => addr;
}
