// author: Onne Gorter, license: MIT (see license.txt)

// basic io functions in hotel

// TODO use runloop mutex to protect open() like syscalls in order to do CLOEXEC if available
// TODO for above, all open/close/fork, from same thread or mutex

//. module io:

#include "../llib/lqueue.h"

#include "platform.h"
#include "evio.h"

#include "value.h"
#include "task.h"
#include "vm.h"
#include "lock.h"
#include "worker.h"
#include "queue.h"
#include "frame.h"
#include "buffer.h"

#define EV_STANDALONE 1
#define EV_MULTIPLICITY 0
#include "../libev/ev.h"
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/resource.h>

static tlSym _s_cwd;

static tlObject* _statMap;
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

static tlObject* _usageMap;
static tlSym _s_cpu;
static tlSym _s_mem;
static tlSym _s_pid;
static tlSym _s_gcs;
static tlSym _s_gcmem;

tlString* tl_cwd;

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

static tlHandle _io_getrusage(tlTask* task, tlArgs* args) {
    tlObject *res = tlClone(_usageMap);
    struct rusage use;
    getrusage(RUSAGE_SELF, &use);
    tlObjectSet_(res, _s_cpu, tlINT(use.ru_utime.tv_sec + use.ru_stime.tv_sec));
    tlObjectSet_(res, _s_mem, tlINT(use.ru_maxrss));
    tlObjectSet_(res, _s_pid, tlINT(getpid()));
#ifdef HAVE_BOEHMGC
#if GC_VERSION_MAJOR >= 7 && GC_VERSION_MINOR <= 3
    tlObjectSet_(res, _s_gcs, tlINT(GC_gc_no));
#else
    tlObjectSet_(res, _s_gcs, tlINT(GC_get_gc_no()));
#endif
    tlObjectSet_(res, _s_gcmem, tlINT(GC_get_heap_size()));
#endif
    return res;
}
static tlHandle _io_getenv(tlTask* task, tlArgs* args) {
    tlString* str = tlStringCast(tlArgsGet(args, 0));
    if (!str) TL_THROW("expected a String");
    const char* s = getenv(tlStringData(str));
    if (!s) return tlNull;
    return tlStringFromCopy(s, 0);
}

static tlHandle _io_getpid(tlTask* task, tlArgs* args) {
    return tlINT(getpid());
}

static tlHandle _io_procname(tlTask* task, tlArgs* args) {
    tlVm* vm = tlVmCurrent(task);
    return vm->procname;
}

static tlHandle _io_gethostname(tlTask* task, tlArgs* args) {
    char buf[MAXHOSTNAMELEN + 1];
    int r = gethostname(buf, sizeof(buf));
    if (r != 0) TL_THROW("gethostname '%s'", strerror(errno));
    return tlStringFromCopy(buf, 0);
}

static tlHandle _io_getdomainname(tlTask* task, tlArgs* args) {
    char buf[MAXHOSTNAMELEN + 1];
    int r = getdomainname(buf, sizeof(buf));
    if (r != 0) TL_THROW("getdomainname '%s'", strerror(errno));
    return tlStringFromCopy(buf, 0);
}

// TODO normalize /../ and /./
static tlString* path_join(tlString* lhs, tlString* rhs) {
    if (!lhs) return rhs;
    if (!rhs) return lhs;
    const char* l = tlStringData(lhs);
    const char* r = tlStringData(rhs);
    if (r[0] == '/') return rhs;

    int llen = strlen(l);
    int rlen = strlen(r);
    if (llen == 0) return rhs;
    if (rlen == 0) return lhs;

    char* buf = malloc(llen + 1 + rlen + 1);
    buf[0] = 0;
    strcat(buf, l);
    strcat(buf + llen, "/");
    strcat(buf + llen + 1, r);

    return tlStringFromTake(buf, llen + 1 + rlen);
}
static tlString* cwd_join(tlTask* task, tlString* path) {
    tlString* cwd = null;
    if (task->locals) cwd = tlStringCast(tlObjectGetSym(task->locals, _s_cwd));
    tlString* res = path_join(cwd, path);
    return res;
}

static tlHandle _io_chdir(tlTask* task, tlArgs* args) {
    tlString* str = tlStringCast(tlArgsGet(args, 0));
    if (!str) TL_THROW("expected a String");

    tlString* path = cwd_join(task, str);
    const char *p = tlStringData(path);
    if (p[0] != '/') TL_THROW("chdir: invalid cwd");

    struct stat buf;
    int r = stat(p, &buf);
    if (r == -1) TL_THROW("chdir: %s", strerror(errno));
    if (!(buf.st_mode & S_IFDIR)) TL_THROW("chdir: not a directory: %s", p);

    task->locals = tlObjectSet(task->locals, _s_cwd, path);
    return tlNull;
}
static tlHandle _io_mkdir(tlTask* task, tlArgs* args) {
    tlString* str = tlStringCast(tlArgsGet(args, 0));
    if (!str) TL_THROW("expected a String");

    tlString* path = cwd_join(task, str);
    const char *p = tlStringData(path);
    if (p[0] != '/') TL_THROW("mkdir: invalid cwd");

    int perms = 0777;
    if (mkdir(p, perms)) {
        TL_THROW("mkdir: %s", strerror(errno));
    }
    return tlNull;
}
static tlHandle _io_rmdir(tlTask* task, tlArgs* args) {
    tlString* str = tlStringCast(tlArgsGet(args, 0));
    if (!str) TL_THROW("expected a String");

    tlString* path = cwd_join(task, str);
    const char *p = tlStringData(path);
    if (p[0] != '/') TL_THROW("rmdir: invalid cwd");

    if (rmdir(p)) {
        TL_THROW("rmdir: %s", strerror(errno));
    }
    return tlNull;
}
static tlHandle _io_rename(tlTask* task, tlArgs* args) {
    tlString* str = tlStringCast(tlArgsGet(args, 0));
    if (!str) TL_THROW("expected a String");

    tlString* path = cwd_join(task, str);
    const char *p = tlStringData(path);
    if (p[0] != '/') TL_THROW("rename: invalid cwd");

    tlString* toString = tlStringCast(tlArgsGet(args, 1));
    if (!toString) TL_THROW("expected a String");

    tlString* topath = cwd_join(task, toString);
    const char *to_p = tlStringData(topath);
    if (to_p[0] != '/') TL_THROW("rename: invalid cwd");

    if (rename(p, to_p)) {
        TL_THROW("rename: %s", strerror(errno));
    }
    return tlNull;
}
static tlHandle _io_unlink(tlTask* task, tlArgs* args) {
    tlString* str = tlStringCast(tlArgsGet(args, 0));
    if (!str) TL_THROW("expected a String");

    tlString* path = cwd_join(task, str);
    const char *p = tlStringData(path);
    if (p[0] != '/') TL_THROW("unlink: invalid cwd");

    if (unlink(p)) {
        TL_THROW("unlink: %s", strerror(errno));
    }
    return tlNull;
}

