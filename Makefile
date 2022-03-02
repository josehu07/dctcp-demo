NSDIR:=ns-3.35
ROOTDIR:=$(dir $(abspath $(lastword $(MAKEFILE_LIST))))


.PHONY: all build clean test simulation paper-expt

all: build


build:
	cd $(NSDIR) && \
		./waf configure --enable-examples --enable-tests -d release && \
		./waf build

clean:
	rm -rf $(NSDIR)/build


test:
	cd $(NSDIR) && \
		./waf check


simulation:
	@echo
	@echo "=== Running Linux Reno ==="
	@echo
	cd $(NSDIR) && \
		./waf --run scratch/trace-tcp-reno | tee $(ROOTDIR)/trace-tcp-reno.log
	@echo
	@echo "=== Running My DCTCP ==="
	@echo
	cd $(NSDIR) && \
		./waf --run scratch/trace-dctcp-my | tee $(ROOTDIR)/trace-dctcp-my.log

paper-expt:
	cd $(NSDIR) && \
		./waf --run scratch/paper-dctcp-my
