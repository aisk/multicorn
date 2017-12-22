#include <assert.h>
#include <err.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/queue.h>

#include <event2/buffer.h>
#include <event2/event.h>
#include <event2/http.h>
#include <event2/keyvalq_struct.h>
#include <python3.6m/Python.h>

#define ADDRESS "0.0.0.0"
#define PORT 8000
#define WSGI_MODULE "app"
#define WSGI_FUNC "application"

struct event_base *base;
PyObject *pyResponseStatus;
PyObject *pyResponseHeaders;
PyObject *pyBytesIO;
PyObject *pyStderr;

char *strupr(char *str) {
  char *ptr = str;
  while (*ptr != '\0') {
    if (islower(*ptr)) {
      *ptr = toupper(*ptr);
    }
    ptr++;
  }
  return str;
}

void ngruPyvmInit() {
  Py_Initialize();

  // add current path to sys path
  PyRun_SimpleString("import sys\n");
  PyRun_SimpleString("sys.path.insert(0, \".\")\n");

  PyObject *ioModule = PyImport_ImportModule("io");
  assert(ioModule != NULL);
  pyBytesIO = PyObject_GetAttrString(ioModule, "BytesIO");
  assert(pyBytesIO != NULL);
  Py_DECREF(ioModule);

  PyObject *sys = PyImport_ImportModule("sys");
  pyStderr = PyObject_GetAttrString(sys, "stderr");
  assert(pyStderr != NULL);
  Py_DECREF(sys);
}

void ngruPyvmDestoy() { Py_Finalize(); }

PyObject *PyDict_SetStringItemString(PyObject *pDict, const char *key,
                                     const char *value) {
  PyObject *pValue = PyUnicode_FromString(value);
  PyDict_SetItemString(pDict, key, pValue);
  Py_DecRef(pValue);
  return pDict;
}

PyObject *PyDict_SetIntItemString(PyObject *pDict, const char *key, int value) {
  PyObject *pValue = PyLong_FromLong(value);
  PyDict_SetItemString(pDict, key, pValue);
  Py_DecRef(pValue);
  return pDict;
}

PyObject *ngruWsgiFuncGet() {
  // get wsgi app module
  PyObject *pModule = PyImport_ImportModule(WSGI_MODULE);
  assert(pModule != NULL);
  // get wsgi func
  PyObject *pWsgiFunc = PyObject_GetAttrString(pModule, WSGI_FUNC);
  assert(pWsgiFunc != NULL);

  Py_DECREF(pModule);
  return pWsgiFunc;
}

PyObject *ngruStartResponse(PyObject *self, PyObject *args) {
  pyResponseStatus = PyTuple_GetItem(args, 0);
  pyResponseHeaders = PyTuple_GetItem(args, 1);
  Py_INCREF(pyResponseHeaders);
  Py_INCREF(pyResponseStatus);
  return Py_None;
}

struct evbuffer *ngruParseWsgiResult(PyObject *pResult) {
  struct evbuffer *buf = evbuffer_new();
  assert(buf != NULL);

  PyObject *iterator = PyObject_GetIter(pResult);
  PyObject *item;

  while ((item = PyIter_Next(iterator))) {
    const char *strResult = PyBytes_AsString(item);
    int size = PyBytes_Size(item);
    assert(strResult != NULL);
    evbuffer_add(buf, strResult, size);
    Py_DECREF(item);
  }
  Py_DECREF(iterator);
  return buf;
}

