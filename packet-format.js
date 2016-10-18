const EventEmitter = require('eventemitter');
const ByteStream = require('byte-stream');

const TYPE_LEN = 4;
const ENDPOINT_NAME_LEN = 16;
const LENGTH_LEN = 4;

const TARGET_LEN = ENDPOINT_NAME_LEN;
const ORIGIN_LEN = ENDPOINT_NAME_LEN;

const TYPE_OFFSET = 0;
const TARGET_OFFSET = TYPE_OFFSET + TYPE_LEN;
const ORIGIN_OFFSET = TARGET_OFFSET + TARGET_LEN;
const LENGTH_OFFSET = ORIGIN_OFFSET + ORIGIN_LEN;
const DATA_OFFSET = LENGTH_OFFSET + LENGTH_LEN;

/* Bit 30 instead of 31, since bitwise arithmetic in Java* languages is shite */
const FOREIGN_BIT = 1<<30;

/* Read null-terminated ASCII string from buffer */
const read_str = buf => {
	let len = buf.indexOf(0);
	if (len === -1) {
		len = buf.length;
	}
	return buf.slice(0, len).toString('ascii');
};

const write_str = (buf, str, offset, length) =>
	buf.write(str.substr(0, length), offset, Math.min(str.length, length), 'ascii');

module.exports.Reader = Reader;
module.exports.Writer = Writer;

/* Basically an asynchronous fold over the input stream */
Reader.prototype = new EventEmitter();
function Reader() {
	EventEmitter.call(this);

	const stream = new ByteStream();

	const newPacket = () => ({
		type: null,
		remote: null,
		local: null,
		length: null,
		data: null,
		foreign: false
	});

	let packet = newPacket();

	const readPacket = () => {
		if (packet.type === null) {
			const buf = stream.read(TYPE_LEN);
			if (!buf) {
				return false;
			}
			packet.type = read_str(buf);
		}
		if (packet.remote === null) {
			const buf = stream.read(TARGET_LEN);
			if (!buf) {
				return false;
			}
			packet.remote = read_str(buf);
		}
		if (packet.local === null) {
			const buf = stream.read(ORIGIN_LEN);
			if (!buf) {
				return false;
			}
			packet.local = read_str(buf);
		}
		if (packet.length === null) {
			const buf = stream.read(LENGTH_LEN);
			if (!buf) {
				return false;
			}
			const length = buf.readUInt32BE(0);
			packet.foreign = (length & FOREIGN_BIT) !== 0;
			packet.length = length & ~FOREIGN_BIT;
		}
		const buf = packet.length ? stream.read(packet.length) : new Buffer([]);
		if (!buf) {
			return false;
		}
		packet.data = buf;
		return true;
	};

	const streamOnData = () => {
		while (readPacket()) {
			this.emit('data', packet);
			packet = newPacket();
		}
	};

	stream.on('data', streamOnData);
	this.write = buf => stream.write(buf);
}

Writer.prototype = new EventEmitter();
function Writer() {
	EventEmitter.call(this);

	const write = packet => {
		let { data } = packet;
		if (typeof data !== 'string' && !(data instanceof Buffer)) {
			throw new Error('Invalid packet data');
		}
		if (typeof data === 'string') {
			data = Buffer.from(data);
		}
		const { type, remote, local, length = data.length, foreign = false } = packet;
		if (data.length !== length) {
			throw new Error(`Packet length mismatch: ${data.length} != ${length}`);
		}
		if (typeof type !== 'string' || type.length > TYPE_LEN) {
			throw new Error(`Invalid packet type: ${JSON.stringify(type)}`);
		}
		if (typeof remote !== 'string' || remote.length > TARGET_LEN) {
			throw new Error(`Invalid packet remote: ${JSON.stringify(remote)}`);
		}
		if (typeof local !== 'string' || local.length > ORIGIN_LEN) {
			throw new Error(`Invalid packet local: ${JSON.stringify(local)}`);
		}
		if (typeof length !== 'number' || length < 0) {
			throw new Error(`Invalid packet length: ${JSON.stringify(length)}`);
		}
		const buf = Buffer.alloc(DATA_OFFSET + length, 0);
		const lenfield = length | (foreign ? FOREIGN_BIT : 0);
		write_str(buf, type, TYPE_OFFSET, TYPE_LEN);
		write_str(buf, remote, TARGET_OFFSET, TARGET_LEN);
		write_str(buf, local, ORIGIN_OFFSET, ORIGIN_LEN);
		buf.writeInt32BE(lenfield, LENGTH_OFFSET);
		data.copy(buf, DATA_OFFSET);
		this.emit('data', buf);
	};

	this.write = write;
}

if (!module.parent) {
	const test_timeout = 50;
	const test = expect => new Promise((done, err) => {
		const reader = new Reader();
		const writer = new Writer();
		const timeout = setTimeout(() => err('Timeout'), test_timeout);
		reader.on('error', err);
		writer.on('error', err);
		writer.on('data', buf => reader.write(buf));
		reader.on('data', actual => {
			console.log(JSON.stringify(expect));
			actual.data = actual.data.toString();
			console.log(JSON.stringify(actual));
			clearTimeout(timeout);
			done();
		});
		writer.write(expect);
	});
	const samples = [
		{ type: 'halo', local: 'red', remote: 'blue', data: 'I have a message for you', foreign: true },
		{ type: 'ND', local: 'me', remote: 'you', data: '' },
		{ type: 'NR', local: 'me', remote: '', data: 'Data' },
		{ type: 'NDR', local: 'me', remote: '', data: '' },
		{ type: 'AUTH', local: 'me', remote: '', data: '', foreign: false },
		{ type: 'AUTH', local: 'me', remote: '', data: '', foreign: true }
	];
	const next = () => {
		if (samples.length) {
			const sample = samples.shift();
			console.log('----------------------------------------');
			test(sample).catch(console.error).then(next);
		}
	};
	next();
}
