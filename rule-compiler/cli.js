'use strict';

var fs = require('fs');

var RuleCompiler = require('./rule-compiler.js');

if (process.argv.length < 3) {
	console.log('Usage: node cli.js <rules script>');
	process.exit(1);
}

var src = fs.readFileSync(process.argv[2]).toString();

var rules = [];
var caps = {};
var tags = {};
var err = RuleCompiler.compile(src,rules,caps,tags);

if (err) {
	console.log('ERROR parsing '+process.argv[2]+' line '+err[0]+' column '+err[1]+': '+err[2]);
	process.exit(1);
} else {
	console.log(JSON.stringify({
		rules: rules,
		caps: caps,
		tags: tags
	},null,2));
	process.exit(0);
}
