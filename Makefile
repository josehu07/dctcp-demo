NSDIR=ns-3.35


.PHONY: all build clean test perf

all: build


build:
	cd $(NSDIR) && \
		./waf configure --enable-examples --enable-tests -d release && \
		./waf build

clean:
	rm -rf $(NSDIR)/build


test:
	echo "Test"

perf:
	echo "Perf"
