all:
	mkdir -p build
	jsx --target es3 -x jsx . ./build
	rm -f ztui.min.js
	minify build/*.js >>ztui.min.js
	rm -rf build
