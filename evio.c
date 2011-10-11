// ** basic io functions in hotel **

// TODO use O_CLOEXEC if available; set FD_CLOEXEC otherwise
// TODO use getaddrinfo stuff to get ipv6 compat; take care when doing name resolutions though
// TODO thread safety when we want to run multiple workers and a ev loop thread ... how?
// TODO a loop per vm? and make this more like a "plugin"?

#define EV_STANDALONE 1
#define EV_MULTIPLICITY 0
#include "ev/ev.h"

#include "trace-on.h"

static tlMap* _statMap;
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
    return tlPauseAlloc(task, sizeof(tlPause), 0, null);
}


// ** file descriptors **

static int nonblock(int fd) {
    int flags = 0;
    if ((flags = fcntl(fd, F_GETFL, 0)) < 0) return -1;
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) return -1;
    return 0;
}
static int nosigpipe(int fd) {
    if (fcntl(fd, F_SETNOSIGPIPE, 1) < 0) return -1;
    return 0;
}

INTERNAL tlPause* _FileAct(tlTask* task, tlArgs* args);

TL_REF_TYPE(File);
TL_REF_TYPE(Socket);
TL_REF_TYPE(ServerSocket);

// TODO should be able to read and write at same time ...
struct tlFile {
    tlActor actor;
    ev_io ev;
};
// TODO add peer address ...
struct tlSocket {
    tlActor actor;
    ev_io ev;
};
struct tlServerSocket {
    tlActor actor;
    ev_io ev;
};
static tlClass _tlFileClass = {
    .name = "File",
    .send = tlActorReceive,
};
static tlClass _tlSocketClass = {
    .name = "Socket",
    .send = tlActorReceive,
};
static tlClass _tlServerSocketClass = {
    .name = "ServerSocket",
    .send = tlActorReceive,
};
tlClass* tlFileClass = &_tlFileClass;
tlClass* tlSocketClass = &_tlSocketClass;
tlClass* tlServerSocketClass = &_tlServerSocketClass;

static tlFile* tlFileOrSocketAs(tlValue v) {
    assert(tlFileIs(v)||tlSocketIs(v)||tlServerSocketIs(v)); return (tlFile*)v;
}
static tlFile* tlFileOrSocketCast(tlValue v) {
    return (tlFileIs(v)||tlSocketIs(v)||tlServerSocketIs(v))?(tlFile*)v:null;
}

static tlFile* tlFileFrom(ev_io *ev) {
    return tlFileOrSocketAs(((char*)ev) - ((unsigned long)&((tlFile*)0)->ev));
}
static tlSocket* tlSocketFrom(ev_io *ev) {
    return tlSocketAs(((char*)ev) - ((unsigned long)&((tlFile*)0)->ev));
}
static tlServerSocket* tlServerSocketFrom(ev_io *ev) {
    return tlServerSocketAs(((char*)ev) - ((unsigned long)&((tlFile*)0)->ev));
}

static tlFile* tlFileNew(tlTask* task, int fd) {
    print("fd: %d", fd);
    tlFile *file = tlAlloc(task, tlFileClass, sizeof(tlFile));
    ev_io_init(&file->ev, null, fd, 0);
    return file;
}
static tlSocket* tlSocketNew(tlTask* task, int fd) {
    tlSocket *sock = tlAlloc(task, tlSocketClass, sizeof(tlSocket));
    ev_io_init(&sock->ev, null, fd, 0);
    return sock;
}
static tlServerSocket* tlServerSocketNew(tlTask* task, int fd) {
    tlServerSocket *sock = tlAlloc(task, tlServerSocketClass, sizeof(tlServerSocket));
    ev_io_init(&sock->ev, null, fd, 0);
    return sock;
}

static tlPause* _FileClose(tlTask* task, tlArgs* args) {
    tlFile* file = tlFileOrSocketCast(tlArgsTarget(args));
    if (!file) TL_THROW("expected a File");

    ev_io *ev = &file->ev;
    trace("close: %p %d", ev, ev->fd);
    ev_io_stop(ev);

    int r = close(ev->fd);
    if (r < 0) TL_THROW("close: failed: %s", strerror(errno));
    TL_RETURN(tlNull);
}

