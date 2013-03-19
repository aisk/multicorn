#include <assert.h>
#include <err.h>

#include <event2/event.h>
#include <event2/buffer.h>
#include <event2/http.h>
#include <python2.7/Python.h>


void ngruPyvmInit()
{
    Py_Initialize();

    PyRun_SimpleString("print 'Python VM init sucessfull!'\n");

    // add current path to sys path
    PyRun_SimpleString("import sys\n");
    PyRun_SimpleString("sys.path.insert(0, \".\")\n");

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
    PyObject *pModule, *pName;
    pName = PyString_FromString("app");
    pModule = PyImport_Import(pName);
    assert(pModule!=NULL);
    // get wsgi func
    pWsgiFunc = PyObject_GetAttrString(pModule, "app");
    assert(pWsgiFunc!=NULL);


    Py_DECREF(pName);
    Py_DECREF(pModule);
    return pWsgiFunc;
}

PyObject *ngruStartResponse(PyObject* status, PyObject* headers)
{
    return PyString_FromString("sad");
}

char *ngruParseWsgiResult(PyObject *pResult)
{
    assert(PyList_Check(pResult));
    PyObject *pResultString = PyList_GetItem(pResult, 0);
    char *strResult = PyString_AsString(pResultString);
    return strResult;
}

void ngruWsgiHandler(struct evhttp_request *req, void *arg)
{
    // build the python `start_response` function
    PyObject* (*fpFunc)(PyObject*, PyObject*) = ngruStartResponse;
    PyMethodDef methd = {"start_response", fpFunc, METH_VARARGS, "a new function"};
    PyObject *name = PyString_FromString(methd.ml_name);
    PyObject* pStartResponse = PyCFunction_NewEx(&methd, NULL, name);
    Py_DECREF(name);

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

    puts(evhttp_uri_get_scheme(ev_uri));

    int port;
    port = evhttp_uri_get_port(ev_uri);

    char *method;
    switch (evhttp_request_get_command(req)) {
        case (EVHTTP_REQ_GET):
            method = "GET";
            break;
        case (EVHTTP_REQ_POST):
            method = "POST";
            break;
        // TODO
        default:
            break;
    }

    printf("%s: %s\n", method, uri);

    // build the python environ dict
    PyObject *environ;
    environ = PyDict_New();

    PyDict_SetStringItemString(environ, "REQUEST_METHOD", method);
    PyDict_SetStringItemString(environ, "SCRIPT_NAME", "");
    PyDict_SetStringItemString(environ, "SERVER_NAME", host);
    PyDict_SetStringItemString(environ, "QUERY_STRING", query);
    PyDict_SetIntItemString(environ, "SERVER_PORT", port);

    //free(uri);
    //free(method);
    //free(host);
    

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
    char *strResult = ngruParseWsgiResult(pResult);
    
    struct evbuffer *buf;
    buf = evbuffer_new();
    assert(buf != NULL);

    evbuffer_add_printf(buf, strResult);

    evhttp_send_reply(req, HTTP_OK, "OK", buf);

    evbuffer_free(buf);

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

int main(int argc, char* argv[])
{
    ngruPyvmInit();

    struct event_base *base;
    base = event_base_new();
    assert(base != NULL);

    struct evhttp *http;
    http = evhttp_new(base);
    assert(evhttp_bind_socket(http, "0.0.0.0", 8000) == 0);

    evhttp_set_gencb(http, ngruWsgiHandler, NULL);

    puts("Server started at 0.0.0.0:8000");
    //assert(event_base_dispatch(base) == 1);
    event_base_dispatch(base);

    evhttp_free(http);
    event_base_free(base);
    
    ngruPyvmDestoy();
    return 0;

}

