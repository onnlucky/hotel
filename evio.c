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

static void io_cb(ev_io *ev, int revents);

static int nonblock(int fd) {
    int flags = 0;
    if ((flags = fcntl(fd, F_GETFL, 0)) < 0) return -1;
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) return -1;
    return 0;
}

/*
static int nosigpipe(int fd) {
    if (fcntl(fd, F_SETNOSIGPIPE, 1) < 0) return -1;
    return 0;
}
*/

static tlValue _io_chdir(tlTask* task, tlArgs* args) {
    tlText* text = tlTextCast(tlArgsGet(args, 0));
    if (!text) TL_THROW("expected a Text");
    if (chdir(tlTextData(text))) {
        TL_THROW("%s", strerror(errno));
    }
    return tlNull;
}

TL_REF_TYPE(tlReader);
TL_REF_TYPE(tlWriter);

struct tlReader {
    tlLock lock;
    ev_io* ev;
    tlMessage* msg; // TODO remove, lock->owner == sender
};
struct tlWriter {
    tlLock lock;
    ev_io* ev;
    tlMessage* msg; // TODO remove ...
};
static tlClass _tlReaderClass = {
    .name = "Reader",
    .locked = true,
};
static tlClass _tlWriterClass = {
    .name = "Writer",
    .locked = true,
};
tlClass* tlReaderClass = &_tlReaderClass;
tlClass* tlWriterClass = &_tlWriterClass;

TL_REF_TYPE(tlFile);
TL_REF_TYPE(tlSocket);
TL_REF_TYPE(tlServerSocket);

struct tlFile {
    tlHead head;
    ev_io ev;
    tlReader* reader;
    tlWriter* writer;
};
struct tlSocket {
    tlHead head;
    ev_io ev;
    tlReader* reader;
    tlWriter* writer;
};
struct tlServerSocket {
    tlLock lock;
    ev_io ev;
};
static tlClass _tlFileClass = {
    .name = "File",
};
static tlClass _tlSocketClass = {
    .name = "Socket",
};
static tlClass _tlServerSocketClass = {
    .name = "ServerSocket",
    .locked = true,
};
tlClass* tlFileClass = &_tlFileClass;
tlClass* tlSocketClass = &_tlSocketClass;
tlClass* tlServerSocketClass = &_tlServerSocketClass;

