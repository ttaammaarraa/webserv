#!/usr/bin/env python3

import sys

data = sys.stdin.read()

print("Content-Type: text/html")
print()
print("<h1>" + data + "</h1>")