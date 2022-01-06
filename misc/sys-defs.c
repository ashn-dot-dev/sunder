// System definitions & constants for x86-64 Linux.
// Currently only tested (and ordered numerically) using x86-64 Linux w/ glibc.
//
// Definitions taken from:
// https://pubs.opengroup.org/onlinepubs/9699919799.2018edition/functions/open.html
// https://pubs.opengroup.org/onlinepubs/9699919799.2018edition/basedefs/sys_stat.h.html
// https://pubs.opengroup.org/onlinepubs/9699919799/functions/mmap.html
#include <fcntl.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>


int
main(void)
{
#define PRINT_O_VALUE(o_value) \
    printf("const %-12s s32 = 0x%08xs32;\n", #o_value ":", o_value)
    // Applications shall specify exactly one of the first five values (file access modes) below in the value of oflag:
    /* not in glibc
    PRINT_O_VALUE(O_EXEC);   // Open for execute only (non-directory files). The result is unspecified if this flag is applied to a directory.
    */
    PRINT_O_VALUE(O_RDONLY); // Open for reading only.
    PRINT_O_VALUE(O_WRONLY); // Open for writing only.
    PRINT_O_VALUE(O_RDWR);   // Open for reading and writing. The result is undefined if this flag is applied to a FIFO.
    /*  not in glibc
    PRINT_O_VALUE(O_SEARCH); // Open directory for search only. The result is unspecified if this flag is applied to a non-directory file.
    */

    // Any combination of the following may be used:
    PRINT_O_VALUE(O_CREAT);     // If the file exists, this flag has no effect except as noted under O_EXCL below. Otherwise, if O_DIRECTORY is not set the file shall be created as a regular file; the user ID of the file shall be set to the effective user ID of the process; the group ID of the file shall be set to the group ID of the file's parent directory or to the effective group ID of the process; and the access permission bits (see <sys/stat.h>) of the file mode shall be set to the value of the argument following the oflag argument taken as type mode_t modified as follows: a bitwise AND is performed on the file-mode bits and the corresponding bits in the complement of the process' file mode creation mask. Thus, all bits in the file mode whose corresponding bit in the file mode creation mask is set are cleared. When bits other than the file permission bits are set, the effect is unspecified. The argument following the oflag argument does not affect whether the file is open for reading, writing, or for both. Implementations shall provide a way to initialize the file's group ID to the group ID of the parent directory. Implementations may, but need not, provide an implementation-defined way to initialize the file's group ID to the effective group ID of the calling process.
    PRINT_O_VALUE(O_EXCL);      // If O_CREAT and O_EXCL are set, open() shall fail if the file exists. The check for the existence of the file and the creation of the file if it does not exist shall be atomic with respect to other threads executing open() naming the same filename in the same directory with O_EXCL and O_CREAT set. If O_EXCL and O_CREAT are set, and path names a symbolic link, open() shall fail and set errno to [EEXIST], regardless of the contents of the symbolic link. If O_EXCL is set and O_CREAT is not set, the result is undefined.
    PRINT_O_VALUE(O_NOCTTY);    // If set and path identifies a terminal device, open() shall not cause the terminal device to become the controlling terminal for the process. If path does not identify a terminal device, O_NOCTTY shall be ignored.
    PRINT_O_VALUE(O_TRUNC);     // If the file exists and is a regular file, and the file is successfully opened O_RDWR or O_WRONLY, its length shall be truncated to 0, and the mode and owner shall be unchanged. It shall have no effect on FIFO special files or terminal device files. Its effect on other file types is implementation-defined. The result of using O_TRUNC without either O_RDWR or O_WRONLY is undefined.
    PRINT_O_VALUE(O_APPEND);    // If set, the file offset shall be set to the end of the file prior to each write.
    PRINT_O_VALUE(O_CLOEXEC);   // If set, the FD_CLOEXEC flag for the new file descriptor shall be set.
    PRINT_O_VALUE(O_DIRECTORY); // If path resolves to a non-directory file, fail and set errno to [ENOTDIR].
    PRINT_O_VALUE(O_NOFOLLOW);  // If path names a symbolic link, fail and set errno to [ELOOP].
    PRINT_O_VALUE(O_NONBLOCK);  // When opening a FIFO with O_RDONLY or O_WRONLY set: If O_NONBLOCK is set, an open() for reading-only shall return without delay. An open() for writing-only shall return an error if no process currently has the file open for reading. If O_NONBLOCK is clear, an open() for reading-only shall block the calling thread until a thread opens the file for writing. An open() for writing-only shall block the calling thread until a thread opens the file for reading. When opening a block special or character special file that supports non-blocking opens: If O_NONBLOCK is set, the open() function shall return without blocking for the device to be ready or available. Subsequent behavior of the device is device-specific. If O_NONBLOCK is clear, the open() function shall block the calling thread until the device is ready or available before returning. Otherwise, the O_NONBLOCK flag shall not cause an error, but it is unspecified whether the file status flags will include the O_NONBLOCK flag.
    PRINT_O_VALUE(O_DSYNC);     // [SIO] [Option Start] Write I/O operations on the file descriptor shall complete as defined by synchronized I/O data integrity completion. [Option End]
    PRINT_O_VALUE(O_RSYNC);     // [SIO] [Option Start] Read I/O operations on the file descriptor shall complete at the same level of integrity as specified by the O_DSYNC and O_SYNC flags. If both O_DSYNC and O_RSYNC are set in oflag, all I/O operations on the file descriptor shall complete as defined by synchronized I/O data integrity completion. If both O_SYNC and O_RSYNC are set in flags, all I/O operations on the file descriptor shall complete as defined by synchronized I/O file integrity completion. [Option End]
    PRINT_O_VALUE(O_SYNC);      // [XSI|SIO] [Option Start] Write I/O operations on the file descriptor shall complete as defined by synchronized I/O file integrity completion. [Option End] [XSI] [Option Start] The O_SYNC flag shall be supported for regular files, even if the Synchronized Input and Output option is not supported. [Option End]
    /*  not in glibc
    PRINT_O_VALUE(O_TTY_INIT);  // If path identifies a terminal device other than a pseudo-terminal, the device is not already open in any process, and either O_TTY_INIT is set in oflag or O_TTY_INIT has the value zero, open() shall set any non-standard termios structure terminal parameters to a state that provides conforming behavior; see XBD Parameters that Can be Set. It is unspecified whether O_TTY_INIT has any effect if the device is already open in any process. If path identifies the slave side of a pseudo-terminal that is not already open in any process, open() shall set any non-standard termios structure terminal parameters to a state that provides conforming behavior, regardless of whether O_TTY_INIT is set. If path does not identify a terminal device, O_TTY_INIT shall be ignored.
    */
#undef PRINT_O_VALUE

    fputc('\n', stdout);

#define PRINT_S_VALUE(s_value) \
    printf("const %-8s u16 = 0o%03ou16;\n", #s_value ":", s_value)
    // The <sys/stat.h> header shall define the following symbolic constants for the file mode bits encoded in type mode_t, with the indicated numeric values. These macros shall expand to an expression which has a type that allows them to be used, either singly or OR'ed together, as the third argument to open() without the need for a mode_t cast. The values shall be suitable for use in #if preprocessing directives.
    PRINT_S_VALUE(S_IRWXU); // Read, write, execute/search by owner.
    PRINT_S_VALUE(S_IRUSR); // Read permission, owner.
    PRINT_S_VALUE(S_IWUSR); // Write permission, owner.
    PRINT_S_VALUE(S_IXUSR); // Execute/search permission, owner.
    fputc('\n', stdout);
    PRINT_S_VALUE(S_IRWXG); // Read, write, execute/search by group.
    PRINT_S_VALUE(S_IRGRP); // Read permission, group.
    PRINT_S_VALUE(S_IWGRP); // Write permission, group.
    PRINT_S_VALUE(S_IXGRP); // Execute/search permission, group.
    fputc('\n', stdout);
    PRINT_S_VALUE(S_IRWXO); // Read, write, execute/search by others.
    PRINT_S_VALUE(S_IROTH); // Read permission, others.
    PRINT_S_VALUE(S_IWOTH); // Write permission, others.
    PRINT_S_VALUE(S_IXOTH); // Execute/search permission, others.
#undef PRINT_S_VALUE

    fputc('\n', stdout);

#define PRINT_S_VALUE(s_value) \
    printf("const %-9s u16 = 0x%04xu16;\n", #s_value ":", s_value)
    // The <sys/stat.h> header shall define the following symbolic constants for the file types encoded in type mode_t. The values shall be suitable for use in #if preprocessing directives:
    PRINT_S_VALUE(S_IFMT);   // [XSI] [Option Start] Type of file.
    PRINT_S_VALUE(S_IFIFO);  // FIFO special.
    PRINT_S_VALUE(S_IFCHR);  // Character special.
    PRINT_S_VALUE(S_IFDIR);  // Directory.
    PRINT_S_VALUE(S_IFBLK);  // Block special.
    PRINT_S_VALUE(S_IFREG);  // Regular.
    PRINT_S_VALUE(S_IFLNK);  // Symbolic link.
    PRINT_S_VALUE(S_IFSOCK); // Socket.
#undef PRINT_S_VALUE

    fputc('\n', stdout);

#define PRINT_PROT_VALUE(prot_value) \
    printf("const %-11s ssize = 0x%01xs;\n", #prot_value ":", prot_value)
    // The parameter prot determines whether read, write, execute, or some combination of accesses are permitted to the data being mapped. The prot shall be either PROT_NONE or the bitwise-inclusive OR of one or more of the other flags in the following table, defined in the <sys/mman.h> header.
    PRINT_PROT_VALUE(PROT_NONE);  // Data cannot be accessed.
    PRINT_PROT_VALUE(PROT_READ);  // Data can be read.
    PRINT_PROT_VALUE(PROT_WRITE); // Data can be written.
    PRINT_PROT_VALUE(PROT_EXEC);  // Data can be executed.
#undef PRINT_PROT_VALUE

    fputc('\n', stdout);

#define PRINT_MAP_VALUE(map_value) \
    printf("const %-14s ssize = 0x%02xs;\n", #map_value ":", map_value)
    // The parameter flags provides other information about the handling of the mapped data. The value of flags is the bitwise-inclusive OR of these options, defined in <sys/mman.h>:
    PRINT_MAP_VALUE(MAP_SHARED);  // Changes are shared.
    PRINT_MAP_VALUE(MAP_PRIVATE); // Changes are private.
    PRINT_MAP_VALUE(MAP_FIXED);   // Interpret addr exactly.
    // The following flags are *NOT* part of the POSIX standard but are defined on Linux.
    // Descriptions have been taken from man pages.
    PRINT_MAP_VALUE(MAP_ANONYMOUS); // The mapping is not backed by any file; its contents are initialized to zero.  The fd argument is ignored; however, some implementations require fd to be -1 if  MAP_ANONYMOUS  (or MAP_ANON) is specified, and portable applications should ensure this.  The offset argument should be zero.  The use of MAP_ANONYMOUS in conjunction with MAP_SHARED is supported on Linux only since kernel 2.4.
#undef PRINT_MAP_VALUE

}
