# gdb --batch --command=misc/types.gdb --args ./a.out
#
# Used in conjunction with the executable produced by types.c.

set pagination off
break main
run

printf "%-12s: ", "blkcnt_t"
whatis blkcnt_t

printf "%-12s: ", "blksize_t"
whatis blksize_t

printf "%-12s: ", "clock_t"
whatis clock_t

printf "%-12s: ", "clockid_t"
whatis clockid_t

printf "%-12s: ", "dev_t"
whatis dev_t

printf "%-12s: ", "fsblkcnt_t"
whatis fsblkcnt_t

printf "%-12s: ", "fsfilcnt_t"
whatis fsfilcnt_t

printf "%-12s: ", "gid_t"
whatis gid_t

printf "%-12s: ", "id_t"
whatis id_t

printf "%-12s: ", "ino_t"
whatis ino_t

printf "%-12s: ", "key_t"
whatis key_t

printf "%-12s: ", "mode_t"
whatis mode_t

printf "%-12s: ", "nlink_t"
whatis nlink_t

printf "%-12s: ", "off_t"
whatis off_t

printf "%-12s: ", "pid_t"
whatis pid_t

printf "%-12s: ", "size_t"
whatis size_t

printf "%-12s: ", "ssize_t"
whatis ssize_t

printf "%-12s: ", "suseconds_t"
whatis suseconds_t

printf "%-12s: ", "time_t"
whatis time_t

printf "%-12s: ", "timer_t"
whatis timer_t

printf "%-12s: ", "uid_t"
whatis uid_t
