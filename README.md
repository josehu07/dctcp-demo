# Data Center TCP (DCTCP) Demo

Guanzhou Hu @ UW-Madison, CS740, Spring 2022

This is a minimal implementation of the DCTCP congestion control protocol ([SIGCOMM paper](https://people.csail.mit.edu/alizadeh/papers/dctcp-sigcomm10.pdf), [RFC 8257](https://datatracker.ietf.org/doc/html/rfc8257)) with the [ns-3](https://www.nsnam.org/) simulator. Tested on Ubuntu 20.04.

It turns out that ns-3.35 already ships with a default implementation of DCTCP. This repo holds my own implementation based on my understanding of the original SIGCOMM paper. I then compare the performance results to both the ns-3 DCTCP implementation (through simulation) and the native Linux kernel DCTCP implementation (through direct code execution, DCE).


## Content

Due to the design of ns-3, all new components and simulation scripts must be put within the `ns-3.35` folder, be built by the default `./waf` toolchain, and be run with `./waf --run`. Hence, ns-3.35 source code is included in-place. I added my new DCTCP component into the source folder and modified the build scripts to involve the new stuff.

Files added or modified:

* `ns-3.35/scratch/tcp-dctcp-my-perf.cc`: TODO
* `ns-3.35/src/internet/model/tcp-dctcp-my.cc`: my implementation of `TcpDctcpMy` component
* `ns-3.35/src/internet/model/tcp-dctcp-my.h`: a duplicate of `tcp-dctcp.h`
* `ns-3.35/src/internet/test/tcp-dctcp-my-test.cc`: a duplicate of `tcp-dctcp.test.cc`
* `ns-3.35/src/internet/wscript`: modified to involve `TcpDctcpMy` stuff when building


## Usage

TODO
