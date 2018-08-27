Sparkey is a simple constant key/value storage library. It is mostly suited for
read heavy systems with infrequent large bulk inserts. It includes both a C
library for working with sparkey index and log files (`libsparkey`), and a
command line utility for getting info about and reading values from a sparkey
index/log (`sparkey`).

### Travis
Continuous integration with [travis](https://travis-ci.org/spotify/sparkey).

[![Build Status](https://travis-ci.org/spotify/sparkey.svg?branch=master)](https://travis-ci.org/spotify/sparkey)

Dependencies
------------

* GNU build system (autoconf, automake, libtool)
* [Snappy](http://google.github.io/snappy/)

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
    sudo make install && sudo ldconfig

Related projects
----------------

* [spotify/sparkey-python: Official python bindings](https://github.com/spotify/sparkey-python)
* [spotify/sparkey-java: Official java implementation](https://github.com/spotify/sparkey-java)
* [emnl/gnista: Unofficial ruby bindings](https://github.com/emnl/gnista)
* [adamtanner/sparkey: Unofficial ruby bindings](https://github.com/adamtanner/sparkey)
* [stephenmathieson/node-sparkey: Unofficial node bindings](https://github.com/stephenmathieson/node-sparkey)
* [tiegz/sparkey-go: Unofficial go bindings](https://github.com/tiegz/sparkey-go)

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

Limitations
-----------
The hash writing process requires memory allocation of num_entries * 16 * 1.3 bytes.
This means that you may run out of memory if trying to write a hash index for too many entries.
For instance, with 16 GB available RAM you may write 825 million entries.

This limitation has been removed in sparkey-java, but it has not yet been implemented in this version.

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
A very simple benchmark program is included - see src/bench.c.
The program is designed to be easily extended to measure other key value stores if anyone wants to.
Running it on a production-like server (Intel(R) Xeon(R) CPU L5630 @ 2.13GHz) we get the following:

    Testing bulk insert of 1000 elements and 1000.000 random lookups
      Candidate: Sparkey
        creation time (wall):     0.00
        creation time (cpu):      0.00
        throughput (puts/cpusec): 1098272.88
        file size:                28384
        lookup time (wall):          0.50
        lookup time (cpu):           0.58
        throughput (lookups/cpusec): 1724692.62

    Testing bulk insert of 1000.000 elements and 1000.000 random lookups
      Candidate: Sparkey
        creation time (wall):     0.50
        creation time (cpu):      0.69
        throughput (puts/cpusec): 1448618.25
        file size:                34177984
        lookup time (wall):          1.00
        lookup time (cpu):           0.78
        throughput (lookups/cpusec): 1284477.75

    Testing bulk insert of 10.000.000 elements and 1000.000 random lookups
      Candidate: Sparkey
        creation time (wall):     7.50
        creation time (cpu):      7.73
        throughput (puts/cpusec): 1294209.62
        file size:                413777988
        lookup time (wall):          1.00
        lookup time (cpu):           0.99
        throughput (lookups/cpusec): 1014608.94

    Testing bulk insert of 100.000.000 elements and 1000.000 random lookups
      Candidate: Sparkey
        creation time (wall):     82.00
        creation time (cpu):      81.58
        throughput (puts/cpusec): 1225726.75
        file size:                4337777988
        lookup time (wall):          2.00
        lookup time (cpu):           1.98
        throughput (lookups/cpusec): 503818.84

    Testing bulk insert of 1000 elements and 1000.000 random lookups
      Candidate: Sparkey compressed(1024)
        creation time (wall):     0.00
        creation time (cpu):      0.00
        throughput (puts/cpusec): 1101445.38
        file size:                19085
        lookup time (wall):          3.50
        lookup time (cpu):           3.30
        throughput (lookups/cpusec): 303335.78

    Testing bulk insert of 1000.000 elements and 1000.000 random lookups
      Candidate: Sparkey compressed(1024)
        creation time (wall):     0.50
        creation time (cpu):      0.75
        throughput (puts/cpusec): 1333903.25
        file size:                19168683
        lookup time (wall):          3.00
        lookup time (cpu):           2.91
        throughput (lookups/cpusec): 343833.28

    Testing bulk insert of 10.000.000 elements and 1000.000 random lookups
      Candidate: Sparkey compressed(1024)
        creation time (wall):     8.50
        creation time (cpu):      8.50
        throughput (puts/cpusec): 1176634.25
        file size:                311872187
        lookup time (wall):          3.00
        lookup time (cpu):           2.99
        throughput (lookups/cpusec): 334490.22

    Testing bulk insert of 100.000.000 elements and 1000.000 random lookups
      Candidate: Sparkey compressed(1024)
        creation time (wall):     90.50
        creation time (cpu):      90.46
        throughput (puts/cpusec): 1105412.00
        file size:                3162865465
        lookup time (wall):          3.50
        lookup time (cpu):           3.60
        throughput (lookups/cpusec): 277477.41


File format details
-------------------

### Log file format
The contents of the log file starts with a constant size header, describing some metadata about the log file.
After that is just a sequence of entries, where each entry consists of a type, key and a value.

Each entry begins with two Variable Length Quantity (VLQ) non-negative integers, A and B. The type is determined by the A.
If A = 0, it's a DELETE, and B represents the length of the key to delete.
If A > 0, it's a PUT and the key length is A - 1, and the value length is B.

(It gets slightly more complex if block level compression is used, but we'll ignore that for now.)

### Hash file format
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

(It recently came to my attention that the method described below is basically the same thing as Robin Hood hashing with backward shift deletion)

Let's define displacement as the distance from the calculated optimal slot for a given hash to the slot it's actually placed in. Distance in this case is defined as the number of steps you need to move forward from your optimal slot to reach the actual slot.

The trivial and naive solution for this is to simply start with an empty hash table, move through the entries and put them in the first available slot, starting from the optimal slot, and this is almost what we do.

If we consider the average displacement, we can't really do better than that. We can however minimize the maximum displacement, which gives us some nice properties:

* We can store the maximum displacement in the header, so we have an upper bound on traversals. We could possibly even use this information to binary search for the entry.
* As soon as we reach an entry with higher displacement than the thing we're looking for, we can abort the lookup.

It's very easy to set up the hash table like this, we just need to do insertions into slots instead of appends. As soon as we reach a slot with a smaller displacement than our own, we shift the following slots up until the first empty slot one step and insert our own element.

Let's illustrate it with an example - let's start off with an empty hash table with a capacity of 7:

             hash value   log offset    optimal slot    displacement
           +------------+------------+
    slot 0 |            |            |
           +------------+------------+
    slot 1 |            |            |
           +------------+------------+
    slot 2 |            |            |
           +------------+------------+
    slot 3 |            |            |
           +------------+------------+
    slot 4 |            |            |
           +------------+------------+
    slot 5 |            |            |
           +------------+------------+
    slot 6 |            |            |
           +------------+------------+

We add the key "key0" which happens to end up in slot 3, h("key0") % 7 == 3. The slot is empty, so this is trivial:

             hash value   log offset    optimal slot    displacement
           +------------+------------+
    slot 0 |            |            |
           +------------+------------+
    slot 1 |            |            |
           +------------+------------+
    slot 2 |            |            |
           +------------+------------+
    slot 3 | h(key0)    | 1          |  3               0
           +------------+------------+
    slot 4 |            |            |
           +------------+------------+
    slot 5 |            |            |
           +------------+------------+
    slot 6 |            |            |
           +------------+------------+

Now we add "key1" which happens to end up in slot 4:

             hash value   log offset    optimal slot    displacement
           +------------+------------+
    slot 0 |            |            |
           +------------+------------+
    slot 1 |            |            |
           +------------+------------+
    slot 2 |            |            |
           +------------+------------+
    slot 3 | h(key0)    | 1          |  3               0
           +------------+------------+
    slot 4 | h(key1)    | 11         |  4               0
           +------------+------------+
    slot 5 |            |            |
           +------------+------------+
    slot 6 |            |            |
           +------------+------------+

Now we add "key2" which also wants to be in slot 3. This is a conflict, so we skip forward until we found a slot which has a lower displacement than our current displacement.
When we find that slot, all following entries until the next empty slot move down one step:

             hash value   log offset    optimal slot    displacement
           +------------+------------+
    slot 0 |            |            |
           +------------+------------+
    slot 1 |            |            |
           +------------+------------+
    slot 2 |            |            |
           +------------+------------+
    slot 3 | h(key0)    | 1          |  3               0
           +------------+------------+
    slot 4 | h(key2)    | 21         |  3               1
           +------------+------------+
    slot 5 | h(key1)    | 11         |  4               1
           +------------+------------+
    slot 6 |            |            |
           +------------+------------+

Let's add "key3" which maps to slot 5. We can't push down key1, because it already has displacement 1 and our current displacement for key3 is 0, so we have to move forward:

             hash value   log offset    optimal slot    displacement
           +------------+------------+
    slot 0 |            |            |
           +------------+------------+
    slot 1 |            |            |
           +------------+------------+
    slot 2 |            |            |
           +------------+------------+
    slot 3 | h(key0)    | 1          |  3               0
           +------------+------------+
    slot 4 | h(key2)    | 21         |  3               1
           +------------+------------+
    slot 5 | h(key1)    | 11         |  4               1
           +------------+------------+
    slot 6 | h(key3)    | 31         |  5               1
           +------------+------------+
Adding "key4" for slot 3. It ends up in slot 5 with displacement 2 and key3 loops around to slot 0:

             hash value   log offset    optimal slot    displacement
           +------------+------------+
    slot 0 | key(key3)  | 31         |  5               2
           +------------+------------+
    slot 1 |            |            |
           +------------+------------+
    slot 2 |            |            |
           +------------+------------+
    slot 3 | h(key0)    | 1          |  3               0
           +------------+------------+
    slot 4 | h(key2)    | 21         |  3               1
           +------------+------------+
    slot 5 | h(key4)    | 41         |  3               2
           +------------+------------+
    slot 6 | h(key1)    | 11         |  4               2
           +------------+------------+

Now, if we search for key123 which maps to slot 3 (but doesn't exist!), we can stop scanning as soon as we reach slot 6, because then the current displacement (3) is higher than the displacement of the entry at the current slot (2).

Compression
-----------
Sparkey also supports block level compression using google snappy. You select a block size which is then used to split the contents of the log into blocks. Each block is compressed independently with snappy. This can be useful if your bottleneck is file size and there is a lot of redundant data across adjacent entries. The downside of using this is that during lookups, at least one block needs to be decompressed. The larger blocks you choose, the better compression you may get, but you will also have higher lookup cost. This is a tradeoff that needs to be empirically evaluated for each use case.

