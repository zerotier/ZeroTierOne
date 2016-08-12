A super-minimal in-filesystem persistent JSON object
======

[We](https://www.zerotier.com/) like minimalism.

Offbase is an extension of the excellent [nlohmann/json](https://github.com/nlohmann/json) C++11 JSON class that adds simple object persistence to/from the filesystem. Objects are stored into a directory hierarchy in fully "expanded" form with each field/value being represented by a separate file.

Features:

 - Very easy to use
 - Minimal!
   - Easy to understand and maintain
	 - Trivial to implement in other languages
	 - No dependencies beyond standard libraries
	 - Small code footprint in both source and binary form
	 - Easy to port to other platforms
 - Exactly reproduces JSON object hierarchies including all JSON type information
 - Database can be explored from the shell, browsed in a web browser or file explorer, scanned with `find` and `grep`, etc.
 - Database can be backed up, restored, versioned, etc. with tools like `git`, `rsync`, `duplicity`, etc.
 - Alien files like `.git` or `.DS_Store` are harmlessly ignored if present
 - Saving only changes what's changed to reduce I/O overhead and SSD wear

Limitations and shortcomings:

 - This creates a lot of tiny files, which is inefficient on some filesystems and might run into inode limits in extreme cases. For data sets with more than, say, a million items we recommend a filesystem like `btrfs` or `reiserfs`. Things like [redisfs](https://steve.fi/Software/redisfs/) are also worth exploring. On Linux another alternative is to put the database into `/dev/shm` (RAM disk) and then regularly back it up with `duplicity` or similar.
 - The whole JSON object is held in memory *twice* for diffing purposes.
 - Diffing traverses the whole tree and then updates the shadow copy, which makes `commit()` slow for huge data sets. This is not suitable for "big" data where "big" here is probably more than a few hundred megabytes.
 - Recursion is used, so if you have object hierarchies that are incredibly deep (hundreds or more) it might be possible to overflow your stack and crash your app.
 - This is not thread safe and must be guarded by a mutex if used in a threaded app.

Caveats:

 - Key names are escaped for safety in the filesystem, but we still don't recommend allowing external users to set just anything into your JSON store. See the point about recursion under limitations.

Future:

 - It would not be too hard to tie this into a filesystem change monitoring API and automatically read changes from disk if they are detected. This would allow the database to be edited "live" in the filesystem.
   - In theory this could provide replication or clustering through distributed filesystems, file syncing, or things like [Amazon Elastic Filesystem](https://aws.amazon.com/efs/).
 - Recursion could be factored out to get rid of any object hierarchy depth constraints.
 - Mutexes could be integrated somehow to allow for finer grained locking in multithreaded apps.
 - Diffing and selective updates could be made more memory and CPU efficient using hashes, etc.

## How to Use

The `offbase` class just extends [nlohmann::json](https://github.com/nlohmann/json) and gives you a JSON object. Take care to make sure you don't change the type of the 'root' object represented by the 'offbase' instance from JSON 'object'. Anything under it can of course be any JSON type, including any object.

Just put data into the object and then periodically call `commit()` to persist changes to disk. The `commit()` method diffs the current contents of the object with what it knows to have been previously persisted to disk and modifies the representation on disk to match. This can be done after writes or periodically in a background thread.

See comments in `offbase.hpp` for full documentation including details about error handling, etc.

## Persistence format

The base object represented by the `offbase` instance is persisted into a directory hierarchy under its base path. Files and directories are named according to a simple convention of `keyname.typecode` where `keyname` is an escaped key name (or hex array index in the case of arrays) and `typecode` is a single character indicating whether the item is a JSON value, array, or object.

 - `*.V`: JSON values (actual value type is inferred during JSON parse)
 - `*.O`: JSON objects (these are subdirectories)
 - `*.A`: JSON arrays (also subdirectories containing items by hex array index)

There are in theory simpler ways to represent JSON in a filesystem, such as the "flattened" JSON "pointer" format, but this has the disadvantage of not disambiguating objects vs. arrays. Offbase's persistence format is designed to perfectly reproduce the exact same JSON tree on load as was most recently committed.
