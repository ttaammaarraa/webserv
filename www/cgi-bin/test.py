#!/usr/bin/python3
import sys
import os

content_length = int(os.environ.get('CONTENT_LENGTH', 0))
data = sys.stdin.read(content_length)

print("Content-Type: text/html\r\n\r\n")
print("<html><body>")
print("<h1>CGI Script is Working!</h1>")
print("<p>Received Data: " + data + "</p>")
print("</body></html>")
