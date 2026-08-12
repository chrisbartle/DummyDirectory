# DummyDirectory
DummyDirectory is a small, standalone, command line application that can rapidly generate and verify dummy data. It can be helpful with the following tasks:
- **Volume Testing** DummyDirectory can create a directory tree of unlimited size and complexity with just a few commands.
- **Benchmarking** DummyDirectory can be used to simulate common file operations including add, modifying, deleting, renaming, and moving files.
- **Backup Testing** DummyDirectory can generate identical data in multiple locations, then verify that the data is identical.

DummyDirectory has the following features:
- Supports data generation into the exobyte range. Equally capable of testing a small USB drive and a massive fiber array.
- Written in basic C++ with no library dependencies; can be easily compiled onto nearly any platform.
- Extremely fast, competitive with other benchmarking tools.
- All generated data is pseudorandom and deterministic. A seed string can be provided to force the same data to be generated or a replay file will allow identical data to be regenerated. This makes it easy to reproduce identical data for testing purposes.
- The MD5 hash is calculated and stored for all generated items. An MD5 hash is provided for the entire directory. It is simple to verify the integrity of the data at any time.
- Free and open source under the BSD license

# USAGE
DummyDirectory is a command line utility. Each time it is executed it performs a single type of operation, such as file creation, directory creation, file modification, etc. This operation is performed randomly on a number of files inside the dummy directory.

For example, to create a dummy directory filled with 10 gigabytes of dummy data, one must simply run:
```
> dummydir add -s 10g ddtest

All supported options can be viewed at any time by running:
```
dummydir --help
```
The man page is included during installation and is available here:
[Dummy Directory man page](https://www.chrisbartle.com/mandummydirectory/)

# INSTALLATION
The Windows binary and Linux installation files are available at:
[github releases](https://github.com/chrisbartle/DummyDirectory/releases)
[Dummy Directory webpage](https://www.chrisbartle.com/dummydirectory/)

For all other operating systems, use the following commands:
```
git clone https://github.com/chrisbartle/DummyDirectory
cd dummydir
cmake -S . -B build
cmake --build build --parallel
sudo cmake --install build
```
