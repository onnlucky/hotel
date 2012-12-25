db = Persist.new("test.db")
series = db.List.get("series")
if series.size < 10: {
    series.add db.Object.new { name: "The Mentalist", season: 5, episode: 11 }
}
series.each: { s ->
    print "FOUND:", s[#name], s[#season], s[#episode]
}

