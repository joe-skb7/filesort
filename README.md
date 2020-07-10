# filesort

Program sorts integers in a text file using limited RAM specified by
`BUFFER_SIZE` with multiple `THREADS` threads. It's just a test task, so this
program can't be considered really stable or fast.

## Internals

As the input file might not fit into the RAM (`BUFFER_SIZE`), additional files
must be used to store temporary sorting results. Such an algorithm is called
**External Sorting** [1,2,3].

Here is the short description of program's internal implementation:

1. Read input text file, parsing integers, by small chunks (`BUFFER_SIZE`)
2. Sort each chunk using multiple threads (`THREADS`). **Parallel Merge Sort**
   algorithm [4,5] is used for that. Basically it splits all values between
   threads equally, then each thread runs a regular merge sort on its data set,
   and in the end (once all threads are finished) the final merge is done
   recursively. This is probably the only place where we can benefit from
   multiple threads.
3. Store sorted chunks into temporary binary files
4. Merge those files into a single binary file using **K-way merge** algorithm
   [6]. 16-way merge is used by default, as it was shown empirically to be
   optimal value. This is done in a single thread, as it consists mostly of I/O
   operations, so it's not CPU bound and there is not much sense in trying to do
   that in parallel. In order to keep all **K** chunks sorted while merging,
   **priority queue** data structure is used, built on top of **heap** (binary
   tree) [7].
5. Store the final merged binary file into the output text file

Of course, the same behavior can be achieved with UNIX `sort` tool:

```bash
    $ sort -n --parallel=4 -S 1M < in.txt > out.txt
```

Though `filesort` works approx. ~4 times faster, probably due to storing the
temporary files in binary representation [8].

## Stability

All basic corner cases were checked for. Sanity test exists (`make test`), which
generates big file of integers, then runs `sort` and `filesort` on it, and
compares the results. For better stability the program should be covered with
unit-tests.

## Performance

Program can be optimized further, e.g. input text file can be read using bigger
chunks, parsing can be done in parallel with I/O operations, etc. There is no
sense in it, though, as for real tasks we can use `sort`, SQL databases, etc.

## Documentation

Apart from this file, the public part of the code is mostly covered with
Doxygen comments. One can run `make doxy` rule to generate html/pdf.

## Authors

**Sam Protsenko**

## License

This project is licensed under the GPLv3.

## References

[1] https://en.wikipedia.org/wiki/External_sorting#External_merge_sort

[2] https://stackoverflow.com/questions/38932827/external-multithreading-sort

[3] https://stackoverflow.com/questions/1645566/efficient-out-of-core-sorting

[4] https://en.wikipedia.org/wiki/Merge_sort#Parallel_merge_sort

[5] https://malithjayaweera.com/2019/02/parallel-merge-sort/

[6] https://en.wikipedia.org/wiki/Merge_algorithm#K-way_merging

[7] https://www.geeksforgeeks.org/binary-heap/

[8] http://www.maizure.org/projects/decoded-gnu-coreutils/sort.html
