DEFAULT_LEVEL = 7
levels = [TRACE=8,DEBUG=7,INFO=6,NOTICE=5,WARNING=4,ERROR=3,FATAL=2]

# global mutable state ...
var loglevel = DEFAULT_LEVEL
filelevels = HashMap.new

# to get and cache the hostname
# TODO use uname syscall instead, and muck with domains?
var _hostname_set = 0
var _hostname = null
hostname = ts ->
    # we cache a hostname for a minute
    if _hostname and _hostname_set > ts - 60: return _hostname
    _hostname_set = time
    _hostname = try(io.run("uname", "-n"), "-").trim

loggertime = ->
    x = time()
    ts = x.floor
    millis = ((x - ts) * 1000 + 1_000).round.toString[2:] # trick to format string as 001
    return strftime("%Y-%m-%dT%H:%M:%S.$millis%z", localtime(x)), ts

var facility = 16 # local0
var rfc5424 = false
var logsocket = null
sockets = { system.platform == "macosx" }: ["/var/run/syslog", "/dev/log", "/var/run/log"]
          {                             }: ["/dev/log", "/var/run/syslog", "/var/run/log"]

outputlog = level, levelname, as ->
    file, function, line = Task.stacktrace(2)[1]
    flevel = filelevels[file]
    tlevel = levels[Task.locals?loglevel]
    high = { tlevel and flevel }: max(tlevel, flevel)
           { tlevel            }: tlevel
           { flevel            }: flevel
           {                   }: loglevel
    if level > high: return

    msg = as.map(toString).join(" ")
    time, ts = loggertime()
    host = hostname(ts)
    app = Task.locals?procname or io.procname

    usercontext = Task.locals?logcontext
    context = { usercontext }: "[$(io.pid):$(Task.id):$(usercontext)]"
              {             }: "[$(io.pid):$(Task.id)]"

    if logsocket and rfc5424:
        pri = "<$(facility * 8 + min(7, level))>1 "
        logsocket.write pri, time, " ", host, " ", app, " ", context, " - - ", msg, " - $file:$line $function"
        return
    if logsocket:
        pri = "<$(facility * 8 + min(7, level))>"
        logsocket.write pri, context, " ", app, ": ", msg, " - $file:$line $function"
        return
    print time, host, app, context, "-", levelname, msg, "-", "$file:$line", function

opensyslog = fac, rfc ->
    var firsterror = null
    if not logsocket:
        sockets.each: sock ->
            catch: e -> firsterror = e
            logsocket = io.Socket.unix(sock, true)
            facility = fac or 16
            rfc5424 = rfc
            return
    outputlog(4, "WARNING", ["unable to open syslog socket, tried:", repr(sockets), "reason:", firsterror])

levelToNumber = level ->
    levels[level] or Error("unknown level '$level'").throw

#. object log
{
    #. setOutput(backend, facility?): switch logger backend, either "syslog" or "stdout" (or "stderr", in the future)
    setOutput = backend, facility, rfc5424 ->
        { backend == "syslog" }: opensyslog(facility, rfc5424)
        {                     }: closesyslog()

    #. setGlobalLevel(level): set a default global log level, messages below this priority will be ignored, unless locally overridden by file or task
    setGlobalLevel = level ->
        loglevel = levelToNumber(level)

    #. setLevel(level): set a default level for the current module (filename based)
    #. [file]: if explicitly given, set level for this file
    setLevel = level ->
        n = levelToNumber(level)
        file = { args["file"] }: args["file"]
               {              }: Task.stacktrace(2)[1]
        filelevels[file] = n

    #. setTaskLevel(level): set a default level for the current task
    #. if both a module level and task level are set, the lowest log level is used
    #. e.g. if module level is INFO and task level is TRACE, logging will happen at TRACE
    setTaskLevel = level ->
        levelToNumber(level)
        Task.setLocals(Object.set(Task.locals, {loglevel=level}))

    trace = -> outputlog(8, "TRACE", args)
    debug = -> outputlog(7, "DEBUG", args)
    info = -> outputlog(6, "INFO", args)
    notice = -> outputlog(5, "NOTICE", args)
    warning = -> outputlog(4, "WARNING", args)
    error = -> outputlog(3, "ERROR", args)
    fatal = -> outputlog(2, "FATAL", args) # TODO actually kill the system here? or the task?
}

