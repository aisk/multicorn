class HelloWorld:
    def __init__(self, scope):
        ...

    async def __call__(self, receive, send):
        await send({
            'type': 'http.response.start',
            'status': 200,
            'headers': [
                [b'content-type', b'text/plain'],
            ]
        })
        print(1)
        await send({
            'type': 'http.response.body',
            'body': b'Hello, world!',
        })
        print(2)