static tlFile* tlFileOrSocketAs(tlValue v) {
    assert(tlFileIs(v)||tlSocketIs(v)); return (tlFile*)v;
}
static tlFile* tlFileOrSocketCast(tlValue v) {
    return (tlFileIs(v)||tlSocketIs(v))?(tlFile*)v:null;
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

static tlReader* tlReaderNew(tlTask* task, ev_io* ev) {
    tlReader* reader = tlAlloc(task, tlReaderClass, sizeof(tlReader));
    reader->ev = ev;
    return reader;
}

static tlWriter* tlWriterNew(tlTask* task, ev_io* ev) {
    tlWriter* writer = tlAlloc(task, tlWriterClass, sizeof(tlWriter));
    writer->ev = ev;
    return writer;
}

void close_ev_io(void* _file, void* data) {
    tlFile* file = tlFileOrSocketAs(_file);
    ev_io_stop(&file->ev);
    close(file->ev.fd);
    //print(">>>> CLOSED: %d <<<<", file->ev.fd);
}

static tlFile* tlFileNew(tlTask* task, int fd) {
    tlFile *file = tlAlloc(task, tlFileClass, sizeof(tlFile));
    ev_io_init(&file->ev, io_cb, fd, 0);
    // file->reader points back to file ... a harmless cycle, but we need the no_order
    GC_REGISTER_FINALIZER_NO_ORDER(file, close_ev_io, null, null, null);
    trace("open: %p %d", file, fd);
    return file;
}
static tlSocket* tlSocketNew(tlTask* task, int fd) {
    tlSocket *sock = tlAlloc(task, tlSocketClass, sizeof(tlSocket));
    ev_io_init(&sock->ev, io_cb, fd, 0);
    GC_REGISTER_FINALIZER_NO_ORDER(sock, close_ev_io, null, null, null);
    return sock;
}
static tlServerSocket* tlServerSocketNew(tlTask* task, int fd) {
    tlServerSocket *sock = tlAlloc(task, tlServerSocketClass, sizeof(tlServerSocket));
    ev_io_init(&sock->ev, io_cb, fd, 0);
    return sock;
}

static tlValue _file_close(tlTask* task, tlArgs* args) {
    tlFile* file = tlFileOrSocketCast(tlArgsTarget(args));
    if (!file) TL_THROW("expected a File");

    ev_io *ev = &file->ev;
    trace("close: %p %d", file, ev->fd);
    ev_io_stop(ev);

    int r = close(ev->fd);
    if (r < 0) TL_THROW("close: failed: %s", strerror(errno));
    return tlNull;
}

static tlValue _file_reader(tlTask* task, tlArgs* args) {
    tlFile* file = tlFileOrSocketCast(tlArgsTarget(args));
    if (!file) TL_THROW("expected a File");

    if (file->reader) return file->reader;
    return file->reader = tlReaderNew(task, &file->ev);
}

static tlValue _file_writer(tlTask* task, tlArgs* args) {
    tlFile* file = tlFileOrSocketCast(tlArgsTarget(args));
    if (!file) TL_THROW("expected a File");

    if (file->writer) return file->writer;
    return file->writer = tlWriterNew(task, &file->ev);
}

static tlValue _reader_read(tlTask* task, tlArgs* args) {
    trace("");
    tlReader* reader = tlReaderCast(tlArgsTarget(args));
    tlBuffer* buffer = tlBufferCast(tlArgsGet(args, 0));
    if (!reader || !tlLockIsOwner(tlLockAs(reader), task)) TL_THROW("expected a locked Reader");
    if (!buffer || !tlLockIsOwner(tlLockAs(buffer), task)) TL_THROW("expected a locked Buffer");

    tl_buf* buf = buffer->buf;
    tlbuf_autogrow(buf);
    if (canwrite(buf) <= 0) TL_THROW("read: failed: buffer full");

    int len = read(reader->ev->fd, writebuf(buf), canwrite(buf));
    if (len < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) return tlNull;
        TL_THROW("%d: read: failed: %s", reader->ev->fd, strerror(errno));
    }
    didwrite(buf, len);
    return tlINT(len);
}

static tlValue _writer_write(tlTask* task, tlArgs* args) {
    trace("");
    tlWriter* writer = tlWriterCast(tlArgsTarget(args));
    tlBuffer* buffer = tlBufferCast(tlArgsGet(args, 0));
    if (!writer || !tlLockIsOwner(tlLockAs(writer), task)) TL_THROW("expected a locked Writer");
    if (!buffer || !tlLockIsOwner(tlLockAs(buffer), task)) TL_THROW("expected a locked Buffer");

    tl_buf* buf = buffer->buf;
    if (canread(buf) <= 0) TL_THROW("write: failed: buffer empty");

    int len = write(writer->ev->fd, readbuf(buf), canread(buf));
    if (len < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) return tlNull;
        // TODO for EINPROGRESS on connect(); but only if not UDP?
        if (errno == ENOTCONN) return tlNull;
        TL_THROW("%d: write: failed: %s", writer->ev->fd, strerror(errno));
    }
    didread(buf, len);
    return tlINT(len);
}

static tlValue _File_open(tlTask* task, tlArgs* args) {
    tlText* name = tlTextCast(tlArgsGet(args, 0));
    if (!name) TL_THROW("expected a file name");
    trace("open: %s", tl_str(name));
    int flags = tl_int_or(tlArgsGet(args, 1), -1);
    if (flags < 0) TL_THROW("expected flags");
    int perms = 0666;

    int fd = open(tlTextData(name), flags|O_NONBLOCK, perms);
    if (fd < 0) TL_THROW("file_open: failed: %s file: '%s'", strerror(errno), tlTextData(name));
    return tlFileNew(task, fd);
}

