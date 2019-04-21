class HelloWorld:
    def __init__(self, scope):
        print(self, scope)

    def __call__(self, receive, send):
        print('xxx')
        print(self, receive, send)
        send({
            'type': 'http.response.start',
            'status': 200,
            'headers': [
                [b'content-type', b'text/plain'],
            ]
        })
        send({
            'type': 'http.response.body',
            'body': b'Hello, world!',
        })

    # def __repr__(self):
    #     print(self.__call__, callable(self))
    #     return "???"
