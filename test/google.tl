buf = http.get "http://google.com"
text = buf.read
page = html.parse text
page.find("A").text
