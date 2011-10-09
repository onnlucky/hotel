// ** basic io functions in hotel **

// TODO use O_CLOEXEC if available; set FD_CLOEXEC otherwise
// TODO use getaddrinfo stuff to get ipv6 compat; take care when doing name resolutions though
// TODO thread safety when we want to run multiple workers and a ev loop thread ... how?

#define EV_STANDALONE 1
#define EV_MULTIPLICITY 0
#include "ev/ev.c"

#include "trace-on.h"

static tlSym s_file;
static tlSym s_dir;
static tlSym s_child;

//static tlMap *_stat_tlmap;
static tlSym _s_dev;
static tlSym _s_ino;
static tlSym _s_mode;
static tlSym _s_nlink;
static tlSym _s_uid;
static tlSym _s_gid;
static tlSym _s_rdev;
static tlSym _s_size;
static tlSym _s_blksize;
static tlSym _s_blocks;
static tlSym _s_atime;
static tlSym _s_mtime;

static void signal_cb(ev_signal *signal, int revents) {
    print("signal");
}

int test_ev(char **args, int argc) {
    ev_signal signal;
    ev_signal_init(&signal, signal_cb, SIGINT);
    ev_signal_start(&signal);
    return 0;
}


// ** timers **
// TODO repeating timers maybe?

static void timer_cb(ev_timer *timer, int revents) {
    trace("timer_cb: %p", timer);
    tlTask* task = tltask_as(timer->data);
    tlTaskReady(task);
    free(timer);
}

static tlPause* _io_sleep(tlTask* task, tlArgs* args) {
    int millis = tl_int_or(tlArgsAt(args, 0), 1000);
    trace("sleep: %d", millis);
    float ms = millis / 1000.0;

    ev_timer *timer = malloc(sizeof(ev_timer));
    timer->data = task;
    ev_timer_init(timer, timer_cb, ms, 0);
    ev_timer_start(timer);

    tlTaskWaitSystem(task);
    // TODO must return something to indicate pausing ...
    return null;
}

#if 0
// ** file descriptors **

static int nonblock(int fd) {
    int flags = 0;
    if ((flags = fcntl(fd, F_GETFL, 0)) < 0) return -1;
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) return -1;
    return 0;
}

typedef struct FileWrap {
    Wrap wrap;
    ev_io file;
} FileWrap;

static FileWrap * filewrap_from(ev_io *file) {
    return (FileWrap *) (((char *)file) - ((unsigned long) &((FileWrap *)0)->file));
}

static Value io_file(Task *task, int fd) {
    FileWrap *wrap = malloc(sizeof(FileWrap));
    init_head((Value)wrap, TWrap, 0);
    wrap->wrap.task = task;
    wrap->wrap.tag = s_file;
    ev_io_init(&wrap->file, null, fd, 0);
    return wrap;
}

static Value _io_close(CONTEXT) {
    FileWrap *wrap = _ARG1; TYPECHECK(wrap, TWrap); WRAPCHECK(wrap, task, s_file);
    assert(wrap->wrap.task == task);
    assert(wrap->wrap.tag == s_file);

    ev_io *file = &wrap->file;
    trace("close: %p %d", file, file->fd);
    ev_io_stop(file);

    int r = close(file->fd);
    if (r < 0) vm_throw(task, "close: failed:", strerror(errno), null);
    else vm_return(task, Null);

    wrap->wrap.task = null;
    return Ignore;
}

static void read_cb(ev_io *file, int revents) {
    trace("read_cb: %p %d", file, file->fd);
    FileWrap *wrap = filewrap_from(file);
    Task *task = asTask(wrap->wrap.task);
    Buffer *buf = (Buffer *)file->data;
    assert(isTask(wrap->wrap.task));
    assert(wrap->wrap.tag == s_file);

    int len = read(file->fd, writebuf(buf), canwrite(buf));
    if (len < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            ev_io_start(file); trace("read: EAGAIN || EWOULDBLOCK"); return;
        }
        vm_throw(task, "read: failed:", strerror(errno), null);
    } else {
        didwrite(buf, len);
        vm_return(task, INT(len));
    }
    ev_io_stop(file);
    if (task->state == P_WAIT) task_ready_system(task);
}

