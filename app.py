import cgi
from wsgiref.simple_server import make_server
from pprint import pprint

def app(environ, start_response):
    pprint(environ)
    parameters = cgi.parse_qs(environ.get('QUERY_STRING', ''))
    if 'subject' in parameters:
        subject = cgi.escape(parameters['subject'][0])
    else:
        subject = 'World'
    start_response('200 OK', [('Content-Type', 'text/plain')])
    return ["<html><head></head><body><h1>Hello %s!</h1></body></html>" %subject]

if __name__ == '__main__':
    httpd = make_server('', 8000, app)
    print "Serving on port 8000..."
    httpd.serve_forever()
