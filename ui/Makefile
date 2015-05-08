all:
	mkdir -p build
	jsx --target es3 -x jsx . ./build
	minify build/*.js >>ztui.min.js
	rm -rf build
