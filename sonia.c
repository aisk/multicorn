#include <stdlib.h>
#include <stdio.h>
#include <sys/queue.h>

#include <Python.h>
#include <event2/buffer.h>
#include <event2/event.h>
#include <event2/http.h>
#include <event2/keyvalq_struct.h>


struct event_base *base = NULL;
PyObject *Future;


void handle_signal(int signal) {
    event_base_loopbreak(base);
}


void initialize() {
    signal(SIGINT, handle_signal);

    Py_Initialize();

    // add current path to sys path
    PyRun_SimpleString("import sys\n");
    PyRun_SimpleString("sys.path.insert(0, \".\")\n");


    PyObject *sys = PyImport_ImportModule("asyncio.futures");
    Future = PyObject_GetAttrString(sys, "Future");
}


void finalize() {
    Py_Finalize();
}


PyObject *import_application() {
    PyObject *module = PyImport_ImportModule("app");
    PyObject *application = PyObject_GetAttrString(module, "HelloWorld");
    return application;
}

PyObject *extract_scope(struct evhttp_request *request) {
    const struct evhttp_uri *ev_uri = evhttp_request_get_evhttp_uri(request);
    const char *query_string = evhttp_uri_get_query(ev_uri);
    if (query_string == NULL) {
        query_string = "";
    }
    const char *path = evhttp_uri_get_path(ev_uri);
    const char *uri = evhttp_request_get_uri(request);
    const char *host = evhttp_request_get_host(request);
    int port = evhttp_uri_get_port(ev_uri);
    const char *scheme = evhttp_uri_get_scheme(ev_uri);
    char *method = "";
    switch (evhttp_request_get_command(request)) {
        case (EVHTTP_REQ_GET):
            method = "GET";
            break;
        case (EVHTTP_REQ_POST):
            method = "POST";
            break;
        case (EVHTTP_REQ_HEAD):
            method = "HEAD";
            break;
        case (EVHTTP_REQ_PUT):
            method = "PUT";
            break;
        case (EVHTTP_REQ_DELETE):
            method = "DELETE";
            break;
        case (EVHTTP_REQ_OPTIONS):
            method = "OPTIONS";
            break;
        case (EVHTTP_REQ_TRACE):
            method = "TRACE";
            break;
        case (EVHTTP_REQ_CONNECT):
            method = "CONNECT";
            break;
        case (EVHTTP_REQ_PATCH):
            method = "PATCH";
            break;
        default:
            break;
    }


    PyObject *headers = PyList_New(0);
    struct evkeyval *header;
    TAILQ_FOREACH(header, evhttp_request_get_input_headers(request), next) {
        PyObject *pair = PyTuple_Pack(2, PyBytes_FromString(header->key), PyBytes_FromString(header->value));
        PyList_Append(headers, pair);
    }

    PyObject *scope = PyDict_New();
    PyDict_SetItemString(scope, "type", PyUnicode_FromString("http"));
    PyDict_SetItemString(scope, "http_version", PyUnicode_FromString("1.1"));
    PyDict_SetItemString(scope, "method", PyUnicode_FromString(method));
    PyDict_SetItemString(scope, "path", PyUnicode_FromString(path));
    PyDict_SetItemString(scope, "root_path", PyUnicode_FromString(""));
    PyDict_SetItemString(scope, "scheme", PyUnicode_FromString("http"));
    PyDict_SetItemString(scope, "query_string", PyUnicode_FromString(query_string));
    PyDict_SetItemString(scope, "headers", headers);

    return scope;
}


PyObject *send_callback(PyObject *self, PyObject *arguments) {
    PyObject *data = PyTuple_GetItem(arguments, 0);
    PyObject_Print(data, stdout, 0);
    puts("");
    PyObject *future = PyObject_CallObject(Future, NULL);
    PyObject_Print(future, stdout, 0);
    puts("");
    PyObject_CallMethodObjArgs(future, PyBytes_FromString("set_result"), Py_None, NULL);
    return future;
}


void handler(struct evhttp_request *req, void *args) {
    PyObject *application = import_application();
    PyObject *scope = extract_scope(req);
    PyObject *arguments = PyTuple_Pack(1, scope);
    PyObject *result = PyObject_CallObject(application, arguments);

    arguments = PyTuple_Pack(2, Py_None, Py_None);
    PyObject_Print(result, stdout, 0);
    puts("");
    // PyObject_CallObject(result, arguments);

    struct evbuffer *buf = evbuffer_new();
    evbuffer_add_printf(buf, "<html><head><title>Hello!</title></head>");
    evbuffer_add_printf(buf, "<body><h1>It worked!</h1></body></html>");
    evhttp_send_reply_start(req, HTTP_OK, "OK");
    evhttp_send_reply_chunk(req, buf);
    evhttp_send_reply_end(req);
    evbuffer_free(buf);
} 


int main(int argc, char *argv[]) {
    initialize();
    base = event_base_new();
    struct evhttp *http = evhttp_new(base);
    evhttp_set_gencb(http, handler, NULL);
    evhttp_bind_socket(http, "localhost", 8080);
    event_base_dispatch(base);
    evhttp_free(http);
    event_base_free(base);
    finalize();
    return 0;
}
