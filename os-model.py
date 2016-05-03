from enum import Enum


class SystemCall(Enum):
    READ = "read"
    WRITE = "write"
    OPEN = "open"
    CLOSE = "close"
    EXIT = "exit"
    KILL = "kill"
    FORK = "fork"
    PIPE = "pipe"
    DUP2 = "dup2"

pid_count = 0
cur_pid = -1

process_list = []
file_table = []
pipe_ends = []
pipe_buffers = []
blocked_process_list = []
last_contexts = []
per_process_fdtables = [dict()]


def open_fd(path, mode):
    global cur_pid
    if len(per_process_fdtables[cur_pid]) > 0:
        fildes = max(per_process_fdtables[cur_pid]) + 1
    else:
        fildes = 3  # stdin/stdout/stderr are 0, 1, 2
    offset = 0
    file_table.append((open(path, mode), mode, offset))
    per_process_fdtables[cur_pid][fildes] = len(file_table) - 1
    return fildes


def close(fildes):
    global cur_pid
    del per_process_fdtables[cur_pid][fildes]
    return 0
    # free resources?


def read(fildes, buffer, count):
    global cur_pid
    if fildes in list(pipe_ends.keys()):
        offset = 0
        pipe_buf = pipe_buffers[cur_pid][fildes]
        while pipe_buf[offset] is not None:
            buffer[offset] = pipe_buf[offset]
            pipe_buf[offset] = None
            offset += 1
        if offset > 0:
            for context in blocked_process_list:
                if context[0] == SystemCall.WRITE and context[1][0] in pipe_buffers[cur_pid]:
                    blocked_process_list.remove(context)
                    process_list.append(context[2])

        return offset

    else:
        file_handle = file_table[per_process_fdtables[cur_pid][fildes]]
        file_handle[0].seek(1, file_handle[2])
        result = file_handle[0].read(count)
        file_table[per_process_fdtables[cur_pid][fildes]] = (file_handle[0], file_handle[1], file_handle[2] + len(result))
        for i in range(min(len(result), len(buffer))):
            buffer[i] = result[i]
        return len(result)


def write(fildes, buffer, count):
    global cur_pid
    if fildes in pipe_ends.values():
        pipe_buf = pipe_buffers[cur_pid][list(pipe_ends.keys())[list(pipe_ends.values()).index(fildes)]]
        if pipe_buf[len(pipe_buf) - 1] is not None:
            blocked_process_list.append(last_context)
        offset = len(pipe_buf) - 1
        while offset > 0 and pipe_buf[offset] is not None:
            offset -= 1
        offset += 1
        for i in range(offset, len(pipe_buf)):
            pipe_buf[i] = buffer[i]
        return len(pipe_buf) - offset

    else:
        file_handle = file_table[per_process_fdtables[cur_pid][fildes]]
        file_handle[0].seek(1, file_handle[2])
        result = file_handle[0].write(''.join(buffer[:count]))
        file_table[per_process_fdtables[cur_pid][fildes]] = (file_handle[0], file_handle[1], file_handle[2] + result)
        return result


def dup2(from_fd, to_fd):
    global cur_pid
    close(to_fd)
    per_process_fdtables[cur_pid][to_fd] = per_process_fdtables[cur_pid][from_fd]
    return to_fd


def pipe():
    global cur_pid
    if len(per_process_fdtables[cur_pid]) > 0:
        in_end = max(per_process_fdtables[cur_pid]) + 1
    else:
        in_end = 3
    out_end = in_end + 1
    per_process_fdtables[cur_pid][in_end] = -1
    per_process_fdtables[cur_pid][out_end] = -1
    pipe_ends[cur_pid][in_end] = out_end
    pipe_buffers[cur_pid][in_end] = [None] * 16
    return in_end, out_end


def kill(pid):
    global process_list
    process_list = list(filter(lambda x: x[2] != pid, process_list))
    return 0


def kernel(program, args):
    global pid_count
    global cur_pid
    pid_count += 1
    pid = pid_count
    per_process_fdtables.append(dict())
    pipe_ends.append(dict())
    pipe_buffers.append([None] * 16)
    last_contexts.append([])

    process_list.append((program, args, pid))

    while len(process_list) > 0:

        (next_process, next_args, next_pid) = process_list.pop()
        cur_pid = next_pid
        (sys_call, args, cont) = next_process(*next_args)
        last_contexts[cur_pid] = (sys_call, args, (next_process, next_args, next_pid))

        if sys_call == SystemCall.READ:
            read_result = read(*args)
            process_list.append((cont, [read_result], next_pid))

        elif sys_call == SystemCall.WRITE:
            write_result = write(*args)
            process_list.append((cont, [write_result], next_pid))

        elif sys_call == SystemCall.OPEN:
            open_result = open_fd(*args)
            process_list.append((cont, [open_result], next_pid))

        elif sys_call == SystemCall.CLOSE:
            close_result = close(args[0])
            process_list.append((cont, [close_result], next_pid))

        elif sys_call == SystemCall.DUP2:
            dup2_result = dup2(*args)
            process_list.append((cont, [dup2_result], next_pid))

        elif sys_call == SystemCall.PIPE:
            pipe_result = pipe()
            process_list.append((cont, [pipe_result], next_pid))

        elif sys_call == SystemCall.KILL:
            kill_result = kill(args[0])
            process_list.append((cont, [kill_result], next_pid))

        elif sys_call == SystemCall.FORK:
            pid_count += 1
            process_list.append((cont, [pid_count], cur_pid))
            process_list.append((cont, [0], pid_count))
            per_process_fdtables.append(dict())  # imagine that we put stdin/stdout/stderr abstractions here
            pipe_buffers.append([None] * 16)
            last_contexts.append([])
            pipe_ends.append(dict())

        elif sys_call == SystemCall.EXIT:
            for fildes in list(per_process_fdtables[cur_pid].keys()):
                close(fildes)

        else:
            print("ERROR: no such system call")


buf = [None] * 200
def open_file_0():
    return SystemCall.OPEN, ["/home/heat_wave/.bashrc", "r"], open_file_1


def open_file_1(fildes):
    return SystemCall.READ, [fildes, buf, 40], open_file_2


def open_file_2(read_count):
    print(''.join(buf[:read_count]))
    return SystemCall.EXIT, [0], None


def call_pipe_0():
    return SystemCall.PIPE, [], call_pipe_1


def call_pipe_1(pipe):
    return SystemCall.CLOSE, [pipe[0]], call_pipe_2


def call_pipe_2(result):
    return SystemCall.EXIT, [0], None

kernel(call_pipe_0, [], [])
kernel(call_pipe_0, [], [])