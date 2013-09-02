Sparkey is a simple constant key/value storage library. It is mostly suited for
read heavy systems with infrequent large bulk inserts. It includes both a C
library for working with sparkey index and log files (`libsparkey`), and a
command line utility for getting info about and reading values from a sparkey
index/log (`sparkey`).

Dependencies
------------

* GNU build system (autoconf, automake, libtool)
* [Snappy](https://code.google.com/p/snappy/)

Optional

* [Doxygen](http://www.doxygen.org/)

Building
--------

    autoreconf --install
    ./configure
    make
    make check

API documentation can be generated with `doxygen`.

Installing
----------

    make install

Description
------------
Sparkey is an extremely simple persistent key-value store.
You could think of it as a read-only hashtable on disk and you wouldn't be far off.
It is designed and optimized for some server side usecases at Spotify but it is written to be completely generic
and makes no assumptions about what kind of data is stored.

Some key characteristics:

* Supports data sizes up to 2^63 - 1 bytes.
* Supports iteration, get, put, delete
* Optimized for bulk writes.
* Immutable hash table.
* Any amount of concurrent independent readers.
* Only allows one writer at a time per storage unit.
* Cross platform storage file.
* Low overhead per entry.
* Constant read startup cost
* Low number of disk seeks per read
* Support for block level compression.
* Data agnostic, it just maps byte arrays to byte arrays.

What it's not:

* It's not a distributed key value store - it's just a hash table on disk.
* It's not a compacted data store, but that can be implemented on top of it, if needed.
* It's not robust against data corruption.

The usecase we have for it at Spotify is serving data that rarely gets updated to
users or other services. The fast and efficient bulk writes makes it feasible to periodically rebuild the data,
and the fast random access reads makes it suitable for high throughput low latency services.
For some services we have been able to saturate network interfaces while keeping cpu usage really low.

Usage
-----
Sparkey is meant to be used as a library embedded in other software. Take a look at the API documentation which gives examples on how to use it.

License
-------
Apache License, Version 2.0

Design
------
Sparkey uses two files on disk to store its data. The first is the sparkey log file (.spl), which is simply a sequence of key value pairs.
This is an append-only file. You're not allowed to modify it in the middle, and you can't use more than one writer to append to it.

The other file is the sparkey index file (.spi) which is a just a hashtable pointing at entries in the log.
This is an immutable file, so you would typically only update it once you're done with your bulk appends.

Doing a random lookup involves first finding the proper entry in the hashtable, and then doing a seek to the right offset in the log file.
On average, this means two disk seeks per access for a cold disk cache. If you mlock the index file, it goes down to one seek.
For some of our usecases, the total data set is less than the available RAM, so it makes sense to mlock everything.

The advantages of having two files instead of just one (another solution would be to append the hash table at the end) is that it's trivial to mlock one of the files and not the other. It also enables us to append more data to existing log files, even after it's already in use.

History
-------
Sparkey is the product of hackdays at Spotify, where our developers get to spend some of their time on anything they think is interesting.

We have several usecases where we need to serve large amounts of static data with high throughput and low latency.
To do this, we've built our own services, backed by various storage systems. Our flow consists of first generating large static storage files in our offline-systems, which then gets pushed out to the user facing services to serve the data.

The storage solutions we used for that have all served us well for a time, but they had limitations that became problematic.

* We used to rely a lot on CDB (which is a really great piece of software). It performed blazingly quick and produces compact files. We only stopped using it when our data started growing close to the 4 GB limit
* We also used (and still use) Tokyo Cabinet for a bunch of usecases. It performs really well for reading, but the write throughput really suffers when you can no longer keep the entire dataset in memory, and there were issues with opening the same file multiple times from the same process.

We needed a key-value store with the following characteristics:

* random read throughput comparable to tokyo cabinet and cdb.
* high throughput bulk writes.
* low overhead.
* high limit on data size.

For fun, we started hacking on a new key-value store on our internal hackdays, where developers get to work on whatever they're interested in.
The result was this project.

Performance
-----------
One of these days I'll run some performance benchmarks.

File format details
-------------------

Log file format
---------------
The contents of the log file starts with a constant size header, describing some metadata about the log file.
After that is just a sequence of entries, where each entry consists of a type, key and a value.

Each entry begins with two Variable Length Quantity (VLQ) non-negative integers, A and B. The type is determined by the A.
If A = 0, it's a DELETE, and B represents the length of the key to delete.
If A > 0, it's a PUT and the key length is A - 1, and the value length is B.

(It gets slightly more complex if block level compression is used, but we'll ignore that for now.)

Hash file format
----------------
The contents of the hash file starts with a constant size header, similarly to the log file.
The rest of the file is a hash table, represented as capacity * slotsize bytes.
The capacity is simply an upper bound of the number of live entries multiplied by a hash density factor > 1.0.
The default implementation uses density factor = 1.3.
Each slot consists of two parts, the hash value part and the address.
The size of the hash value is either 4 or 8 bytes, depending on the hash algorithm. It currently uses murmurhash32 if the number of entries is small, and a 64 bit truncation of murmurhash128 if the number of entries is large.
The address is simply a reference into the log file, either as 4 or 8 bytes, depending on the size of the log file.
That means that the slotsize is usually 16 bytes for any reasonably large set of entries.
By storing the hash value itself in each slot we're wasting some space, but in return we can expect to avoid visiting the log file in most cases.

Hash lookup algorithm
----------------------
One of few non-trivial parts in Sparkey is the way it does hash lookups. With hashtables there is always a risk of collisions. Even if the hash itself may not collide, the assigned slots may.

Let's define displacement as the distance from the calculated optimal slot for a given hash to the slot it's actually placed in. Distance in this case is defined as the number of steps you need to move forward from your optimal slot to reach the actual slot.

The trivial and naive solution for this is to simply start with an empty hash table, move through the entries and put them in the first available slot, starting from the optimal slot, and this is almost what we do.

If we consider the average displacement, we can't really do better than that. We can however minimize the maximum displacement, which gives us some nice properties:

* We can store the maximum displacement in the header, so we have an upper bound on traversals. We could possibly even use this information to binary search for the entry.
* As soon as we reach an entry with higher displacement than the thing we're looking for, we can abort the lookup.

It's very easy to set up the hash table like this, we just need to do insertions into slots instead of appends. As soon as we reach a slot with a smaller displacement than our own, we shift the following slots up until the first empty slot one step and insert our own element.

Compression
-----------
Sparkey also supports block level compression using google snappy. You select a block size which is then used to split the contents of the log into blocks. Each block is compressed independently with snappy. This can be useful if your bottleneck is file size and there is a lot of redundant data across adjacent entries. The downside of using this is that during lookups, at least one block needs to be decompressed. The larger blocks you choose, the better compression you may get, but you will also have higher lookup cost. This is a tradeoff that needs to be emperically evaluated for each use case.

