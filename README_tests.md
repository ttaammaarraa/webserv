## Test Cases

Start the server before running any test:
```bash
./webserv basic.conf
```

---

### GET — Static file

```bash
curl -v http://localhost:8080/index.html
```
**Expected:**
```
HTTP/1.1 200 OK
Content-Type: text/html
Content-Length: <size>
```

---

### GET — File not found

```bash
curl -v http://localhost:8080/doesnotexist.html
```
**Expected:**
```
HTTP/1.1 404 Not Found
```

---

### GET — Path traversal blocked

```bash
# Path traversal blocked
printf "GET /../../../etc/passwd HTTP/1.1\r\nHost: localhost:8080\r\n\r\n" | nc localhost 8080
```
**Expected:**
```
HTTP/1.1 403 Forbidden
```
Note: curl normalises the URL before sending so the server receives `/etc/passwd` — the `..` check blocks it.

---

### GET — Directory without trailing slash redirects

```bash
curl -v http://localhost:8080/files
```
**Expected:**
```
HTTP/1.1 301 Moved Permanently
Location: /files/
```

---

### GET — Autoindex directory listing

```bash
curl -v http://localhost:8080/files/
```
**Expected:**
```
HTTP/1.1 200 OK
Content-Type: text/html
<body contains directory listing>
```

---

### HEAD — Headers only, no body

```bash
curl -v -I http://localhost:8080/index.html
```
**Expected:**
```
HTTP/1.1 200 OK
Content-Length: <size>
<no body after blank line>
```
The `Content-Length` must match the GET response for the same file.

---

### POST — File upload

```bash
curl -v -X POST http://localhost:8080/upload/hello.txt \
  -H "Content-Type: text/plain" \
  --data-binary "hello world"
```
**Expected:**
```
HTTP/1.1 201 Created
```
Verify the file was saved:
```bash
cat www/upload/hello.txt
```

---

### POST — Payload too large

```bash
python3 -c "print('A'*10000001)" | curl -v -X POST http://localhost:8080/upload/big.txt \
  -H "Content-Type: text/plain" \
  -H "Content-Length: 10000001" \
  --data-binary @-
```
**Expected:**
```
HTTP/1.1 413 Payload Too Large
```

---

### DELETE — Delete existing file

```bash
echo "deleteme" > www/upload/deleteme.txt
chmod o+r www/upload/deleteme.txt
curl -v -X DELETE http://localhost:8080/upload/deleteme.txt
```
**Expected:**
```
HTTP/1.1 204 No Content
```
Verify the file is gone:
```bash
ls www/upload/deleteme.txt  # should say: No such file or directory
```

---

### DELETE — File not found

```bash
curl -v -X DELETE http://localhost:8080/upload/ghost.txt
```
**Expected:**
```
HTTP/1.1 404 Not Found
```

---

### Method not allowed

```bash
curl -v -X DELETE http://localhost:8080/index.html
```
**Expected:**
```
HTTP/1.1 405 Method Not Allowed
```
(`/` location only allows GET HEAD POST DELETE, but the root file is not in the upload path — depending on your config this returns 405 if DELETE is not listed for that location.)

---

### CGI — GET with query string

Start server with `cgi.conf`:
```bash
./webserv cgi.conf
curl -v "http://localhost:8080/cgi-bin/echo.py?name=foo&age=42"
```
**Expected:**
```
HTTP/1.1 200 OK
Content-Type: text/plain

METHOD: GET
QUERY: name=foo&age=42
```

---

### CGI — POST with body

```bash
curl -v -X POST http://localhost:8080/cgi-bin/echo.py \
  -H "Content-Type: application/x-www-form-urlencoded" \
  -d "name=hello&value=world"
```
**Expected:**
```
HTTP/1.1 200 OK

METHOD: POST
BODY: name=hello&value=world
```

---

### CGI — Timeout (hanging script)

```bash
curl -v --max-time 15 http://localhost:8080/cgi-bin/hang.py
```
**Expected:** Server kills the CGI process after 10 seconds and responds:
```
HTTP/1.1 500 Internal Server Error
```
Response must arrive before curl's 15s timeout.

---

### Redirect

Start server with `redirect_autoindex.conf`:
```bash
./webserv redirect_autoindex.conf
curl -v http://localhost:8080/old
```
**Expected:**
```
HTTP/1.1 301 Moved Permanently
Location: /new
```

---

### Multiple simultaneous connections

```bash
for i in $(seq 1 10); do
  curl -s http://localhost:8080/index.html > /dev/null &
done
wait
echo "All done"
```
**Expected:** All 10 requests complete successfully, `All done` is printed. No hang.

---

### Multiple servers (multi_server.conf)

```bash
./webserv multi_server.conf
curl -s -o /dev/null -w "%{http_code}" http://localhost:8080/index.html
curl -s -o /dev/null -w "%{http_code}" http://localhost:8081/index.html
```
**Expected:** Both return `200`.

---

## CGI Test Scripts

### `www/cgi-bin/echo.py`
```python
#!/usr/bin/env python3
import os, sys

length = int(os.environ.get("CONTENT_LENGTH", 0) or 0)
body = sys.stdin.read(length) if length > 0 else ""

print("Content-Type: text/plain\r")
print("\r")
print("METHOD:", os.environ.get("REQUEST_METHOD"))
print("QUERY:", os.environ.get("QUERY_STRING"))
print("BODY:", body)
```

### `www/cgi-bin/hang.py`
```python
#!/usr/bin/env python3
import time
time.sleep(9999)
print("Content-Type: text/plain\r\n\r\nshould never get here")
```

Make both executable:
```bash
chmod +x www/cgi-bin/echo.py www/cgi-bin/hang.py
```