PyObject *ngruParseEnviron(struct evhttp_request *req) {
  const struct evhttp_uri *ev_uri = evhttp_request_get_evhttp_uri(req);
  assert(ev_uri != NULL);

  const char *query = evhttp_uri_get_query(ev_uri);
  if (query == NULL) {
    query = "";
  }

  const char *path = evhttp_uri_get_path(ev_uri);

  const char *uri = evhttp_request_get_uri(req);

  const char *host = evhttp_request_get_host(req);

  // int port;
  // port = evhttp_uri_get_port(ev_uri);

  const char *scheme = evhttp_uri_get_scheme(ev_uri);
  if (scheme == NULL) {
    scheme = "http";
  }

  char *method;
  switch (evhttp_request_get_command(req)) {
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

  printf("%s: %s\n", method, uri);

  // build the python environ dict
  PyObject *environ = PyDict_New();

  PyDict_SetStringItemString(environ, "REQUEST_METHOD", method);
  PyDict_SetStringItemString(environ, "SCRIPT_NAME",
                             ""); // TODO(aisk): fix this
  PyDict_SetStringItemString(environ, "PATH_INFO", path);
  PyDict_SetStringItemString(environ, "QUERY_STRING", query);
  PyDict_SetStringItemString(environ, "CONTENT_TYPE", "text/plain");
  PyDict_SetStringItemString(environ, "CONTENT_LENGTH", "");
  PyDict_SetStringItemString(environ, "SERVER_NAME", host);
  char port[10];
  sprintf(port, "%d", PORT);
  PyDict_SetStringItemString(environ, "SERVER_PORT", port);
  PyDict_SetStringItemString(environ, "SERVER_PROTOCOL",
                             "HTTP/1.1"); // TODO(aisk): fix this

  PyDict_SetStringItemString(environ, "wsgi.url_scheme", scheme);
  PyDict_SetItemString(environ, "wsgi.multithread", Py_False);
  PyDict_SetItemString(environ, "wsgi.multiprocess", Py_False);
  PyDict_SetItemString(environ, "wsgi.run_once", Py_False);
  PyDict_SetItemString(environ, "wsgi.errors", pyStderr);

  // wsgi.input, use io.BytesIO as file object
  struct evbuffer *buf = evhttp_request_get_input_buffer(req);
  int length = evbuffer_get_length(buf);
  char *body = (char *)malloc(length + 1);
  memset(body, '\0', length + 1);
  evbuffer_copyout(buf, body, length);
  PyObject *pArgs = PyTuple_New(1);
  PyTuple_SetItem(pArgs, 0, PyBytes_FromString(body));
  PyObject *input = PyObject_CallObject(pyBytesIO, pArgs);
  if (input != NULL) {
    PyErr_Print();
  }
  assert(input != NULL);
  PyDict_SetItemString(environ, "wsgi.input", input);
  free(body);
  Py_DECREF(pArgs);
  Py_DECREF(input);

  struct evkeyvalq *headers = evhttp_request_get_input_headers(req);
  struct evkeyval *header;
  TAILQ_FOREACH(header, headers, next) {
    // TODO(aisk): ingore case?
    if (strcmp(header->key, "Content-Length") == 0) {
      PyDict_SetStringItemString(environ, "CONTENT_LENGTH", header->value);
    } else if (strcmp(header->key, "Content-Type") == 0) {
      PyDict_SetStringItemString(environ, "CONTENT_TYPE", header->value);
    } else {
      PyObject *key = PyUnicode_FromFormat("HTTP_%s", header->key);
      PyObject *value = PyUnicode_FromString(header->value);
      PyDict_SetItem(environ, key, value);
      Py_DECREF(key);
      Py_DECREF(value);
    }
  }

  return environ;
}

void ngruWsgiHandler(struct evhttp_request *req, void *arg) {
  // build the python `start_response` function
  PyObject *(*fpFunc)(PyObject *, PyObject *) = ngruStartResponse;
  PyMethodDef methd = {"start_response", fpFunc, METH_VARARGS,
                       "a new function"};
  PyObject *name = PyBytes_FromString(methd.ml_name);
  PyObject *pStartResponse = PyCFunction_NewEx(&methd, NULL, name);
  Py_DECREF(name);

  PyObject *environ = ngruParseEnviron(req);

  // call python wsgi application function
  PyObject *pWsgiFunc = ngruWsgiFuncGet();
  PyObject *pArgs = PyTuple_New(2);
  PyTuple_SetItem(pArgs, 0, environ);
  PyTuple_SetItem(pArgs, 1, pStartResponse);

  PyObject *pResult = PyObject_CallObject(pWsgiFunc, pArgs);
  assert(pResult != NULL);

  struct evkeyvalq *output_headers = evhttp_request_get_output_headers(req);
  evhttp_add_header(output_headers, "Wsgi-Server", "Ngru");
  Py_ssize_t i;
  PyObject *header;
  for (i = 0; i < PyList_Size(pyResponseHeaders); i++) {
    header = PyList_GetItem(pyResponseHeaders, i);
    const char *key = PyUnicode_AS_DATA(PyTuple_GetItem(header, 0));
    const char *value = PyUnicode_AS_DATA(PyTuple_GetItem(header, 1));
    evhttp_add_header(output_headers, key, value);
  }

  struct evbuffer *buf = ngruParseWsgiResult(pResult);

  evhttp_send_reply(req, HTTP_OK, "OK", buf);

  evbuffer_free(buf);

  Py_DECREF(pyResponseStatus);
  Py_DECREF(pyResponseHeaders);
  Py_DECREF(pStartResponse);
  Py_DECREF(pArgs);
  Py_DECREF(pResult);
  Py_DECREF(environ);
}

void ngruCommonHandler(struct evhttp_request *req, void *arg) {
  struct evbuffer *buf = evbuffer_new();

  if (buf == NULL) {
    err(1, "failed to create response buffer");
  }

  evbuffer_add_printf(buf, "<html><head><title>Hello!</title></head>");
  evbuffer_add_printf(buf, "<body><h1>It worked!</h1></body></html>");

  evhttp_send_reply(req, HTTP_OK, "OK", buf);

  evbuffer_free(buf);
}

void signalHandler(int signal) { event_base_loopbreak(base); }

int main(int argc, char *argv[]) {
  ngruPyvmInit();

  base = event_base_new();
  assert(base != NULL);

  // for ctrl-c
  signal(SIGINT, signalHandler);
  signal(SIGTERM, signalHandler);

  struct evhttp *http = evhttp_new(base);
  assert(evhttp_bind_socket(http, ADDRESS, PORT) == 0);

  evhttp_set_gencb(http, ngruWsgiHandler, NULL);

  printf("Server started at %s:%d\n", ADDRESS, PORT);
  // assert(event_base_dispatch(base) == 1);
  event_base_dispatch(base);

  evhttp_free(http);
  event_base_free(base);

  ngruPyvmDestoy();
  return 0;
}