static void read_cb(ev_io *ev, int revents) {
    trace("read_cb: %p %d", ev, ev->fd);
    tlFile* file = tlFileFrom(ev);
    tlTask* task = file->actor.owner;
    tl_buf* buf = (tl_buf*)ev->data;
    assert(task);

    int len = read(ev->fd, writebuf(buf), canwrite(buf));
    if (len < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            ev_io_start(ev); trace("read: EAGAIN || EWOULDBLOCK"); return;
        }
        TL_THROW_SET("read: failed: %s", strerror(errno));
    } else {
        didwrite(buf, len);
        TL_RETURN_SET(tlINT(len));
    }
    ev_io_stop(ev);
    if (task->state == TL_STATE_WAIT) tlTaskReady(task);
}

static tlPause* _FileRead2(tlTask* task, tlActor* actor, void* data) {
    tlFile* file = tlFileOrSocketAs(data);
    tlBuffer* buffer = tlBufferAs(actor);
    assert(file->actor.owner == task);
    assert(buffer->actor.owner == task);

    // release buffer or let Aquire do a pause with release?
    if (canwrite(buffer->buf) <= 0) TL_THROW("read: failed: buffer full");

    ev_io* ev = &file->ev;
    trace("read: %p, %d", ev, ev->fd);
    ev->cb = read_cb;
    ev->data = buffer->buf;

    int revents = ev_clear_pending(ev);
    if (revents & EV_READ) { read_cb(ev, revents); return null; }

    ev->events = EV_READ;
    ev_io_start(ev);
    tlTaskWaitSystem(task);

    trace("read: waiting");
    return tlPauseAlloc(task, sizeof(tlPause), 0, null);
}

static tlPause* _FileRead(tlTask* task, tlArgs* args) {
    tlFile* file = tlFileOrSocketCast(tlArgsTarget(args));
    if (!file) TL_THROW("expected a File");

    tlBuffer* buffer = tlBufferCast(tlArgsAt(args, 0));
    if (!buffer) TL_THROW("expected a Buffer");

    return tlActorAquire(task, tlActorAs(buffer), _FileRead2, file);
}

static void write_cb(ev_io *ev, int revents) {
    trace("read_cb: %p %d", ev, ev->fd);
    tlFile* file = tlFileFrom(ev);
    tlTask* task = file->actor.owner;
    tl_buf* buf = (tl_buf*)ev->data;
    assert(task);

    int len = write(ev->fd, readbuf(buf), canread(buf));
    if (len < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            ev_io_start(ev); trace("write: EAGAIN || EWOULDBLOCK"); return;
        }
        TL_THROW_SET("write: failed: %s", strerror(errno));
    } else {
        didread(buf, len);
        TL_RETURN_SET(tlINT(len));
    }
    ev_io_stop(ev);
    if (task->state == TL_STATE_WAIT) tlTaskReady(task);
}

static tlPause* _FileWrite2(tlTask* task, tlActor* actor, void* data) {
    tlFile* file = tlFileOrSocketAs(data);
    tlBuffer* buffer = tlBufferAs(actor);
    assert(file->actor.owner == task);
    assert(buffer->actor.owner == task);

    // TODO release
    if (canread(buffer->buf) <= 0) TL_THROW("write: failed: buffer empty");

    ev_io* ev = &file->ev;
    trace("write: %p, %d", ev, ev->fd);
    ev->cb = write_cb;
    ev->data = buffer->buf;

    int revents = ev_clear_pending(ev);
    if (revents & EV_WRITE) { read_cb(ev, revents); return null; }

    ev->events = EV_WRITE;
    ev_io_start(ev);
    tlTaskWaitSystem(task);

    trace("write: waiting");
    return tlPauseAlloc(task, sizeof(tlPause), 0, null);
}

static tlPause* _FileWrite(tlTask* task, tlArgs* args) {
    tlFile* file = tlFileOrSocketCast(tlArgsTarget(args));
    if (!file) TL_THROW("expected a File");

    tlBuffer* buffer = tlBufferCast(tlArgsAt(args, 0));
    if (!buffer) TL_THROW("expected a Buffer");

    return tlActorAquire(task, tlActorAs(buffer), _FileWrite2, file);
}

