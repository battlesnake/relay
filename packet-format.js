const EventEmitter = require('eventemitter');
const ByteStream = require('byte-stream');

const TYPE_LEN = 4;
const NAME_LEN = 8;
const LENGTH_LEN = 4;

const TYPE_OFFSET = 0;
const NAME_OFFSET = TYPE_OFFSET + TYPE_LEN;
const LENGTH_OFFSET = NAME_OFFSET + NAME_LEN;
const DATA_OFFSET = LENGTH_OFFSET + LENGTH_LEN;

/* Read null-terminated ASCII string from buffer */
const read_str = buf => buf.slice(0, buf.indexOf(0)).toString('ascii');

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
		length: null,
		data: null
	});

	let packet = newPacket();

	const streamOnData = () => {
		while (readPacket()) {
			this.emit('data', packet);
			packet = newPacket();
		}
	};

	const readPacket = () => {
		if (packet.type === null) {
			const buf = stream.read(TYPE_LEN);
			if (!buf) {
				return false;
			}
			packet.type = buf.toString('ascii');
		}
		if (packet.remote === null) {
			const buf = stream.read(NAME_LEN);
			if (!buf) {
				return false;
			}
			packet.remote = read_str(buf);
		}
		if (packet.length === null) {
			const buf = stream.read(LENGTH_LEN);
			if (!buf) {
				return false;
			}
			packet.length = buf.readUInt32BE(0);
		}
		const buf = stream.read(packet.length);
		if (!buf) {
			return false;
		}
		packet.data = buf;
		return true;
	};

	stream.on('data', streamOnData);
	this.write = buf => stream.write(buf);
}

Writer.prototype = new EventEmitter();
function Writer(stream) {
	EventEmitter.call(this);

	const write = packet => {
		let { type, remote, length = packet.data.length, data = null } = packet;
		if (typeof type !== 'string' || type.length > TYPE_LEN) {
			throw new Error('Invalid packet type');
		}
		if (typeof remote !== 'string' || remote.length >= NAME_LEN) {
			throw new Error('Invalid packet remote');
		}
		if (typeof length !== 'number' || length < 0) {
			throw new Error('Invalid packet length');
		}
		if (typeof data === 'string') {
			data = Buffer.from(data);
		} else if (data === null) {
			data = new Buffer(0);
		} else if (!(data instanceof Buffer)) {
			throw new Error('Invalid packet data');
		}
		if (data.length !== length) {
			throw new Error('Packet length mismatch');
		}
		const buf = new Buffer(DATA_OFFSET + length);
		buf.write(type, TYPE_OFFSET, TYPE_LEN, 'ascii');
		buf.write(remote + '\0', NAME_OFFSET, NAME_LEN, 'ascii');
		buf.writeUInt32BE(length, LENGTH_OFFSET);
		data.copy(buf, DATA_OFFSET);
		this.emit('data', buf);
	};

	this.write = write;
}