static Value _io_read(CONTEXT) {
    FileWrap *wrap = _ARG1; WRAPCHECK(wrap, task, s_file);
    Buffer *buf = unwrap(_ARG2, task, s_buffer); WRAPCHECK(_ARG2, task, s_buffer);
    //int ms = int_from_or(_ARG3, 0);

    if (canwrite(buf) <= 0) return vm_throw(task, "read: failed: buffer full", null);

    ev_io *file = &wrap->file;
    trace("read: %p, %d", file, file->fd);
    file->cb = read_cb;
    file->data = buf;

    int revents = ev_clear_pending(file);
    if (revents & EV_READ) { read_cb(file, revents); return Ignore; }

    file->events = EV_READ;
    ev_io_start(file);
    task_wait_system(task);
    return Ignore;
}

static void write_cb(ev_io *file, int revents) {
    trace("read_cb: %p %d", file, file->fd);
    FileWrap *wrap = filewrap_from(file);
    Task *task = asTask(wrap->wrap.task);
    Buffer *buf = (Buffer *)file->data;
    assert(isTask(wrap->wrap.task));
    assert(wrap->wrap.tag == s_file);

    int len = write(file->fd, readbuf(buf), canread(buf));
    if (len < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            ev_io_start(file); trace("write: EAGAIN || EWOULDBLOCK"); return;
        }
        vm_throw(task, "write: failed:", strerror(errno), null);
    } else {
        didread(buf, len);
        vm_return(task, INT(len));
    }
    ev_io_stop(file);
    if (task->state == P_WAIT) task_ready_system(task);
}

static Value _io_write(CONTEXT) {
    FileWrap *wrap = _ARG1; TYPECHECK(wrap, TWrap); WRAPCHECK(wrap, task, s_file);
    Buffer *buf = unwrap(_ARG2, task, s_buffer); WRAPCHECK(_ARG2, task, s_buffer);
    //int ms = int_from_or(_ARG3, 0);

    if (canread(buf) <= 0) return vm_throw(task, "write: failed: buffer empty", null);

    ev_io *file = &wrap->file;
    trace("write: %p, %d", file, file->fd);
    file->cb = write_cb;
    file->data = buf;

    int revents = ev_clear_pending(file);
    if (revents & EV_WRITE) { read_cb(file, revents); return Ignore; }

    file->events = EV_WRITE;
    ev_io_start(file);
    task_wait_system(task);
    return Ignore;
}

// TODO fix perms by passing them in
static Value _io_file_open(CONTEXT) {
    ARG1(Text, name);
    ARG2(Int, vflags);
    trace("open: %s", string_from(name));
    int flags = int_from(vflags);
    int perms = 0666;

    int fd = open(text_to_char(name), flags | O_NONBLOCK, perms);
    if (fd < 0) return vm_throw(task, "file_open: failed: ", strerror(errno), " file: '", string_from(name), "'", null);
    return io_file(task, fd);
}

static Value _io_tcp_open(CONTEXT) {
    ARG1(Text, ip_text);
    ARG2(Int, vport); int port = int_from(vport);
    trace("tcp_open: %s:%d", string_from(ip_text), port);

    struct in_addr ip;
    if (!inet_aton(text_to_char(ip_text), &ip)) {
        return vm_throw(task, "tcp_open: invalid ip: ", string_from(ip_text), null);
    }

    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    bcopy(&ip, &address.sin_addr.s_addr, sizeof(ip));
    address.sin_port = htons(port);

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return vm_throw(task, "tcp_open: failed: ", strerror(errno), null);

    if (nonblock(fd) < 0) vm_throw(task, "tcp_open: nonblock failed: ", strerror(errno), null);

    int r = connect(fd, (struct sockaddr *)&address, sizeof(address));
    if (r < 0 && errno != EINPROGRESS) {
        return vm_throw(task, "tcp_open: connect failed: ", strerror(errno), null);
    }
    if (errno == EINPROGRESS) trace("tcp_open: EINPROGRESS");

    return io_file(task, fd);
}