static tlPause* _File_open(tlTask* task, tlArgs* args) {
    tlText* name = tlTextCast(tlArgsAt(args, 0));
    if (!name) TL_THROW("expected a file name");
    trace("open: %s", tl_str(name));
    int flags = tl_int_or(tlArgsAt(args, 1), -1);
    if (flags < 0) TL_THROW("expected flags");
    int perms = 0666;

    int fd = open(tlTextData(name), flags|O_NONBLOCK, perms);
    if (fd < 0) TL_THROW("file_open: failed: %s file: '%s'", strerror(errno), tlTextData(name));
    TL_RETURN(tlFileNew(task, fd));
}

// ** sockets **

// TOOD this is a blocking call
static tlPause* _Socket_resolve(tlTask* task, tlArgs* args) {
    tlText* name = tlTextCast(tlArgsAt(args, 0));
    if (!name) TL_THROW("expected a Text");

    struct hostent *hp = gethostbyname(tlTextData(name));
    if (!hp) TL_RETURN(tlNull);
    if (!hp->h_addr_list[0]) TL_RETURN(tlNull);
    TL_RETURN(tlTextNewTake(task, inet_ntoa(*(struct in_addr*)(hp->h_addr_list[0]))));
}

static tlPause* _Socket_connect(tlTask* task, tlArgs* args) {
    tlText* address = tlTextCast(tlArgsAt(args, 0));
    if (!address) TL_THROW("expected a ip address");
    int port = tl_int_or(tlArgsAt(args, 1), -1);
    if (port < 0) TL_THROW("expected a port");

    trace("tcp_open: %s:%d", tl_str(address), port);

    struct in_addr ip;
    if (!inet_aton(tlTextData(address), &ip)) TL_THROW("tcp_open: invalid ip: %s", tl_str(address));

    struct sockaddr_in sockaddr;
    bzero(&sockaddr, sizeof(sockaddr));
    sockaddr.sin_family = AF_INET;
    bcopy(&ip, &sockaddr.sin_addr.s_addr, sizeof(ip));
    sockaddr.sin_port = htons(port);

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) TL_THROW("tcp_connect: failed: %s", strerror(errno));

    if (nonblock(fd) < 0) TL_THROW("tcp_connect: nonblock failed: %s", strerror(errno));

    int r = connect(fd, (struct sockaddr *)&sockaddr, sizeof(sockaddr));
    if (r < 0 && errno != EINPROGRESS) TL_THROW("tcp_connect: connect failed: %s", strerror(errno));

    if (errno == EINPROGRESS) trace("tcp_connect: EINPROGRESS");
    TL_RETURN(tlSocketNew(task, fd));
}

// TODO make backlog configurable
static tlPause* _ServerSocket_listen(tlTask* task, tlArgs* args) {
    int port = tl_int_or(tlArgsAt(args, 0), -1);
    trace("tcp_listen: 0.0.0.0:%d", port);

    struct sockaddr_in sockaddr;
    bzero(&sockaddr, sizeof(sockaddr));
    sockaddr.sin_family = AF_INET;
    sockaddr.sin_addr.s_addr = INADDR_ANY;
    sockaddr.sin_port = htons(port);

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) TL_THROW("tcp_listen: failed: %s", strerror(errno));

    if (nonblock(fd) < 0) TL_THROW("tcp_listen: nonblock failed: %s", strerror(errno));

    int flags = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (void *)&flags, sizeof(flags)) < 0) {
        TL_THROW("tcp_listen: so_reuseaddr failed: %s", strerror(errno));
    }

    int r = bind(fd, (struct sockaddr *)&sockaddr, sizeof(sockaddr));
    if (r < 0) TL_THROW("tcp_listen: bind failed: %s", strerror(errno));

    listen(fd, 128); // backlog, configurable?
    TL_RETURN(tlServerSocketNew(task, fd));
}

// TODO return multiple, socket and peer address
static void accept_cb(ev_io* ev, int revents) {
    trace("accept_cb: %p %d", ev, ev->fd);
    tlFile* file = tlFileFrom(ev);
    tlTask* task = file->actor.owner;
    assert(task);

    struct sockaddr_in sockaddr;
    bzero(&sockaddr, sizeof(sockaddr));
    socklen_t len = sizeof(sockaddr);
    int fd = accept(ev->fd, (struct sockaddr *)&sockaddr, &len);
    if (fd < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            ev_io_start(ev); trace("tcp_accept: EAGAIN || EWOULDBLOCK"); return;
        }
        TL_THROW_SET("tcp_accept: failed: %s", strerror(errno));
    } else {
        if (nonblock(fd) < 0) {
            TL_THROW_SET("tcp_accept: nonblock failed: %s", strerror(errno));
        } else {
            TL_RETURN_SET(tlSocketNew(task, fd));
        }
    }
    ev_io_stop(ev);
    if (task->state == TL_STATE_WAIT) tlTaskReady(task);
}

