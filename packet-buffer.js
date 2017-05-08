const Component = require('component');

PacketBuffer.prototype = new Component();
function PacketBuffer() {
	Component.call(this, 'Packet buffer', true);
	const queue = [];
	this.push = packet => queue.push(packet);
	this.flush = () => {
		const q = [...queue];
		this.clear();
		for (const packet of q) {
			this.emit('flush', packet);
		}
	};
	this.clear = () => {
		queue.length = 0;
	};
}

module.exports = PacketBuffer;
