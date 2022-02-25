# Data Center TCP (DCTCP) Demo

Guanzhou Hu @ UW-Madison, CS740, Spring 2022

This is a minimal implementation of the DCTCP congestion control protocol ([SIGCOMM paper](https://people.csail.mit.edu/alizadeh/papers/dctcp-sigcomm10.pdf), [RFC 8257](https://datatracker.ietf.org/doc/html/rfc8257)) with the [ns-3](https://www.nsnam.org/) simulator. Tested on Ubuntu 20.04.

It turns out that ns-3.35 already ships with a default implementation of DCTCP. This repo holds my own implementation based on my understanding of the original SIGCOMM paper. I then compare the performance results to both the ns-3 DCTCP implementation (through simulation) and the native Linux kernel DCTCP implementation (through direct code execution, DCE).

This repo could also serve as a minimal example of adding a custom congestion control algoirthm implementation into ns-3's TCP stack model.


## ns-3 Setup

Get ns-3 (version 3.35) source:

```bash
# be sure you are at the root dir of this repo, containing this README.md
wget https://www.nsnam.org/releases/ns-allinone-3.35.tar.bz2
tar xjf ns-allinone-3.35.tar.bz2
```

Build ns-3's default components:

```bash
cd ns-allinone-3.35/ns-3.35
./waf configure --enable-examples --enable-tests
./waf build
```

Test ns-3 build:

```bash
./test.py
```

Return to the root directory of this repo:

```bash
cd ../..
```


## TODO

TODO
