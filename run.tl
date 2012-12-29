db = Persist.new("test.db")
series = db.List.get("series")
if series.size < 1: {
    series.add db.Object.new { name: "The Mentalist", season: 5, episode: 11 }
}
listall = { ->
    print "----"
    series.each: s -> print "FOUND:", s.name, s.season, s.episode
    print "----"
}
listall()

apiList = { conn ->
    conn.write "["
    series.each: { s ->
        conn.write "{'name':'$(s.name)','season':'$(s.season)','episode':'$(s.episode)'}"
    }
    conn.write "]"
}
apiGet = { conn, query ->
    serie = series.find: e -> e.name == query["name"]
    if not serie: conn.write "null"; return
    conn.write "{'name':'$(s.name)','season':'$(s.season)','episode':'$(s.episode)'}"
}
apiAdd = { conn, query ->
    print "add:", query["name"]
    serie = series.find: e -> e.name == query["name"]
    if serie: return conn.write "{'ok':false}"
    series.add db.Object.new {
        name: query["name"]
        season: query["season"].eval
        episode: query["episode"].eval
    }
    conn.write "{'ok':true}"
    listall()
}
apiSet = { conn, query ->
    print "set:", query["name"]
    serie = series.find: e -> e.name == query["name"]
    if not serie: return conn.write "{'ok':false'}"
    query.each: { k, v ->
        if k == "cmd" or k == "name": continue
        serie._set(k.toSym, v)
    }
    conn.write "{'ok':true}"
    listall()
}
apiRemove = { conn, query ->
    print "remove:", query["name"]
    serie, at = series.find: e -> e.name == query["name"]
    if at: series.remove(at)
    conn.write "{'ok':true}"
    listall()
}
apiCheck = { conn, query ->
    print "check:", query["name"]
    serie = series.find: e -> e.name == query["name"]
    if not serie: return conn.write "{'ok':false'}"
    // update(serie)
    conn.write "{'ok':true,'season':serie.season,'episode':serie.episode}"
}

http.Server.new(8888).serve: { conn ->
    path = conn.req.url.path
    if path.startsWith("/api"): {
        conn.contentType = "text/json"
        query = conn.req.url.query
        cmd = query["cmd"]
        if cmd == "list": return apiList(conn)
        if cmd == "get": return apiGet(conn, query)
        if cmd == "add": return apiAdd(conn, query)
        if cmd == "set": return apiSet(conn, query)
        if cmd == "remove": return apiRemove(conn, query)
        if cmd == "check": return apiCheck(conn, query)
        conn.status = 400
        conn.write "{'error':true}";
        return
    }
    if io.Path(path).isFile: {
        conn.write io.File(path).read
        return
    }
    conn.status = 404
    conn.contentType = "text/html"
    conn.write "<p>error</p>"
}

