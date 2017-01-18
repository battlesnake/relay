#!/usr/bin/env node

'use strict';

const net = require('net');
const _ = require('lodash');
const EventEmitter = require('eventemitter');

const packetFormat = require('./packet-format');

module.exports = Client;

const defaultOpts = {
	port: 49501
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

	socket.connect(opts.port, opts.server, () => {
		writer.write({ type: 'AUTH', local: opts.local, remote: '', data: opts.local });
		this.emit('open');
	});

	socket.on('data', buf => reader.write(buf));
	reader.on('data', packet => this.emit('data', packet));

	this.write = packet => writer.write(packet);
	writer.on('data', buf => socket.write(buf));

	socket.on('close', close);
	socket.on('error', err => {
		this.emit('error', err);
		close();
	});

	this.close = close;
}

if (!module.parent) {
	const server = process.env.SERVER || 'localhost';
	const port = +process.env.PORT || 49501;
	const local = 'echo';
	const client = new Client({ server, port, local });
	client.on('info', console.info);
	client.on('error', console.error);
	client.on('open', () => {
		client.write({ type: 'DATA', local, remote: 'echo', data: 'hello world' });
		console.info('  [SEND] echo request');
	});
	client.on('data', packet => {
		const { type, local, remote } = packet;
		if (type === 'DATA') {
			client.write({ type: 'ECHO', local: remote, remote: local, data: packet.data });
			console.info(`  [ECHO] remote="${local}"`);
		} else {
			console.info(`  [RECV] type="${packet.type}" local="${local}" remote="${remote}" data="${packet.data.toString()}"`);
		}
	});
}
