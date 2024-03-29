# Performance Comparison of Operations in the File System and in Embedded Key-Value Databases
  A routine scenario when developing PC applications is storing data (in small files or records) and then retrieving and manipulating that data with a distinctive identifier (ID). In these scenarios, the developer can save the records in an ID as the filename or use an embedded on-disk key-value database. The issue is that many file systems can have performance issues when handling many small files, and developers would rather avoid depending on an embedded database if it offers little benefit or has a detrimental effect on performance for their use case. Our contribution to this topic is comparing the different key-value databases-- SQLite3, LevelDB, RocksDB, and Berkeley DB by different factors, such as different file systems-- NTFS and EXT4, and explaining why the results happened. The metrics and technologies to be evaluated extend the metrics evaluated in our previous research work \cite{pastpaper}. We will compare these key-value databases through two Windows machines: a solid-state drive and a hard disk drive. This research used the Windows Subsystem for Linux to work with the machines and NTFS file systems.

# The Benchmark
A benchmark algorithm will be implemented to compare the storage method and usage patterns, generating random data for the operation and then taking measurements for the performance of the embedded database on the following metrics on Windows: create, which is the adding of information onto something, in this case, a key-value database (in SQL this is an update statement); read, which is the viewing of or selecting of information from a key-value database (in SQL this is a select statement); update, which is the changing of information which is already on the database; and delete, which is the removal of information from a database. The algorithm loops over different combinations of store type, data type, record size range, and record count range. Each combination benchmarks 1,000 iterations of each of the four key-value operations (create, read, update, delete) using a random access pattern and random data. It then records each combination's peak memory usage and disk space efficiency. Finally, the results are outputted to a CSV file. These files will then be converted into tables to simplify our results. 

For more details, look at the paper "A Further Performance Comparison of Operations in the File System and in Embedded Key-Value Databases". 

# Hardware
We used one machine with an NVIDIA GeForce RTX 3060 processor, 8 GiB of DDR4 RAM with 250 GB HDD storage, and ran on Windows 11, we also used a portable SSD and ran some tests on there as well to compare the two. 

# Results
The results of the benchmark in CSV format and an Excel spreadsheet that imports the CSV data and shows interactive charts are in the [`results`](results) folder.

# Acknowledgements
The project was done with the assistance of [Nicholas Cunningham](https://github.com/thaamazingone), [Jesse Hines](https://github.com/jesse-r-s-hines), and [Germán H. Alférez](http://harveyalferez.com)

Thank you to [Southern Adventist University](https://www.southern.edu) as well. 
