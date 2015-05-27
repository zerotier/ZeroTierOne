'use strict'

function convertType(v,t)
{
	if (Array.isArray(t)) {
		var r = v;
		if (t.length !== 0) {
			if (Array.isArray(v)) {
				r = [];
				for(var i=0;i<v.length;++i)
					r.push(convertType(v[i],t[0]));
			} else r = [ convertType(v,t[0]) ];
		} else r = [ v ];
		return r;
	} else if (t === 'string') {
		if (typeof v === 'string')
			return v;
		else if ((typeof v === 'boolean')||(typeof v === 'number'))
			return v.toString();
		else if (Array.isArray(v)||(typeof v === 'object'))
			return JSON.stringify(v);
		else return '';
	} else if (t === 'integer') {
		if (typeof v === 'number')
			return Math.round(v);
		else if (typeof v === 'string')
			return parseInt(v);
		else if (typeof v === 'boolean')
			return ((v) ? 1 : 0);
		else return 0;
	} else if (t === 'number') {
		if (typeof v === 'number')
			return v;
		else if (typeof v === 'string')
			return parseFloat(v);
		else if (typeof v === 'boolean')
			return ((v) ? 1 : 0);
		else return 0;
	} else if (t === 'boolean') {
		return ((v) ? true : false);
	} else if (typeof t === 'object') {
		if ((v !== null)&&(typeof v === 'object'))
			return constrainTypes(v,t);
		else return {};
	} else return v;
}

function constrainTypes(obj,typeMap)
{
	var r = {};
	for(var k in obj) {
		var t = typeMap[k];
		r[k] = convertType(v,t);
	}
	return r;
}

exports = constrainTypes;