static tlPause* _SocketAccept(tlTask* task, tlArgs* args) {
    tlFile* file = tlFileOrSocketCast(tlArgsTarget(args));
    if (!file) TL_THROW("expected a File");

    ev_io *ev = &file->ev;
    trace("tcp_accept: %p, %d", ev, ev->fd);
    ev->cb = accept_cb;

    int revents = ev_clear_pending(ev);
    if (revents & EV_READ) { read_cb(ev, revents); return null; }

    ev->events = EV_READ;
    ev_io_start(ev);
    tlTaskWaitSystem(task);
    return tlPauseAlloc(task, sizeof(tlPause), 0, null);
}

// ** paths **

static tlPause* _Path_stat(tlTask* task, tlArgs* args) {
    tlText* name = tlTextCast(tlArgsAt(args, 0));
    if (!name) TL_THROW("expected a name");

    struct stat buf;
    int r = stat(tlTextData(name), &buf);
    if (r == -1) TL_THROW("stat failed: %s for: '%s'", strerror(errno), tlTextData(name));

    tlMap *res = tlAllocClone(task, _statMap, sizeof(tlMap), tlmap_size(_statMap));
    tlmap_set_sym_(res, _s_dev, tlINT(buf.st_dev));
    tlmap_set_sym_(res, _s_ino, tlINT(buf.st_ino));
    tlmap_set_sym_(res, _s_mode, tlINT(buf.st_mode));
    tlmap_set_sym_(res, _s_nlink, tlINT(buf.st_nlink));
    tlmap_set_sym_(res, _s_uid, tlINT(buf.st_uid));
    tlmap_set_sym_(res, _s_gid, tlINT(buf.st_gid));
    tlmap_set_sym_(res, _s_rdev, tlINT(buf.st_rdev));
    tlmap_set_sym_(res, _s_size, tlINT(buf.st_size));
    tlmap_set_sym_(res, _s_blksize, tlINT(buf.st_blksize));
    tlmap_set_sym_(res, _s_blocks, tlINT(buf.st_blocks));
    tlmap_set_sym_(res, _s_atime, tlINT(buf.st_atime));
    tlmap_set_sym_(res, _s_mtime, tlINT(buf.st_mtime));
    TL_RETURN(res);
}

TL_REF_TYPE(Dir);
struct tlDir {
    tlActor actor;
    DIR* p;
};
tlClass _tlDirClass = {
    .name = "Dir",
    .send = tlActorReceive,
};
tlClass* tlDirClass = &_tlDirClass;

static tlDir* tlDirNew(tlTask* task, DIR* p) {
    tlDir* dir = tlAlloc(task, tlDirClass, sizeof(tlDir));
    dir->p = p;
    return dir;
}

static tlPause* _Dir_open(tlTask* task, tlArgs* args) {
    tlText* name = tlTextCast(tlArgsAt(args, 0));
    trace("opendir: %s", tl_str(name));
    DIR *p = opendir(tlTextData(name));
    if (!p) TL_THROW("opendir: failed: %s for: '%s'", strerror(errno), tl_str(name));
    TL_RETURN(tlDirNew(task, p));
}

static tlPause* _DirClose(tlTask* task, tlArgs* args) {
    tlDir* dir = tlDirCast(tlArgsTarget(args));
    if (!dir) TL_THROW("expected a Dir");
    trace("closedir: %p", dir);
    if (closedir(dir->p)) TL_THROW("closedir: failed: %s", strerror(errno));
    TL_RETURN(tlNull);
}

static tlPause* _DirRead(tlTask* task, tlArgs* args) {
    tlDir* dir = tlDirCast(tlArgsTarget(args));
    if (!dir) TL_THROW("expected a Dir");

    struct dirent dp;
    struct dirent *dpp;
    if (readdir_r(dir->p, &dp, &dpp)) TL_THROW("readdir: failed: %s", strerror(errno));
    trace("readdir: %p", dpp);
    if (!dpp) TL_RETURN(tlNull);
    TL_RETURN(tlTextNewCopy(task, dp.d_name));
}


