'use strict';

var fs = require('fs');
var RuleCompiler = require('./rule-compiler.js');

if (process.argv.length < 3) {
	console.log('Usage: node cli.js <rules script>');
	process.exit(1);
}

var rules = [];
var caps = {};
var tags = {};
var err = RuleCompiler.compile(fs.readFileSync(process.argv[2]).toString(),rules,caps,tags);

if (err) {
	console.error('ERROR parsing '+process.argv[2]+' line '+err[0]+' column '+err[1]+': '+err[2]);
	process.exit(1);
} else {
	let capsArray = [];
	let capabilitiesByName = {};
	for(let n in caps) {
		capsArray.push(caps[n]);
		capabilitiesByName[n] = caps[n].id;
	}
	let tagsArray = [];
	for(let n in tags) {
		let t = tags[n];
		let dfl = t['default'];
		tagsArray.push({
			'id': t.id,
			'default': (((dfl)||(dfl === 0)) ? dfl : null)
		});
	}

	console.log(JSON.stringify({
		config: {
			rules: rules,
			capabilities: capsArray,
			tags: tagsArray
		},
		capabilitiesByName: capabilitiesByName,
		tagsByName: tags
	},null,1));

	process.exit(0);
}
