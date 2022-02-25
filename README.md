# Data Center TCP (DCTCP) Demo

This is a minimal implementation of the [DCTCP](https://people.csail.mit.edu/alizadeh/papers/dctcp-sigcomm10.pdf) congestion control protocol with the [ns-3](https://www.nsnam.org/) simulator.


## ns-3 Setup

Get ns-3 (version 3.35) source:

```bash
wget https://www.nsnam.org/releases/ns-allinone-3.35.tar.bz2
tar xjf ns-allinone-3.35.tar.bz2
```

Build ns-3:

```bash
cd ns-allinone-3.35/ns-3.35
./waf configure --enable-examples --enable-tests
./waf build
```

Test ns-3 build:

```bash
./test.py
```


## TODO

TODO
