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
DummyDirectory is a command line utility. Each time it is executed it performs a single type of operation, such as file creation, directory creation, file modification, etc. This operation is performed randomly on a number of files inside the dummy directory.

To start simply, let's create a new dummy directory named ddtest and put 10 directories in it:
```
temp$ dummydir dadd ddtest
Dummy Directory version 0.9.1.0
Written by Chris Bartle
"temp/ddtest" was created.
Manifest contains 1 directories and 0 files.
0 bytes total.
Processing...
10 items processed. 0 bytes written in 0.000510 seconds (0.00 bytes per second)
Processing complete!
Manifest contains 11 directories and 0 files.
0 bytes total.
Manifest hash is c1b3fd7743b927076cd70f1235ca9b2c
```
Now let's add 10 gigabytes of data into it. By default, DummyDirectory will divide the total number of bytes across multiple randomly generated and named files:
```
temp$ dummydir add -s 10g ddtest
Dummy Directory version 0.9.1.0
Written by Chris Bartle
Manifest contains 11 directories and 0 files.
0 bytes total.
Processing...
2,035 items processed. 10,737,418,240 bytes written in 4.046256 seconds (2,653,667,811.46 bytes per second)
Processing complete!
Manifest contains 11 directories and 2,035 files.
10,737,418,240 bytes total.
Manifest hash is b4489f1b228b3b35ed80195859757126
```
For fun, let's modify half of the files:
```
temp$ dummydir modify -c 50% ddtest
Dummy Directory version 0.9.1.0
Written by Chris Bartle
Manifest contains 11 directories and 2,035 files.
10,737,418,240 bytes total.
Processing...
1,018 items processed. 2,797,448,475 bytes worth of changes in 1.644833 seconds (1,700,748,993.07 bytes per second)
The total size of all affected files is 5,444,226,289 bytes
Processing complete!
Manifest contains 11 directories and 2,035 files.
10,798,918,933 bytes total.
Manifest hash is 1890a970c936c7d83299a74428fa9201
```
Finally, we'll have the software clean up its work and return the dummy directory to a nearly empty state:
```
temp$ dummydir clean ddtest
Dummy Directory version 0.9.1.0
Written by Chris Bartle
Manifest contains 11 directories and 2,035 files.
10,798,918,933 bytes total.
Processing...
2,045 items removed in 0.286340 seconds
Cleaning complete!
```

All supported options can be viewed at any time by running:
```
dummydir --help
```