// TODO make backlog configurable, as SO_REUSEADDR and such
static Value _io_tcp_listen(CONTEXT) {
    ARG1(Int, vport); int port = int_from(vport);
    trace("tcp_listen: 0.0.0.0:%d", port);

    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return vm_throw(task, "tcp_listen: failed: ", strerror(errno), null);

    if (nonblock(fd) < 0) return vm_throw(task, "tcp_listen: nonblock failed: ", strerror(errno), null);

    int flags = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (void *)&flags, sizeof(flags)) < 0) {
        return vm_throw(task, "tcp_listen: so_reuseaddr failed: ", strerror(errno), null);
    }

    int r = bind(fd, (struct sockaddr *)&address, sizeof(address));
    if (r < 0) return vm_throw(task, "tcp_listen: bind failed: ", strerror(errno), null);

    listen(fd, 128); // backlog, configurable?
    return io_file(task, fd);
}

// TODO return multiple, socket and peer address
static void accept_cb(ev_io *file, int revents) {
    trace("accept_cb: %p %d", file, file->fd);
    FileWrap *wrap = filewrap_from(file);
    Task *task = asTask(wrap->wrap.task);
    assert(isTask(wrap->wrap.task));
    assert(wrap->wrap.tag == s_file);

    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    socklen_t len = sizeof(address);
    int clientfd = accept(file->fd, (struct sockaddr *)&address, &len);
    if (clientfd < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            ev_io_start(file); trace("tcp_accept: EAGAIN || EWOULDBLOCK"); return;
        }
        vm_throw(task, "tcp_accept: failed: ", strerror(errno), null);
    } else {
        if (nonblock(clientfd) < 0) {
            vm_throw(task, "tcp_accept: nonblock failed: ", strerror(errno), null);
        } else {
            vm_return(task, io_file(task, clientfd));
        }
    }
    ev_io_stop(file);
    if (task->state == P_WAIT) task_ready_system(task);
}

static Value _io_tcp_accept(CONTEXT) {
    FileWrap *wrap = _ARG1; TYPECHECK(wrap, TWrap); WRAPCHECK(wrap, task, s_file);
    //int ms = int_from_or(_ARG2, 0);

    ev_io *file = &wrap->file;
    trace("tcp_accept: %p, %d", file, file->fd);
    file->cb = accept_cb;

    int revents = ev_clear_pending(file);
    if (revents & EV_READ) { read_cb(file, revents); return Ignore; }

    file->events = EV_READ;
    ev_io_start(file);
    task_wait_system(task);
    return Ignore;
}

static Value _io_stat(CONTEXT) {
    ARG1(Text, file);
    struct stat buf;
    int r = stat(string_from(file), &buf);
    if (r == -1) {
        return vm_throw(task, "stat failed: ", strerror(errno), " for: '", string_from(file), "'", null);
    }
    tlMap *res = tlmap_copy(_stat_tlmap, 12);
    tlmap_set_(res, _s_dev, INT(buf.st_dev));
    tlmap_set_(res, _s_ino, INT(buf.st_ino));
    tlmap_set_(res, _s_mode, INT(buf.st_mode));
    tlmap_set_(res, _s_nlink, INT(buf.st_nlink));
    tlmap_set_(res, _s_uid, INT(buf.st_uid));
    tlmap_set_(res, _s_gid, INT(buf.st_gid));
    tlmap_set_(res, _s_rdev, INT(buf.st_rdev));
    tlmap_set_(res, _s_size, INT(buf.st_size));
    tlmap_set_(res, _s_blksize, INT(buf.st_blksize));
    tlmap_set_(res, _s_blocks, INT(buf.st_blocks));
    tlmap_set_(res, _s_atime, FLOAT(buf.st_atime));
    tlmap_set_(res, _s_mtime, FLOAT(buf.st_mtime));
    return res;
}

static Value _io_opendir(CONTEXT) {
    ARG1(Text, dir);
    trace("opendir: %s", string_from(dir));
    DIR *p = opendir(string_from(dir));
    if (!p) return vm_throw(task, "opendir: failed: ", strerror(errno), null);
    return WRAP(p, task, s_dir);
}

