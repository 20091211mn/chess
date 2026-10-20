from http.server import HTTPServer, BaseHTTPRequestHandler

class CustomHandler(BaseHTTPRequestHandler):
    def do_GET(self):
        if self.path == '/ping':
            self.send_response(200)
            self.send_header("Content-type", "text/plain")
            self.end_headers()
            self.wfile.write(b"pong")
        else:
            self.send_response(404)
            self.send_header("Content-type", "text/plain")
            self.end_headers()
            self.wfile.write(b"Not Found")

class ReusableHTTPServer(HTTPServer):
    allow_reuse_address = True

server = ReusableHTTPServer(('0.0.0.0', 8080), CustomHandler)
print("Server running successfully on port 8080!")
server.serve_forever()
