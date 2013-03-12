from wsgiref.simple_server import make_server


def app(environ, start_response):
    print locals()
    start_response('200 OK', [('Content-Type', 'text/plain')])
    return ["Hello Ngru!"]

if __name__ == '__main__':
    httpd = make_server('', 8000, app)
    print "Serving on port 8000..."
    httpd.serve_forever()
