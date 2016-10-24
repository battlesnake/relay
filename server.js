#!/usr/bin/env node
'use strict';

const net = require('net');
const _ = require('lodash');
const EventEmitter = require('eventemitter');

const Session = require('./session');
const SessionList = require('./session-list');

module.exports = Server;

const defaultOpts = {
	nameValidator: name => /^\w[\w\d]{3,}$/.test(name),
	port: 49501
};

Server.prototype = new EventEmitter();
function Server(opts) {
	EventEmitter.call(this);

	opts = _.defaults({}, opts, defaultOpts);

	const clients = new SessionList();

	const accept = socket => {

		socket.setKeepAlive(true, 10000);
		socket.setNoDelay(true);

		const client = new Session(socket, opts);

		const onPacket = packet => {
			const targets = clients.get(packet.remote);
			if (process.env.LOG_PACKETS) {
				console.log(`Packet of type ${packet.type} from ${client.getName()} to ${packet.remote} (x${[...targets].length})`);
			}
			packet.remote = client.getName();
			for (let target of targets) {
				target.send(packet);
			}
		};

		const onOpen = () => {
			clients.add(client);
			this.emit('info', `Registering client ${client.getName()}`);
		};

		const onClose = () => {
			clients.remove(client);
			this.emit('info', `Unregistering client ${client.getName()}`);
		};

		client.on('open', onOpen);
		client.on('close', onClose);

		client.on('data', onPacket);

		client.on('error', error => this.emit('error', error));
		client.on('info', info => this.emit('info', info));
	};

	const server = net.createServer(accept);
	server.on('listening', () => this.emit('listening'));

	server.listen(opts.port);
}

if (!module.parent) {
	const host = process.env.HOST || '0.0.0.0';
	const port = +process.env.PORT || 49501;
	const server = new Server(port, host);
	server.on('listening', () => console.log(`Listening on ${host}:${port}`));
	server.on('info', console.info);
	server.on('error', err => process.env.DEBUG ? console.error(err) : console.error('ERROR: ' + err.message));
}
