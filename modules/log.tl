DEFAULT_LEVEL = 1
levels = [TRACE=0,DEBUG=1,INFO=2,NOTICE=3,WARNING=3,ERROR=5,FATAL=6]

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

# TODO output to syslog

outputlog = level, levelname, as ->
    file, function, line = Task.stacktrace(2)[1]
    flevel = filelevels[file]
    tlevel = levels[Task.locals?loglevel]
    low = { tlevel and flevel }: min(tlevel, flevel)
          { tlevel            }: tlevel
          { flevel            }: flevel
          {                   }: loglevel
    if level < low: return

    msg = as.map(toString).join(" ")
    time, ts = loggertime()
    host = hostname(ts)
    app = Task.locals?procname or io.procname

    usercontext = Task.locals?logcontext
    context = { usercontext }: "[$(io.pid):$(Task.id):$(usercontext)]"
              {             }: "[$(io.pid):$(Task.id)]"
    print time, host, app, context, "-", levelname, msg, "-", "$file:$line", function

levelToNumber = level ->
    levels[level] or Error("unknown level '$level'").throw

{
    setGlobalLevel = level ->
        loglevel = levelToNumber(level)

    setLevel = level ->
        n = levelToNumber(level)
        file = { args["file"] }: args["file"]
               {              }: Task.stacktrace(2)[1]
        filelevels[file] = n

    setTaskLevel = level ->
        levelToNumber(level)
        Task.setLocals(Object.set(Task.locals, {loglevel=level}))

    trace = -> outputlog(0, "TRACE", args)
    debug = -> outputlog(1, "DEBUG", args)
    info = -> outputlog(2, "INFO", args)
    notice = -> outputlog(3, "NOTICE", args)
    warning = -> outputlog(4, "WARNING", args)
    error = -> outputlog(5, "ERROR", args)
}

