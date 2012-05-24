// ** basic io functions in hotel **

// TODO use runloop mutex to protect open() like syscalls in order to do CLOEXEC if available
// TODO use getaddrinfo stuff to get ipv6 compat; take care when doing name resolutions though

#define EV_STANDALONE 1
#define EV_MULTIPLICITY 0
#include "ev/ev.h"

#include "trace-on.h"

static tlSym _s_cwd;

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

static tlMap* _usageMap;
static tlSym _s_cpu;
static tlSym _s_mem;
static tlSym _s_pid;
static tlSym _s_gcs;

tlText* tl_cwd;

static void io_cb(ev_io *ev, int revents);

static int nonblock(int fd) {
    int flags = 0;
    if ((flags = fcntl(fd, F_GETFL, 0)) < 0) return -1;
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) return -1;
    return 0;
}
static int setblock(int fd) {
    int flags = 0;
    if ((flags = fcntl(fd, F_GETFL, 0)) < 0) return -1;
    if (fcntl(fd, F_SETFL, flags & ~O_NONBLOCK) < 0) return -1;
    return 0;
}

static tlValue _io_getrusage(tlArgs* args) {
    tlMap *res = tlClone(_usageMap);
    struct rusage use;
    getrusage(RUSAGE_SELF, &use);
    tlMapSetSym_(res, _s_cpu, tlINT(use.ru_utime.tv_sec + use.ru_stime.tv_sec));
    tlMapSetSym_(res, _s_mem, tlINT(use.ru_maxrss));
    tlMapSetSym_(res, _s_pid, tlINT(getpid()));
#ifdef HAVE_BOEHMGC
    tlMapSetSym_(res, _s_gcs, tlINT(GC_gc_no));
#endif
    return res;
}
static tlValue _io_getenv(tlArgs* args) {
    tlText* text = tlTextCast(tlArgsGet(args, 0));
    if (!text) TL_THROW("expected a Text");
    const char* c = getenv(tlTextData(text));
    if (!c) return tlNull;
    return tlTextFromCopy(c, 0);
}

// TODO normalize /../ and /./
static tlText* path_join(tlText* lhs, tlText* rhs) {
    if (!lhs) return rhs;
    if (!rhs) return lhs;
    const char* l = tlTextData(lhs);
    const char* r = tlTextData(rhs);
    if (r[0] == '/') return rhs;

    int llen = strlen(l);
    int rlen = strlen(r);
    if (llen == 0) return rhs;
    if (rlen == 0) return lhs;

    char* buf = malloc(llen + 1 + rlen + 1);
    strcat(buf, l);
    strcat(buf + llen, "/");
    strcat(buf + llen + 1, r);

    return tlTextFromTake(buf, llen + 1 + rlen);
}
static tlText* cwd_join(tlText* path) {
    tlTask* task = tlTaskCurrent();
    tlText* cwd = null;
    if (task->locals) cwd = tlTextCast(tlMapGetSym(task->locals, _s_cwd));
    tlText* res = path_join(cwd, path);
    return res;
}

static tlValue _io_chdir(tlArgs* args) {
    tlText* text = tlTextCast(tlArgsGet(args, 0));
    if (!text) TL_THROW("expected a Text");

    tlText* path = cwd_join(text);
    const char *p = tlTextData(path);
    if (p[0] != '/') TL_THROW("chdir: invalid cwd");

    struct stat buf;
    int r = stat(p, &buf);
    if (r == -1) TL_THROW("chdir: %s", strerror(errno));
    if (!(buf.st_mode & S_IFDIR)) TL_THROW("chdir: not a directory: %s", p);

    tlTask* task = tlTaskCurrent();
    task->locals = tlMapSet(task->locals, _s_cwd, path);
    return tlNull;
}
static tlValue _io_mkdir(tlArgs* args) {
    tlText* text = tlTextCast(tlArgsGet(args, 0));
    if (!text) TL_THROW("expected a Text");

    tlText* path = cwd_join(text);
    const char *p = tlTextData(path);
    if (p[0] != '/') TL_THROW("mkdir: invalid cwd");

    int perms = 0777;
    if (mkdir(p, perms)) {
        TL_THROW("mkdir: %s", strerror(errno));
    }
    return tlNull;
}
static tlValue _io_rmdir(tlArgs* args) {
    tlText* text = tlTextCast(tlArgsGet(args, 0));
    if (!text) TL_THROW("expected a Text");

    tlText* path = cwd_join(text);
    const char *p = tlTextData(path);
    if (p[0] != '/') TL_THROW("rmdir: invalid cwd");

    if (rmdir(p)) {
        TL_THROW("rmdir: %s", strerror(errno));
    }
    return tlNull;
}
static tlValue _io_rename(tlArgs* args) {
    tlText* text = tlTextCast(tlArgsGet(args, 0));
    if (!text) TL_THROW("expected a Text");

    tlText* path = cwd_join(text);
    const char *p = tlTextData(path);
    if (p[0] != '/') TL_THROW("rename: invalid cwd");

    tlText* totext = tlTextCast(tlArgsGet(args, 1));
    if (!totext) TL_THROW("expected a Text");

    tlText* topath = cwd_join(totext);
    const char *to_p = tlTextData(topath);
    if (to_p[0] != '/') TL_THROW("rename: invalid cwd");

    if (rename(p, to_p)) {
        TL_THROW("rename: %s", strerror(errno));
    }
    return tlNull;
}
static tlValue _io_unlink(tlArgs* args) {
    tlText* text = tlTextCast(tlArgsGet(args, 0));
    if (!text) TL_THROW("expected a Text");

    tlText* path = cwd_join(text);
    const char *p = tlTextData(path);
    if (p[0] != '/') TL_THROW("unlink: invalid cwd");

    if (unlink(p)) {
        TL_THROW("unlink: %s", strerror(errno));
    }
    return tlNull;
}