static tlValue _File_from(tlTask* task, tlArgs* args) {
    tlInt fd = tlIntCast(tlArgsGet(args, 0));
    if (!fd) TL_THROW("espected a file descriptor");
    return tlFileNew(task, tl_int(fd));
}


// ** sockets **

// TODO this is a blocking call
static tlValue _Socket_resolve(tlTask* task, tlArgs* args) {
    tlText* name = tlTextCast(tlArgsGet(args, 0));
    if (!name) TL_THROW("expected a Text");

    struct hostent *hp = gethostbyname(tlTextData(name));
    if (!hp) return tlNull;
    if (!hp->h_addr_list[0]) return tlNull;
    return tlTextFromTake(task, inet_ntoa(*(struct in_addr*)(hp->h_addr_list[0])), 0);
}

static tlValue _Socket_connect(tlTask* task, tlArgs* args) {
    tlText* address = tlTextCast(tlArgsGet(args, 0));
    if (!address) TL_THROW("expected a ip address");
    int port = tl_int_or(tlArgsGet(args, 1), -1);
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
    return tlSocketNew(task, fd);
}

// TODO make backlog configurable
static tlValue _ServerSocket_listen(tlTask* task, tlArgs* args) {
    int port = tl_int_or(tlArgsGet(args, 0), -1);
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
    return tlServerSocketNew(task, fd);
}

// TODO return multiple, socket and peer address
static void accept_cb(ev_io* ev, int revents) {
    /*
    tlFile* file = tlFileFrom(ev);
    tlTask* task = file->lock.owner;
    assert(task);
    trace("accept_cb: %p %d", file, ev->fd);

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
            task->value = tlSocketNew(task, fd);
        }
    }
    ev_io_stop(ev);
    //if (task->state == TL_STATE_IOWAIT) tlTaskReady(task);
    */
}

static tlValue _SocketAccept(tlTask* task, tlArgs* args) {
    tlFile* file = tlFileOrSocketCast(tlArgsTarget(args));
    if (!file) TL_THROW("expected a File");

    /*
    ev_io *ev = &file->ev;
    trace("tcp_accept: %p %d", file, ev->fd);
    ev->cb = accept_cb;

    int revents = ev_clear_pending(ev);
    if (revents & EV_READ) { read_cb(ev, revents); return task->value; }
    // TODO return null on throw ...

    ev->events = EV_READ;
    ev_io_start(ev);
    tlTaskWaitIo(task);
    return tlTaskPause(task, tlFrameAlloc(task, null, sizeof(tlFrame)));
    */
    fatal("not implemented");
    return tlNull;
}

// ** paths **

static tlValue _Path_stat(tlTask* task, tlArgs* args) {
    tlText* name = tlTextCast(tlArgsGet(args, 0));
    if (!name) TL_THROW("expected a name");

    struct stat buf;
    int r = stat(tlTextData(name), &buf);
    if (r == -1) TL_THROW("stat failed: %s for: '%s'", strerror(errno), tlTextData(name));

    tlMap *res = tlAllocClone(task, _statMap, sizeof(tlMap));
    tlMapSetSym_(res, _s_dev, tlINT(buf.st_dev));
    tlMapSetSym_(res, _s_ino, tlINT(buf.st_ino));
    tlMapSetSym_(res, _s_mode, tlINT(buf.st_mode));
    tlMapSetSym_(res, _s_nlink, tlINT(buf.st_nlink));
    tlMapSetSym_(res, _s_uid, tlINT(buf.st_uid));
    tlMapSetSym_(res, _s_gid, tlINT(buf.st_gid));
    tlMapSetSym_(res, _s_rdev, tlINT(buf.st_rdev));
    tlMapSetSym_(res, _s_size, tlINT(buf.st_size));
    tlMapSetSym_(res, _s_blksize, tlINT(buf.st_blksize));
    tlMapSetSym_(res, _s_blocks, tlINT(buf.st_blocks));
    tlMapSetSym_(res, _s_atime, tlINT(buf.st_atime));
    tlMapSetSym_(res, _s_mtime, tlINT(buf.st_mtime));
    return res;
}

