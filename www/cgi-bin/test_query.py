#!/usr/bin/env python3
import cgi

# Print required HTTP headers for CGI
print("Content-Type: text/html\n")

# Parse the fields
form = cgi.FieldStorage()

# Fetch specific parameters
# Example URL: test.py?name=Alice&age=30
name = form.getvalue('name', 'Guest')
age = form.getvalue('age', 'Unknown')

print(f"<h1>Hello, {name}!</h1>")
print(f"<p>Age: {age}</p>")
