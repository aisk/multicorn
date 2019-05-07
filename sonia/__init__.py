from asyncio.futures import Future

from _event_ffi import ffi, lib


base = None


def get_event_version():
    return ffi.string(lib.event_get_version()).decode('utf-8')


def start_http_server():
    global base
    base = lib.event_base_new()
    http = lib.evhttp_new(base)
    lib.evhttp_set_gencb(http, lib.http_handler, ffi.NULL)
    lib.evhttp_bind_socket(http, b"localhost", 8080)
    lib.event_base_dispatch(base)
    lib.evhttp_free(http)
    lib.event_base_free(base)


@ffi.def_extern()
def http_handler(req, args):
    scope = extract_scope(req)
    from app import HelloWorld
    instance = HelloWorld(scope)

    def send(data):
        if data['type'] == 'http.response.start':
            status_code = data['status']
            lib.evhttp_send_reply_start(req, status_code, b'Ok')
        if data['type'] == 'http.response.body':
            body = data['body']
            buf = lib.evbuffer_new()
            lib.evbuffer_add_printf(buf, body)
            lib.evhttp_send_reply_chunk(req, buf)
            lib.evhttp_send_reply_end(req)
            lib.evbuffer_free(buf)
        future = Future()
        future.set_result(None)
        return future

    co = instance(lambda *args, **kwargs: None, send)
    try:
        co.send(None)
    except StopIteration:
        pass


def extract_scope(req):
    uri = lib.evhttp_request_get_evhttp_uri(req)
    query_string = lib.evhttp_uri_get_query(uri)
    query_string = b'' if query_string == ffi.NULL else ffi.string(query_string)
    query_string.decode('utf-8')
    path = ffi.string(lib.evhttp_uri_get_path(uri)).decode('utf-8')
    method = lib.evhttp_request_get_command(req)
    method = {
        lib.EVHTTP_REQ_GET: 'GET',
        lib.EVHTTP_REQ_POST: 'POST',
        lib.EVHTTP_REQ_HEAD: 'HEAD',
        lib.EVHTTP_REQ_PUT: 'PUT',
        lib.EVHTTP_REQ_DELETE: 'DELETE',
        lib.EVHTTP_REQ_OPTIONS: 'OPTIONS',
        lib.EVHTTP_REQ_TRACE: 'TRACE',
        lib.EVHTTP_REQ_CONNECT: 'CONNECT',
        lib.EVHTTP_REQ_PATCH: 'PATCH',
    }[method]
    ev_headers = lib.evhttp_request_get_input_headers(req)
    raw_headers = lib.get_headers_from_event(ev_headers)
    headers = []
    for i in range(100):
        key = ffi.string(raw_headers[i].key)
        value = ffi.string(raw_headers[i].value)
        if key == b'' and value == b'':
            break
        headers.append((key, value))

    scope = {
        'type': 'http',
        'http_version': '1.1',
        'method': method,
        'path': path,
        'root_path': '',
        'scheme': 'http',
        'query_string': query_string,
        'headers': headers,
    }
    return scope