TL_REF_TYPE(tlDir);
struct tlDir {
    tlLock lock;
    DIR* p;
};
tlClass _tlDirClass = {
    .name = "Dir",
    .locked = true,
};
tlClass* tlDirClass = &_tlDirClass;

static tlDir* tlDirNew(tlTask* task, DIR* p) {
    tlDir* dir = tlAlloc(task, tlDirClass, sizeof(tlDir));
    dir->p = p;
    return dir;
}

static tlValue _Dir_open(tlTask* task, tlArgs* args) {
    tlText* name = tlTextCast(tlArgsGet(args, 0));
    trace("opendir: %s", tl_str(name));
    DIR *p = opendir(tlTextData(name));
    if (!p) TL_THROW("opendir: failed: %s for: '%s'", strerror(errno), tl_str(name));
    return tlDirNew(task, p);
}

static tlValue _DirClose(tlTask* task, tlArgs* args) {
    tlDir* dir = tlDirCast(tlArgsTarget(args));
    if (!dir) TL_THROW("expected a Dir");
    trace("closedir: %p", dir);
    if (closedir(dir->p)) TL_THROW("closedir: failed: %s", strerror(errno));
    return tlNull;
}

static tlValue _DirRead(tlTask* task, tlArgs* args) {
    tlDir* dir = tlDirCast(tlArgsTarget(args));
    if (!dir) TL_THROW("expected a Dir");

    struct dirent dp;
    struct dirent *dpp;
    if (readdir_r(dir->p, &dp, &dpp)) TL_THROW("readdir: failed: %s", strerror(errno));
    trace("readdir: %p", dpp);
    if (!dpp) return tlNull;
    return tlTextFromCopy(task, dp.d_name, 0);
}

typedef struct tlDirEachFrame {
    tlFrame frame;
    tlDir* dir;
    tlValue* block;
} tlDirEachFrame;

static tlValue resumeDirEach(tlTask* task, tlFrame* _frame, tlValue res, tlError* err) {
    if (err) {
        if (tlErrorValue(err) == s_break) return tlNull;
        if (tlErrorValue(err) != s_continue) return null;
    }
    tlDirEachFrame* frame = (tlDirEachFrame*)_frame;
again:;
    struct dirent dp;
    struct dirent *dpp;
    if (readdir_r(frame->dir->p, &dp, &dpp)) TL_THROW("readdir: failed: %s", strerror(errno));
    trace("readdir: %p", dpp);
    if (!dpp) return tlNull;
    res = tlEval(task, tlCallFrom(task, frame->block, tlTextFromCopy(task, dp.d_name, 0), null));
    if (!res) return tlTaskPauseAttach(task, frame);
    goto again;
    return tlNull;
}

static tlValue _DirEach(tlTask* task, tlArgs* args) {
    tlDir* dir = tlDirCast(tlArgsTarget(args));
    if (!dir) TL_THROW("expected a Dir");
    tlValue* block = tlArgsMapGet(args, tlSYM("block"));
    if (!block) return tlNull;

    tlDirEachFrame* frame = tlFrameAlloc(task, resumeDirEach, sizeof(tlDirEachFrame));
    frame->dir = dir;
    frame->block = block;
    return resumeDirEach(task, (tlValue)frame, tlNull, null);
}


// ** child processes **

TL_REF_TYPE(tlChild);
struct tlChild {
    tlLock lock;
    ev_child ev;
    lqueue wait_q; // notice we don't need need the concurrent list here, but tasks can enter these
    tlValue in;
    tlValue out;
    tlValue err;
};
tlClass _tlChildClass = {
    .name = "Child",
    .locked = true,
};
tlClass* tlChildClass = &_tlChildClass;

static tlChild* tlChildFrom(ev_child *ev) {
    return tlChildAs(((char*)ev) - ((unsigned long)&((tlChild*)0)->ev));
}

