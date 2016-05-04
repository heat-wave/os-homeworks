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


'''
The code below might contain an arbitrary number of errors; they are, however,
tiny and easy to fix in case you need a fully working model of kernel. It is already
operational, but IPC and piping are not properly tested, thereby not guaranteed.

DISCLAIMER: Due to timing restrictions, the code hasn't been cleaned up or
    refactored. Excessive use of global variables is heavily discouraged, as is
    ambiguous behaviour or type structure. Please do not repeat this.

Based on A.Komarov & J.Pjankova's kernel model:
[http://neerc.ifmo.ru/~os/static/model.py]

The file system model is as follows:
- each process has its own file descriptor table
- each file descriptor is a small, nonnegative integer
 that maps into an entry in the global file table
- file table entries contain links to the corresponding
 inodes and other relevant information about the file
- inodes are here considered magically given and wrapped
 for us for simplicity's sake

How processes are handled here:
- starting process has PID 1
- fork syscall forks current process, so
 the child has PID = max(previous PIDs) + 1
- different processes have different std/stdout/stderr
 and file descriptor table, as well as helper structures
- kill sends a termination signal to all processes with
 the specified PID, removing them from the stack completely
- exit ends the process gracefully, allowing it to free
 resources such as file descriptors and do something on exit

How kernel works in short:
- regular code is broken into almost-atomic pieces which contain
 a system call, the arguments to execute it with, and a reference
 to the piece that will follow
- when the piece is popped off the stack, its system call is executed,
 and the new piece is pushed onto the stack
- this process repeats until the stack is completely empty, meaning
 that the program has finished working and no longer makes system calls
'''

pid_count = 0
cur_pid = -1

'''
This is a reference table in case you get stuck in Python's type system

 per_process_fdtables: PID -> (Map: Int (index in fd table) -> Int (index in file table))
 file_table:           Int (index) -> FileInfo
 FileInfo:             Tuple: IOWrapper (inode abstraction), String (mode), Int (offset in seekable file)
 pipe_ends:            PID -> (Map: Int (write end) -> Int (read end))
 blocked_process_list: PID -> List[Context]
 Context:              Tuple: SystemCall syscall, List args, Process next
 Process:              Tuple: Function toExecute, List args, PID pid
 PID:                  Int
'''
process_list = []
file_table = []
pipe_ends = []
pipe_buffers = []
blocked_process_list = []
last_contexts = []
per_process_fdtables = [dict()]


'''
Opens the file at the specified path with given mode
 (read/write/both), creating an open file description
 if there wasn't one for that file

Parameters:
__________
path: string
    system-specific file path
mode: string
    "r", "w", or "rw"

Returns:
__________
fildes: int
    file descriptor that for this process maps into
    an entry in the file table (one for all processes)
    corresponding to the file
'''
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


'''
Closes the specified file descriptor,
freeing any resources that become
useless in the process

Parameters:
__________
fildes: int
    file descriptor to close

Returns:
__________
res: int
    success/error code
'''
def close(fildes):
    global cur_pid
    del per_process_fdtables[cur_pid][fildes]
    return 0
    # here we also free any open file descriptions that
    # no one is referring to. Feel free to implement


'''
Reads up to `count` bytes to `buffer` from
whatever `fildes` is pointing to

Parameters:
__________
fildes: int
    file descriptor to read from
buffer: list
    byte array to read to
count: int
    number of bytes to read

Returns:
__________
res: int
    number of bytes actually read
'''
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


'''
Writes up to `count` bytes from `buffer` to
whatever `fildes` is pointing to

Parameters:
__________
fildes: int
    receiving file descriptor
buffer: list
    byte array to write from
count: int
    number of bytes to write

Returns:
__________
res: int
    number of bytes actually written
'''
def write(fildes, buffer, count):
    global cur_pid
    if fildes in pipe_ends.values():
        pipe_buf = pipe_buffers[cur_pid][list(pipe_ends.keys())[list(pipe_ends.values()).index(fildes)]]
        if pipe_buf[len(pipe_buf) - 1] is not None:
            blocked_process_list.append(last_contexts[cur_pid])
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


'''
Makes the file descriptor `to_fd` refer to the
same open file description as `from_fd`
Closes `to_fd` before redirecting it to avoid
resource leaks

Parameters:
__________
from_fd: int
    the file descriptor where `to_fd` should point
to_fd: int
    the file descriptor to modify

Returns:
__________
to_fd: int
    the modified file descriptor
'''
def dup2(from_fd, to_fd):
    global cur_pid
    close(to_fd)
    per_process_fdtables[cur_pid][to_fd] = per_process_fdtables[cur_pid][from_fd]
    return to_fd


'''
Creates a pipe (a unidirectional data channel that
 can be used for interprocess communication)
 Data written to the write end of pipe is buffered
 until it is read from the read end of the pipe

Parameters:
__________

Returns:
__________
in_end: int
    file descriptor referring to the write end of the pipe
out_end: int
    file descriptor referring to the read end of the pipe
'''
def pipe():
    global cur_pid
    if len(per_process_fdtables[cur_pid]) > 0:
        in_end = max(per_process_fdtables[cur_pid]) + 1
    else:
        in_end = 3  # again, stdin/stdout/stderr go first
    out_end = in_end + 1
    per_process_fdtables[cur_pid][in_end] = -1
    per_process_fdtables[cur_pid][out_end] = -1
    pipe_ends[cur_pid][in_end] = out_end
    pipe_buffers[cur_pid][in_end] = [None] * 16
    return in_end, out_end


'''
Simply put, sends a kill signal to the process with
the specified process ID

Parameters:
__________
pid: int
    process ID whose instances should be terminated

Returns:
__________
code: int
    success or error code
'''
def kill(pid):
    global process_list
    process_list = list(filter(lambda x: x[2] != pid, process_list))
    return 0


'''
Runs a program with its arguments, managing security,
process and resource isolation, necessary abstractions, etc.

Parameters:
__________
program: function
    the program to be executed
args: list
    the parameters for the program

Returns:
__________
'''
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


'''
Examples of use are below

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
'''