// ** child processes **

TL_REF_TYPE(Child);
struct tlChild {
    tlActor actor;
    ev_child ev;
    lqueue wait_q;
    tlValue in;
    tlValue out;
    tlValue err;
};
tlClass _tlChildClass = {
    .name = "Child",
    .send = tlActorReceive,
};
tlClass* tlChildClass = &_tlChildClass;

static tlChild* tlChildFrom(ev_io *ev) {
    return tlChildAs(((char*)ev) - ((unsigned long)&((tlChild*)0)->ev));
}

// exec ... replaces current process, stopping hotel effectively
// TODO do we want to close file descriptors?
// TODO do we want to reset signal handlers?
static tlPause* _Child_exec(tlTask* task, tlArgs* args) {
    char** argv = malloc(sizeof(char*) * (tlArgsSize(args) + 1));
    for (int i = 0; i < tlArgsSize(args); i++) {
        tlText* text = tlTextCast(tlArgsAt(args, i));
        if (!text) {
            free(argv);
            TL_THROW("expected a Text");
        }
        argv[i] = (char*)tlTextData(text);
    }
    argv[tlArgsSize(args)] = 0;

    execvp(argv[0], argv);

    // if we get here, something went wrong
    free(argv);
    TL_THROW("Process.exec: failed: %s", strerror(errno));
}

static void child_cb(ev_child *ev, int revents) {
    trace("child_cb: %p", ev);
    ev_child_stop(ev);
    tlTask* task = tltask_as(ev->data);
    if (!task) return;

    TL_RETURN_SET(tlINT(ev->rstatus));
    if (task->state == TL_STATE_WAIT) tlTaskReady(task);
}

static tlChild* tlChildNew(tlTask* task, pid_t pid, int in, int out, int err) {
    tlChild* child = tlAlloc(task, tlChildClass, sizeof(tlChild));
    ev_child_init(&child->ev, child_cb, pid, 0);
    ev_child_start(&child->ev);
    child->in = tlINT(in);
    child->out = tlINT(out);
    child->err = tlINT(err);
    return child;
}

static tlPause* _ChildWait(tlTask* task, tlArgs* args) {
    tlChild* child = tlChildCast(tlArgsTarget(args));
    if (!child) TL_THROW("expected a Child");
    trace("child_wait: %d", child->ev.pid);

    if (!ev_is_active(child)) { // already exited
        TL_RETURN(tlINT(child->ev.rstatus));
    }

    // TODO "park" the task; by disowning the current actor ... how?
    tlTaskWaitSystem(task);
    lqueue_put(&child->wait_q, &task->entry);
    return tlPauseAlloc(task, sizeof(tlPause), 0, null);
}

static tlPause* _ChildIn(tlTask* task, tlArgs* args) {
    tlChild* child = tlChildCast(tlArgsTarget(args));
    if (!child) TL_THROW("expected a Child");
    if (tlIntIs(child->in)) child->in = tlFileNew(task, tl_int(child->in));
    TL_RETURN(child->in);
}
static tlPause* _ChildOut(tlTask* task, tlArgs* args) {
    tlChild* child = tlChildCast(tlArgsTarget(args));
    if (!child) TL_THROW("expected a Child");
    if (tlIntIs(child->out)) child->out = tlFileNew(task, tl_int(child->out));
    TL_RETURN(child->out);
}
static tlPause* _ChildErr(tlTask* task, tlArgs* args) {
    tlChild* child = tlChildCast(tlArgsTarget(args));
    if (!child) TL_THROW("expected a Child");
    if (tlIntIs(child->err)) child->err = tlFileNew(task, tl_int(child->err));
    TL_RETURN(child->err);
}

static tlPause* _ChildStatus(tlTask* task, tlArgs* args) {
    tlChild* child = tlChildCast(tlArgsTarget(args));
    if (!child) TL_THROW("expected a Child");
    trace("child_status: %d, status: %d", child->ev.pid, child->ev.rstatus);
    TL_RETURN(tlINT(child->ev.rstatus));
}

