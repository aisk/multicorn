from cffi import FFI


ffibuilder = FFI()

ffibuilder.cdef('''

    typedef void(* event_callback_fn) (int, short, void *);
    const char* event_get_version(void);
    struct event_base *event_base_new(void);
    void event_base_free (struct event_base *);
    int event_base_dispatch (struct event_base *);
    int event_base_loop(struct event_base *, int);
    int event_base_loopbreak (struct event_base *);
    struct event *event_new(struct event_base *, int, short, event_callback_fn, void *);
    int event_add(struct event *ev, const struct timeval *timeout);

    #define EVLOOP_ONCE 0x01
    #define EVLOOP_NONBLOCK 0x02
    #define EVLOOP_NO_EXIT_ON_EMPTY 0x04

    #define EV_TIMEOUT 0x01
    #define EV_READ 0x02
    #define EV_WRITE 0x04
    #define EV_SIGNAL 0x08
    #define EV_PERSIST 0x10
    #define EV_ET 0x20
    #define EV_FINALIZE 0x40
    #define EV_CLOSED 0x80

    enum evhttp_cmd_type {
        EVHTTP_REQ_GET     = 1,
        EVHTTP_REQ_POST    = 2,
        EVHTTP_REQ_HEAD    = 4,
        EVHTTP_REQ_PUT     = 8,
        EVHTTP_REQ_DELETE  = 16,
        EVHTTP_REQ_OPTIONS = 32,
        EVHTTP_REQ_TRACE   = 64,
        EVHTTP_REQ_CONNECT = 128,
        EVHTTP_REQ_PATCH   = 256
    };
    struct evhttp *evhttp_new(struct event_base *base);
    void evhttp_free(struct evhttp *http);
    int evhttp_bind_socket(struct evhttp *http, const char *address, uint16_t port);
    void evhttp_set_gencb(struct evhttp *http, void(*cb)(struct evhttp_request *, void *), void *arg);
    void evhttp_send_reply(struct evhttp_request *req, int code, const char *reason, struct evbuffer *databuf);
    void evhttp_send_reply_start(struct evhttp_request *req, int code, const char *reason);
    void evhttp_send_reply_chunk(struct evhttp_request *req, struct evbuffer *databuf);
    void evhttp_send_reply_end(struct evhttp_request *req);
    void evhttp_request_free(struct evhttp_request *req);
    const struct evhttp_uri *evhttp_request_get_evhttp_uri(const struct evhttp_request *req);
    void evhttp_uri_free(struct evhttp_uri *uri);
    const char *evhttp_uri_get_query(const struct evhttp_uri *uri);
    const char *evhttp_uri_get_path(const struct evhttp_uri *uri);
    const char *evhttp_request_get_uri(const struct evhttp_request *req);
    const char *evhttp_request_get_host(struct evhttp_request *req);
    int evhttp_uri_get_port(const struct evhttp_uri *uri);
    const char *evhttp_uri_get_scheme(const struct evhttp_uri *uri);
    enum evhttp_cmd_type evhttp_request_get_command(const struct evhttp_request *req);
    struct evkeyvalq *evhttp_request_get_input_headers(struct evhttp_request *req);

    struct evbuffer *evbuffer_new(void);
    void evbuffer_free(struct evbuffer *buf);
    int evbuffer_add_printf(struct evbuffer *buf, const char *fmt,...);

    extern "Python" void http_handler(struct evhttp_request *req, void *args);
    extern "Python" void signal_handler(int, short, void *);
    extern "Python" void timer_handler(int, short, void *);
    struct pair {
        char *key;
        char *value;
    };

    struct pair *get_headers_from_event(struct evkeyvalq *evheader);
    struct timeval *new_timeval(long sec, long usec);
''')

ffibuilder.set_source('_event_ffi', '''
    #include <event2/buffer.h>
    #include <event2/event.h>
    #include <event2/http.h>
    #include <event2/keyvalq_struct.h>
    #include <event2/util.h>
    #include <sys/queue.h>
    #include <sys/time.h>

    struct pair {
        char *key;
        char *value;
    };

    struct pair *get_headers_from_event(struct evkeyvalq *evheader) {
        struct pair *result = malloc(100 * sizeof (struct pair));  // TODO: check max of headers
        int i = 0;
        struct evkeyval *header;
        TAILQ_FOREACH(header, evheader, next) {
            result[i].key = header->key;
            result[i].value = header->value;
            i++;
        }
        result[i].key = "";
        result[i].value = "";
        return result;
    }

    struct timeval *new_timeval(long sec, long usec) {
        struct timeval *val = malloc(sizeof (struct timeval));
        val->tv_sec = sec;
        val->tv_usec = usec;
        return val;
    }
''', libraries=['event'])


if __name__ == '__main__':
    ffibuilder.compile(verbose=True)
