#include <stdlib.h>
#include <stdio.h>
#include <sys/queue.h>

#include <Python.h>
#include <event2/buffer.h>
#include <event2/event.h>
#include <event2/http.h>
#include <event2/keyvalq_struct.h>

#include "sonia.h"


struct event_base *base = NULL;
PyObject *Future;


static PyObject *
donothing(PyObject *self, PyObject *args) {
    Py_RETURN_NONE;
}
static PyMethodDef donothing_ml = {"donothing", donothing, METH_VARARGS, NULL};


void handle_signal(int signal) {
    event_base_loopbreak(base);
}


void initialize() {
    signal(SIGINT, handle_signal);

    Py_Initialize();

    // add current path to sys path
    PyRun_SimpleString("import sys\n");
    PyRun_SimpleString("sys.path.insert(0, \".\")\n");

    PyObject *futures = PyImport_ImportModule("asyncio.futures");
    Future = PyObject_GetAttrString(futures, "Future");
    Py_DECREF(futures);
}


void finalize() {
    Py_Finalize();
}


PyObject *import_application() {
    PyObject *module = PyImport_ImportModule("app");
    PyObject *application = PyObject_GetAttrString(module, "HelloWorld");
    Py_DECREF(module);
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
    struct evhttp_request *req = PyCapsule_GetPointer(self, NULL);
    PyObject *data = PyTuple_GetItem(arguments, 0);
    PyObject *type = PyDict_GetItemString(data, "type");
    if (PyUnicode_CompareWithASCIIString(type, "http.response.start") == 0) {
        int code = PyLong_AsLong(PyDict_GetItemString(data, "status"));
        evhttp_send_reply_start(req, code, code_to_message(code));
    } else if (PyUnicode_CompareWithASCIIString(type, "http.response.body") == 0) {
        PyObject *body = PyDict_GetItemString(data, "body");
        struct evbuffer *buf = evbuffer_new();
        evbuffer_add_printf(buf, PyBytes_AsString(body));
        evhttp_send_reply_chunk(req, buf);
        evhttp_send_reply_end(req);
        evbuffer_free(buf);
    }
    PyObject *future = PyObject_CallObject(Future, NULL);
    PyObject_CallMethodObjArgs(future, PyUnicode_FromString("set_result"), Py_None, NULL);
    return future;
}
static PyMethodDef send_method = {"send", send_callback, METH_VARARGS, NULL};


void handler(struct evhttp_request *req, void *args) {
    PyObject *application = import_application();
    PyObject *scope = extract_scope(req);
    PyObject *arguments = PyTuple_Pack(1, scope);
    PyObject *result = PyObject_CallObject(application, arguments);
    Py_DECREF(application);
    Py_DECREF(scope);
    Py_DECREF(arguments);

    arguments = PyTuple_Pack(2, PyCFunction_New(&donothing_ml, NULL), PyCFunction_New(&send_method, PyCapsule_New(req, NULL, NULL)));
    PyObject *coroutine = PyObject_CallObject(result, arguments);
    PyObject_CallMethodObjArgs(coroutine, PyUnicode_FromString("send"), Py_None, NULL);
    Py_DECREF(result);
    Py_DECREF(arguments);
    Py_DECREF(coroutine);
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
