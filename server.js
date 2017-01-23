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
	port: 49501,
	keepAliveInterval: 10000,
	noDelay: true
};

Server.prototype = new EventEmitter();
function Server(opts) {
	EventEmitter.call(this);

	opts = _.defaults({}, opts, defaultOpts);

	const clients = new SessionList();

	const accept = socket => {

		socket.setKeepAlive(!!opts.keepAliveInterval, opts.keepAliveInterval);
		socket.setNoDelay(!!opts.noDelay);

		const client = new Session(socket, opts);

		const onPacket = packet => {
			const targets = clients.get(packet.remote);
			if (process.env.LOG_PACKETS) {
				const real_origin = client.getName();
				const claim_origin = packet.local;
				const local = real_origin === claim_origin ? real_origin : `${real_origin} ("${claim_origin}")`;
				console.log(`Packet of type ${packet.type} from ${local} to ${packet.remote} (x${[...targets].length})`);
			}
			packet.local = packet.remote;
			packet.remote = client.getName();
			for (const remote of targets) {
				remote.send(packet);
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
	const port = +process.env.PORT || defaultOpts.port;
	const server = new Server(port, host);
	server.on('listening', () => console.log(`Listening on ${host}:${port}`));
	server.on('info', console.info);
	server.on('error', err => process.env.DEBUG ? console.error(err) : console.error(`ERROR: ${err.message}`));
}
