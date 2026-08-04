# DummyDirectory
DummyDirectory is a small, standalone, command line application that can rapidly generate and verify dummy data. It can be helpful with the following tasks:
- **Volume Testing** DummyDirectory can create a directory tree of unlimited size and complexity with just a few commands.
- **Benchmarking** DummyDirectory can be used to simulate common file operations including add, modifying, deleting, renaming, and moving files.
- **Backup Testing** DummyDirectory can generate identical data in multiple locations, then verify that the data is identical.

DummyDirectory has the following features:
- Supports data generation into the exobyte range. Equally capable of testing a small USB drive and a massive fiber array.
- Written in basic C++ with no library dependencies; can be easily compiled onto nearly any platform.
- Extremely fast, beats out other benchmarking tools in certain situations.
- All generated data is pseudorandom and deterministic. A seed string can be provided to force the same data to be generated or a replay file will allow identical data to be regenerated. This makes it easy to reproduce identical data for testing purposes.
- The MD5 hash is calculated and stored for all generated items. An MD5 hash is provided for the entire directory. It is simple to verify the integrity of the data at any time.

# USAGE
All supported options can be viewed at any time by running:
> dummydir --help