//. readlink(path, otherwise?): see man 2 readlink, will return the target file of a symbolic link
//. if not a link, will return otherwise (null if not passed in)
//. if errors occur, will throw that error
static tlHandle _io_readlink(tlTask* task, tlArgs* args) {
    tlString* str = tlStringCast(tlArgsGet(args, 0));
    if (!str) TL_THROW("expected a String");

    tlString* path = cwd_join(task, str);
    const char *p = tlStringData(path);
    if (p[0] != '/') TL_THROW("readlink: invalid cwd");

    char buf[PATH_MAX + 1];
    ssize_t len = readlink(p, buf, sizeof(buf) - 1);
    if (len == -1) {
        if (errno == EINVAL) return tlOR_NULL(tlArgsGet(args, 1)); // not a link
        TL_THROW("readlink: %s", strerror(errno));
    }
    buf[len] = 0;

    return tlStringFromCopy(buf, len);
}

TL_REF_TYPE(tlFile);
TL_REF_TYPE(tlReader);
TL_REF_TYPE(tlWriter);

struct tlReader {
    tlLock lock;
    tlFile* file;
    bool closed;
};
struct tlWriter {
    tlLock lock;
    tlFile* file;
    bool closed;
};
// TODO how thread save is tlFile like this? does it need to be?
struct tlFile {
    tlHead head;
    ev_io ev;
    // cannot embed these, as pointers need to be 8 byte aligned
    tlReader* reader;
    tlWriter* writer;
};
static void fileFinalizer(tlHandle handle);
static tlKind _tlFileKind = {
    .name = "File",
    .finalizer = fileFinalizer,
};
static tlKind _tlReaderKind = {
    .name = "Reader",
    .locked = true,
};
static tlKind _tlWriterKind = {
    .name = "Writer",
    .locked = true,
};
tlKind* tlFileKind;
tlKind* tlReaderKind;
tlKind* tlWriterKind;

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

static void fileFinalizer(tlHandle handle) {
    tlFile* file = tlFileAs(handle);
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
    trace("open: %p %d", file, fd);
    return file;
}

static tlHandle _file_port(tlTask* task, tlArgs* args) {
    tlFile* file = tlFileAs(tlArgsTarget(args));

    struct sockaddr_storage addr;
    bzero(&addr, sizeof(addr));
    socklen_t len = sizeof(addr);
    int r = getsockname(file->ev.fd, (struct sockaddr *)&addr, &len);
    if (r < 0) TL_THROW("file.port: %s", strerror(errno));

    if (addr.ss_family == AF_INET) {
        return tlINT(ntohs(((struct sockaddr_in*)&addr)->sin_port));
    } else {
        return tlINT(ntohs(((struct sockaddr_in6*)&addr)->sin6_port));
    }
}

static tlHandle _file_ip(tlTask* task, tlArgs* args) {
    tlFile* file = tlFileAs(tlArgsTarget(args));

    struct sockaddr_storage addr;
    bzero(&addr, sizeof(addr));
    socklen_t len = sizeof(addr);
    int r = getsockname(file->ev.fd, (struct sockaddr *)&addr, &len);
    if (r < 0) TL_THROW("file.ip: %s", strerror(errno));

    char str[INET6_ADDRSTRLEN];
    if (addr.ss_family == AF_INET) {
        return tlStringFromCopy(inet_ntop(AF_INET, &((struct sockaddr_in*)&addr)->sin_addr, str, sizeof(str)), 0);
    } else {
        return tlStringFromCopy(inet_ntop(AF_INET6, &((struct sockaddr_in6*)&addr)->sin6_addr, str, sizeof(str)), 0);
    }
}

static tlHandle _file_peer_port(tlTask* task, tlArgs* args) {
    tlFile* file = tlFileAs(tlArgsTarget(args));

    struct sockaddr_storage addr;
    bzero(&addr, sizeof(addr));
    socklen_t len = sizeof(addr);
    int r = getpeername(file->ev.fd, (struct sockaddr *)&addr, &len);
    if (r < 0) TL_THROW("file.port: %s", strerror(errno));

    if (addr.ss_family == AF_INET) {
        return tlINT(ntohs(((struct sockaddr_in*)&addr)->sin_port));
    } else {
        return tlINT(ntohs(((struct sockaddr_in6*)&addr)->sin6_port));
    }
}

static tlHandle _file_peer_ip(tlTask* task, tlArgs* args) {
    tlFile* file = tlFileAs(tlArgsTarget(args));

    struct sockaddr_storage addr;
    bzero(&addr, sizeof(addr));
    socklen_t len = sizeof(addr);
    int r = getpeername(file->ev.fd, (struct sockaddr *)&addr, &len);
    if (r < 0) TL_THROW("file.peer_ip: %s", strerror(errno));

    char str[INET6_ADDRSTRLEN];
    if (addr.ss_family == AF_INET) {
        return tlStringFromCopy(inet_ntop(AF_INET, &((struct sockaddr_in*)&addr)->sin_addr, str, sizeof(str)), 0);
    } else {
        return tlStringFromCopy(inet_ntop(AF_INET6, &((struct sockaddr_in6*)&addr)->sin6_addr, str, sizeof(str)), 0);
    }
}

static tlHandle _file_isClosed(tlTask* task, tlArgs* args) {
    tlFile* file = tlFileAs(tlArgsTarget(args));
    return tlBOOL(file->ev.fd < 0);
}

