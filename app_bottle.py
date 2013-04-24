import bottle
from bottle import route, run, template, static_file
from wsgiref.simple_server import make_server
from pprint import pprint

@route('/')
def index():
    return 'It works!'

@route('/hello/:name')
def hello(name='World'):
    return template('<b>Hello {{name}}</b>!', name=name)

@route('/static/<filename>')
def serve_static(filename):
    return static_file(filename, root='')

def app(*args, **kwargs):
    ret = bottle.app().wsgi(*args, **kwargs)
    return ret

if __name__ == '__main__':
    httpd = make_server('', 8000, app)
    print 'Serving on port 8000...'
    httpd.serve_forever()