TL_REF_TYPE(tlFile);
TL_REF_TYPE(tlReader);
TL_REF_TYPE(tlWriter);

struct tlReader {
    tlLock lock;
    tlFile* file;
};
struct tlWriter {
    tlLock lock;
    tlFile* file;
};
// TODO how thread save is tlFile like this? does it need to be?
struct tlFile {
    tlHead head;
    ev_io ev;
    // cannot embed these, as pointers need to be 8 byte aligned
    tlReader* reader;
    tlWriter* writer;
};
static tlKind _tlFileKind = {
    .name = "File",
};
static tlKind _tlReaderKind = {
    .name = "Reader",
    .locked = true,
};
static tlKind _tlWriterKind = {
    .name = "Writer",
    .locked = true,
};
tlKind* tlFileKind = &_tlFileKind;
tlKind* tlReaderKind = &_tlReaderKind;
tlKind* tlWriterKind = &_tlWriterKind;

static tlFile* tlFileFromEv(ev_io *ev) {
    return tlFileAs(((char*)ev) - ((unsigned long)&((tlFile*)0)->ev));
}
static tlFile* tlFileFromReader(tlReader* reader) {
    return reader->file;
    //return tlFileAs(((char*)reader) - ((unsigned long)&((tlFile*)0)->reader));
}
static tlFile* tlFileFromWriter(tlWriter* writer) {
    return writer->file;
    //return tlFileAs(((char*)writer) - ((unsigned long)&((tlFile*)0)->writer));
}

void close_ev_io(void* _file, void* data) {
    tlFile* file = tlFileAs(_file);
    if (file->ev.fd < 0) return;
    int r = close(file->ev.fd);
    if (r) warning("%d: error: gc close file: %s", file->ev.fd, strerror(errno));
    trace(">>>> GC CLOSED FILE: %d <<<<", file->ev.fd);
    file->ev.fd = -1;
}
static tlFile* tlFileNew(int fd) {
    tlFile *file = tlAlloc(tlFileKind, sizeof(tlFile));
    file->reader = tlAlloc(tlReaderKind, sizeof(tlReader));
    file->reader->file = file;
    file->writer = tlAlloc(tlWriterKind, sizeof(tlWriter));
    file->writer->file = file;
    ev_io_init(&file->ev, io_cb, fd, 0);
#ifdef HAVE_BOEHMGC
    GC_REGISTER_FINALIZER_NO_ORDER(file, close_ev_io, null, null, null);
#endif
    trace("open: %p %d", file, fd);
    return file;
}

static tlValue _file_isClosed(tlArgs* args) {
    tlFile* file = tlFileCast(tlArgsTarget(args));
    if (!file) TL_THROW("expected a File");
    return tlBOOL(file->ev.fd < 0);
}

static tlValue _file_close(tlArgs* args) {
    tlTask* task = tlTaskCurrent();
    tlFile* file = tlFileCast(tlArgsTarget(args));
    if (!file) TL_THROW("expected a File");
    if (!tlLockIsOwner(tlLockAs(file->reader), task)) TL_THROW("expected a locked Reader");
    if (!tlLockIsOwner(tlLockAs(file->writer), task)) TL_THROW("expected a locked Writer");

    // already closed
    if (file->ev.fd < 0) return tlNull;

    trace("close: %p %d", file, file->ev.fd);
    //ev_io_stop(&file->ev);

    int r = close(file->ev.fd);
    if (r < 0) TL_THROW("close: failed: %s", strerror(errno));
    file->ev.fd = -1;
    return tlNull;
}

static tlValue _file_reader(tlArgs* args) {
    tlFile* file = tlFileCast(tlArgsTarget(args));
    if (!file) TL_THROW("expected a File");
    if (file->reader) return file->reader;
    return tlNull;
}
static tlValue _file_writer(tlArgs* args) {
    tlFile* file = tlFileCast(tlArgsTarget(args));
    if (!file) TL_THROW("expected a File");
    if (file->writer) return file->writer;
    return tlNull;
}

