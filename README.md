# Performance Comparison of Operations in the File System and in Embedded Key-Value Databases
This repository contains the source code and results of the benchmark in the "Performance Comparison of Operations in the File System and in Embedded Key-Value Databases" paper. 

A common scenario when developing local PC applications such as games, mobile apps, or presentation software is storing many small files or records as application data and needing to retrieve and manipulate those records with some unique ID. In this kind of scenario, a developer has the choice of simply saving the records as files with their unique ID as the filename or using an embedded on-disk key-value database. Many file systems have performance issues when handling large numbers of small files, but developers may want to avoid a dependency on an embedded database if it offers little benefit or has a detrimental effect on performance for their use case. This benchmark compares the performance of the insert, update, get, and remove operations and the space efficiency of storing records as files vs. using key-value embedded databases including SQLite3, LevelDB, RocksDB, and Berkeley DB.

# The Benchmark
This benchmark compares 6 different store types: 4 embedded databases, and 2 strategies for storing key-value records on disk. We compared SQLite3, LevelDB, RocksDB, and Berkeley DB. Then, we compared the embedded databases with two strategies for storing key-value records on disk, flat and nested. The flat storage strategy places all the records as files with their key as their name under one folder. The nested storage strategy uses a nested directory structure based on the filename to avoid having more than a few hundred files directly under a single folder.

The benchmark loops over combintations of the key-value stores, data type (compressible text or incompressible binary data), record size, and record count. For each combination it benchmarks 1,000 iterations of each of the 4 key-value operations using a random access pattern and random data. It then records the peak memory usage and disk space efficiency for each combination. 

For more details look at the paper "Performance Comparison of Operations in the File System and in Embedded Key-Value Databases". 

# Hardware
The benchmark was run on an virtual machine provided by Southern Adventist University. The VM ran Ubuntu Server 21.10 and was given 2 cores of a AMD EPYC 7402P processor, 8 GiB of DDR4 s667 MT/s RAM, and 250 GiB of Vess R2600ti HDD. 

# Results
The results of the benchmark in CSV format, as well as an Excel spreadsheet that imports the CSV data and shows interactive charts, are in the [`results`](results) folder.

# Acknowledgements
The project was made by [Nicholas Cunningham](https://github.com/thaamazingone), [Jesse Hines](https://github.com/jesse-r-s-hines), and [Germán H. Alférez](http://harveyalferez.com)
# MastersThesis
