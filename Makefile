# Just a dummy Makefile that copies things into ns-3.35/scratch and uses
# the built-in waf toolchain to build and run the scripts.


NSDIR=ns-allinone-3.35/ns-3.35

EXECS=dctco-sim



.PHONY: all cpin build clean run


all: cpin build


cpin:
	$(foreach src,$(SRCS),cp $(src) $(NSDIR)/scratch/;)

build: cpin
	cd $(NSDIR) && ./waf build


clean:
	rm -rf $(NSDIR)/build/scratch/$(BIN)*


run:
	cd $(NSDIR) && ./waf --run scratch/$(BIN)