static tlValue _reader_close(tlArgs* args) {
    tlTask* task = tlTaskCurrent();
    trace("");
    tlReader* reader = tlReaderCast(tlArgsTarget(args));
    if (!reader || !tlLockIsOwner(tlLockAs(reader), task)) TL_THROW("expected a locked Reader");

    tlFile* file = tlFileFromReader(reader);
    assert(tlFileIs(file));

    // we ignore errors or already closed ...
    if (file->ev.fd < 0) return tlNull;
    int r = shutdown(file->ev.fd, SHUT_RD);
    if (r) trace("error in shutdown: %s", strerror(errno));
    return tlNull;
}
static tlValue _reader_read(tlArgs* args) {
    tlTask* task = tlTaskCurrent();
    trace("");
    tlReader* reader = tlReaderCast(tlArgsTarget(args));
    tlBuffer* buffer = tlBufferCast(tlArgsGet(args, 0));
    if (!reader || !tlLockIsOwner(tlLockAs(reader), task)) TL_THROW("expected a locked Reader");
    if (!buffer || !tlLockIsOwner(tlLockAs(buffer), task)) TL_THROW("expected a locked Buffer");

    tlFile* file = tlFileFromReader(reader);
    assert(tlFileIs(file));

    if (file->ev.fd < 0) TL_THROW("read: already closed");

    tl_buf* buf = buffer->buf;
    tlbuf_autogrow(buf);
    if (canwrite(buf) <= 0) TL_THROW("read: failed: buffer full");

    int len = read(file->ev.fd, writebuf(buf), canwrite(buf));
    if (len < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) { trace("EGAIN"); return tlNull; }
        TL_THROW("%d: read: failed: %s", file->ev.fd, strerror(errno));
    }
    didwrite(buf, len);
    trace("read: %d %d", file->ev.fd, len);
    return tlINT(len);
}

static tlValue _writer_close(tlArgs* args) {
    tlTask* task = tlTaskCurrent();
    trace("");
    tlWriter* writer = tlWriterCast(tlArgsTarget(args));
    if (!writer || !tlLockIsOwner(tlLockAs(writer), task)) TL_THROW("expected a locked Writer");

    tlFile* file = tlFileFromWriter(writer);
    assert(tlFileIs(file));

    // we ignore errors or already closed ...
    if (file->ev.fd < 0) return tlNull;
    int r = shutdown(file->ev.fd, SHUT_WR);
    if (r) trace("error in shutdown: %s", strerror(errno));
    return tlNull;
}
static tlValue _writer_write(tlArgs* args) {
    tlTask* task = tlTaskCurrent();
    trace("");
    tlWriter* writer = tlWriterCast(tlArgsTarget(args));
    tlBuffer* buffer = tlBufferCast(tlArgsGet(args, 0));
    if (!writer || !tlLockIsOwner(tlLockAs(writer), task)) TL_THROW("expected a locked Writer");
    if (!buffer || !tlLockIsOwner(tlLockAs(buffer), task)) TL_THROW("expected a locked Buffer");

    tlFile* file = tlFileFromWriter(writer);
    assert(tlFileIs(file));

    if (file->ev.fd < 0) TL_THROW("write: already closed");

    tl_buf* buf = buffer->buf;
    if (canread(buf) <= 0) TL_THROW("write: failed: buffer empty");

    int len = write(file->ev.fd, readbuf(buf), canread(buf));
    if (len < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) return tlNull;
        // TODO for EINPROGRESS on connect(); but only if not UDP?
        if (errno == ENOTCONN) return tlNull;
        TL_THROW("%d: write: failed: %s", file->ev.fd, strerror(errno));
    }
    didread(buf, len);
    trace("write: %d %d", file->ev.fd, len);
    return tlINT(len);
}

static tlValue _reader_accept(tlArgs* args) {
    tlTask* task = tlTaskCurrent();
    trace("");
    tlReader* reader = tlReaderCast(tlArgsTarget(args));
    if (!reader || !tlLockIsOwner(tlLockAs(reader), task)) TL_THROW("expected a locked Reader");
    tlFile* file = tlFileFromReader(reader);
    assert(tlFileIs(file));

    if (file->ev.fd < 0) TL_THROW("accept: already closed");

    struct sockaddr_in sockaddr;
    bzero(&sockaddr, sizeof(sockaddr));
    socklen_t len = sizeof(sockaddr);
    int fd = accept(file->ev.fd, (struct sockaddr *)&sockaddr, &len);
    if (fd < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) { trace("EAGAIN"); return tlNull; }
        TL_THROW("%d: accept: failed: %s", file->ev.fd, strerror(errno));
    }

    if (nonblock(fd) < 0) TL_THROW("accept: nonblock failed: %s", strerror(errno));
    trace("accept: %d %d", file->ev.fd, fd);
    return tlFileNew(fd);
}

static tlValue _File_open(tlArgs* args) {
    tlText* name = tlTextCast(tlArgsGet(args, 0));
    if (!name) TL_THROW("expected a file name");
    trace("open: %s", tl_str(name));
    int flags = tl_int_or(tlArgsGet(args, 1), -1);
    if (flags < 0) TL_THROW("expected flags");
    int perms = 0666;

    tlText* path = cwd_join(name);
    const char *p = tlTextData(path);
    if (p[0] != '/') TL_THROW("open: invalid cwd");

    int fd = open(p, flags|O_NONBLOCK, perms);
    if (fd < 0) TL_THROW("open: failed: %s file: '%s'", strerror(errno), tlTextData(name));
    return tlFileNew(fd);
}

static tlValue _File_from(tlArgs* args) {
    tlInt fd = tlIntCast(tlArgsGet(args, 0));
    if (!fd) TL_THROW("espected a file descriptor");
    //int r = nonblock(tl_int(fd));
    //if (r) TL_THROW("_File_from: failed: %s", strerror(errno));
    return tlFileNew(tl_int(fd));
}


