# libtfs: A simple wrapper for C stdio with tar access

## Compiling
```shell
make
```

## Usage
### Compile
```shell
<cc> ... -include <path/to/tfs.h> ...
<ld> ... -L <path/to/libtfs> -ltfs
```

### to access a file in tar

For example, '/dir/file.suf'

```C
// ...
/* init tfs to a tar file */
tfs_inittarfile("path/to/file.tar");
/* open a file */
FILE* fp = fopen("@/dir/file.suf");
/* whatever */
// ...
/* close file handle */
fclose(fp);
```