static tlHandle _file_close(tlTask* task, tlArgs* args) {
    tlFile* file = tlFileAs(tlArgsTarget(args));
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

static tlHandle _file_reader(tlTask* task, tlArgs* args) {
    tlFile* file = tlFileAs(tlArgsTarget(args));
    if (file->reader) return file->reader;
    return tlNull;
}

static tlHandle _file_writer(tlTask* task, tlArgs* args) {
    tlFile* file = tlFileAs(tlArgsTarget(args));
    if (file->writer) return file->writer;
    return tlNull;
}

static tlHandle _reader_isClosed(tlTask* task, tlArgs* args) {
    tlReader* reader = tlReaderAs(tlArgsTarget(args));
    if (!tlLockIsOwner(tlLockAs(reader), task)) TL_THROW("expected a locked Reader");

    tlFile* file = tlFileFromReader(reader);
    assert(tlFileIs(file));

    return tlBOOL(file->ev.fd < 0 || reader->closed);
}

static tlHandle _reader_close(tlTask* task, tlArgs* args) {
    tlReader* reader = tlReaderAs(tlArgsTarget(args));
    if (!tlLockIsOwner(tlLockAs(reader), task)) TL_THROW("expected a locked Reader");

    tlFile* file = tlFileFromReader(reader);
    assert(tlFileIs(file));

    // we ignore errors or already closed ...
    if (file->ev.fd < 0) return tlNull;
    int r = shutdown(file->ev.fd, SHUT_RD);
    if (r) trace("error in shutdown: %s", strerror(errno));
    return tlNull;
}

static tlHandle _reader_read(tlTask* task, tlArgs* args) {
    tlReader* reader = tlReaderAs(tlArgsTarget(args));
    tlBuffer* buf= tlBufferCast(tlArgsGet(args, 0));
    if (!tlLockIsOwner(tlLockAs(reader), task)) TL_THROW("expected a locked Reader");
    if (!buf|| !tlLockIsOwner(tlLockAs(buf), task)) TL_THROW("expected a locked Buffer");

    tlFile* file = tlFileFromReader(reader);
    assert(tlFileIs(file));

    if (file->ev.fd < 0) return tlINT(0);
    if (reader->closed) return tlINT(0);

    // TODO figure out how much data there is to read ...
    // TODO do this in a while loop until all data from kernel is read?
    tlBufferBeforeWrite(buf, 5 * 1024);
    assert(canwrite(buf));

    int len = read(file->ev.fd, writebuf(buf), canwrite(buf));
    if (len < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) { trace("EGAIN"); return tlNull; }
        // TODO can it be some already closed error?
        TL_THROW("%d: read: failed: %s", file->ev.fd, strerror(errno));
    }
    if (len == 0) reader->closed = true;
    didwrite(buf, len);
    trace("read: %d %d", file->ev.fd, len);
    return tlINT(len);
}

static tlHandle _writer_isClosed(tlTask* task, tlArgs* args) {
    tlWriter* writer = tlWriterAs(tlArgsTarget(args));
    if (!tlLockIsOwner(tlLockAs(writer), task)) TL_THROW("expected a locked Writer");

    tlFile* file = tlFileFromWriter(writer);
    assert(tlFileIs(file));

    return tlBOOL(file->ev.fd < 0 || writer->closed);
}

static tlHandle _writer_close(tlTask* task, tlArgs* args) {
    tlWriter* writer = tlWriterAs(tlArgsTarget(args));
    if (!tlLockIsOwner(tlLockAs(writer), task)) TL_THROW("expected a locked Writer");

    tlFile* file = tlFileFromWriter(writer);
    assert(tlFileIs(file));

    // we ignore errors or already closed ...
    if (file->ev.fd < 0) return tlNull;
    int r = shutdown(file->ev.fd, SHUT_WR);
    if (r) trace("error in shutdown: %s", strerror(errno));
    writer->closed = true;
    return tlNull;
}

static tlHandle _writer_write(tlTask* task, tlArgs* args) {
    tlWriter* writer = tlWriterAs(tlArgsTarget(args));
    tlBuffer* buf = tlBufferCast(tlArgsGet(args, 0));
    if (!tlLockIsOwner(tlLockAs(writer), task)) TL_THROW("expected a locked Writer");
    if (!buf || !tlLockIsOwner(tlLockAs(buf), task)) TL_THROW("expected a locked Buffer");

    tlFile* file = tlFileFromWriter(writer);
    assert(tlFileIs(file));

    if (file->ev.fd < 0) TL_THROW("write: already closed");
    if (writer->closed) TL_THROW("writer: already closed");

    if (tlBufferSize(buf) <= 0) TL_THROW("write: failed: buffer empty");

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

static tlHandle _reader_accept(tlTask* task, tlArgs* args) {
    tlReader* reader = tlReaderAs(tlArgsTarget(args));
    if (!tlLockIsOwner(tlLockAs(reader), task)) TL_THROW("expected a locked Reader");
    tlFile* file = tlFileFromReader(reader);
    assert(tlFileIs(file));

    if (file->ev.fd < 0) return tlNull;

    struct sockaddr_storage sockaddr;
    bzero(&sockaddr, sizeof(sockaddr));
    socklen_t len = sizeof(sockaddr);
    int fd = accept(file->ev.fd, (struct sockaddr *)&sockaddr, &len);
    if (fd < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) { trace("EAGAIN"); return tlNull; }
        TL_THROW("%d: accept: failed: %s", file->ev.fd, strerror(errno));
    }

    if (nonblock(fd) < 0) {
        close(fd);
        TL_THROW("accept: nonblock failed: %s", strerror(errno));
    }
    trace("accept: %d %d", file->ev.fd, fd);
    return tlFileNew(fd);
}

static tlHandle _File_open(tlTask* task, tlArgs* args) {
    tlString* name = tlStringCast(tlArgsGet(args, 0));
    if (!name) TL_THROW("expected a file name");
    trace("open: %s", tl_str(name));
    int flags = tl_int_or(tlArgsGet(args, 1), -1);
    if (flags < 0) TL_THROW("expected flags");
    int perms = 0666;

    tlString* path = cwd_join(task, name);
    const char *p = tlStringData(path);
    if (p[0] != '/') TL_THROW("open: invalid cwd");

    int fd = open(p, flags|O_NONBLOCK, perms);
    if (fd < 0) TL_THROW("open: failed: %s file: '%s'", strerror(errno), tlStringData(name));
    return tlFileNew(fd);
}

static tlHandle _File_from(tlTask* task, tlArgs* args) {
    tlInt fd = tlIntCast(tlArgsGet(args, 0));
    if (!fd) TL_THROW("espected a file descriptor");
    //int r = nonblock(tl_int(fd));
    //if (r) TL_THROW("_File_from: failed: %s", strerror(errno));
    return tlFileNew(tl_int(fd));
}


// ** sockets **

// NOTE this is a blocking call
static tlHandle _Socket_resolve(tlTask* task, tlArgs* args) {
    tlString* name = tlStringCast(tlArgsGet(args, 0));
    if (!name) TL_THROW("expected a String");

    struct addrinfo* res;
    struct addrinfo* rp;

    int error = getaddrinfo(tlStringData(name), null, null, &res);
    if (error) TL_THROW("resolve: getaddrinfo failed: %s", gai_strerror(error));

    for (rp = res; rp != null; rp = rp->ai_next) {
        char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];
        if (getnameinfo(rp->ai_addr, rp->ai_addrlen, hbuf, sizeof(hbuf), sbuf, sizeof(sbuf), NI_NUMERICHOST | NI_NUMERICSERV) == 0) {
            freeaddrinfo(res);
            return tlStringFromCopy(hbuf, 0);
        }
    }
    freeaddrinfo(res);
    return tlNull;
}

