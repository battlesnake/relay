#!/usr/bin/env node

'use strict';

const net = require('net');
const _ = require('lodash');
const EventEmitter = require('eventemitter');

const packetFormat = require('./packet-format');

module.exports = Client;

const defaultOpts = {
	port: 3031
};

Client.prototype = new EventEmitter();
function Client(opts) {
	EventEmitter.call(this);

	opts = _.defaults({}, opts, defaultOpts);

	if (typeof opts.local !== 'string' || !opts.local.length) {
		throw new Error('Invalid local name');
	}

	const socket = new net.Socket();

	const reader = new packetFormat.Reader();
	const writer = new packetFormat.Writer();

	let closing = false;

	const close = () => {
		if (!closing) {
			return;
		}
		closing = true;
		socket.destroy();
		this.emit('close');
	};

	socket.on('data', buf => reader.write(buf));
	reader.once('data', ({ type }) => {
		if (type !== 'AUTH') {
			this.emit('error', 'Authentication handshake failed');
			this.close();
		} else {
			reader.on('data', packet => this.emit('data', packet));
			this.emit('open');
		}
	});

	this.write = packet => writer.write(packet);
	writer.on('data', buf => socket.write(buf));

	socket.on('close', close);
	socket.on('error', err => {
		this.emit('error', err);
		close();
	});

	this.close = close;

	socket.connect(opts.port, opts.server, () => {
		writer.write({ type: 'AUTH', local: opts.local, remote: '', data: opts.local });
	});
}

if (!module.parent) {
	/* Server address */
	const server = process.env.SERVER || 'localhost';
	/* Server port */
	const port = +process.env.PORT || defaultOpts.port;
	/* Create "red" client */
	const red = new Client({ server, port, local: 'red' });
	red.on('info', console.info);
	red.on('error', console.error);
	/* Create blue client */
	const blue = new Client({ server, port, local: 'blue' });
	blue.on('info', console.info);
	blue.on('error', console.error);
	/* Add event handlers for ping/pong */
	red.on('data', packet => {
		const { type, local, remote, data } = packet;
		console.info(`  [RECV] type="${type}" local="${local}" remote="${remote}" data="${data.toString()}"`);
		red.close();
	});
	blue.on('data', packet => {
		const { type, local, remote, data } = packet;
		blue.write({ type: 'PONG', local, remote, data });
		console.info(`  [ECHO] type="${type}" local="${local}" remote="${remote}" data="${data.toString()}"`);
		blue.close();
	});
	/* When both connections are opened, send the ping */
	let opened = 0;
	const one_opened = () => {
		if (++opened < 2) {
			return;
		}
		red.write({ type: 'PING', local: 'red', remote: 'blue', data: 'hello world' });
		console.info('  [SEND] echo request');
	};
	blue.on('open', one_opened);
	red.on('open', one_opened);
}
