#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <err.h>
#include <assert.h>
#include <sys/queue.h>

#include <event2/event.h>
#include <event2/buffer.h>
#include <event2/http.h>
#include <event2/keyvalq_struct.h>
#include <python2.7/Python.h>

#define ADDRESS "0.0.0.0"
#define PORT 8000
#define WSGI_MODULE "app"
#define WSGI_FUNC "app"

struct event_base *base;
PyObject *pyResponseStatus;
PyObject *pyResponseHeaders;
PyObject *pyStringIO;
PyObject *pyStderr;

char *strupr(char *str) 
{ 
    char *ptr = str; 
    while (*ptr != '\0') { 
        if (islower(*ptr)) 
            *ptr = toupper(*ptr); 
        ptr++; 
    } 
    return str; 
}

void ngruPyvmInit()
{
    Py_Initialize();
    //PyRun_SimpleString("print 'Python VM init sucessfull!'\n");

    // add current path to sys path
    PyRun_SimpleString("import sys\n");
    PyRun_SimpleString("sys.path.insert(0, \".\")\n");

    PyObject *cStringIO;
    cStringIO = PyImport_ImportModule("cStringIO");
    pyStringIO = PyObject_GetAttrString(cStringIO, "StringIO");
    Py_DECREF(cStringIO);

    PyObject *sys;
    sys = PyImport_ImportModule("sys");
    pyStderr = PyObject_GetAttrString(sys, "stderr");
    Py_DECREF(sys);
}

void ngruPyvmDestoy()
{
    Py_Finalize();

}

PyObject* PyDict_SetStringItemString(PyObject *pDict, const char *key, const char *value)
{
    PyObject *pValue;
    pValue = PyString_FromString(value);
    PyDict_SetItemString(pDict, key, pValue);
    Py_DecRef(pValue);
    return pDict;
}

PyObject* PyDict_SetIntItemString(PyObject *pDict, const char *key, int value)
{
    PyObject *pValue;
    pValue = PyInt_FromLong(value);
    PyDict_SetItemString(pDict, key, pValue);
    Py_DecRef(pValue);
    return pDict;
}

PyObject* ngruWsgiFuncGet()
{
    // get wsgi app module
    PyObject *pWsgiFunc;
    PyObject *pModule;
    pModule = PyImport_ImportModule(WSGI_MODULE);
    assert(pModule!=NULL);
    // get wsgi func
    pWsgiFunc = PyObject_GetAttrString(pModule, WSGI_FUNC);
    assert(pWsgiFunc!=NULL);


    Py_DECREF(pModule);
    return pWsgiFunc;
}

PyObject *ngruStartResponse(PyObject* self, PyObject* args)
{
    pyResponseStatus = PyTuple_GetItem(args, 0);
    pyResponseHeaders = PyTuple_GetItem(args, 1);
    Py_INCREF(pyResponseHeaders);
    Py_INCREF(pyResponseStatus);
    return Py_None;//PyString_FromString("sad");
}

struct evbuffer *ngruParseWsgiResult(PyObject *pResult)
{
    //assert(PyIter_Check(pResult));

    struct evbuffer *buf;
    buf = evbuffer_new();
    assert(buf != NULL);

    PyObject *item;
    PyObject *iterator;
    iterator = PySeqIter_New(pResult);
    while ((item=PyIter_Next(iterator)) != NULL) {
        const char *strResult = PyString_AsString(item);
        evbuffer_add_printf(buf, strResult);
        Py_DECREF(item);
    }
    return buf;
}