// ** sockets **

// TODO this is a blocking call
static tlValue _Socket_resolve(tlArgs* args) {
    tlText* name = tlTextCast(tlArgsGet(args, 0));
    if (!name) TL_THROW("expected a Text");

    struct hostent *hp = gethostbyname(tlTextData(name));
    if (!hp) return tlNull;
    if (!hp->h_addr_list[0]) return tlNull;
    return tlTextFromTake(inet_ntoa(*(struct in_addr*)(hp->h_addr_list[0])), 0);
}

static tlValue _Socket_connect(tlArgs* args) {
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
    return tlFileNew(fd);
}

static tlValue _Socket_connect_unix(tlArgs* args) {
    tlText* address = tlTextCast(tlArgsGet(args, 0));
    if (!address) TL_THROW("expected a unix path");

    trace("unix_open: %s", tl_str(address));

    struct sockaddr_un sockaddr;
    bzero(&sockaddr, sizeof(sockaddr));
    sockaddr.sun_family = AF_UNIX;
    strcpy(sockaddr.sun_path, tlTextData(address));

    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) TL_THROW("unix_connect: failed: %s", strerror(errno));

    if (nonblock(fd) < 0) TL_THROW("unix_connect: nonblock failed: %s", strerror(errno));

    int r = connect(fd, (struct sockaddr *)&sockaddr, sizeof(sockaddr) + strlen(sockaddr.sun_path));
    if (r < 0 && errno != EINPROGRESS) TL_THROW("unix_connect: connect failed: %s", strerror(errno));

    if (errno == EINPROGRESS) trace("unix_connect: EINPROGRESS");
    return tlFileNew(fd);
}

// TODO make backlog configurable
static tlValue _ServerSocket_listen(tlArgs* args) {
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

    listen(fd, 1024); // backlog, configurable?
    return tlFileNew(fd);
}


// ** paths **

