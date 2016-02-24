# fastfindem
Equivalent of find -type f &lt;path> -mtime +&lt;days> -delete

Version 0.1 is roughly 6 times slower than find, due to realpath() checking every folder in the tree.
