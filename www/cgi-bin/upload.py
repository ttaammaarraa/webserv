#!/usr/bin/env python3
import os
import sys
import cgi

upload_dir = "./uploads"

if not os.path.exists(upload_dir):
    os.makedirs(upload_dir)

form = cgi.FieldStorage()

if "file" in form:
    fileitem = form["file"]
    if fileitem.filename:
        fn = os.path.basename(fileitem.filename)
        with open(os.path.join(upload_dir, fn), 'wb') as f:
            f.write(fileitem.file.read())
        print("Status: 201 Created\r\n\r\n")
        print(f"File '{fn}' uploaded successfully.")
    else:
        print("Status: 400 Bad Request\r\n\r\n")
else:
    print("Status: 400 Bad Request\r\n\r\n")