static Value _io_closedir(CONTEXT) {
    ARG1(Wrap, wrap);
    trace("closedir: %p", wrap);
    if (closedir(unwrap(wrap, task, s_dir))) {
        return vm_throw(task, "closedir: failed: ", strerror(errno), null);
    }
    return Null;
}

static Value _io_readdir(CONTEXT) {
    ARG1(Wrap, wrap);
    struct dirent dp;
    struct dirent *dpp;
    if (readdir_r(unwrap(wrap, task, s_dir), &dp, &dpp)) {
        return vm_throw(task, "readdir: failed: ", strerror(errno), null);
    }
    trace("readdir: %p", dpp);
    if (!dpp) return Null;
    Value res = text_new_copy(dp.d_name, strlen(dp.d_name));
    return res;
}

// exec ... replaces current process, stopping hotel effectively
// TODO do we want to close file descriptors?
// TODO do we want to reset signal handlers?
static Value _io_exec(CONTEXT) {
    char **argv = malloc(sizeof(char *) * (args_size(args) + 1));
    for (int i = 0; i < args_size(args); i++) {
        argv[i] = (char *)text_to_char(args_get(args, i));
    }
    execvp(argv[0], argv);
    return vm_throw(task, "exec: failed: ", strerror(errno), null);
}

static void child_cb(ev_child *child, int revents) {
    trace("child_cb: %p", child);
    ev_child_stop(child);
    if (child->data) {
        Task *task = asTask(child->data);
        vm_return(task, INT(child->rstatus));
        if (task->state == P_WAIT) task_ready_system(task);
    }
}

static Value io_child(Task *task, int pid) {
    ev_child *child = malloc(sizeof(ev_child));
    ev_child_init(child, child_cb, pid, 0);
    ev_child_start(child);
    return WRAP(child, task, s_child);
}

static Value _io_child_wait(CONTEXT) {
    ARG1(Wrap, childw);
    ev_child *child = unwrap(childw, task, s_child);

    trace("child_wait: %d", child->pid);
    if (!ev_is_active(child)) { // already exited
        return vm_return(task, INT(child->rstatus));
    }

    child->data = task;
    task_wait_system(task);
    return Ignore;
}

static Value _io_child_status(CONTEXT) {
    ARG1(Wrap, childw);
    ev_child *child = unwrap(childw, task, s_child);

    trace("child_status: %d, status: %d", child->pid, child->rstatus);
    return INT(child->rstatus);
}

// TODO do we want to reset signal handlers in child too?
// TODO we should also set nonblocking to pipes right?
static Value _io_child_exec(CONTEXT) {
    char **argv = malloc(sizeof(char *) * (args_size(args) + 1));
    for (int i = 0; i < args_size(args); i++)
        argv[i] = (char *)text_to_char(args_get(args, i));
    argv[args_size(args)] = 0;

    int _in[2];
    int _out[2];
    int _err[2];
    if (pipe(_in) || pipe(_out) || pipe(_err)) {
        return vm_throw(task, "child exec: failed: ", strerror(errno), null);
        return Null;
    }

    int pid = fork();
    if (pid) {
        // parent
        if (close(_in[0]) || close(_out[1]) || close(_err[1])) {
            return vm_throw(task, "child exec: failed: ", strerror(errno), null);
        }
        List *list = new(task, List, 4);
        list_set_(list, 0, io_child(task, pid));
        list_set_(list, 1, io_file(task, _in[1]));
        list_set_(list, 2, io_file(task, _out[0]));
        list_set_(list, 3, io_file(task, _err[0]));
        return vm_return_many(task, (Value)list);
    }

    // child
    dup2(_in[0], 0);
    dup2(_out[1], 1);
    dup2(_err[1], 2);
    //int max = getdtablesize();
    //if (max < 0) max = 50000;
    //for (int i = 3; i < max; i++) close(i); // by lack of anything better ...
    execvp(argv[0], argv);
    warning("child exec: failed: %s", strerror(errno));
    _exit(1);
}

