#!/usr/bin/env python3
import os

print("Content-Type: text/html\r")
print("\r")
print("<html><body>")
print("<h1>CGI Works!</h1>")
print(f"<p>Method: {os.environ.get('REQUEST_METHOD', 'unknown')}</p>")
print(f"<p>Path: {os.environ.get('REQUEST_URI', 'unknown')}</p>")
print("</body></html>")
