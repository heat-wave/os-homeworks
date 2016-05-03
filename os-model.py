from enum import Enum


class SystemCall(Enum):
    READ = "read"
    WRITE = "write"
    OPEN = "open"
    CLOSE = "close"
    EXIT = "exit"
    KILL = "kill"

pid_count = 0

buf = [None] * 200

process_list = []
file_table = []
pipe_ends = dict()

per_process_fdtables = [dict()]


def open_fd(path, mode):
    pid = 0
    if len(per_process_fdtables[pid]) > 0:
        fildes = max(per_process_fdtables[pid]) + 1
    else:
        fildes = 3  # default stdin/stdout/stderr
    offset = 0
    file_table.append((open(path, mode), mode, offset))
    per_process_fdtables[pid][fildes] = len(file_table) - 1
    return fildes


def close(fildes):
    pid = 0
    del per_process_fdtables[pid][fildes]
    return 0
    # free resources?


def read(fildes, buffer, count):
    pid = 0
    if fildes in pipe_ends:
        pass

    else:
        file_handle = file_table[per_process_fdtables[pid][fildes]]
        file_handle[0].seek(1, file_handle[2])
        result = file_handle[0].read(count)
        file_table[per_process_fdtables[pid][fildes]] = (file_handle[0], file_handle[1], file_handle[2] + len(result))
        for i in range(min(len(result), len(buffer))):
            buffer[i] = result[i]
        return len(result)


def write(fildes, buffer, count):
    pid = 0
    file_handle = file_table[per_process_fdtables[pid][fildes]]
    file_handle[0].seek(1, file_handle[2])
    result = file_handle[0].write(''.join(buffer[:count]))
    file_table[per_process_fdtables[pid][fildes]] = (file_handle[0], file_handle[1], file_handle[2] + result)
    return result


def dup2(from_fd, to_fd):
    pid = 0
    per_process_fdtables[pid][from_fd] = per_process_fdtables[pid][to_fd]


def pipe():
    pid = 0
    if len(per_process_fdtables[pid]) > 0:
        in_end = max(per_process_fdtables[pid]) + 1
    else:
        in_end = 3
    out_end = in_end + 1
    per_process_fdtables[pid][in_end] = -1
    per_process_fdtables[pid][out_end] = -1
    pipe_ends[in_end] = out_end
    return (in_end, out_end)


def kill(pid):
    pass


def kernel(program, args, stdin):
    print("Running process {} with args={}, stdin={}".format(program, args, stdin))
    global pid_count
    pid_count += 1
    pid = 0
    per_process_fdtables[pid] = dict()

    process_list.append((program, args))

    while len(process_list) > 0:

        (next_process, next_args) = process_list.pop()
        (sys_call, args, cont) = next_process(*next_args)

        if sys_call == SystemCall.READ:
            read_result = read(*args)
            process_list.append((cont, [read_result]))
        elif sys_call == SystemCall.WRITE:
            write_result = write(*args)
            process_list.append((cont, [write_result]))
        elif sys_call == SystemCall.OPEN:
            open_result = open_fd(args[0], 'r')
            process_list.append((cont, [open_result]))
        elif sys_call == SystemCall.CLOSE:
            close_result = close(args[0])
            process_list.append((cont, [close_result]))
        elif sys_call == SystemCall.KILL:
            pass
        elif sys_call == SystemCall.EXIT:
            print("Exit code: {}".format(args[0]))
        else:
            print("ERROR")


def open_file_0():
    return (SystemCall.OPEN, ["/home/heat_wave/.bashrc"], open_file_1)


def open_file_1(fildes):
    return (SystemCall.READ, [fildes, 40, buf], open_file_2)


def open_file_2(read_count):
    print(*buf[:read_count])
    return (SystemCall.EXIT, [0], None)

kernel(open_file_0, [], [])