page = html.parse("<p>he<span class=xxx>ll</span>o</p><p>world</p>")
assert page.text == "helloworld"
assert page.find("p").text == "hello"
page.findAll("p span.xxx").each: n -> assert n.text == "ll"
assert page.find("p span.xxx").text == "ll"

text = http.get("http://google.com")
page = html.parse(text)
assert page.find("a").text == "here"
