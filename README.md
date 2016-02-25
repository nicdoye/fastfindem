# fastfindem
Equivalent of find -type f &lt;path> -mtime +&lt;days> -delete

* Version 0.1 is roughly 6 times slower than find, due to realpath() checking every folder in the tree. Uses an (admittedly inefficient) linked list to
save canonical directory names to guarantee no loops happen.
* Version 0.2 is roughly 20% slower than find. There's no realpath(), no linked list, and we cache directory pathlen.

The idea is to create a stripped down version of the above for maximum efficiency. However, I/O is probably the limiting factor, and everything else is
irrelevant.

glibc contains ftw which may be simpler to use than the current implementation.

Would reading the directory and then going back with all the deletes at once be any more efficient? I doubt it as Linux (the target platform) "bulks up" I/O 
operations before sending them off to disk.

## Future directions
A system using inotify cannot work for the use case I have, as the target directory is in NFS.

I am thinking of 'finding', to prime the cache and perform mop-up every so often, and saving the file and mtime in a K-V store, such as redis. 

From there, there would need to be some other method for placing new files in the K-V store. 
Due to the way we collect and generate new files, this is entirely feasible. 
We would then read from the K-V store to know when to delete a file. 

Indeed, with an "mtime" of n days, even that may not be necessary. We need only update the K-V store every n days (using the find/prime-pump,
or other applicaple method) and continue to delete files on a daily basis.

