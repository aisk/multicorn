import cgi
from wsgiref.simple_server import make_server
from pprint import pprint

def application(environ, start_response):
    parameters = cgi.parse_qs(environ.get('QUERY_STRING', ''))
    #print environ['wsgi.input'].read(int(environ['CONTENT_LENGTH']))
    if 'subject' in parameters:
        subject = cgi.escape(parameters['subject'][0])
    else:
        subject = 'World'
    start_response('200 OK', [
        ('Content-Type', 'text/html'),
        ('Foo', 'bar'),
    ])
    rets = [
        '<html><head></head><body>',
        '<h1>Hello %s!</h1>' %subject,
        '<p>It works!</p>',
        '<table border="1"><tbody>'
    ]
    for key in sorted(environ.keys()):
        rets.append('<tr><td>%s</td><td>%s</td></tr>' %(key, cgi.escape(str(environ[key]))))
    rets.append('</tbody></table></body></html>')
    return rets

if __name__ == '__main__':
    httpd = make_server('', 8000, app)
    print 'Serving on port 8000...'
    httpd.serve_forever()
