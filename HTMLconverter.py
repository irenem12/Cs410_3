#!/usr/bin/env python3

import sys
content = str(sys.argv[1])
page = open('convertedHtml.html', 'wb')

loadpart = "<html><head><title>html converter</title></head><body>"
loadpart += "<p style='color:blue;'><strong>" + content + "<strong></p>"
loadpart = loadpart + "</body></html>"
loadpart = loadpart.encode()

page.write(loadpart)
page.close()

with open("convertedHtml.html", 'rb') as html:
    print(html.read())
