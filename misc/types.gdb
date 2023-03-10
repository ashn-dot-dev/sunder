# gdb --batch --command=misc/types.gdb --args ./a.out
#
# Used in conjunction with the executable produced by types.c.

set pagination off
break main
run

printf "%-12s: ", "uint8_t"
ptype/r uint8_t

printf "%-12s: ", "uint16_t"
ptype/r uint16_t

printf "%-12s: ", "uint32_t"
ptype/r uint32_t

printf "%-12s: ", "uint64_t"
ptype/r uint64_t

printf "%-12s: ", "int8_t"
ptype/r int8_t

printf "%-12s: ", "int16_t"
ptype/r int16_t

printf "%-12s: ", "int32_t"
ptype/r int32_t

printf "%-12s: ", "int64_t"
ptype/r int64_t

printf "%-12s: ", "blkcnt_t"
ptype/r blkcnt_t

printf "%-12s: ", "blkcnt_t"
ptype/r blkcnt_t

printf "%-12s: ", "blksize_t"
ptype/r blksize_t

printf "%-12s: ", "clock_t"
ptype/r clock_t

printf "%-12s: ", "clockid_t"
ptype/r clockid_t

printf "%-12s: ", "dev_t"
ptype/r dev_t

printf "%-12s: ", "fsblkcnt_t"
ptype/r fsblkcnt_t

printf "%-12s: ", "fsfilcnt_t"
ptype/r fsfilcnt_t

printf "%-12s: ", "gid_t"
ptype/r gid_t

printf "%-12s: ", "id_t"
ptype/r id_t

printf "%-12s: ", "ino_t"
ptype/r ino_t

printf "%-12s: ", "key_t"
ptype/r key_t

printf "%-12s: ", "mode_t"
ptype/r mode_t

printf "%-12s: ", "nlink_t"
ptype/r nlink_t

printf "%-12s: ", "off_t"
ptype/r off_t

printf "%-12s: ", "pid_t"
ptype/r pid_t

printf "%-12s: ", "size_t"
ptype/r size_t

printf "%-12s: ", "ssize_t"
ptype/r ssize_t

printf "%-12s: ", "suseconds_t"
ptype/r suseconds_t

printf "%-12s: ", "time_t"
ptype/r time_t

printf "%-12s: ", "timer_t"
ptype/r timer_t

printf "%-12s: ", "uid_t"
ptype/r uid_t