// exec ... replaces current process, stopping hotel effectively
// TODO do we want to close file descriptors?
// TODO do we want to reset signal handlers?
static tlValue _Child_exec(tlTask* task, tlArgs* args) {
    char** argv = malloc(sizeof(char*) * (tlArgsSize(args) + 1));
    for (int i = 0; i < tlArgsSize(args); i++) {
        tlText* text = tlTextCast(tlArgsGet(args, i));
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
    tlChild* child = tlChildFrom(ev);
    trace("!! !! !!child_cb: %p", ev);
    ev_child_stop(ev);

    while (true) {
        tlTask* task = tlTaskFromEntry(lqueue_get(&child->wait_q));
        if (!task) return;
        task->value = tlINT(WEXITSTATUS(child->ev.rstatus));
        tlTaskReady(task);
    }
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

static tlValue resumeChildWait(tlTask* task, tlFrame* frame, tlValue res, tlError* err) {
    tlChild* child = tlChildAs(res);
    frame->resumecb = null;
    fatal("not implemented");
    //tlTaskWaitIo(task);
    lqueue_put(&child->wait_q, &task->entry);
    return tlTaskNotRunning;
}
static tlValue _child_running(tlTask* task, tlArgs* args) {
    tlChild* child = tlChildCast(tlArgsTarget(args));
    if (!child) TL_THROW("expected a Child");
    return tlBOOL(ev_is_active(&child->ev));
}
static tlValue _ChildWait(tlTask* task, tlArgs* args) {
    tlChild* child = tlChildCast(tlArgsTarget(args));
    if (!child) TL_THROW("expected a Child");
    trace("child_wait: %d", child->ev.pid);

    if (!ev_is_active(&child->ev)) { // already exited
        assert(kill(child->ev.pid, 0) == -1);
        return tlINT(WEXITSTATUS(child->ev.rstatus));
    }
    return tlTaskPauseResuming(task, resumeChildWait, child);
}

static tlValue _ChildIn(tlTask* task, tlArgs* args) {
    tlChild* child = tlChildCast(tlArgsTarget(args));
    if (!child) TL_THROW("expected a Child");
    if (tlIntIs(child->in)) child->in = tlFileNew(task, tl_int(child->in));
    return child->in;
}
static tlValue _ChildOut(tlTask* task, tlArgs* args) {
    tlChild* child = tlChildCast(tlArgsTarget(args));
    if (!child) TL_THROW("expected a Child");
    if (tlIntIs(child->out)) child->out = tlFileNew(task, tl_int(child->out));
    return child->out;
}
static tlValue _ChildErr(tlTask* task, tlArgs* args) {
    tlChild* child = tlChildCast(tlArgsTarget(args));
    if (!child) TL_THROW("expected a Child");
    if (tlIntIs(child->err)) child->err = tlFileNew(task, tl_int(child->err));
    return child->err;
}

static tlValue _ChildStatus(tlTask* task, tlArgs* args) {
    tlChild* child = tlChildCast(tlArgsTarget(args));
    if (!child) TL_THROW("expected a Child");
    trace("child_status: %d, status: %d", child->ev.pid, child->ev.rstatus);
    return tlINT(WEXITSTATUS(child->ev.rstatus));
}

// TODO do we want to reset signal handlers in child too?
// TODO we should also set nonblocking to pipes right?
static tlValue _Child_run(tlTask* task, tlArgs* args) {
    char** argv = malloc(sizeof(char*) * (tlArgsSize(args) + 1));
    for (int i = 0; i < tlArgsSize(args); i++) {
        tlText* text = tlTextCast(tlArgsGet(args, i));
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
        return child;
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

INTERNAL const char* fileToText(tlValue v, char* buf, int size) {
    tlFile* file = tlFileOrSocketAs(v);
    tlClass* klass = tl_class(file);
    snprintf(buf, size, "<%s@%d>", klass->name, file->ev.fd);
    return buf;
}


// ** newstyle io, where languages controls all */

static void io_cb(ev_io *ev, int revents) {
    trace("io_cb: %p", ev);
    tlFile* file = tlFileFrom(ev);
    if (revents & EV_READ) {
        trace("CANREAD");
        assert(file->reader);
        assert(file->reader->msg);
        tlMessage* msg = tlMessageAs(file->reader->msg);
        tlVm* vm = tlTaskGetVm(tlMessageGetSender(msg));
        vm->iowaiting -= 1;
        file->reader->msg = null;
        ev->events &= ~EV_READ;
        tlMessageReply(msg, null);
    }
    if (revents & EV_WRITE) {
        trace("CANWRITE");
        assert(file->writer);
        assert(file->writer->msg);
        tlMessage* msg = tlMessageAs(file->writer->msg);
        tlVm* vm = tlTaskGetVm(tlMessageGetSender(msg));
        vm->iowaiting -= 1;
        file->writer->msg = null;
        ev->events &= ~EV_WRITE;
        tlMessageReply(msg, null);
    }
    if (!ev->events) ev_io_stop(ev);
}

static tlValue _io_waitread(tlTask* task, tlArgs* args) {
    tlVm* vm = tlTaskGetVm(task);

    tlReader* reader = tlReaderCast(tlArgsGet(args, 0));
    if (!reader) TL_THROW("expected a Reader");
    tlMessage* msg = tlMessageCast(tlArgsGet(args, 1));
    if (!msg) TL_THROW("expect a Msg");
    tlTask* sender = tlMessageGetSender(msg);
    if (!tlLockIsOwner(tlLockAs(reader), sender)) TL_THROW("expected a locked Reader");

    // TODO make use of this ...
    assert(reader->lock.owner == msg->sender);
    assert(msg->sender->value == msg);
    reader->msg = msg;

    reader->ev->events |= EV_READ;
    ev_io_start(reader->ev);

    vm->iowaiting += 1;
    return tlNull;
}

static tlValue _io_waitwrite(tlTask* task, tlArgs* args) {
    tlVm* vm = tlTaskGetVm(task);

    tlWriter* writer = tlWriterCast(tlArgsGet(args, 0));
    if (!writer) TL_THROW("expected a Writer");
    tlMessage* msg = tlMessageCast(tlArgsGet(args, 1));
    if (!msg) TL_THROW("expect a Msg");
    tlTask* sender = tlMessageGetSender(msg);
    if (!tlLockIsOwner(tlLockAs(writer), sender)) TL_THROW("expected a locked Writer");

    writer->msg = msg;

    writer->ev->events |= EV_WRITE;
    ev_io_start(writer->ev);

    vm->iowaiting += 1;
    return tlNull;
}

static void timer_cb(ev_timer* timer, int revents) {
    trace("timer_cb: %p", timer);
    tlVm* vm = tlTaskGetVm(tlMessageGetSender(tlMessageAs(timer->data)));
    vm->iowaiting -= 1;
    tlMessageReply(tlMessageAs(timer->data), null);
    free(timer);
}
static tlValue _io_wait(tlTask* task, tlArgs* args) {
    tlVm* vm = tlTaskGetVm(task);
    tlMessage* msg = tlMessageCast(tlArgsGet(args, 1));
    if (!msg) TL_THROW("expect a Msg");

    int millis = tl_int_or(tlArgsGet(args, 0), 1000);
    trace("sleep: %d", millis);
    float ms = millis / 1000.0;
    ms = ms;

    ev_timer *timer = malloc(sizeof(ev_timer));
    timer->data = msg;
    ev_timer_init(timer, timer_cb, ms, 0);
    ev_timer_start(timer);
    vm->iowaiting += 1;
    return tlNull;
}


static ev_async loop_interrupt;
static void async_cb(ev_async* async, int revents) { }

static void iointerrupt() { ev_async_send(&loop_interrupt); }

static tlValue _io_init(tlTask* task, tlArgs* args) {
    tlQueue* queue = tlQueueNew(task);
    queue->signalcb = iointerrupt;
    return queue;
}

static tlValue _io_haswaiting(tlTask* task, tlArgs* args) {
    tlVm* vm = tlTaskGetVm(task);
    assert(vm->iowaiting >= 0);
    trace("IO HAS WAITING: %zd <<<<<", vm->iowaiting);
    if (vm->iowaiting > 0) return tlTrue;
    if (a_get(&vm->tasks) > 1) return tlTrue;
    return tlFalse;
}

static tlValue _io_run(tlTask* task, tlArgs* args) {
    tlVm* vm = tlTaskGetVm(task);
    if (vm->lock) {
        trace("blocking for events");
        ev_run(EVRUN_ONCE);
    } else {
        if (vm->tasks - vm->iowaiting > 1) {
            trace("checking for events");
            ev_run(EVRUN_NOWAIT);
        } else {
            trace("blocking for events");
            ev_run(EVRUN_ONCE);
        }
    }
    trace("done");
    return tlNull;
}

static const tlNativeCbs __evio_natives[] = {
    { "_io_chdir", _io_chdir },
    { "_File_open", _File_open },
    { "_File_from", _File_from },
    { "_Socket_connect", _Socket_connect },
    { "_Socket_resolve", _Socket_resolve },
    { "_ServerSocket_listen", _ServerSocket_listen },
    { "_Path_stat", _Path_stat },
    { "_Dir_open", _Dir_open },
    { "_Child_exec", _Child_exec },
    { "_Child_run", _Child_run },

    { "_io_wait", _io_wait },
    { "_io_waitread", _io_waitread },
    { "_io_waitwrite", _io_waitwrite },

    { "_io_haswaiting", _io_haswaiting },
    { "_io_run", _io_run },
    { "_io_init", _io_init },
    { 0, 0 }
};

void evio_init() {
    _tlReaderClass.map = tlClassMapFrom(
        "read", _reader_read,
        null
    );
    _tlWriterClass.map = tlClassMapFrom(
        "write", _writer_write,
        null
    );
    _tlFileClass.toText = fileToText;
    _tlFileClass.map = tlClassMapFrom(
        "close", _file_close,
        "reader", _file_reader,
        "writer", _file_writer,
        null
    );
    _tlSocketClass.toText = fileToText;
    _tlSocketClass.map = tlClassMapFrom(
        "close", _file_close,
        "reader", _file_reader,
        "writer", _file_writer,
        null
    );
    _tlServerSocketClass.toText = fileToText;
    _tlServerSocketClass.map = tlClassMapFrom(
        "close", _file_close,
        "accept", _SocketAccept,
        null
    );
    _tlDirClass.map = tlClassMapFrom(
        "read", _DirRead,
        "each", _DirEach,
        null
    );
    _tlChildClass.map = tlClassMapFrom(
        "running", _child_running,
        "wait", _ChildWait,
        "status", _ChildStatus,
        "in", _ChildIn,
        "out", _ChildOut,
        "err", _ChildErr,
        null
    );

    tl_register_natives(__evio_natives);
    tl_register_global("_File_RDONLY",   tlINT(O_RDONLY));
    tl_register_global("_File_WRONLY",   tlINT(O_WRONLY));
    tl_register_global("_File_RDWR",     tlINT(O_RDWR));

    tl_register_global("_File_APPEND",   tlINT(O_APPEND));
    tl_register_global("_File_CREAT",    tlINT(O_CREAT));
    tl_register_global("_File_TRUNC",    tlINT(O_TRUNC));
    tl_register_global("_File_EXCL",     tlINT(O_EXCL));

    tlSet* keys = tlSetNew(null, 12);
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
    tlSetAdd_(keys, _s_dev);
    tlSetAdd_(keys, _s_ino);
    tlSetAdd_(keys, _s_mode);
    tlSetAdd_(keys, _s_nlink);
    tlSetAdd_(keys, _s_uid);
    tlSetAdd_(keys, _s_gid);
    tlSetAdd_(keys, _s_rdev);
    tlSetAdd_(keys, _s_size);
    tlSetAdd_(keys, _s_blksize);
    tlSetAdd_(keys, _s_blocks);
    tlSetAdd_(keys, _s_atime);
    tlSetAdd_(keys, _s_mtime);

    _statMap = tlMapNew(null, keys);
    tlMapToObject_(_statMap);

    signal(SIGPIPE, SIG_IGN);

    ev_default_loop(0);
    ev_async_init(&loop_interrupt, async_cb);
    ev_async_start(&loop_interrupt);

    // TODO this is here as a "test"
    ev_async_send(&loop_interrupt);
    ev_run(EVRUN_NOWAIT);
}

