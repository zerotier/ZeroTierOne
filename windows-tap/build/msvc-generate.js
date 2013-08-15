/*
 * msvc-generate.js  - string transformation
 *
 * Copyright (C) 2008-2012 Alon Bar-Lev <alon.barlev@gmail.com>
 *
 * BSD License
 * ============
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *     o Redistributions of source code must retain the above copyright notice,
 *       this list of conditions and the following disclaimer.
 *     o Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     o Neither the name of the Alon Bar-Lev nor the names of its
 *       contributors may be used to endorse or promote products derived from
 *       this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */

var ForReading = 1;
var fso = new ActiveXObject("Scripting.FileSystemObject");
var input = "nul";
var output = "nul";
var files = new Array();
var env = new Array();

function initialize() {
	for (var i=0;i<WScript.Arguments.length;i++) {
		var arg = WScript.Arguments(i);
		if (arg.match(/^--input=(.*)$/)) {
			input=RegExp.$1;
		}
		else if (arg.match(/^--output=(.*)$/)) {
			output=RegExp.$1;
		}
		else if (arg.match(/^--config=(.*)$/)) {
			files.push(RegExp.$1);
		}
		else if (arg.match(/^--var=([^=]*)=(.*)$/)) {
			env[RegExp.$1] = RegExp.$2;
		}
	}
}

function process_config(vars, file) {
	try {
		var fin = fso.OpenTextFile(file, ForReading);

		while (!fin.AtEndOfStream) {
			var content = fin.ReadLine();
			if (content.match(/^[ \t]*define\(\[(.*)\],[ \t]*\[(.*)\]\)[ \t]*/)) {
				vars[RegExp.$1] = RegExp.$2;
			}
		}
	}
	catch(e) {
		throw new Error(1, "Cannot process '" + file + "'.");
	}
}

function process_file(vars, input, output) {
	var fin = fso.OpenTextFile(input, ForReading);
	var fout = fso.CreateTextFile(output);
	var content = fin.ReadAll();

	for (var i in vars) {
		content = content.replace(new RegExp("@"+i+"@", "g"), vars[i]);
	}

	fout.Write(content);
}

function build_vars() {
	var vars = new Array();
	for (var f in files) {
		process_config(vars, files[f]);
	}
	for (var e in env) {
		vars[e] = env[e];
	}
	return vars;
}

function main() {
	try {
		initialize();

		var vars = build_vars();

		process_file(
			vars,
			input,
			output
		);

		WScript.Quit(0);
	}
	catch(e) {
		WScript.Echo("ERROR: when procssing " + output + ": " + e.description);
		WScript.Quit(1);
	}
}

main();
