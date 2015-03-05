DEFAULT_LEVEL = 1
levels = [TRACE=0,DEBUG=1,INFO=2,NOTICE=3,WARNING=3,ERROR=5,FATAL=6]

# global mutable state ...
var loglevel = DEFAULT_LEVEL
filelevels = HashMap.new

loggertime = ->
    x = time()
    millis = ((x - x.floor) * 1000).round
    return strftime("%Y-%m-%d %H:%M:%S.$millis", x)

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
    time = loggertime()
    usercontext = Task.locals?logcontext
    proc = Task.locals?procname or io.procname
    context = { usercontext }: "[$(proc):$(io.pid):$(Task.id):$(usercontext)]"
              {             }: "[$(proc):$(io.pid):$(Task.id)]"
    print time, context, levelname, "-", msg, "-", "$file:$line", function

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
    warn = -> outputlog(4, "WARNING", args)
    error = -> outputlog(5, "ERROR", args)
}

