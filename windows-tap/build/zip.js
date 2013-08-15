/*
 * zip.js  - a simple zip implementation in jscript.
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

var ForReading = 0;
var ForWriting = 1;
var fso = new ActiveXObject("Scripting.FileSystemObject");

function zip(source, destination) {
	try {
		var f = OpenTextFile(destination, ForReading);
		f.Close();
	}
	catch(e) {
		var f = fso.CreateTextFile(destination, ForWriting);
		var zipheader = "PK" + String.fromCharCode(5) + String.fromCharCode(6);
		for (var i=0;i<18;i++) {
			zipheader += String.fromCharCode(0);
		}
		f.Write(zipheader);
		f.Close();
	}

	var shell = new ActiveXObject("Shell.Application");
	var source = shell.NameSpace(fso.GetAbsolutePathName(source));
	var destination = shell.NameSpace(fso.GetAbsolutePathName(destination));
	
	destination.CopyHere(source.Items(), 4);
	while(source.Items().Count != destination.Items().Count) {
		WScript.Sleep(1000);
	}
}

var index = 0;
var source = WScript.Arguments(index++);
var destination = WScript.Arguments(index++);
try {
	zip(source, destination);
	WScript.Quit(0);
}
catch(e) {
	WScript.Echo("ERROR: Cannot zip '" + destination + "'.");
	WScript.Quit(1);
}

