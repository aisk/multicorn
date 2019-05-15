import asyncio


class HelloWorld:
    def __init__(self, scope):
        ...

    async def __call__(self, receive, send):
        await asyncio.sleep(1)
        await send({
            'type': 'http.response.start',
            'status': 200,
            'headers': [
                [b'content-type', b'text/plain'],
            ]
        })
        await send({
            'type': 'http.response.body',
            'body': b'Hello, world!',
        })
