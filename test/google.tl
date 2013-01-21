page = html.parse "<p>he<span class=xxx>ll</span>o</p><p>world</p>"
assert page.text == "helloworld"
assert page.find("p").text == "hello"
page.findAll("p span.xxx").each: n -> assert n.text == "ll"
page.find("p span.xxx").text == "ll"

buf = http.get "http://google.com"
text = buf.read
page = html.parse text
page.find("A").text