static tlHandle _Socket_udp(tlTask* task, tlArgs* args) {
    static tlSym s_host;
    if (!s_host) s_host = tlSYM("host");

    int port = tl_int_or(tlArgsGet(args, 0), 0);
    tlString* host = tlStringCast(tlArgsGetNamed(args, s_host));
    bool broadcast = tl_bool_or(tlArgsGet(args, 1), false);

    const char* shost = host? tlStringData(host) : null;

    trace("udp_open: %s:%d, broadcast: %s", shost, port, broadcast? "true":"false");

    struct addrinfo hints;
    bzero(&hints, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE;

    char sport[7];
    snprintf(sport, sizeof(sport), "%d", port);

    struct addrinfo* res;
    int error = getaddrinfo(shost, sport, &hints, &res);
    if (error) TL_THROW("udp socket: getaddrinfo failed: %s", gai_strerror(error));

    // TODO perhaps loop?
    int fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (fd < 0) {
        freeaddrinfo(res);
        TL_THROW("udp socket failed: %s", strerror(errno));
    }

    if (tlIntIs(tlArgsGet(args, 0))) {
        int r = bind(fd, res->ai_addr, res->ai_addrlen);
        if (r < 0) {
            close(fd);
            freeaddrinfo(res);
            TL_THROW("udp bind failed: %s", strerror(errno));
        }
    }
    freeaddrinfo(res);

    if (broadcast) {
        int r = setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast));
        if (r < 0) {
            close(fd);
            TL_THROW("udp set broadcast failed: %s", strerror(errno));
        }
    }

    if (nonblock(fd) < 0) {
        close(fd);
        TL_THROW("udp nonblock failed: %s", strerror(errno));
    }
    return tlFileNew(fd);
}

static tlHandle _Socket_recvfrom(tlTask* task, tlArgs* args) {
    tlFile* file = tlFileCast(tlArgsGet(args, 0));
    if (!file) TL_THROW("expected a udp socket");
    tlBuffer* buf = tlBufferCast(tlArgsGet(args, 1));
    if (!buf) TL_THROW("expected a buffer");

    trace("recvfrom: %d", canwrite(buf));

    tlBufferBeforeWrite(buf, 65535); // max UDP packet
    assert(canwrite(buf));

    struct sockaddr_storage from;
    unsigned int from_len = sizeof(from);
    int r = recvfrom(file->ev.fd, writebuf(buf), canwrite(buf), 0, (struct sockaddr*)&from, &from_len);
    if (r < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) { trace("EGAIN"); return tlNull; }
        TL_THROW("%d: recvfrom: failed: %s", file->ev.fd, strerror(errno));
    }
    didwrite(buf, r);

    int port;
    if (from.ss_family == AF_INET) {
        port = ntohs(((struct sockaddr_in*)&from)->sin_port);
    } else {
        port = ntohs(((struct sockaddr_in6*)&from)->sin6_port);
    }

    char str[INET6_ADDRSTRLEN];
    if (from.ss_family == AF_INET) {
        inet_ntop(AF_INET, &((struct sockaddr_in*)&from)->sin_addr, str, sizeof(str));
    } else {
        inet_ntop(AF_INET6, &((struct sockaddr_in6*)&from)->sin6_addr, str, sizeof(str));
    }
    trace("recvfrom: %s:%d - %d", str, port, r);
    tlString* ip = tlStringFromCopy(str, 0);
    return tlResultFrom(buf, ip, tlINT(port));
}

static tlHandle _Socket_sendto(tlTask* task, tlArgs* args) {
    tlFile* file = tlFileCast(tlArgsGet(args, 0));
    if (!file) TL_THROW("expected a udp socket");
    tlString* host = tlStringCast(tlArgsGet(args, 1));
    if (!host) TL_THROW("expected a host");
    int port = tl_int_or(tlArgsGet(args, 2), -1);
    if (port < 0) TL_THROW("expected a port");
    tlBuffer* buf = tlBufferCast(tlArgsGet(args, 3));
    if (!buf) TL_THROW("expected a buffer");

    trace("sendto: %s:%d", tl_str(host), port);

    // we need to know our local socket family
    struct sockaddr_storage addr;
    bzero(&addr, sizeof(addr));
    socklen_t addrlen = sizeof(addr);
    int r = getsockname(file->ev.fd, (struct sockaddr *)&addr, &addrlen);
    if (r < 0) TL_THROW("sendto: getsockname failed: %s", strerror(errno));

    struct addrinfo* res;
    struct addrinfo hints;
    bzero(&hints, sizeof(struct addrinfo));
    hints.ai_family = addr.ss_family;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_V4MAPPED|AI_ADDRCONFIG;

    char sport[7];
    snprintf(sport, sizeof(sport), "%d", port);

    int error = getaddrinfo(tlStringData(host), sport, &hints, &res);
    if (error) TL_THROW("sendto: getaddrinfo failed: %s", gai_strerror(error));
    assert(res->ai_addr->sa_family == addr.ss_family);

    int len = tlBufferSize(buf);
    r = sendto(file->ev.fd, tlBufferData(buf), len, 0, res->ai_addr, res->ai_addrlen);
    freeaddrinfo(res);
    if (r < 0) {
        TL_THROW("sendto: failed: %s", strerror(errno));
    }
    assert(len == r);
    didread(buf, len);
    return tlINT(len);
}