static tlValue _Path_stat(tlArgs* args) {
    tlText* name = tlTextCast(tlArgsGet(args, 0));
    if (!name) TL_THROW("expected a name");

    tlText* path = cwd_join(name);
    const char *p = tlTextData(path);
    if (p[0] != '/') TL_THROW("stat: invalid cwd");

    struct stat buf;
    int r = stat(p, &buf);
    if (r == -1) {
        if (errno == ENOENT || errno == ENOTDIR) {
            bzero(&buf, sizeof(buf));
        } else {
            TL_THROW("stat failed: %s for: '%s'", strerror(errno), tlTextData(name));
        }
    }

    tlMap *res = tlClone(_statMap);
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
tlKind _tlDirKind = {
    .name = "Dir",
    .locked = true,
};
tlKind* tlDirKind = &_tlDirKind;

static tlDir* tlDirNew(DIR* p) {
    tlDir* dir = tlAlloc(tlDirKind, sizeof(tlDir));
    dir->p = p;
    return dir;
}

static tlValue _Dir_open(tlArgs* args) {
    tlText* name = tlTextCast(tlArgsGet(args, 0));
    trace("opendir: %s", tl_str(name));

    tlText* path = cwd_join(name);
    const char *p = tlTextData(path);
    if (p[0] != '/') TL_THROW("opendir: invalid cwd");

    DIR *dir = opendir(p);
    if (!dir) TL_THROW("opendir: failed: %s for: '%s'", strerror(errno), tl_str(name));
    return tlDirNew(dir);
}

static tlValue _DirClose(tlArgs* args) {
    tlDir* dir = tlDirCast(tlArgsTarget(args));
    if (!dir) TL_THROW("expected a Dir");
    trace("closedir: %p", dir);
    if (closedir(dir->p)) TL_THROW("closedir: failed: %s", strerror(errno));
    return tlNull;
}

static tlValue _DirRead(tlArgs* args) {
    tlDir* dir = tlDirCast(tlArgsTarget(args));
    if (!dir) TL_THROW("expected a Dir");

    struct dirent dp;
    struct dirent *dpp;
    if (readdir_r(dir->p, &dp, &dpp)) TL_THROW("readdir: failed: %s", strerror(errno));
    trace("readdir: %p", dpp);
    if (!dpp) return tlNull;
    return tlTextFromCopy(dp.d_name, 0);
}

typedef struct tlDirEachFrame {
    tlFrame frame;
    tlDir* dir;
    tlValue* block;
} tlDirEachFrame;

static tlValue resumeDirEach(tlFrame* _frame, tlValue res, tlValue throw) {
    if (throw && throw == s_break) return tlNull;
    if (throw && throw != s_continue) return null;
    if (!throw && !res) return null;

    tlDirEachFrame* frame = (tlDirEachFrame*)_frame;
again:;
    struct dirent dp;
    struct dirent *dpp;
    if (readdir_r(frame->dir->p, &dp, &dpp)) TL_THROW("readdir: failed: %s", strerror(errno));
    trace("readdir: %p", dpp);
    if (!dpp) return tlNull;
    res = tlEval(tlCallFrom(frame->block, tlTextFromCopy(dp.d_name, 0), null));
    if (!res) return tlTaskPauseAttach(frame);
    goto again;
    return tlNull;
}

static tlValue _DirEach(tlArgs* args) {
    tlDir* dir = tlDirCast(tlArgsTarget(args));
    if (!dir) TL_THROW("expected a Dir");
    tlValue* block = tlArgsMapGet(args, tlSYM("block"));
    if (!block) return tlNull;

    tlDirEachFrame* frame = tlFrameAlloc(resumeDirEach, sizeof(tlDirEachFrame));
    frame->dir = dir;
    frame->block = block;
    return resumeDirEach((tlValue)frame, tlNull, null);
}


// ** child processes **

// does a exec after closing all fds and resetting signals etc
static void launch(const char* cwd, char** argv) {
    // close all fds
    int max = getdtablesize();
    if (max < 0) max = 50000;
    for (int i = 3; i < max; i++) close(i); // by lack of anything better ...

    // reset signal handlers; again, by lack of anything better ...
    for (int i = 1; i < 63; i++) {
        sig_t sig = signal(i, SIG_DFL);
        if (sig == SIG_ERR) trace("reset signal %d: %s", i, strerror(errno));
    }

    errno = 0;

    if (chdir(cwd)) {
        warning("chdir failed: %s", strerror(errno));
        _exit(255);
    }

    // actually launch
    execvp(argv[0], argv);
    warning("execvp failed: %s", strerror(errno));
    _exit(255);
}

// exec ... replaces current process, stopping hotel effectively
static tlValue _io_exec(tlArgs* args) {
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

    tlText* path = cwd_join(null);
    const char *p = tlTextData(path);
    if (p[0] != '/') TL_THROW("exec: invalid cwd");

    launch(p, argv);

    // this cannot happen, as launch does exit
    TL_THROW("oeps: %s", strerror(errno));
}

// TODO don't really need the lock, unless we would like to do in/out/err lazy again
TL_REF_TYPE(tlChild);
struct tlChild {
    tlLock lock;
    ev_child ev;
    lqueue wait_q; // maybe use a queue instead, or do what tlTask does ... ?
    tlValue res; // the result code
    tlValue in;
    tlValue out;
    tlValue err;
};
tlKind _tlChildKind = {
    .name = "Child",
    .locked = true,
};
tlKind* tlChildKind = &_tlChildKind;

static tlChild* tlChildFrom(ev_child *ev) {
    return tlChildAs(((char*)ev) - ((unsigned long)&((tlChild*)0)->ev));
}

static void child_cb(ev_child *ev, int revents) {
    tlChild* child = tlChildFrom(ev);
    trace("%p", ev);
    ev_child_stop(ev);
    child->res = tlINT(WEXITSTATUS(child->ev.rstatus));

    tlVm* vm = null;
    while (true) {
        tlTask* task = tlTaskFromEntry(lqueue_get(&child->wait_q));
        if (!task) return;
        if (!vm) vm = tlTaskGetVm(task);
        vm->waitevent -= 1;
        task->value = child->res;
        tlTaskReady(task);
    }
}

static tlChild* tlChildNew(pid_t pid, int in, int out, int err) {
    tlChild* child = tlAlloc(tlChildKind, sizeof(tlChild));
    ev_child_init(&child->ev, child_cb, pid, 0);
    ev_child_start(&child->ev);
    // we can do this lazily ... but then we need a finalizer to close fds
    child->in = tlFileNew(in);
    child->out = tlFileNew(out);
    child->err = tlFileNew(err);
    return child;
}

static tlValue resumeChildWait(tlFrame* frame, tlValue res, tlValue throw) {
    if (!res) return null;
    tlChild* child = tlChildAs(res);
    if (child->res) return child->res;

    // TODO not thread save ... needs mutex ...
    tlTask* task = tlTaskCurrent();
    tlVm* vm = tlTaskGetVm(task);
    vm->waitevent += 1;

    frame->resumecb = null;
    tlTaskWaitFor(null);
    lqueue_put(&child->wait_q, &task->entry);
    return tlTaskNotRunning;
}
static tlValue _child_wait(tlArgs* args) {
    tlChild* child = tlChildCast(tlArgsTarget(args));
    if (!child) TL_THROW("expected a Child");
    trace("child_wait: %d", child->ev.pid);

    // TODO use a_var and this will be thread safe ... (child_cb)
    if (child->res) return child->res;
    return tlTaskPauseResuming(resumeChildWait, child);
}
static tlValue _child_running(tlArgs* args) {
    tlChild* child = tlChildCast(tlArgsTarget(args));
    if (!child) TL_THROW("expected a Child");
    return tlBOOL(child->res == 0);
}
static tlValue _child_in(tlArgs* args) {
    tlChild* child = tlChildCast(tlArgsTarget(args));
    if (!child) TL_THROW("expected a Child");
    return child->in;
}
static tlValue _child_out(tlArgs* args) {
    tlChild* child = tlChildCast(tlArgsTarget(args));
    if (!child) TL_THROW("expected a Child");
    return child->out;
}
static tlValue _child_err(tlArgs* args) {
    tlChild* child = tlChildCast(tlArgsTarget(args));
    if (!child) TL_THROW("expected a Child");
    return child->err;
}

// launch a child process
// TODO allow passing in an tlFile as in/out/err ...
static tlValue _io_launch(tlArgs* args) {
    static int null_fd;
    if (!null_fd) {
        null_fd = open("/dev/null", O_RDWR);
        if (null_fd < 0) fatal("unable to open(/dev/null): %s", strerror(errno));
    }
    tlText* cwd = tlTextCast(tlArgsGet(args, 0));
    if (!cwd) TL_THROW("expected a Text as cwd");
    tlList* as = tlListCast(tlArgsGet(args, 1));
    if (!as) TL_THROW("expected a List");
    char** argv = malloc(sizeof(char*) * (tlListSize(as) + 1));
    for (int i = 0; i < tlListSize(as); i++) {
        tlText* text = tlTextCast(tlListGet(as, i));
        if (!text) { free(argv); TL_THROW("expected a Text"); }
        argv[i] = (char*)tlTextData(text);
    }
    argv[tlListSize(as)] = 0;

    bool want_in = tl_bool_or(tlArgsGet(args, 2), true);
    bool want_out = tl_bool_or(tlArgsGet(args, 3), true);
    bool want_err = tl_bool_or(tlArgsGet(args, 4), true);
    bool join_err = tl_bool_or(tlArgsGet(args, 5), false);
    trace("launch: %s [%d] %d,%d,%d,%d", argv[0], tlListSize(as) - 1,
            want_in, want_out, want_err, join_err);

    int _in[2] = {-1, -1};
    int _out[2] = {-1, -1};
    int _err[2] = {-1, -1};
    // create all the stdin/stdout/stderr fds; bail and cleanup if anything is wrong
    errno = 0;
    do {
        if (want_in) { if (pipe(_in)) break; } else {
            if ((_in[0] = dup(null_fd)) < 0) break;
            if ((_in[1] = dup(null_fd)) < 0) break;
        }
        if (want_out) { if (pipe(_out)) break; } else {
            if ((_out[0] = dup(null_fd)) < 0) break;
            if ((_out[1] = dup(null_fd)) < 0) break;
        }
        if (want_err) { if (pipe(_err)) break; } else {
        } if (join_err) {
            assert(_err[0] == -1 && _err[1] == -1);
            if ((_err[0] = dup(_out[0])) < 0) break;
            if ((_err[1] = dup(_out[1])) < 0) break;
        } else {
            assert(_err[0] == -1 && _err[1] == -1);
            if ((_err[0] = dup(null_fd)) < 0) break;
            if ((_err[1] = dup(null_fd)) < 0) break;
        }
        assert(_in[0] >= 0 && _in[1] >= 0);
        assert(_out[0] >= 0 && _out[1] >= 0);
        assert(_err[0] >= 0 && _err[1] >= 0);
    } while (false);
    int pid = -1;
    if (!errno) pid = fork();
    if (errno) {
        TL_THROW_SET("child exec: failed: %s", strerror(errno));
        close(_in[0]); close(_in[1]);
        close(_out[0]); close(_out[1]);
        close(_err[0]); close(_err[1]);
        free(argv);
        return null;
    }
    assert(pid >= 0);
    if (pid) {
        // parent; no error checking ... there would be nothing we could do
        close(_in[0]); close(_out[1]); close(_err[1]);
        nonblock(_in[1]); nonblock(_out[0]); nonblock(_err[0]);
        tlChild* child = tlChildNew(pid, _in[1], _out[0], _err[0]);
        return child;
    }

    // child
    dup2(_in[0], 0);
    dup2(_out[1], 1);
    dup2(_err[1], 2);

    launch(tlTextData(cwd), argv);
    // cannot happen, launch does exit
    TL_THROW("oeps: %s", strerror(errno));
}

INTERNAL const char* fileToText(tlValue v, char* buf, int size) {
    tlFile* file = tlFileAs(v);
    tlKind* kind = tl_kind(file);
    snprintf(buf, size, "<%s@%d>", kind->name, file->ev.fd);
    return buf;
}


// ** newstyle io, where languages controls all */

static void io_cb(ev_io *ev, int revents) {
    trace("io_cb: %p", ev);
    assert(ev->fd >= 0);
    tlFile* file = tlFileFromEv(ev);
    if (revents & EV_READ) {
        trace("CANREAD");
        assert(file->reader);
        assert(file->reader->lock.owner);
        tlMessage* msg = tlMessageAs(file->reader->lock.owner->value);
        tlVm* vm = tlTaskGetVm(file->reader->lock.owner);
        vm->waitevent -= 1;
        ev->events &= ~EV_READ;
        tlMessageReply(msg, null);
    }
    if (revents & EV_WRITE) {
        trace("CANWRITE");
        assert(file->writer);
        assert(file->writer->lock.owner);
        tlMessage* msg = tlMessageAs(file->writer->lock.owner->value);
        tlVm* vm = tlTaskGetVm(tlMessageGetSender(msg));
        vm->waitevent -= 1;
        ev->events &= ~EV_WRITE;
        tlMessageReply(msg, null);
    }
    if (!ev->events) ev_io_stop(ev);
}

static tlValue _io_close(tlArgs* args) {
    tlFile* file = tlFileCast(tlArgsGet(args, 0));
    if (!file) TL_THROW("expected a File");

    // already closed
    if (file->ev.fd < 0) return tlNull;

    trace("close: %p %d", file, file->ev.fd);
    ev_io_stop(&file->ev);

    int r = close(file->ev.fd);
    if (r < 0) TL_THROW("close: failed: %s", strerror(errno));
    file->ev.fd = -1;

    if (file->ev.events & EV_READ) {
        trace("CLOSED WITH READER");
        assert(file->reader);
        assert(file->reader->lock.owner);
        tlMessage* msg = tlMessageAs(file->reader->lock.owner->value);
        tlVm* vm = tlTaskGetVm(file->reader->lock.owner);
        vm->waitevent -= 1;
        file->ev.events &= ~EV_READ;
        tlMessageReply(msg, null);
    }
    if (file->ev.events & EV_WRITE) {
        trace("CLOSED WITH WRITER");
        assert(file->writer);
        assert(file->writer->lock.owner);
        tlMessage* msg = tlMessageAs(file->writer->lock.owner->value);
        tlVm* vm = tlTaskGetVm(tlMessageGetSender(msg));
        vm->waitevent -= 1;
        file->ev.events &= ~EV_WRITE;
        tlMessageReply(msg, null);
    }
    return tlNull;
}

static tlValue _io_waitread(tlArgs* args) {
    tlTask* task = tlTaskCurrent();
    tlVm* vm = tlTaskGetVm(task);

    tlReader* reader = tlReaderCast(tlArgsGet(args, 0));
    if (!reader) TL_THROW("expected a Reader");
    tlMessage* msg = tlMessageCast(tlArgsGet(args, 1));
    if (!msg) TL_THROW("expect a Msg");
    tlTask* sender = tlMessageGetSender(msg);
    if (!tlLockIsOwner(tlLockAs(reader), sender)) TL_THROW("expected a locked Reader");

    tlFile* file = tlFileFromReader(reader);
    assert(tlFileIs(file));

    assert(reader->lock.owner == msg->sender);
    assert(msg->sender->value == msg);

    // TODO we should actually reply to the message instead
    assert(file->ev.fd >= 0);

    file->ev.events |= EV_READ;
    ev_io_start(&file->ev);
    vm->waitevent += 1;

    return tlNull;
}

static tlValue _io_waitwrite(tlArgs* args) {
    tlTask* task = tlTaskCurrent();
    tlVm* vm = tlTaskGetVm(task);

    tlWriter* writer = tlWriterCast(tlArgsGet(args, 0));
    if (!writer) TL_THROW("expected a Writer");
    tlMessage* msg = tlMessageCast(tlArgsGet(args, 1));
    if (!msg) TL_THROW("expect a Msg");
    tlTask* sender = tlMessageGetSender(msg);
    if (!tlLockIsOwner(tlLockAs(writer), sender)) TL_THROW("expected a locked Writer");

    tlFile* file = tlFileFromWriter(writer);
    assert(tlFileIs(file));

    assert(writer->lock.owner == msg->sender);
    assert(msg->sender->value == msg);

    // TODO we should actually reply to the message instead
    assert(file->ev.fd >= 0);

    file->ev.events |= EV_WRITE;
    ev_io_start(&file->ev);
    vm->waitevent += 1;

    return tlNull;
}

static void timer_cb(ev_timer* timer, int revents) {
    trace("timer_cb: %p", timer);
    tlVm* vm = tlTaskGetVm(tlMessageGetSender(tlMessageAs(timer->data)));
    ev_timer_stop(timer);
    vm->waitevent -= 1;
    tlMessageReply(tlMessageAs(timer->data), null);
    free(timer);
}
static tlValue _io_wait(tlArgs* args) {
    tlTask* task = tlTaskCurrent();
    tlVm* vm = tlTaskGetVm(task);
    tlMessage* msg = tlMessageCast(tlArgsGet(args, 1));
    if (!msg) TL_THROW("expect a Msg");

    int millis = tl_int_or(tlArgsGet(args, 0), 1000);
    trace("sleep: %d", millis);
    float ms = millis / 1000.0;

    ev_timer *timer = malloc(sizeof(ev_timer));
    timer->data = msg;
    ev_timer_init(timer, timer_cb, ms, 0);
    ev_timer_start(timer);
    vm->waitevent += 1;

    return tlNull;
}


static ev_async loop_interrupt;
static void async_cb(ev_async* async, int revents) { }

static void iointerrupt() { ev_async_send(&loop_interrupt); }

static tlValue _io_init(tlArgs* args) {
    tlQueue* queue = tlQueueNew();
    queue->signalcb = iointerrupt;
    return queue;
}

static tlValue _io_haswaiting(tlArgs* args) {
    tlTask* task = tlTaskCurrent();
    tlVm* vm = tlTaskGetVm(task);
    assert(vm->waitevent >= 0);
    trace("tasks=%zd, run=%zd, io=%zd", vm->tasks, vm->runnable, vm->waitevent);
    if (vm->waitevent > 0) return tlTrue;
    // TODO should not be "> 1" but "> runloops"
    // TODO is this thread safe? I think so ...
    // if the runloops are the only tasks, no other task can be spawned
    if (a_get(&vm->runnable) > 1) return tlTrue;
    return tlFalse;
}

static tlValue _io_run(tlArgs* args) {
    tlTask* task = tlTaskCurrent();
    tlVm* vm = tlTaskGetVm(task);
    if (vm->lock) {
        trace("blocking for events");
        ev_run(EVRUN_ONCE);
    } else {
        if (a_get(&vm->runnable) > 1) {
            trace("checking for events; tasks=%zd, run=%zd, io=%zd",
                    vm->tasks, vm->runnable, vm->waitevent);
            ev_run(EVRUN_NOWAIT);
        } else {
            trace("blocking for events; tasks=%zd, run=%zd, io=%zd",
                    vm->tasks, vm->runnable, vm->waitevent);
            ev_run(EVRUN_ONCE);
        }
    }
    trace("done");
    return tlNull;
}

static const tlNativeCbs __evio_natives[] = {
    { "_io_getrusage", _io_getrusage },
    { "_io_getenv", _io_getenv },

    { "_io_chdir", _io_chdir },
    { "_io_mkdir", _io_mkdir },
    { "_io_rmdir", _io_rmdir },
    { "_io_rename", _io_rename },
    { "_io_unlink", _io_unlink },

    { "_File_open", _File_open },
    { "_File_from", _File_from },
    { "_Socket_connect", _Socket_connect },
    { "_Socket_connect_unix", _Socket_connect_unix },
    { "_Socket_resolve", _Socket_resolve },
    { "_ServerSocket_listen", _ServerSocket_listen },
    { "_Path_stat", _Path_stat },
    { "_Dir_open", _Dir_open },

    { "_io_exec", _io_exec },
    { "_io_launch", _io_launch },

    { "_io_wait", _io_wait },
    { "_io_close", _io_close },
    { "_io_waitread", _io_waitread },
    { "_io_waitwrite", _io_waitwrite },

    { "_io_haswaiting", _io_haswaiting },
    { "_io_run", _io_run },
    { "_io_init", _io_init },
    { 0, 0 }
};

void evio_init() {
    _tlReaderKind.klass = tlClassMapFrom(
        "read", _reader_read,
        "accept", _reader_accept,
        "close", _reader_close,
        null
    );
    _tlWriterKind.klass = tlClassMapFrom(
        "write", _writer_write,
        "close", _writer_close,
        null
    );
    _tlFileKind.toText = fileToText;
    _tlFileKind.klass = tlClassMapFrom(
        "isClosed", _file_isClosed,
        "close", _file_close,
        "reader", _file_reader,
        "writer", _file_writer,
        null
    );
    _tlDirKind.klass = tlClassMapFrom(
        "read", _DirRead,
        "each", _DirEach,
        null
    );
    _tlChildKind.klass = tlClassMapFrom(
        "isRunning", _child_running,
        "wait", _child_wait,
        "in", _child_in,
        "out", _child_out,
        "err", _child_err,
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

    tl_register_global("_Stat_IFMT", tlINT(S_IFMT));
    tl_register_global("_Stat_IFREG", tlINT(S_IFREG));
    tl_register_global("_Stat_IFDIR", tlINT(S_IFDIR));
    tl_register_global("_Stat_IFLNK", tlINT(S_IFLNK));

    _s_cwd = tlSYM("cwd");

    // for stat syscall
    tlSet* keys = tlSetNew(12);
    _s_dev = tlSYM("dev"); tlSetAdd_(keys, _s_dev);
    _s_ino = tlSYM("ino"); tlSetAdd_(keys, _s_ino);
    _s_mode = tlSYM("mode"); tlSetAdd_(keys, _s_mode);
    _s_nlink = tlSYM("nlink"); tlSetAdd_(keys, _s_nlink);
    _s_uid = tlSYM("uid"); tlSetAdd_(keys, _s_uid);
    _s_gid = tlSYM("gid"); tlSetAdd_(keys, _s_gid);
    _s_rdev = tlSYM("rdev"); tlSetAdd_(keys, _s_rdev);
    _s_size = tlSYM("size"); tlSetAdd_(keys, _s_size);
    _s_blksize = tlSYM("blksize"); tlSetAdd_(keys, _s_blksize);
    _s_blocks = tlSYM("blocks"); tlSetAdd_(keys, _s_blocks);
    _s_atime = tlSYM("atime"); tlSetAdd_(keys, _s_atime);
    _s_mtime = tlSYM("mtime"); tlSetAdd_(keys, _s_mtime);
    _statMap = tlMapNew(keys);
    tlMapToObject_(_statMap);

    // for rusage syscall
    keys = tlSetNew(4);
    _s_cpu = tlSYM("cpu"); tlSetAdd_(keys, _s_cpu);
    _s_mem = tlSYM("mem"); tlSetAdd_(keys, _s_mem);
    _s_pid = tlSYM("pid"); tlSetAdd_(keys, _s_pid);
    _s_gcs = tlSYM("gcs"); tlSetAdd_(keys, _s_gcs);
    _usageMap = tlMapNew(keys);
    tlMapToObject_(_usageMap);

    char buf[MAXPATHLEN + 1];
    char* cwd = getcwd(buf, sizeof(buf));
    tl_cwd = tlTextFromCopy(cwd, 0);

    signal(SIGPIPE, SIG_IGN);

    ev_default_loop(0);
    ev_async_init(&loop_interrupt, async_cb);
    ev_async_start(&loop_interrupt);

    // TODO this is here as a "test"
    ev_async_send(&loop_interrupt);
    ev_run(EVRUN_NOWAIT);
}

