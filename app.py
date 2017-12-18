import cgi
from wsgiref.simple_server import make_server
from pprint import pprint

def application(environ, start_response):
    parameters = cgi.parse_qs(environ.get(b'QUERY_STRING', ''))
    #print environ['wsgi.input'].read(int(environ['CONTENT_LENGTH']))
    if 'subject' in parameters:
        subject = cgi.escape(parameters['subject'][0])
    else:
        subject = b'World'
    start_response('200 OK', [
        ('Content-Type', 'text/html'),
        ('Foo', 'bar'),
    ])
    rets = [
        b'<html><head></head><body>',
        b'<h1>Hello %b!</h1>' % subject,
        b'<p>It works!</p>',
        b'<table border="1"><tbody>'
    ]
    for key in sorted(environ.keys()):
        rets.append(b'<tr><td>%s</td><td>%s</td></tr>' % (key.encode('utf-8'), cgi.escape(str(environ[key])).encode('utf-8')))
    rets.append(b'</tbody></table></body></html>')
    return rets

if __name__ == '__main__':
    httpd = make_server('', 8000, application)
    print('Serving on port 8000...')
    httpd.serve_forever()