// TODO do we want to reset signal handlers in child too?
// TODO we should also set nonblocking to pipes right?
static tlPause* _Child_run(tlTask* task, tlArgs* args) {
    char** argv = malloc(sizeof(char*) * (tlArgsSize(args) + 1));
    for (int i = 0; i < tlArgsSize(args); i++) {
        tlText* text = tlTextCast(tlArgsAt(args, i));
        if (!text) {
            free(argv);
            TL_THROW("expected a Text");
        }
        argv[i] = (char*)tlTextData(text);
    }
    argv[tlArgsSize(args)] = 0;

    int _in[2];
    int _out[2];
    int _err[2];
    if (pipe(_in) || pipe(_out) || pipe(_err)) {
        TL_THROW_SET("child exec: failed: %s", strerror(errno));
        close(_in[0]); close(_in[1]); close(_out[0]); close(_out[1]); close(_err[0]); close(_err[1]);
        free(argv);
        return null;
    }

    int pid = fork();
    if (pid) {
        // parent
        if (close(_in[0]) || close(_out[1]) || close(_err[1])) {
            TL_THROW_SET("child exec: failed: %s", strerror(errno));
            close(_in[0]); close(_in[1]); close(_out[0]); close(_out[1]); close(_err[0]); close(_err[1]);
            free(argv);
            return null;
        }
        tlChild* child = tlChildNew(task, pid, _in[1], _out[0], _err[0]);
        TL_RETURN(child);
    }

    // child
    dup2(_in[0], 0);
    dup2(_out[1], 1);
    dup2(_err[1], 2);
    int max = getdtablesize();
    if (max < 0) max = 50000;
    for (int i = 3; i < max; i++) close(i); // by lack of anything better ...
    execvp(argv[0], argv);
    warning("tl run failed: %s", strerror(errno));
    _exit(255);
}

static const tlHostCbs __evio_hostcbs[] = {
    { "sleep", _io_sleep },
    { "_File_open", _File_open },
    { "_Socket_connect", _Socket_connect },
    { "_Socket_resolve", _Socket_resolve },
    { "_ServerSocket_listen", _ServerSocket_listen },
    { "_Path_stat", _Path_stat },
    { "_Dir_open", _Dir_open },
    { "_Child_exec", _Child_exec },
    { "_Child_run", _Child_run },
    { 0, 0 }
};

void evio_init() {
    _tlFileClass.map = tlClassMapFrom(
        "close", _FileClose,
        "read", _FileRead,
        "write", _FileWrite,
        null
    );
    _tlSocketClass.map = tlClassMapFrom(
        "close", _FileClose,
        "read", _FileRead,
        "write", _FileWrite,
        null
    );
    _tlServerSocketClass.map = tlClassMapFrom(
        "close", _FileClose,
        "accept", _SocketAccept,
        null
    );
    _tlDirClass.map = tlClassMapFrom(
        "read", _DirRead,
        null
    );
    _tlChildClass.map = tlClassMapFrom(
        "wait", _ChildWait,
        "in", _ChildIn,
        "out", _ChildOut,
        "err", _ChildErr,
        null
    );

    tl_register_hostcbs(__evio_hostcbs);
    tl_register_global("_File_RDONLY",   tlINT(O_RDONLY));
    tl_register_global("_File_WRONLY",   tlINT(O_WRONLY));
    tl_register_global("_File_RDWR",     tlINT(O_RDWR));
    tl_register_global("_File_APPEND",   tlINT(O_APPEND));
    tl_register_global("_File_TRUNC",    tlINT(O_TRUNC));
    tl_register_global("_File_CREAT",    tlINT(O_CREAT));

    tlSet* keys = tlset_new(null, 12);
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
    tlset_add_(keys, _s_dev);
    tlset_add_(keys, _s_ino);
    tlset_add_(keys, _s_mode);
    tlset_add_(keys, _s_nlink);
    tlset_add_(keys, _s_uid);
    tlset_add_(keys, _s_gid);
    tlset_add_(keys, _s_rdev);
    tlset_add_(keys, _s_size);
    tlset_add_(keys, _s_blksize);
    tlset_add_(keys, _s_blocks);
    tlset_add_(keys, _s_atime);
    tlset_add_(keys, _s_mtime);

    _statMap = tlmap_new(null, keys);
    // TODO make this api somewhere?
    _statMap->head.klass = tlValueObjectClass;

    ev_default_loop(0);
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