PyObject *ngruParseEnviron(struct evhttp_request *req)
{
    const struct evhttp_uri* ev_uri;
    ev_uri = evhttp_request_get_evhttp_uri(req);
    assert(ev_uri!=NULL);

    const char *query;
    query = evhttp_uri_get_query(ev_uri);
    if (query==NULL) query = "";

    const char *path;
    path = evhttp_uri_get_path(ev_uri);

    const char *uri;
    uri  = evhttp_request_get_uri(req);

    const char *host;
    host = evhttp_request_get_host(req);

    //int port;
    //port = evhttp_uri_get_port(ev_uri);

    const char *scheme;
    scheme = evhttp_uri_get_scheme(ev_uri);
    if (scheme==NULL) scheme = "http";

    char *method;
    switch (evhttp_request_get_command(req)) {
        case (EVHTTP_REQ_GET): method = "GET"; break;
        case (EVHTTP_REQ_POST): method = "POST"; break;
        case (EVHTTP_REQ_HEAD): method = "HEAD"; break;
	    case (EVHTTP_REQ_PUT): method = "PUT"; break;
        case (EVHTTP_REQ_DELETE): method = "DELETE"; break;
        case (EVHTTP_REQ_OPTIONS): method = "OPTIONS"; break;
        case (EVHTTP_REQ_TRACE): method = "TRACE"; break;
        case (EVHTTP_REQ_CONNECT): method = "CONNECT"; break;
        case (EVHTTP_REQ_PATCH): method = "PATCH"; break;
        default: break;
    }

    printf("%s: %s\n", method, uri);

    // build the python environ dict
    PyObject *environ;
    environ = PyDict_New();

    PyDict_SetStringItemString(environ, "REQUEST_METHOD", method);
    PyDict_SetStringItemString(environ, "SCRIPT_NAME", "");             // TODO
    PyDict_SetStringItemString(environ, "PATH_INFO", path);
    PyDict_SetStringItemString(environ, "QUERY_STRING", query);
    PyDict_SetStringItemString(environ, "CONTENT_TYPE", "text/plain");
    PyDict_SetStringItemString(environ, "CONTENT_LENGTH", "");
    PyDict_SetStringItemString(environ, "SERVER_NAME", host);
    char port[10];
    sprintf(port, "%d", PORT);
    PyDict_SetStringItemString(environ, "SERVER_PORT", port);
    PyDict_SetStringItemString(environ, "SERVER_PROTOCOL", "HTTP/1.1"); // TODO

    PyDict_SetStringItemString(environ, "wsgi.url_scheme", scheme);
    PyDict_SetItemString(environ, "wsgi.multithread", Py_False);
    PyDict_SetItemString(environ, "wsgi.multiprocess", Py_False);
    PyDict_SetItemString(environ, "wsgi.run_once", Py_False);
    PyDict_SetItemString(environ, "wsgi.errors", pyStderr);

    // wsgi.input, use cStringIO as file object
    struct evbuffer *buf;
    buf = evhttp_request_get_input_buffer(req);
    char* body;
    int length;
    length = evbuffer_get_length(buf);
    body = (char *)malloc(length + 1);
    memset(body, '\0', length + 1);
    evbuffer_copyout(buf, body, length);
    PyObject *pArgs;
    pArgs = PyTuple_New(1);
    PyTuple_SetItem(pArgs, 0, PyString_FromString(body));
    PyObject *input;
    input = PyObject_CallObject(pyStringIO, pArgs);
    assert(input != NULL);
    PyDict_SetItemString(environ, "wsgi.input", input);
    free(body);
    Py_DECREF(pArgs);
    Py_DECREF(input);

    struct evkeyvalq *headers;
    headers = evhttp_request_get_input_headers(req);
    struct evkeyval *header;
    TAILQ_FOREACH(header, headers, next) {
        // TODO: ingore case?
        if (strcmp(header->key, "Content-Length")==0) {
            PyDict_SetStringItemString(environ, "CONTENT_LENGTH", header->value);
        }
        else if (strcmp(header->key, "Content-Type")==0) {
            PyDict_SetStringItemString(environ, "CONTENT_TYPE", header->value);
        }
        else {
            PyObject *key = PyString_FromFormat("HTTP_%s", header->key);
            PyObject *value = PyString_FromString(header->value);
            PyDict_SetItem(environ, key, value);
            Py_DECREF(key);
            Py_DECREF(value);
        }
    }


    return environ;
}

void ngruWsgiHandler(struct evhttp_request *req, void *arg)
{
    // build the python `start_response` function
    PyObject* (*fpFunc)(PyObject*, PyObject*) = ngruStartResponse;
    PyMethodDef methd = {"start_response", fpFunc, METH_VARARGS, "a new function"};
    PyObject *name = PyString_FromString(methd.ml_name);
    PyObject* pStartResponse = PyCFunction_NewEx(&methd, NULL, name);
    Py_DECREF(name);

    PyObject *environ;
    environ = ngruParseEnviron(req);

    // call python wsgi application function
    PyObject *pWsgiFunc;
    pWsgiFunc = ngruWsgiFuncGet();
    PyObject *pArgs;
    pArgs = PyTuple_New(2);
    PyTuple_SetItem(pArgs, 0, environ);
    PyTuple_SetItem(pArgs, 1, pStartResponse);

    PyObject *pResult;
    pResult = PyObject_CallObject(pWsgiFunc, pArgs);
    assert(pResult!=NULL);
    
    struct evkeyvalq *output_headers;
    output_headers = evhttp_request_get_output_headers(req);
    evhttp_add_header(output_headers, "Wsgi-Server", "Ngru");
    Py_ssize_t i;
    PyObject *header;
    for (i=0; i<PyList_Size(pyResponseHeaders);i++) {
        header = PyList_GetItem(pyResponseHeaders, i);
        char* key = PyString_AsString(PyTuple_GetItem(header, 0));
        char* value = PyString_AsString(PyTuple_GetItem(header, 1));
        evhttp_add_header(output_headers, key, value);
        //printf("%s: %s\n", key, value);
    }


    struct evbuffer *buf;
    buf = ngruParseWsgiResult(pResult);

    evhttp_send_reply(req, HTTP_OK, "OK", buf);

    evbuffer_free(buf);

    Py_DECREF(pyResponseStatus);
    Py_DECREF(pyResponseHeaders);
    Py_DECREF(pStartResponse);
    Py_DECREF(pArgs);
    Py_DECREF(pResult);
    Py_DECREF(environ);
}

void ngruCommonHandler(struct evhttp_request *req, void *arg)
{
    struct evbuffer *buf;
    buf = evbuffer_new();

    if (buf == NULL)
        err(1, "failed to create response buffer");

    evbuffer_add_printf(buf, "<html><head><title>Hello!</title></head>");
    evbuffer_add_printf(buf, "<body><h1>It worked!</h1></body></html>");

    evhttp_send_reply(req, HTTP_OK, "OK", buf);

    evbuffer_free(buf);
}

void signalHandler(int signal)
{
    event_base_loopbreak(base);
}

int main(int argc, char* argv[])
{
    ngruPyvmInit();

    base = event_base_new();
    assert(base != NULL);

    // for ctrl-c
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    struct evhttp *http;
    http = evhttp_new(base);
    assert(evhttp_bind_socket(http, ADDRESS, PORT) == 0);

    evhttp_set_gencb(http, ngruWsgiHandler, NULL);

    printf("Server started at %s:%d\n", ADDRESS, PORT);
    //assert(event_base_dispatch(base) == 1);
    event_base_dispatch(base);

    evhttp_free(http);
    event_base_free(base);
    
    ngruPyvmDestoy();
    return 0;

}

