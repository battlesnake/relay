/* Convert *? wildcard to regular expression */
module.exports = pattern => new RegExp(`^${
	pattern
		.replace(/[\\^$+.()|[\]{}]/g, '\\$&')
		.replace(/\?/g, '.')
		.replace(/\*/g, '.*')
}$`);