// TOOD this is a blocking call
static Value _io_resolve(CONTEXT) {
    ARG1(Text, name);

    struct hostent *hp = gethostbyname(string_from(name));
    if (!hp) return Null;
    if (!hp->h_addr_list[0]) return Null;
    return TEXT(inet_ntoa(*(struct in_addr*)(hp->h_addr_list[0])));
}

static const pfunentry __io_pfuns[] = {
    { "\%io_sleep", _io_sleep },

    { "\%io_read",  _io_read },
    { "\%io_write", _io_write },
    { "\%io_close", _io_close },

    { "\%io_file_open",  _io_file_open },
    { "\%io_tcp_open",   _io_tcp_open },
    { "\%io_tcp_listen", _io_tcp_listen },
    { "\%io_tcp_accept", _io_tcp_accept },
    /*
    { "\%io_udp_open",   _io_udp_open },
    { "\%io_udp_listen", _io_udp_listen },
    { "\%io_udp_accept", _io_udp_accept },
    */

    { "\%io_stat",     _io_stat },
    { "\%io_opendir",  _io_opendir },
    { "\%io_closedir", _io_closedir },
    { "\%io_readdir",  _io_readdir },

    { "\%io_resolve", _io_resolve },

    { "\%io_exec",       _io_exec },
    { "\%io_child_exec", _io_child_exec },
    { "\%io_child_wait", _io_child_wait },

    { 0, 0 }
};

#endif

static const tlHostCbs __evio_hostcbs[] = {
    { "sleep", _io_sleep },
    { 0, 0 }
};

void evio_init() {
    ev_default_loop(0);

    tl_register_hostcbs(__evio_hostcbs);

    s_file  = tlSYM("io_file");
    s_dir   = tlSYM("io_dir");
    s_child = tlSYM("io_child");

    /*
    tl_register_const("O_RDONLY",   tlINT(O_RDONLY));
    tl_register_const("O_WRONLY",   tlINT(O_WRONLY));
    tl_register_const("O_RDWR",     tlINT(O_RDWR));
    tl_register_const("O_APPEND",   tlINT(O_APPEND));
    tl_register_const("O_TRUNC",    tlINT(O_TRUNC));
    tl_register_const("O_CREAT",    tlINT(O_CREAT));
    tl_register_const("O_CANREAD",  tlINT(EV_READ));
    tl_register_const("O_CANWRITE", tlINT(EV_WRITE));

    tl_register_pfuns(__io_pfuns);
    */

    _s_dev = tlSYM("dev");
    _s_ino = tlSYM("ino");
    _s_mode = tlSYM("mode");
    _s_nlink = tlSYM("nlink");
    _s_uid = tlSYM("uid");
    _s_gid = tlSYM("gid");
    _s_rdev = tlSYM("rdev");
    _s_size = tlSYM("size");
    _s_blksize = tlSYM("blksize");
    _s_blocks = tlSYM("blocks");
    _s_atime = tlSYM("atime");
    _s_mtime = tlSYM("mtime");
    /*
    _stat_tlmap = tlmap_new(null, 12);
    tlmap_set_(_stat_tlmap, _s_dev, null);
    tlmap_set_(_stat_tlmap, _s_ino, null);
    tlmap_set_(_stat_tlmap, _s_mode, null);
    tlmap_set_(_stat_tlmap, _s_nlink, null);
    tlmap_set_(_stat_tlmap, _s_uid, null);
    tlmap_set_(_stat_tlmap, _s_gid, null);
    tlmap_set_(_stat_tlmap, _s_rdev, null);
    tlmap_set_(_stat_tlmap, _s_size, null);
    tlmap_set_(_stat_tlmap, _s_blksize, null);
    tlmap_set_(_stat_tlmap, _s_blocks, null);
    tlmap_set_(_stat_tlmap, _s_atime, null);
    tlmap_set_(_stat_tlmap, _s_mtime, null);
    */
}

bool tlIoHasWaiting(tlVm* vm) {
    assert(vm);
    assert(vm->waiting >= 0);
    return vm->waiting > 0;
}

void tlIoWait(tlVm* vm) {
    assert(vm);
    trace("EVLOOP_ONESHOT: %d", vm->waiting);
    ev_loop(EVLOOP_ONESHOT);
}

