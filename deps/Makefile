deps/build/lib/libuv.a:
	@echo "Build deps/libuv"
	mkdir -p deps/build/{include,lib}; \
	cd deps/libuv && \
	./autogen.sh && \
	CFLAGS="$(CFLAGS) -fPIC -g -O2" ./configure --prefix="$(srcdir)/deps/build" && \
	make install

deps/build/lib/libctx.a:
	@echo "Build deps/libctx"
	mkdir -p deps/build/{include,lib}; \
	cd deps/libctx; \
	make && make build