static tlHandle _Socket_connect(tlTask* task, tlArgs* args) {
    tlString* address = tlStringCast(tlArgsGet(args, 0));
    if (!address) TL_THROW("expected a ip address");
    int port = tl_int_or(tlArgsGet(args, 1), -1);
    if (port < 0) TL_THROW("expected a port");

    char pstr[10];
    snprintf(pstr, sizeof(pstr), "%d", port);

    trace("tcp_connect: %s:%s", tl_str(address), pstr);

    struct addrinfo* res;
    struct addrinfo* rp;
    struct addrinfo hints;
    bzero(&hints, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_V4MAPPED|AI_ADDRCONFIG;

    int error = getaddrinfo(tlStringData(address), pstr, &hints, &res);
    if (error) TL_THROW("tcp_connect: getaddrinfo failed: %s", gai_strerror(error));

    int fd = -1;
    for (rp = res; rp != null; rp = rp->ai_next) {
        fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (fd < 0) continue;
        if (nonblock(fd) < 0) {
            close(fd);
            freeaddrinfo(res);
            TL_THROW("tcp_connect: nonblock failed: %s", strerror(errno));
        }
        int r = connect(fd, rp->ai_addr, rp->ai_addrlen);
        if (r < 0 && errno != EINPROGRESS) {
            close(fd);
            continue;
        }
    }
    freeaddrinfo(res);

    return tlFileNew(fd);
}

static tlHandle _Socket_connect_unix(tlTask* task, tlArgs* args) {
    tlString* address = tlStringCast(tlArgsGet(args, 0));
    if (!address) TL_THROW("expected a unix path");
    bool dgram = tl_bool(tlArgsGet(args, 1));

    trace("unix_open: %s", tl_str(address));

    struct sockaddr_un sockaddr;
    bzero(&sockaddr, sizeof(sockaddr));
    sockaddr.sun_family = AF_UNIX;

    if (tlStringSize(address) >= sizeof(sockaddr.sun_path)) TL_THROW("unix path too long, max=%zd", sizeof(sockaddr.sun_path));
    strncpy(sockaddr.sun_path, tlStringData(address), sizeof(sockaddr.sun_path) - 1);

    int fd = socket(AF_UNIX, dgram?SOCK_DGRAM:SOCK_STREAM, 0);
    if (fd < 0) TL_THROW("unix_connect: failed: %s", strerror(errno));

    if (nonblock(fd) < 0) {
        close(fd);
        TL_THROW("unix_connect: nonblock failed: %s", strerror(errno));
    }

#ifndef PLATFORM_MACOSX
    size_t len = sizeof(sockaddr.sun_family) + strlen(sockaddr.sun_path);
#else
    size_t len = sizeof(sockaddr) + strlen(sockaddr.sun_path);
#endif
    int r = connect(fd, (struct sockaddr *)&sockaddr, len);
    if (r < 0 && errno != EINPROGRESS) {
        close(fd);
        TL_THROW("unix_connect: connect failed: %s", strerror(errno));
    }

    if (errno == EINPROGRESS) trace("unix_connect: EINPROGRESS");
    return tlFileNew(fd);
}

static tlHandle _ServerSocket_listen(tlTask* task, tlArgs* args) {
    static tlSym s_host;
    if (!s_host) s_host = tlSYM("host");

    int port = tl_int_or(tlArgsGet(args, 0), 0);
    int backlog = tl_int_or(tlArgsGet(args, 1), 256);
    tlString* host = tlStringCast(tlArgsGetNamed(args, s_host));

    const char* shost = host? tlStringData(host) : null;

    trace("tcp_listen: %s:%d", shost, port);

    struct addrinfo hints;
    bzero(&hints, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    char sport[7];
    snprintf(sport, sizeof(sport), "%d", port);

    struct addrinfo* res;
    int error = getaddrinfo(shost, sport, &hints, &res);
    if (error) TL_THROW("tcp_listen: getaddrinfo failed: %s", gai_strerror(error));

    // TODO perhaps loop?
    int fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (fd < 0) {
        freeaddrinfo(res);
        TL_THROW("tcp_listen: failed: %s", strerror(errno));
    }

    if (nonblock(fd) < 0) {
        close(fd);
        freeaddrinfo(res);
        TL_THROW("tcp_listen: nonblock failed: %s", strerror(errno));
    }

    int flags = 1;
    int r = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (void *)&flags, sizeof(flags));
    if (r < 0) {
        close(fd);
        freeaddrinfo(res);
        TL_THROW("tcp_listen: so_reuseaddr failed: %s", strerror(errno));
    }

    r = bind(fd, res->ai_addr, res->ai_addrlen);
    if (r < 0) {
        close(fd);
        freeaddrinfo(res);
        TL_THROW("tcp_listen: bind failed: %s", strerror(errno));
    }

    freeaddrinfo(res);

    r = listen(fd, backlog);
    if (r < 0) {
        close(fd);
        TL_THROW("tcp_listen: listen failed: %s", strerror(errno));
    }

    return tlFileNew(fd);
}


// ** paths **

static tlHandle _Path_stat(tlTask* task, tlArgs* args) {
    tlString* name = tlStringCast(tlArgsGet(args, 0));
    if (!name) TL_THROW("expected a name");

    tlString* path = cwd_join(task, name);
    const char *p = tlStringData(path);
    if (p[0] != '/') TL_THROW("stat: invalid cwd");

    struct stat buf;
    int r = stat(p, &buf);
    if (r == -1) {
        if (errno == ENOENT || errno == ENOTDIR) {
            bzero(&buf, sizeof(buf));
        } else {
            TL_THROW("stat failed: %s for: '%s'", strerror(errno), tlStringData(name));
        }
    }

    tlObject *res = tlClone(_statMap);
    tlObjectSet_(res, _s_dev, tlNUM(buf.st_dev));
    tlObjectSet_(res, _s_ino, tlNUM(buf.st_ino));
    tlObjectSet_(res, _s_mode, tlNUM(buf.st_mode));
    tlObjectSet_(res, _s_nlink, tlNUM(buf.st_nlink));
    tlObjectSet_(res, _s_uid, tlNUM(buf.st_uid));
    tlObjectSet_(res, _s_gid, tlNUM(buf.st_gid));
    tlObjectSet_(res, _s_rdev, tlNUM(buf.st_rdev));
    tlObjectSet_(res, _s_size, tlNUM(buf.st_size));
    tlObjectSet_(res, _s_blksize, tlNUM(buf.st_blksize));
    tlObjectSet_(res, _s_blocks, tlNUM(buf.st_blocks));
    tlObjectSet_(res, _s_atime, tlNUM(buf.st_atime));
    tlObjectSet_(res, _s_mtime, tlNUM(buf.st_mtime));
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
tlKind* tlDirKind;

static tlDir* tlDirNew(DIR* p) {
    tlDir* dir = tlAlloc(tlDirKind, sizeof(tlDir));
    dir->p = p;
    return dir;
}

static tlHandle _Dir_open(tlTask* task, tlArgs* args) {
    tlString* name = tlStringCast(tlArgsGet(args, 0));
    trace("opendir: %s", tl_str(name));

    tlString* path = cwd_join(task, name);
    const char *p = tlStringData(path);
    if (p[0] != '/') TL_THROW("opendir: invalid cwd");

    DIR *dir = opendir(p);
    if (!dir) TL_THROW("opendir: failed: %s for: '%s'", strerror(errno), tl_str(name));
    return tlDirNew(dir);
}

static tlHandle _dir_close(tlTask* task, tlArgs* args) {
    tlDir* dir = tlDirAs(tlArgsTarget(args));
    trace("closedir: %p", dir);
    if (closedir(dir->p)) TL_THROW("closedir: failed: %s", strerror(errno));
    return tlNull;
}

static tlHandle _dir_read(tlTask* task, tlArgs* args) {
    tlDir* dir = tlDirAs(tlArgsTarget(args));

    struct dirent dp;
    struct dirent *dpp;
    if (readdir_r(dir->p, &dp, &dpp)) TL_THROW("readdir: failed: %s", strerror(errno));
    trace("readdir: %p", dpp);
    if (!dpp) return tlNull;
    return tlStringFromCopy(dp.d_name, 0);
}

typedef struct tlDirEachFrame {
    tlFrame frame;
    tlDir* dir;
    tlHandle* block;
} tlDirEachFrame;

static tlHandle resumeDirEach(tlTask* task, tlFrame* _frame, tlHandle value, tlHandle error) {
    if (error == s_continue) {
        tlTaskClearError(task, tlNull);
        tlTaskPushFrame(task, _frame);
        return null;
    }
    if (error == s_break) return tlTaskClearError(task, tlNull);
    if (!value) return null;

    tlDirEachFrame* frame = (tlDirEachFrame*)_frame;
again:;
    struct dirent dp;
    struct dirent *dpp;
    if (readdir_r(frame->dir->p, &dp, &dpp)) TL_THROW("readdir: failed: %s", strerror(errno));
    trace("readdir: %p", dpp);
    if (!dpp) {
        tlTaskPopFrame(task, _frame);
        return tlNull;
    }
    tlHandle res = tlEval(task, tlCallFrom(frame->block, tlStringFromCopy(dp.d_name, 0), null));
    if (!res) return null;
    goto again;

    fatal("not reached");
    return tlNull;
}

static tlHandle _dir_each(tlTask* task, tlArgs* args) {
    tlDir* dir = tlDirAs(tlArgsTarget(args));
    tlHandle* block = tlArgsBlock(args);
    if (!block) return tlNull;

    tlDirEachFrame* frame = tlFrameAlloc(resumeDirEach, sizeof(tlDirEachFrame));
    frame->dir = dir;
    frame->block = block;
    tlTaskPushFrame(task, (tlFrame*)frame);
    return resumeDirEach(task, (tlHandle)frame, tlNull, null);
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
static tlHandle _io_exec(tlTask* task, tlArgs* args) {
    char** argv = malloc(sizeof(char*) * (tlArgsSize(args) + 1));
    for (int i = 0; i < tlArgsSize(args); i++) {
        tlString* str = tlStringCast(tlArgsGet(args, i));
        if (!str) {
            free(argv);
            TL_THROW("expected a String");
        }
        argv[i] = (char*)tlStringData(str);
    }
    argv[tlArgsSize(args)] = 0;

    tlString* path = cwd_join(task, null);
    const char *p = tlStringData(path);
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
    tlHandle res; // the result code
    tlHandle in;
    tlHandle out;
    tlHandle err;
};
tlKind _tlChildKind = {
    .name = "Child",
    .locked = true,
};
tlKind* tlChildKind;

static tlChild* tlChildFrom(ev_child *ev) {
    return tlChildAs(((char*)ev) - ((unsigned long)&((tlChild*)0)->ev));
}

static void child_cb(ev_child *ev, int revents) {
    tlChild* child = tlChildFrom(ev);
    trace("%p", ev);
    ev_child_stop(ev);
    child->res = tlINT(WEXITSTATUS(child->ev.rstatus));

    while (true) {
        tlTask* task = tlTaskFromEntry(lqueue_get(&child->wait_q));
        if (!task) return;
        task->value = child->res;
        tlTaskReadyExternal(task);
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

static tlHandle _child_wait(tlTask* task, tlArgs* args) {
    tlChild* child = tlChildAs(tlArgsTarget(args));
    trace("child_wait: %d", child->ev.pid);

    // TODO use a_var and this will be thread safe ... (child_cb)
    if (child->res) return child->res;

    tlTaskWaitExternal(task);
    lqueue_put(&child->wait_q, &task->entry);
    return null;
}

static tlHandle _child_running(tlTask* task, tlArgs* args) {
    tlChild* child = tlChildAs(tlArgsTarget(args));
    return tlBOOL(child->res == 0);
}
static tlHandle _child_in(tlTask* task, tlArgs* args) {
    tlChild* child = tlChildAs(tlArgsTarget(args));
    return child->in;
}
static tlHandle _child_out(tlTask* task, tlArgs* args) {
    tlChild* child = tlChildAs(tlArgsTarget(args));
    return child->out;
}
static tlHandle _child_err(tlTask* task, tlArgs* args) {
    tlChild* child = tlChildAs(tlArgsTarget(args));
    return child->err;
}

// launch a child process
// TODO allow passing in an tlFile as in/out/err ...
static tlHandle _io_launch(tlTask* task, tlArgs* args) {
    static int null_fd;
    if (!null_fd) {
        null_fd = open("/dev/null", O_RDWR);
        if (null_fd < 0) fatal("unable to open(/dev/null): %s", strerror(errno));
    }
    tlString* cwd = tlStringCast(tlArgsGet(args, 0));
    if (!cwd) TL_THROW("expected a String as cwd");
    tlList* as = tlListCast(tlArgsGet(args, 1));
    if (!as) TL_THROW("expected a List");
    char** argv = malloc(sizeof(char*) * (tlListSize(as) + 1));
    for (int i = 0; i < tlListSize(as); i++) {
        tlString* str = tlStringCast(tlListGet(as, i));
        if (!str) TL_THROW("expected a String as arg");
        argv[i] = (char*)tlStringData(str);
    }
    argv[tlListSize(as)] = 0;

    bool want_in = tl_bool_or(tlArgsGet(args, 2), true);
    bool want_out = tl_bool_or(tlArgsGet(args, 3), true);
    bool want_err = tl_bool_or(tlArgsGet(args, 4), true);
    bool join_err = tl_bool_or(tlArgsGet(args, 5), false);
    trace("launch: %s [%d] %d,%d,%d,%d", argv[0], tlListSize(as) - 1,
            want_in, want_out, want_err, join_err);

    tlHandle henv = tlArgsGet(args, 6);
    if (tl_bool(henv) && !tlObjectIs(henv)) TL_THROW("environment must be a map");
    tlObject* env = tlObjectCast(henv);
    if (env) for (int i = 0;; i++) {
        tlHandle val = tlObjectValueIter(env, i);
        if (!val) break;
        if (!tlStringIs(val)) TL_THROW("expected only Strings in the environment map");
    }
    bool reset_env = tl_bool_or(tlArgsGet(args, 7), false);

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
    if (!errno) {
        pid = fork();
        if (pid == 0) errno = 0; // clear errno in child, if success, osx seems to set it to invalid arguments
    }
    if (errno) {
        TL_THROW_SET("child exec: failed: %s", strerror(errno));
        close(_in[0]); close(_in[1]);
        close(_out[0]); close(_out[1]);
        close(_err[0]); close(_err[1]);
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
#ifdef HAVE_BOEHMGC
    // stop gc in child process, we don't need it and it is fragile on OSX
    GC_disable();
#endif

    // child
    dup2(_in[0], 0);
    dup2(_out[1], 1);
    dup2(_err[1], 2);

    // set new environment, if any
    if (reset_env) {
        // TODO how?
    }
    if (env) for (int i = 0;; i++) {
        tlString* str = tlStringCast(tlObjectValueIter(env, i));
        if (!str) break;
        tlSym key = tlSetGet(tlObjectKeys(env), i);
        setenv(tlSymData(key), tlStringData(str), 1);
    }

    launch(tlStringData(cwd), argv);
    // cannot happen, launch does exit
    TL_THROW("oeps: %s", strerror(errno));
}

static const char* filetoString(tlHandle v, char* buf, int size) {
    tlFile* file = tlFileAs(v);
    tlKind* kind = tl_kind(file);
    snprintf(buf, size, "<%s@%d>", kind->name, file->ev.fd);
    return buf;
}


// ** newstyle io, where languages controls all */

static void io_cb(ev_io *ev, int revents) {
    trace("io_cb: %p %d", ev, revents);
    assert(ev->fd >= 0);
    tlFile* file = tlFileFromEv(ev);
    if (revents & EV_READ) {
        trace("CANREAD: %d", ev->fd);
        assert(file->reader);
        assert(file->reader->lock.owner);
        tlTask* task = file->reader->lock.owner;
        tlMessage* msg = tlMessageAs(file->reader->lock.owner->value);
        tlVm* vm = tlTaskGetVm(file->reader->lock.owner);
        if (!task->background) a_dec(&vm->waitevent);
        ev->events &= ~EV_READ;
        tlMessageReply(msg, null);
    }
    if (revents & EV_WRITE) {
        trace("CANWRITE: %d", ev->fd);
        assert(file->writer);
        assert(file->writer->lock.owner);
        tlTask* task = file->writer->lock.owner;
        tlMessage* msg = tlMessageAs(file->writer->lock.owner->value);
        tlVm* vm = tlTaskGetVm(tlMessageGetSender(msg));
        if (!task->background) a_dec(&vm->waitevent);
        ev->events &= ~EV_WRITE;
        tlMessageReply(msg, null);
    }
    if (!ev->events) ev_io_stop(ev);
}

static tlHandle _io_close(tlTask* task, tlArgs* args) {
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
        tlTask* task = file->reader->lock.owner;
        tlVm* vm = tlVmCurrent(task);
        if (!task->background) a_dec(&vm->waitevent);
        file->ev.events &= ~EV_READ;
        tlMessageReply(msg, null);
    }
    if (file->ev.events & EV_WRITE) {
        trace("CLOSED WITH WRITER");
        assert(file->writer);
        assert(file->writer->lock.owner);
        tlMessage* msg = tlMessageAs(file->writer->lock.owner->value);
        tlTask* task = file->writer->lock.owner;
        tlVm* vm = tlVmCurrent(task);
        if (!task->background) a_dec(&vm->waitevent);
        file->ev.events &= ~EV_WRITE;
        tlMessageReply(msg, null);
    }
    return tlNull;
}

static tlHandle _io_waitread(tlTask* task, tlArgs* args) {
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

    if (file->ev.fd < 0) TL_THROW("file is closed");

    file->ev.events |= EV_READ;
    ev_io_start(&file->ev);
    if (!sender->background) a_inc(&vm->waitevent);

    return tlNull;
}

static tlHandle _io_waitwrite(tlTask* task, tlArgs* args) {
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

    if (file->ev.fd < 0) TL_THROW("file is closed");

    file->ev.events |= EV_WRITE;
    ev_io_start(&file->ev);
    if (!sender->background) a_inc(&vm->waitevent);

    return tlNull;
}

static void timer_cb(ev_timer* timer, int revents) {
    trace("timer_cb: %p", timer);
    ev_timer_stop(timer);
    tlMessage* msg = tlMessageAs(timer->data);
    tlTask* sender = tlMessageGetSender(msg);
    tlVm* vm = tlVmCurrent(sender);
    if (!sender->background) a_dec(&vm->waitevent);
    tlMessageReply(msg, null);
    free(timer);
}

static tlHandle _io_wait(tlTask* task, tlArgs* args) {
    tlMessage* msg = tlMessageCast(tlArgsGet(args, 1));
    if (!msg) TL_THROW("expect a Message");

    double s = tl_double_or(tlArgsGet(args, 0), 1);
    trace("sleep: %f", s);

    ev_timer *timer = malloc(sizeof(ev_timer));
    timer->data = msg;
    ev_timer_init(timer, timer_cb, s, 0);
    ev_timer_start(timer);

    tlTask* sender = tlMessageGetSender(msg);
    tlVm* vm = tlVmCurrent(sender);
    if (!sender->background) a_inc(&vm->waitevent);

    return tlNull;
}

static ev_async loop_interrupt;
static void async_cb(ev_async* async, int revents) { }

static void iointerrupt() { ev_async_send(&loop_interrupt); }

static tlHandle _io_init(tlTask* task, tlArgs* args) {
    tlMsgQueue* queue = tlMsgQueueNew();
    queue->signalcb = iointerrupt;
    return queue;
}

// block current task, until io happens, will return tlTrue when there is nothing left to do
static tlHandle _io_block(tlTask* task, tlArgs* args) {
    tlMsgQueue* queue = tlMsgQueueCast(tlArgsGet(args, 0));
    if (!queue) TL_THROW("require the queue from init");

    tlVm* vm = tlTaskGetVm(task);
    intptr_t running = a_get(&vm->runnable);
    intptr_t waiting = a_get(&vm->waitevent);
    trace("block for io: events=%s, lock=%s, runnable=%zd, waitevent=%zd", tlMsgQueueIsEmpty(queue)? "no":"yes", vm->lock? "yes":"no", running, waiting);

    // poll the msg queue if events are pending
    if (!tlMsgQueueIsEmpty(queue)) return tlFalse;

    // if the io task is the only runnable task, and no task is waiting for io, nothing can happen for this vm, so signal to exit
    if (running <= 1 && waiting == 0) return tlTrue;

    // if multithreaded, one thread will block and wait for events
    if (vm->lock) {
        trace("blocking for events");
        ev_run(EVRUN_ONCE);
        return tlFalse;
    }

    if (running > 1) {
        trace("checking for events; tasks=%zd, run=%zd, io=%zd", vm->tasks, vm->runnable, vm->waitevent);
        ev_run(EVRUN_NOWAIT);
        return tlFalse;
    }

    trace("blocking for events; tasks=%zd, run=%zd, io=%zd", vm->tasks, vm->runnable, vm->waitevent);
    ev_run(EVRUN_ONCE);
    return tlFalse;
}


// ** crude terminal handling **
// TODO should support many ttys :)

static struct termios tty_orig;
static int tty_fd = - 1;
static void tty_restore() {
    if (tty_fd != -1) {
        setblock(tty_fd);
        tcsetattr(tty_fd, TCSAFLUSH, &tty_orig);
        tty_fd = -1;
        trace("tty restored: %d", tty_fd);
    }
}
static tlHandle _tty_is(tlTask* task, tlArgs* args) {
    tlFile* file = tlFileCast(tlArgsGet(args, 0));
    if (!file) TL_THROW("expect a File");
    if (!isatty(file->ev.fd)) return tlFalse;
    return tlTrue;
}

static tlHandle _tty_setRaw(tlTask* task, tlArgs* args) {
    tlFile* file = tlFileCast(tlArgsGet(args, 0));
    if (!file) TL_THROW("expect a File");
    int fd = file->ev.fd;
    if (!isatty(fd)) return tlFalse;

    struct termios raw = {0};
    if (tcgetattr(fd, &tty_orig) == -1) goto fatal;

    raw = tty_orig;
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 1; raw.c_cc[VTIME] = 0;
    if (tcsetattr(fd, TCSAFLUSH, &raw) < 0) goto fatal;

    if (tty_fd == -1) {
        tty_fd = fd;
        nonblock(fd);
        atexit(tty_restore);
    }
    return tlTrue;

fatal:
    return tlFalse;
}

static tlHandle _tty_setup(tlTask* task, tlArgs* args) {
    tlFile* file = tlFileCast(tlArgsGet(args, 0));
    if (!file) TL_THROW("expect a File");
    int fd = file->ev.fd;

    int rate = tl_int_or(tlArgsGet(args, 1), 19200);
    int baud = B19200;
    switch (rate) {
        case  1200: baud = B1200; break;
        case  2400: baud = B2400; break;
        case  4800: baud = B4800; break;
        case  9600: baud = B9600; break;
        case 19200: baud = B19200; break;
        case 38400: baud = B38400; break;
        default: TL_THROW("not a valid baud rate");
    }
    struct termios tty = {0};

    int bytesize = tl_int_or(tlArgsGet(args, 2), 8);
    switch (bytesize) {
        case 5: tty.c_cflag = CS5; break;
        case 6: tty.c_cflag = CS6; break;
        case 7: tty.c_cflag = CS7; break;
        case 8: tty.c_cflag = CS8; break;
        default: TL_THROW("not a valid byte size");
    }

    // TODO parity and stop bits

    cfsetospeed(&tty, baud);
    cfsetispeed(&tty, baud);
    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 0;
    if (tcsetattr(fd, TCSANOW, &tty) < 0) goto fatal;
    return tlTrue;

fatal:
    return tlFalse;
}

static tlHandle _tty_restore(tlTask* task, tlArgs* args) {
    tlFile* file = tlFileCast(tlArgsGet(args, 0));
    if (!file) TL_THROW("expect a File");
    int fd = file->ev.fd;
    if (!isatty(fd)) return tlFalse;

    if (fd == tty_fd) tty_restore();
    return tlNull;
}
static tlHandle _tty_size(tlTask* task, tlArgs* args) {
    tlFile* file = tlFileCast(tlArgsGet(args, 0));
    if (!file) TL_THROW("expect a File");
    int fd = file->ev.fd;
    if (!isatty(fd)) return tlFalse;

    struct winsize ws;
    if (ioctl(fd, TIOCGWINSZ, &ws) == -1) {
        ws.ws_col = 80; ws.ws_row = 24;
    }
    return tlResultFrom(tlINT(ws.ws_col), tlINT(ws.ws_row), null);
}

static const tlNativeCbs __evio_natives[] = {
    { "_io_getrusage", _io_getrusage },
    { "_io_getenv", _io_getenv },
    { "_io_getpid", _io_getpid },
    { "_io_procname", _io_procname },
    { "_io_gethostname", _io_gethostname },
    { "_io_getdomainname", _io_getdomainname },

    { "_io_chdir", _io_chdir },
    { "_io_mkdir", _io_mkdir },
    { "_io_rmdir", _io_rmdir },
    { "_io_rename", _io_rename },
    { "_io_unlink", _io_unlink },
    { "_io_readlink", _io_readlink },

    { "_File_open", _File_open },
    { "_File_from", _File_from },
    { "_Socket_udp", _Socket_udp },
    { "_Socket_sendto", _Socket_sendto },
    { "_Socket_recvfrom", _Socket_recvfrom },
    { "_Socket_connect", _Socket_connect },
    { "_Socket_connect_unix", _Socket_connect_unix },
    { "_Socket_resolve", _Socket_resolve },
    { "_ServerSocket_listen", _ServerSocket_listen },
    { "_Path_stat", _Path_stat },
    { "_Dir_open", _Dir_open },

    { "_tty_is", _tty_is },
    { "_tty_setRaw", _tty_setRaw },
    { "_tty_setup", _tty_setup },
    { "_tty_restore", _tty_restore },
    { "_tty_size", _tty_size },

    { "_io_exec", _io_exec },
    { "_io_launch", _io_launch },

    { "_io_wait", _io_wait },
    { "_io_close", _io_close },
    { "_io_waitread", _io_waitread },
    { "_io_waitwrite", _io_waitwrite },

    { "_io_block", _io_block },
    { "_io_init", _io_init },
    { 0, 0 }
};

void evio_init() {
    _tlReaderKind.klass = tlClassObjectFrom(
        "read", _reader_read,
        "accept", _reader_accept,
        "isClosed", _reader_isClosed,
        "close", _reader_close,
        null
    );
    _tlWriterKind.klass = tlClassObjectFrom(
        "write", _writer_write,
        "isClosed", _writer_isClosed,
        "close", _writer_close,
        null
    );
    _tlFileKind.toString = filetoString;
    _tlFileKind.klass = tlClassObjectFrom(
        "port", _file_port,
        "ip", _file_ip,
        "peer_port", _file_peer_port,
        "peer_ip", _file_peer_ip,
        "isClosed", _file_isClosed,
        "close", _file_close,
        "reader", _file_reader,
        "writer", _file_writer,
        null
    );
    _tlDirKind.klass = tlClassObjectFrom(
        "read", _dir_read,
        "each", _dir_each,
        null
    );
    _tlChildKind.klass = tlClassObjectFrom(
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
    _statMap = tlObjectNew(keys);

    // for rusage syscall
    keys = tlSetNew(5);
    _s_cpu = tlSYM("cpu"); tlSetAdd_(keys, _s_cpu);
    _s_mem = tlSYM("mem"); tlSetAdd_(keys, _s_mem);
    _s_pid = tlSYM("pid"); tlSetAdd_(keys, _s_pid);
    _s_gcs = tlSYM("gcs"); tlSetAdd_(keys, _s_gcs);
    _s_gcmem = tlSYM("gcmem"); tlSetAdd_(keys, _s_gcmem);
    _usageMap = tlObjectNew(keys);

    char buf[MAXPATHLEN + 1];
    char* cwd = getcwd(buf, sizeof(buf));
    tl_cwd = tlStringFromCopy(cwd, 0);

    signal(SIGPIPE, SIG_IGN);

    ev_default_loop(0);
    ev_async_init(&loop_interrupt, async_cb);
    ev_async_start(&loop_interrupt);

    // TODO this is here as a "test"
    ev_async_send(&loop_interrupt);
    ev_run(EVRUN_NOWAIT);

    INIT_KIND(tlReaderKind);
    INIT_KIND(tlWriterKind);
    INIT_KIND(tlFileKind);
    INIT_KIND(tlDirKind);
    INIT_KIND(tlChildKind);
}

void evio_vm_default(tlVm* vm) {
    vm->signalcb = iointerrupt;
}

