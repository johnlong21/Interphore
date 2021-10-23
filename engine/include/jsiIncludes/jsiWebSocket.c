#ifndef JSI_LITE_ONLY
#if JSI__WEBSOCKET==1
#if JSI__MEMDEBUG
#include "jsiInt.h"
#else

#ifndef JSI_AMALGAMATION
#include "jsi.h"
#endif
JSI_EXTENSION_INI

#define jsi_Sig int

#endif /* JSI_MEM_DEBUG */

#include <time.h>
#include <sys/time.h>

#include <ctype.h>

#ifdef CMAKE_BUILD
#include "lws_config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>
#include <assert.h>
#include <limits.h>
#include <fcntl.h>
#ifdef WIN32
#define _GET_TIME_OF_DAY_H
#ifdef EXTERNAL_POLL
    #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
    #endif
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <stddef.h>

    #include "websock-w32.h"
#endif

#else /* WIN32 */
#include <syslog.h>
#include <unistd.h>
#endif /* WIN32 */

#include <signal.h>

#include <libwebsockets.h>
#if (LWS_LIBRARY_VERSION_MAJOR != 1 || LWS_LIBRARY_VERSION_MINOR != 7)
// Newer versions of lws have changed incompatibly.
#error "Version 1.7 of websockets is required, but have: " ## LWS_LIBRARY_VERSION
#endif

//#define LWS_NO_EXTENSIONS

#ifdef EXTERNAL_POLL
static int max_poll_elements;
static struct pollfd *jws_pollfds;
static int *fd_lookup;
static int jws_num_pollfds;
static int force_exit = 0;
#endif /* EXTERNAL_POLL */

typedef enum {  PWS_DEAD, PWS_HTTP, PWS_CONNECTED, PWS_RECV, PWS_SENT, PWS_SENDERR } pws_state;
enum { JWS_SIG_SYS=0xdeadbeea, JWS_SIG_OBJ, JWS_SIG_PWS };

#ifndef NDEBUG
#ifndef _JSI_MEMCLEAR
#define _JSI_MEMCLEAR(s) memset(s, 0, sizeof(*s));
#endif
#else
#define _JSI_MEMCLEAR(s)
#endif
#define WSSIGASSERT(s,n) assert(s->sig == JWS_SIG_##n)

enum {
    /* always first */
    JWS_PROTOCOL_HTTP = 0,
    JWS_PROTOCOL_WEBSOCK,
    /* always last */
    JWS_PROTOCOL__MAX
};

#ifdef interface
#undef interface
#endif

typedef struct { /* Interp wide data. */
    uint sig;
    Jsi_Interp *interp;
    Jsi_Hash *wsTable;
    int wIdx;
} jws_ObjInterpData;

typedef struct {
    int sentCnt, recvCnt, recvErrCnt, sentErrCnt, httpCnt, uploadCnt;
    time_t sentLast, recvLast, recvErrLast, sentErrLast, httpLast;
    time_t uploadStart, uploadLast, uploadEnd;
    int redirCnt;
    time_t redirLast;
    int connectCnt;
} jws_StatData;

typedef struct { /* Per server (or client) data. */
    uint sig;
    jws_ObjInterpData *interpData;
    Jsi_Interp *interp;
    Jsi_Hash *pssTable;
    Jsi_Value *onAuth, *onCloseLast, *onClose, *onConnect, *onOpen, *onRecv,
        *onUpload, *redirectUrl, *defaultUrl, *onGet, *onUnknown,
        *rootdir, *interface, *address, *mimeTypes;
    bool client, noUpdate, noWebsock, noWarn, use_ssl, local, defHandlers, handlersPkg;
    int version;
    int idx;
    int port;
    int maxUpload;
    int maxDownload;
    jws_StatData stats;
    char *iface;
    const char *clientName;
    const char *clientIP;
    const char *useridPass;
    struct lws_context *instCtx;
    Jsi_Value *getRegexp;
    unsigned int oldus;
    int opts;
    int hasOpts;
    int debug;
    int maxConnects;
    int daemonize;
    int deleted;
    int close_test;
    int createCnt;
    int redirAllCnt;
    bool redirDisable;
    int recvBufSize, recvBufCnt;
    int recvBufTimeout;
    int lastRevCnt; // For update
    time_t createLast;
    time_t startTime;
    char *cmdName;
    struct lws *wsi_choked[20];
    int num_wsi_choked;
    struct lws *wsi;

    struct lws_context *context;
    struct lws_context_creation_info info;
    Jsi_Event *event;
    Jsi_Obj *fobj;
    Jsi_Hash *handlers;
    int objId;
    struct lws_protocols protocols[JWS_PROTOCOL__MAX+1];
    int ietf_version;
    int rx_buffer_size;
    char *ssl_cert_filepath;
    char *ssl_private_key_filepath;
    int ws_uid;
    int ws_gid;
    char *cl_host;
    char *cl_origin;
    // Preserve headers from http for use in websockets.
    int sfd;        // File descriptor for http.
    int hdrSz[200]; // Space for up to 100 headers
    int hdrNum;     // Num of above.
    Jsi_DString dHdrs; // Store header string here.
} jws_CmdObj;

typedef struct { /* Per session connection (to each server) */
    uint sig;
    jws_CmdObj *cmdPtr;
    pws_state state;
    jws_StatData stats;
    struct lws *wsi;
    Jsi_HashEntry *hPtr;
    void *user;
    int cnt;
    lws_filefd_type fd;
    int wid;
    int sfd;
    bool isWebsock;
    const char *clientName;
    const char *clientIP;
    int hdrSz[200]; // Space for up to 100 headers
    int hdrNum;     // Num of above.
    
    // Pointers to reset.
    Jsi_DString dHdrs; // Store header string here.
    Jsi_Stack *stack;
    Jsi_DString uploadData; // Space for file upload data.
    Jsi_DString recvBuf; // To buffer recv when recvJSON is true.
} jws_Pss;

typedef struct {
    Jsi_Value *val, *objVar;
    const char *arg;
    int triedLoad;
    int flags;
} jws_Hander;

#ifndef jsi_IIOF
#define jsi_IIOF .flags=JSI_OPT_INIT_ONLY
#define jsi_IIRO .flags=JSI_OPT_READ_ONLY
#endif

static Jsi_OptionSpec WPSStats[] =
{
    JSI_OPT(INT,        jws_StatData, connectCnt,   .help="Number of active connections", jsi_IIRO),
    JSI_OPT(INT,        jws_StatData, httpCnt,      .help="Number of http reqs", jsi_IIRO),
    JSI_OPT(TIME_T,     jws_StatData, httpLast,     .help="Time of last http reqs", jsi_IIRO),
    JSI_OPT(INT,        jws_StatData, recvCnt,      .help="Number of recieves", jsi_IIRO),
    JSI_OPT(TIME_T,     jws_StatData, recvLast,     .help="Time of last recv", jsi_IIRO),
    JSI_OPT(TIME_T,     jws_StatData, redirLast,    .help="Time of last redirect", jsi_IIRO),
    JSI_OPT(INT,        jws_StatData, redirCnt,     .help="Count of redirects", jsi_IIRO),
    JSI_OPT(INT,        jws_StatData, sentCnt,      .help="Number of sends", jsi_IIRO),
    JSI_OPT(TIME_T,     jws_StatData, sentLast,     .help="Time of last send", jsi_IIRO),
    JSI_OPT(INT,        jws_StatData, sentErrCnt,   .help="Number of sends", jsi_IIRO),
    JSI_OPT(TIME_T,     jws_StatData, sentErrLast,  .help="Time of last sendErr", jsi_IIRO),
    JSI_OPT(TIME_T,     jws_StatData, sentErrLast,  .help="Time of last sendErr", jsi_IIRO),
    JSI_OPT(INT,        jws_StatData, uploadCnt,    .help="Number of uploads", jsi_IIRO),
    JSI_OPT(TIME_T,     jws_StatData, uploadEnd,    .help="Time of upload end", jsi_IIRO),
    JSI_OPT(TIME_T,     jws_StatData, uploadLast,   .help="Time of last upload input", jsi_IIRO),
    JSI_OPT(TIME_T,     jws_StatData, uploadStart,  .help="Time of upload start", jsi_IIRO),
    JSI_OPT_END(jws_StatData, .help="Per-connection statistics")
};

static Jsi_OptionSpec WPSOptions[] =
{
    JSI_OPT(STRKEY,     jws_Pss, clientIP,     .help="Client IP Address", jsi_IIRO),
    JSI_OPT(STRKEY,     jws_Pss, clientName,   .help="Client hostname", jsi_IIRO),
    JSI_OPT(CUSTOM,     jws_Pss, stats,        .help="Statistics for connection", jsi_IIRO, .custom=Jsi_Opt_SwitchSuboption, .data=WPSStats),
    JSI_OPT(BOOL,       jws_Pss, isWebsock,    .help="Socket has been upgraded to a websocket connection" ),
    JSI_OPT(DSTRING,    jws_Pss, uploadData,   .help="Uploaded data (raw)" ),
    JSI_OPT_END(jws_Pss, .help="Per-connection options")
};

static Jsi_OptionSpec WSOptions[] =
{
    JSI_OPT(STRING, jws_CmdObj, address,    .help="In client-mode the address to connect to (127.0.0.1)" ),
    JSI_OPT(BOOL,   jws_CmdObj, client,     .help="Run in client mode", jsi_IIOF),
    JSI_OPT(INT,    jws_CmdObj, debug,      .help="Set debug level. Setting this to 512 will turn on max libwebsocket log levels"),
    JSI_OPT(STRING, jws_CmdObj, defaultUrl, .help="Default when no url or / is given"),
    JSI_OPT(BOOL,   jws_CmdObj, defHandlers,.help="Enable the standard builtin handlers, ie: .htmli, .cssi and .jsi", jsi_IIOF),
    JSI_OPT(REGEXP, jws_CmdObj, getRegexp,  .help="Call onGet() only if Url matches pattern"),
//    JSI_OPT(CUSTOM, jws_CmdObj, handlersPkg,.help="Handlers use package() to upgrade string to function object"),
    JSI_OPT(STRING, jws_CmdObj, interface,  .help="Interface for server to listen on, eg. 'eth0' or 'lo'", jsi_IIOF),
    JSI_OPT(BOOL,   jws_CmdObj, local,      .help="Limit connections to localhost addresses on the 127 network"),
    JSI_OPT(INT,    jws_CmdObj, maxConnects,.help="In server mode, max number of client connections accepted"),
    JSI_OPT(OBJ,    jws_CmdObj, mimeTypes,  .help="Object providing map of file extensions to mime types (eg. {txt:'text/plain', bb:'text/bb'})", jsi_IIOF),
    JSI_OPT(BOOL,   jws_CmdObj, noUpdate,   .help="Disable update event-processing (eg. to exit)"),
    JSI_OPT(BOOL,   jws_CmdObj, noWebsock,  .help="Serve html, but disallow websocket upgrade", jsi_IIOF),
    JSI_OPT(BOOL,   jws_CmdObj, noWarn,     .help="Quietly ignore file related errors"),
    JSI_OPT(FUNC,   jws_CmdObj, onAuth,     .help="Function to call for http basic authentication", .flags=0, .custom=0, .data=(void*)"id:number, url:string, userpass:string"),
    JSI_OPT(FUNC,   jws_CmdObj, onClose,    .help="Function to call when the websocket connection closes", .flags=0, .custom=0, .data=(void*)"id:number"),
    JSI_OPT(FUNC,   jws_CmdObj, onCloseLast,.help="Function to call when last websock connection closes", .flags=0, .custom=0, .data=(void*)""),
    JSI_OPT(FUNC,   jws_CmdObj, onConnect,  .help="Function to call on a new http connection, returns false to kill", .flags=0, .custom=0, .data=(void*)"id:number"),
    JSI_OPT(FUNC,   jws_CmdObj, onGet,      .help="Function to call to server out content", .flags=0, .custom=0, .data=(void*)"id:number, url:string, args:object"),
    JSI_OPT(FUNC,   jws_CmdObj, onOpen,     .help="Function to call when the websocket connection occurs", .flags=0, .custom=0, .data=(void*)"id:number"),
    JSI_OPT(FUNC,   jws_CmdObj, onUnknown,  .help="Function to call to server out content when no file exists", .flags=0, .custom=0, .data=(void*)"id:number, url:string, args:object"),
    JSI_OPT(FUNC,   jws_CmdObj, onUpload,   .help="Function to call when upload starts or completes", .flags=0, .custom=0, .data=(void*)"id:number, complete:boolean"),
    JSI_OPT(FUNC,   jws_CmdObj, onRecv,     .help="Function to call when websock data recieved", .flags=0, .custom=0, .data=(void*)"id:number, data:string"),
    JSI_OPT(INT,    jws_CmdObj, maxDownload,.help="Max size of file download"),
    JSI_OPT(INT,    jws_CmdObj, maxUpload,  .help="Max size of file upload to accept"),
    JSI_OPT(INT,    jws_CmdObj, port,       .help="Port for server to listen on (8080)", jsi_IIOF),
    JSI_OPT(INT,    jws_CmdObj, recvBufSize,.help="Large recv buffer size, eg. to ensure full JSON was been received. -1=disable, 0=default of 1024", jsi_IIOF),
    JSI_OPT(INT,    jws_CmdObj, recvBufTimeout,.help="Timeout wait for recv to finish.  0=default of 60 seconds", jsi_IIOF),
    JSI_OPT(STRING, jws_CmdObj, redirectUrl,.help="Redirect to, when no url or / is given"),
    JSI_OPT(BOOL,   jws_CmdObj, redirDisable,.help="Disable redirects"),
    JSI_OPT(VALUE,  jws_CmdObj, rootdir,    .help="Directory to serve html from (\".\")"),
    JSI_OPT(CUSTOM, jws_CmdObj, stats,      .help="Statistical data", jsi_IIRO, .custom=Jsi_Opt_SwitchSuboption, .data=WPSStats),
    JSI_OPT(TIME_T, jws_CmdObj, startTime,  .help="Time of websocket start", jsi_IIRO),
    JSI_OPT(BOOL,   jws_CmdObj, use_ssl,    .help="Use https (for client)", jsi_IIOF),
    JSI_OPT(STRKEY, jws_CmdObj, useridPass, .help="The USER:PASSWD to use for basic authentication"),
    JSI_OPT(INT,    jws_CmdObj, version,    .help="Version number compiled against", jsi_IIRO),
    JSI_OPT_END(jws_CmdObj, .help="Websocket options")
};

static Jsi_RC jws_websocketObjFree(Jsi_Interp *interp, void *data);
static bool jws_websocketObjIsTrue(void *data);
static bool jws_websocketObjEqual(void *data1, void *data2);


/* this protocol server (always the first one) just knows how to do HTTP */

static int
jws_callback_http(struct lws *wsi,
      enum lws_callback_reasons reason,
      void *user, void *in, size_t len);
static int
jws_callback_websock(struct lws *wsi,
      enum lws_callback_reasons reason,
      void *user, void *in, size_t len);

#if 0
/* list of supported protocols and callbacks */
static struct lws_protocols protocols[] = {
    /* first protocol must always be HTTP handler */

    {
        .name="http-only",
        .callback=jws_callback_http,
        .per_session_data_size=0,
        .rx_buffer_size=0,
    },
    {
        .name="jsi-protocol",
        .callback=jws_callback_websock,
        .per_session_data_size=sizeof(jws_Pss),
        .rx_buffer_size=0,
    },

    { NULL, NULL, 0, 0 } /* terminator */
};
#endif

static jws_Pss*
jws_getPss(jws_CmdObj *cmdPtr, struct lws *wsi, void *user, int create, int ishttp)
{
    Jsi_HashEntry *hPtr;
    bool isNew = 0;
    jws_Pss *pss = (jws_Pss*)user;
    if (user==NULL)
        return NULL;
    int sfd = lws_get_socket_fd(wsi);
    if (sfd<0) {
        //puts("SFD negative");
        return NULL;
    }
    int sid = ((sfd<<1)|ishttp);
    if (create)
        hPtr = Jsi_HashEntryNew(cmdPtr->pssTable, (void*)(intptr_t)sid, &isNew);
    else
        hPtr = Jsi_HashEntryFind(cmdPtr->pssTable, (void*)(intptr_t)sid);
    if (!hPtr)
        return NULL;
    //printf("PSS FD(http=%d, create=%d) = %d\n", ishttp, create, sfd);
    if (create == 0 || isNew == 0) {
        WSSIGASSERT(pss, PWS);
        return pss;
    }
    memset(pss, 0, sizeof(*pss));
    if (!ishttp) {
        cmdPtr->stats.connectCnt++;
        cmdPtr->createCnt++;
        cmdPtr->createLast = time(NULL);
        if (Jsi_DSLength(&cmdPtr->dHdrs)) {
            Jsi_DSAppend(&pss->dHdrs, Jsi_DSValue(&cmdPtr->dHdrs), NULL);
            Jsi_DSFree(&pss->uploadData);
            memcpy(pss->hdrSz, cmdPtr->hdrSz, sizeof(cmdPtr->hdrSz));
        }
        pss->hdrNum = cmdPtr->hdrNum;

        jws_Pss *hpss;
        Jsi_HashEntry *hPtr2 = Jsi_HashEntryFind(cmdPtr->pssTable, (void*)(intptr_t)(sid|1));
        if (0 && hPtr2 && (hpss = (jws_Pss *)Jsi_HashValueGet(hPtr2))) {
            //printf("FOUND HTTP: %d\n", sfd);
            // Transcribe http to websocket.
            memcpy(pss, hpss, sizeof(*pss));
            // Reset pointers.
            pss->stack = 0;
            Jsi_DSInit(&pss->dHdrs);
            Jsi_DSInit(&pss->uploadData);
            if (Jsi_DSLength(&hpss->dHdrs))
                Jsi_DSAppend(&pss->dHdrs, Jsi_DSValue(&hpss->dHdrs), NULL);
        }
    }
    pss->isWebsock = !ishttp;
    pss->sig = JWS_SIG_PWS;
    pss->hPtr = hPtr;
    Jsi_HashValueSet(hPtr, pss);
    pss->cmdPtr = cmdPtr;
    pss->wsi = wsi;
    pss->user = user; /* Same as pss. */
    pss->state = PWS_CONNECTED;
    pss->cnt = cmdPtr->idx++;
    pss->wid = sid;
    pss->sfd = sfd;
    return pss;
}

static Jsi_RC jws_DelPss(Jsi_Interp *interp, void *data) { Jsi_Free(data); return JSI_OK; }
static Jsi_RC jws_recv_flush(jws_CmdObj *cmdPtr, jws_Pss *pss);

static void
jws_deletePss(jws_Pss *pss)
{
    WSSIGASSERT(pss, PWS);
    if (pss->state == PWS_DEAD)
        return;
    jws_recv_flush(pss->cmdPtr, pss);
    if (pss->hPtr) {
        Jsi_HashValueSet(pss->hPtr, NULL);
        Jsi_HashEntryDelete(pss->hPtr);
        pss->hPtr = NULL;
    }
    if (pss->stack) {
        Jsi_StackFreeElements(pss->cmdPtr->interp, pss->stack, jws_DelPss);
        Jsi_StackFree(pss->stack);
    }
    jws_CmdObj*cmdPtr = pss->cmdPtr;
    cmdPtr->sfd = pss->sfd;
    Jsi_DSFree(&cmdPtr->dHdrs);
    if (Jsi_DSLength(&pss->dHdrs)) {
        Jsi_DSAppend(&cmdPtr->dHdrs, Jsi_DSValue(&pss->dHdrs), NULL);
        memcpy(cmdPtr->hdrSz, pss->hdrSz, sizeof(cmdPtr->hdrSz));
        cmdPtr->hdrNum = pss->hdrNum;
    }
    pss->clientName = cmdPtr->clientName;
    pss->clientIP = cmdPtr->clientIP;
    Jsi_DSFree(&pss->dHdrs);
    Jsi_DSFree(&pss->uploadData);
    if (pss->isWebsock)
        pss->cmdPtr->stats.connectCnt--;
    /*Jsi_ObjDecrRefCount(pss->msgs);*/
    pss->state = PWS_DEAD;
}

static int jws_write(jws_Pss* pss, struct lws *wsi, unsigned char *buf, int len, enum lws_write_protocol proto)
{
    jws_CmdObj *cmdPtr = pss->cmdPtr;
    int m = lws_write(wsi, buf, len, proto);
    if (m >= 0) {
        cmdPtr->stats.sentCnt++;
        cmdPtr->stats.sentLast = time(NULL);
        pss->stats.sentCnt++;
        pss->stats.sentLast = time(NULL);
    } else {
        pss->state = PWS_SENDERR;
        pss->stats.sentErrCnt++;
        pss->stats.sentErrLast = time(NULL);
        cmdPtr->stats.sentErrCnt++;
        cmdPtr->stats.sentErrLast = time(NULL);
    }    
    return m;
}

static int jws_ServeString(jws_Pss *pss, struct lws *wsi,
    const char *buf, int code, const char *reason, const char *mime)
{
    int rc, strLen = Jsi_Strlen(buf);
    Jsi_DString jStr = {};
    Jsi_DSPrintf(&jStr,
        "HTTP/1.0 %d %s\x0d\x0a"
        "Server: libwebsockets\x0d\x0a"
        "Content-Type: %s\x0d\x0a"
        "Content-Length: %u\x0d\x0a\x0d\x0a",
        (code<=0?200:code), (reason?reason:"OK"),
        (mime?mime:"text/html"),
        strLen);
    Jsi_DSAppend(&jStr, buf, NULL);
    char *vStr = Jsi_DSValue(&jStr);
    rc = jws_write(pss, wsi, (unsigned char*)vStr, Jsi_Strlen(vStr), LWS_WRITE_HTTP);
    Jsi_DSFree(&jStr);
    return rc;
}

static const char*
jws_Header(jws_Pss *pss, const char *name, int *lenPtr)
{
    int i, nlen = Jsi_Strlen(name);
    const char *ret = NULL, *cp = Jsi_DSValue(&pss->dHdrs);
    for (i=0; i<pss->hdrNum; i+=2) {
        int sz = pss->hdrSz[i];
        int mat = (!Jsi_Strncasecmp(cp, name, nlen) && cp[nlen]=='=');
        cp += 1 + sz;
        if (mat) {
            ret = cp;
            if (lenPtr)
                *lenPtr = pss->hdrSz[i+1];
            break;
        }
        cp += (1 + pss->hdrSz[i+1]);
    }
    return ret;
}


static int
jws_GetHeaders(jws_Pss *pss, struct lws *wsi, Jsi_DString* dStr, int lens[], int hmax)
{
    int n = 0, i = 0;
    char buf[1000];
    const char *cp;
    while ((cp = (char*)lws_token_to_string((enum lws_token_indexes)n))) {
        int len = lws_hdr_copy(wsi, buf, sizeof(buf), ( enum lws_token_indexes)n);
        n++;
        if (i>=(n*2+2)) break;
        if (len<=0) continue;
        buf[sizeof(buf)-1] = 0;
        if (!buf[0]) continue;
        Jsi_DSAppend(dStr, cp, "=", buf, "\n", NULL);
        if (lens) {
            lens[i++] = Jsi_Strlen(cp);
            lens[i++] = Jsi_Strlen(buf);
        }
    }
    return i;
}

static Jsi_Value*
jws_DumpHeaders(jws_CmdObj *cmdPtr, struct lws *wsi)
{
    int n = 0;
    char buf[BUFSIZ];
    Jsi_Interp *interp = cmdPtr->interp;
    Jsi_Obj *nobj = Jsi_ObjNewType(interp, JSI_OT_OBJECT);
    Jsi_Value *nv, *ret = Jsi_ValueMakeObject(interp, NULL, nobj);
    Jsi_ValueMakeObject(interp, &ret, nobj);
#ifdef JSI_MEM_DEBUG
    jsi_ValueDebugLabel(ret, "websock", "dump_handshake");
#endif
    const unsigned char *cp;
    while ((cp = lws_token_to_string((enum lws_token_indexes)n))) {
       lws_hdr_copy(wsi, buf, sizeof(buf), (enum lws_token_indexes)n);
        buf[sizeof(buf)-1] = 0;
        if (!buf[0]) continue;
        nv = Jsi_ValueNewStringDup(interp, buf);
        Jsi_ObjInsert(interp, nobj, (char*)cp, nv, 0);
        n++;
    }
    return ret;
}


static void jws_getUriArgsJSON(struct lws *wsi, Jsi_DString *dStr)
{
    int n = 0;
    char buf[BUFSIZ];
    int cnt = 0;
    while (lws_hdr_copy_fragment(wsi, buf, sizeof(buf), WSI_TOKEN_HTTP_URI_ARGS, n++) > 0) 
    {
         char *cp, *value = buf;
        if ((cp = Jsi_Strchr(buf, '=')))
        {
            *cp = 0;
            value = cp+1;
        }
        Jsi_DSAppend(dStr,  (cnt++?", ":""), "\"", buf, "\":\"", value, "\"", NULL);
    }
}


static Jsi_RC jws_GetCmd(Jsi_Interp *interp, jws_CmdObj *cmdPtr, jws_Pss* pss, struct lws *wsi, const char *inPtr, Jsi_Value *cmd)
{ 
    Jsi_RC jrc;
    Jsi_Value *retStr = Jsi_ValueNew1(interp);
    Jsi_DString dStr = {};
    Jsi_DSPrintf(&dStr, "[%g, \"%s\", {", (Jsi_Number)pss->wid, inPtr );
    jws_getUriArgsJSON(wsi, &dStr);
    Jsi_DSAppend(&dStr, "}]", NULL);
    jrc = Jsi_FunctionInvokeJSON(interp, cmd, Jsi_DSValue(&dStr), &retStr);
    Jsi_DSSetLength(&dStr, 0);
    const char *rstr;
    if (jrc != JSI_OK)
        rstr = "Error";
    else
        rstr = Jsi_ValueGetDString(interp, retStr, &dStr, 0);
    int srv = (rstr[0] != 0);
    if (srv)
        jws_ServeString(pss, wsi, rstr, jrc==JSI_OK?0:404, NULL, NULL);
    else
        jrc = JSI_BREAK;
    Jsi_DecrRefCount(interp, retStr);
    Jsi_DSFree(&dStr);
    return jrc;
}

static int jws_callback_http(struct lws *wsi,
                         enum lws_callback_reasons reason, void *user,
                         void *in, size_t len)
{
    struct lws_context *context = lws_get_context(wsi);
    char buf[BUFSIZ];
    //int sfd;
    const char *ext = NULL;
    const char *inPtr = (char*)in;
    unsigned char buffer[BUFSIZ+LWS_PRE];
    time_t ostart = 0;
    char client_name[128], client_ip[128];
    // int n;
#ifdef EXTERNAL_POLL
    int m;
    int fd = (int)(long)user;
#endif
    jws_Pss *npss = NULL, *pss = (jws_Pss *)user;
    jws_CmdObj *cmdPtr = (jws_CmdObj *)lws_context_user(context);
    Jsi_Interp *interp = cmdPtr->interp;
    int rc = 0;

    buf[0] = 0;
    WSSIGASSERT(cmdPtr, OBJ);
    switch (reason) {
#ifndef EXTERNAL_POLL
    case LWS_CALLBACK_GET_THREAD_ID:
    case LWS_CALLBACK_UNLOCK_POLL:
    case LWS_CALLBACK_PROTOCOL_INIT:
    case LWS_CALLBACK_ADD_POLL_FD:
    case LWS_CALLBACK_DEL_POLL_FD:
    case LWS_CALLBACK_LOCK_POLL:
        return rc;
#else
        /*
         * callbacks for managing the external poll() array appear in
         * protocol 0 callback
         */

    case LWS_CALLBACK_ADD_POLL_FD:

        if (jws_num_pollfds >= max_poll_elements) {
            lwsl_err("LWS_CALLBACK_ADD_POLL_FD: too many sockets to track\n");
            return 1;
        }

        fd_lookup[fd] = jws_num_pollfds;
        jws_pollfds[jws_num_pollfds].fd = fd;
        jws_pollfds[jws_num_pollfds].events = (int)(long)len;
        jws_pollfds[jws_num_pollfds++].revents = 0;
        break;

    case LWS_CALLBACK_DEL_POLL_FD:
        if (!--jws_num_pollfds)
            break;
        m = fd_lookup[fd];
        /* have the last guy take up the vacant slot */
        jws_pollfds[m] = jws_pollfds[jws_num_pollfds];
        fd_lookup[jws_pollfds[jws_num_pollfds].fd] = m;
        break;

#endif
    
    default:
        break;

    }
    
    switch (reason) {
    case LWS_CALLBACK_FILTER_HTTP_CONNECTION:
        //if (cmdPtr->debug)
          //  jws_DumpHeaders(cmdPtr, wsi);
        break;
    case LWS_CALLBACK_PROTOCOL_INIT:
        break;
    case LWS_CALLBACK_CLOSED_HTTP:
        lws_get_socket_fd(wsi);
        //printf("CLOSED: %d\n", sfd);
        jws_deletePss(pss);
        break;
    case LWS_CALLBACK_WSI_CREATE:
    case LWS_CALLBACK_FILTER_NETWORK_CONNECTION:
        break;

    case LWS_CALLBACK_SERVER_NEW_CLIENT_INSTANTIATED:
        client_name[0] = 0;
        client_ip[0] = 0;
        lws_get_peer_addresses(wsi, lws_get_socket_fd(wsi), client_name,
                                         sizeof(client_name), client_ip, sizeof(client_ip));
        if (client_name[0])
            cmdPtr->clientName = Jsi_KeyAdd(interp, client_name);
        if (client_ip[0])
            cmdPtr->clientIP = Jsi_KeyAdd(interp, client_ip);
        
        if (cmdPtr->clientName || cmdPtr->clientIP) {
            cmdPtr->instCtx = context;
            if (cmdPtr->debug)
                fprintf(stderr,  "Received network connect from %s (%s)\n",
                     cmdPtr->clientName, cmdPtr->clientIP);
            if (cmdPtr->local && (cmdPtr->clientName && Jsi_Strcmp(cmdPtr->clientName, "localhost"))) {
                if (cmdPtr->debug)
                    fprintf(stderr,  "Dropping non-localhost connection\n");
                return 1;
            }
        }

        //if (cmdPtr->debug)
        //    jws_DumpHeaders(cmdPtr, wsi);
        if (cmdPtr->maxConnects && cmdPtr->stats.connectCnt>=cmdPtr->maxConnects) {
            if (cmdPtr->debug)
                fprintf(stderr, "maxConnects exceeded: rejecting connection <%p>\n", user);
            rc = -1;
        }
        /* if we returned non-zero from here, we kill the connection */
        break;

    case LWS_CALLBACK_HTTP:
    {
        const char *mime = "text/html";
        time_t now = time(NULL);

        /* if a legal POST URL, let it continue and accept data */
        if (lws_hdr_total_length(wsi, WSI_TOKEN_POST_URI))
            return 0;

        if (cmdPtr->redirDisable && difftime(now, cmdPtr->stats.redirLast)>=600)
            cmdPtr->redirDisable = 0;
        if (cmdPtr->defaultUrl && (inPtr == 0 || *inPtr == 0 || !Jsi_Strcmp(inPtr, "/")) && 
            (inPtr = Jsi_ValueString(cmdPtr->interp, cmdPtr->defaultUrl, NULL)) && inPtr[0]) {
            // replaced.
        }
        else if (cmdPtr->redirectUrl && (inPtr == 0 || *inPtr == 0 || !Jsi_Strcmp(inPtr, "/")) && !cmdPtr->redirDisable) {
            inPtr = Jsi_ValueString(cmdPtr->interp, cmdPtr->redirectUrl, NULL);
            if (inPtr) {
                cmdPtr->stats.redirCnt++;
                if (cmdPtr->stats.redirLast && difftime(now, cmdPtr->stats.redirLast)<300 && ++cmdPtr->redirAllCnt>1000)
                    cmdPtr->redirDisable = 1;
                cmdPtr->stats.redirLast = now;
                snprintf(buf, sizeof(buf), "<head><meta http-equiv=\"refresh\" content=\"1; url=%s\" /></head><body>Redirecting...</body>", inPtr);
                rc = jws_ServeString(pss, wsi, buf, 0, NULL, NULL);
                break;
            }
        }
        npss = jws_getPss(cmdPtr, wsi, user, 1, 1);
        if (cmdPtr->instCtx == context && (cmdPtr->clientName[0] || cmdPtr->clientIP[0])) {
            pss->clientName = cmdPtr->clientName;
            pss->clientIP = cmdPtr->clientIP;
        }
            
        Jsi_DSSetLength(&pss->dHdrs, 0);
        pss->hdrNum = jws_GetHeaders(pss, wsi, &pss->dHdrs, pss->hdrSz, sizeof(pss->hdrSz)/sizeof(int));

        if (!inPtr)
            inPtr = "/";

        if (cmdPtr->useridPass || cmdPtr->onAuth) {
            int ok = 0;
            int alen;
            const char *auth = jws_Header(pss, "authorization", &alen);
            if (auth && Jsi_Strncasecmp(auth, "basic ", 6)) {
                auth += 6;
                Jsi_DString eStr = {}, bStr = {};
                Jsi_DSAppendLen(&eStr, auth, alen);
                Jsi_Base64(Jsi_DSValue(&eStr), -1, &bStr, 1);
                const char *bp = Jsi_DSValue(&bStr);
                if (bp && bp[0]) {
                    if (!cmdPtr->onAuth)
                        ok = (!Jsi_Strcmp(cmdPtr->useridPass, bp));
                    else {
                        /* Pass 3 args: id, url and userid:pass . */
                        Jsi_Obj *oarg1;
                        Jsi_Value *vpargs, *vargs[3];
                        npss = jws_getPss(cmdPtr, wsi, user, 1, 1);
                        vargs[0] = Jsi_ValueNewNumber(interp, (Jsi_Number)(pss->wid));
                        vargs[1] = Jsi_ValueNewStringDup(interp, inPtr);
                        vargs[2] = Jsi_ValueNewStringDup(interp, bp);
                        vpargs = Jsi_ValueMakeObject(interp, NULL, oarg1 = Jsi_ObjNewArray(interp, vargs, 3, 0));
                        Jsi_IncrRefCount(interp, vpargs);
                        bool rb = Jsi_FunctionInvokeBool(interp, cmdPtr->onAuth, vpargs);
                        if (Jsi_InterpGone(interp))
                            return JSI_ERROR;
            
                        Jsi_DecrRefCount(interp, vpargs);
                        if (rb != 0 && rb != 1)
                            rc = JSI_ERROR;
                        if (rc != JSI_OK) 
                            return Jsi_LogError("websock bad rcv eval");
                        ok = rb;
                    }        
                }
                Jsi_DSFree(&eStr);
                Jsi_DSFree(&bStr);
            }
            if (!ok) {
                snprintf(buf, sizeof(buf), "HTTP/1.0 401 unauthorized\x0d\x0a"
                 "WWW-Authenticate: Basic realm=\"jsirealm\"\x0d\x0a"
                 "Content-Length: 0\x0d\x0a"
                 "\x0d\x0a" );
                jws_write(pss, wsi, (unsigned char *)buf, Jsi_Strlen(buf), LWS_WRITE_HTTP);

                return -1;
            }
        }

        if (cmdPtr->onConnect) {
            // 1 arg id
            int killcon = 0;
            Jsi_Obj *oarg1;
            Jsi_Value *vpargs, *vargs[2], *ret = Jsi_ValueNew1(interp);
            npss = jws_getPss(cmdPtr, wsi, user, 1, 1);
            vargs[0] = Jsi_ValueNewNumber(interp, (Jsi_Number)(pss->wid));
            Jsi_IncrRefCount(interp, vargs[0]);
            vpargs = Jsi_ValueMakeObject(interp, NULL, oarg1 = Jsi_ObjNewArray(interp, vargs, 1, 1));
            Jsi_DecrRefCount(interp, vargs[0]);
            Jsi_IncrRefCount(interp, vpargs);
            Jsi_ValueMakeUndef(interp, &ret);
            rc = Jsi_FunctionInvoke(interp, cmdPtr->onConnect, vpargs, &ret, NULL);
            if (Jsi_InterpGone(interp))
                return 1;
            if (rc == JSI_OK && Jsi_ValueIsFalse(interp, ret)) {
                if (cmdPtr->debug)
                    fprintf(stderr, "WS:KILLING CONNECTION: %p\n", pss);
                killcon = 1;
            }

            Jsi_DecrRefCount(interp, vpargs);
            Jsi_DecrRefCount(interp, ret);
            if (rc != JSI_OK) {
                Jsi_LogError("websock bad rcv eval");
                return 1;
            }
            if (killcon)
                return 1;
        }
        
        if (cmdPtr->onGet) {
            Jsi_RC jrc;
            int rrv = 1;
            if (cmdPtr->getRegexp) {
                rrv = 0;
                jrc = Jsi_RegExpMatch(interp, cmdPtr->getRegexp, inPtr, &rrv, NULL);
                if (jrc != JSI_OK)
                    return 1;
            }
            if (rrv) {
                jrc = jws_GetCmd(interp, cmdPtr, pss, wsi, inPtr, cmdPtr->onGet);
                if (jrc == JSI_ERROR)
                    return 1;
                if (jrc == JSI_OK)
                    break;
            }
        }
        ext = Jsi_Strrchr(inPtr, '.');
        
        const char *rootdir = (cmdPtr->rootdir?Jsi_ValueString(cmdPtr->interp, cmdPtr->rootdir, NULL):"./");
        char statPath[PATH_MAX];
        if (!Jsi_Strncmp(inPtr, "/jsi/", 5)) {
            // Get the path for system files, eg /zvfs or /usr/local/lib/jsi
            const char *loadFile = NULL;
            Jsi_PkgRequire(interp, "Sys", 0);
            if (Jsi_PkgVersion(interp, "Sys", &loadFile)>=0) {
                if (loadFile) {
                    snprintf(statPath, sizeof(statPath), "%s", loadFile);
                    char *lcp = Jsi_Strrchr(statPath, '/');
                    if (lcp) {
                        *lcp = 0;
                        inPtr += 5;
                        rootdir = statPath;
                    }
                }
            }
        }

        snprintf(buf, sizeof(buf), "%s/%s", rootdir, inPtr);
        if (ext) {
            Jsi_HashEntry *hPtr;

            if (Jsi_Strncasecmp(ext,".png", -1) == 0) mime = "image/png";
            else if (Jsi_Strncasecmp(ext,".ico", -1) == 0) mime = "image/icon";
            else if (Jsi_Strncasecmp(ext,".gif", -1) == 0) mime = "image/gif";
            else if (Jsi_Strncasecmp(ext,".jpeg", -1) == 0) mime = "image/jpeg";
            else if (Jsi_Strncasecmp(ext,".jpg", -1) == 0) mime = "image/jpeg";
            else if (Jsi_Strncasecmp(ext,".js", -1) == 0) mime = "application/x-javascript";
            else if (Jsi_Strncasecmp(ext,".jsi", -1) == 0) mime = "application/x-javascript";
            else if (Jsi_Strncasecmp(ext,".svg", -1) == 0) mime = "image/svg+xml";
            else if (Jsi_Strncasecmp(ext,".css", -1) == 0) mime = "text/css";
            else if (Jsi_Strncasecmp(ext,".cssi", -1) == 0) mime = "text/css";
            else if (Jsi_Strncasecmp(ext,".json", -1) == 0) mime = "application/json";
            else if (Jsi_Strncasecmp(ext,".txt", -1) == 0) mime = "text/plain";
            else if (Jsi_Strncasecmp(ext,".html", -1) == 0) mime = "text/html";
            else if (Jsi_Strncasecmp(ext,".htmli", -1) == 0) mime = "text/html";
            else if (cmdPtr->mimeTypes) {
                /* Lookup mime type in mimeTypes object. */
                const char *nmime;
                Jsi_Value *mVal = Jsi_ValueObjLookup(interp, cmdPtr->mimeTypes, ext+1, 1);
                if (mVal && ((nmime = Jsi_ValueString(interp, mVal, NULL))))
                    mime = nmime;
            }
            
            if ((hPtr = Jsi_HashEntryFind(cmdPtr->handlers, ext))) {
                /* Use interprete html eg. using jsi_wpp preprocessor */
                Jsi_DString jStr = {};
                Jsi_Value *vrc = NULL;
                int hrc = 0, strLen, evrc, isalloc=0;
                char *vStr, *hstr = NULL;
                jws_Hander *hdlPtr = (jws_Hander*)Jsi_HashValueGet(hPtr);
                Jsi_Value *hv = hdlPtr->val;
                
                if (Jsi_Strchr(buf, '\'') || Jsi_Strchr(buf, '\"')) {
                    jws_ServeString(pss, wsi, "Can not handle quotes in url", 404, NULL, NULL);    
                    return 1;                
                }
                cmdPtr->handlersPkg=1;
                
                // Attempt to load package and get function.
                if ((hdlPtr->flags&1) && cmdPtr->handlersPkg && Jsi_ValueIsString(interp, hv)
                    && ((hstr = Jsi_ValueString(interp, hv, NULL)))) {
                    vrc = Jsi_NameLookup(interp, hstr);
                    if (!vrc) {
                        int pver = Jsi_PkgRequire(interp, hstr, 0);
                        if (pver >= 0)
                            vrc = Jsi_NameLookup(interp, hstr);
                    }
                    if (!vrc || !Jsi_ValueIsFunction(interp, vrc)) {
                        if (vrc)
                            Jsi_DecrRefCount(interp, vrc);
                        Jsi_LogError("Failed to autoload handle: %s", hstr);
                        jws_ServeString(pss, wsi, "Failed to autoload handler", 404, NULL, NULL);    
                        return 1;                
                    }
                    if (hdlPtr->val)
                        Jsi_DecrRefCount(interp, hdlPtr->val);
                    hdlPtr->val = vrc;
                    Jsi_IncrRefCount(interp, vrc);
                    hv = vrc;
                }
                
                if ((hdlPtr->flags&2) && !hdlPtr->triedLoad && !hdlPtr->objVar && Jsi_ValueIsFunction(interp, hv)) {
                    // Run command and from returned object get the parse function.
                    hdlPtr->triedLoad = 1;
                    Jsi_DSAppend(&jStr, "[null", NULL); 
                    if (hdlPtr->arg)
                        Jsi_DSAppend(&jStr, ", ", hdlPtr->arg, NULL); // TODO: JSON encode.
                    Jsi_DSAppend(&jStr, "]", NULL);
                    vrc = Jsi_ValueNew1(interp);
                    evrc = Jsi_FunctionInvokeJSON(interp, hv, Jsi_DSValue(&jStr), &vrc);
                    if (evrc != JSI_OK || !vrc || !Jsi_ValueIsObjType(interp, vrc, JSI_OT_OBJECT)) {
                        Jsi_LogError("Failed to load obj: %s", hstr);
                        jws_ServeString(pss, wsi, "Failed to load obj", 404, NULL, NULL);    
                        return 1;                
                    }
                    Jsi_Value *fvrc = Jsi_ValueObjLookup(interp, vrc, "parse", 0);
                    if (!fvrc || !Jsi_ValueIsFunction(interp, fvrc)) {
                        Jsi_LogError("Failed to find parse: %s", hstr);
                        jws_ServeString(pss, wsi, "Failed to find parse", 404, NULL, NULL);    
                        return 1;                
                    }
                    hdlPtr->objVar = fvrc;
                    Jsi_IncrRefCount(interp, fvrc);
                    hv = vrc;
                    
                }

                if (hdlPtr->objVar) {  // Call the obj.parse function.
                    Jsi_DSAppend(&jStr, "[\"", buf, "\"]", NULL); // TODO: JSON encode.
                    vrc = Jsi_ValueNew1(interp);
                    evrc = Jsi_FunctionInvokeJSON(interp, hdlPtr->objVar, Jsi_DSValue(&jStr), &vrc);
                    isalloc = 1;
                }
                else if (Jsi_ValueIsFunction(interp, hv)) {
                    Jsi_DSAppend(&jStr, "[\"", buf, "\"", NULL); // TODO: JSON encode.
                    if (hdlPtr->arg)
                        Jsi_DSAppend(&jStr, ", ", hdlPtr->arg, NULL);
                    Jsi_DSAppend(&jStr, "]", NULL);
                    vrc = Jsi_ValueNew1(interp);
                    evrc = Jsi_FunctionInvokeJSON(interp, hv, Jsi_DSValue(&jStr), &vrc);
                    isalloc = 1;
                } else {
                    // One shot invoke of string command.
                    hstr = Jsi_ValueString(interp, hv, NULL);
                    Jsi_DSAppend(&jStr, hstr, "('", buf, "'", NULL);
                    if (hdlPtr->arg)
                        Jsi_DSAppend(&jStr, ", ", hdlPtr->arg, NULL);
                    Jsi_DSAppend(&jStr, ");", NULL);
                    evrc = Jsi_EvalString(interp, Jsi_DSValue(&jStr), JSI_EVAL_RETURN);
                    if (evrc == JSI_OK)
                        vrc = Jsi_InterpResult(interp);
                }
                // Take result from vrc and return it.
                if (evrc != JSI_OK) {
                    Jsi_LogError("failure in websocket handler");
                } else if ((!vrc) ||
                    (!(vStr = Jsi_ValueString(interp, vrc, &strLen)))) {
                    Jsi_LogError("failed to get result");
                } else {
                    Jsi_DSSetLength(&jStr, 0);
                    Jsi_DSPrintf(&jStr,
                        "HTTP/1.0 200 OK\x0d\x0a"
                        "Server: libwebsockets\x0d\x0a"
                        "Content-Type: %s\x0d\x0a"
                        "Content-Length: %u\x0d\x0a\x0d\x0a",
                        mime,
                        strLen);
                    vStr = Jsi_DSAppend(&jStr, vStr, NULL);
                    hrc = jws_write(pss, wsi, (unsigned char*)vStr, Jsi_Strlen(vStr), LWS_WRITE_HTTP);
                }
                Jsi_DSFree(&jStr);
                if (isalloc)
                    Jsi_DecrRefCount(interp, vrc);
                if (hrc<=0)
                    return 1;
                goto try_to_reuse;
            }
        }
        if (!buf[0]) {
            if (cmdPtr->debug)
                fprintf(stderr, "Unknown file: %s\n", inPtr);
            break;
        }
        Jsi_Value* fname = Jsi_ValueNewStringDup(interp, buf);
        Jsi_IncrRefCount(interp, fname);

        Jsi_StatBuf jsb;
        if (Jsi_Stat(interp, fname, &jsb) || jsb.st_size<=0) {
nofile:
            if (cmdPtr->onUnknown) {
                Jsi_RC jrc = jws_GetCmd(interp, cmdPtr, pss, wsi, inPtr, cmdPtr->onUnknown);
                if (jrc == JSI_ERROR)
                    return 1;
                if (jrc == JSI_OK)
                    break;
            }

            if (cmdPtr->noWarn==0 && Jsi_Strstr(buf, "favicon.ico")==0)
                fprintf(stderr, "failed open file for read: %s\n", buf);
            rc = jws_ServeString(pss, wsi, "<b style='color:red'>ERROR: can not serve file!</b>", 404, NULL, NULL);
            Jsi_DecrRefCount(interp, fname);
            break;
        }
        if (S_ISDIR(jsb.st_mode)) {
            if (cmdPtr->noWarn==0)
                fprintf(stderr, "can not serve directory: %s\n", buf);
            rc = jws_ServeString(pss, wsi, "<b style='color:red'>ERROR: can not serve directory!</b>", 404, NULL, NULL);
            Jsi_DecrRefCount(interp, fname);
            break;
        }

        if (Jsi_FSNative(interp, fname)) {     
            Jsi_DecrRefCount(interp, fname);
            // Large native files we send asyncronously.
            if (jsb.st_size>(BUFSIZ*4) && !Jsi_InterpSafe(interp)) {
    
                unsigned long file_len;
                unsigned char *p = buffer + LWS_PRE;
                unsigned char *end = p + sizeof(buffer) - LWS_PRE;
    
                pss->fd = lws_plat_file_open(wsi, buf, &file_len,
                                 LWS_O_RDONLY);
    
                if (pss->fd == LWS_INVALID_FILE) {
                    lwsl_err("faild to open file %s\n", buf);
                    return -1;
                }
    
                if (lws_add_http_header_status(wsi, 200, &p, end))
                    return 1;
                if (lws_add_http_header_by_token(wsi, WSI_TOKEN_HTTP_SERVER,
                            (unsigned char *)"libwebsockets",
                        13, &p, end))
                    return 1;
                if (lws_add_http_header_by_token(wsi,
                        WSI_TOKEN_HTTP_CONTENT_TYPE,
                            (unsigned char *)mime,
                        Jsi_Strlen(mime), &p, end))
                    return 1;
                if (lws_add_http_header_content_length(wsi,
                                       file_len, &p,
                                       end))
                    return 1;
                if (lws_finalize_http_header(wsi, &p, end))
                    return 1;
                *p = '\0';
                lwsl_info("%s\n", buffer + LWS_PRE);
    
                int n = jws_write(pss, wsi, buffer + LWS_PRE, p - (buffer + LWS_PRE),
                          LWS_WRITE_HTTP_HEADERS);
                if (n < 0) {
                    lws_plat_file_close(wsi, pss->fd);
                    return -1;
                }

                lws_callback_on_writable(wsi);
                break;
            }
            
            // Smaller files can go in one shot.
            int hrc = lws_serve_http_file(wsi, buf, mime, NULL, 0);
            if (hrc<0) {
                if (cmdPtr->noWarn==0)
                    fprintf(stderr, "can not serve file (%d): %s\n", hrc, buf);
                return 1;
            } else if (hrc > 0 && lws_http_transaction_completed(wsi))
                return -1;
        } else {
            // Need to read data for non-native files.
            Jsi_Channel chan = Jsi_Open(interp, fname, "rb");
            int n, sum = 0;

            if (!chan) {
                goto nofile;
            }
            Jsi_DString dStr = {};
            char sbuf[BUFSIZ];
            Jsi_DSPrintf(&dStr,
                "HTTP/1.0 200 OK\x0d\x0a"
                "Server: libwebsockets\x0d\x0a"
                "Content-Type: %s\x0d\x0a"
                "Content-Length: %u\x0d\x0a\x0d\x0a",
                mime,
                (unsigned int)jsb.st_size);
            while (sum < 10000000 && (n = Jsi_Read(chan, sbuf, sizeof(sbuf))) > 0) {
                Jsi_DSAppendLen(&dStr, sbuf, n);
                sum += n;
            }
            Jsi_Close(chan);
            char *str = Jsi_DSValue(&dStr);
            int hrc, strLen = Jsi_DSLength(&dStr);
            Jsi_DecrRefCount(interp, fname);
            hrc = jws_write(pss, wsi, (unsigned char*)str, strLen, LWS_WRITE_HTTP);
            Jsi_DSFree(&dStr);
            if (hrc<0) {
                if (cmdPtr->noWarn==0)
                    fprintf(stderr, "can not serve data (%d): %s\n", hrc, buf);
                return 1;
            } else if (hrc > 0 && lws_http_transaction_completed(wsi))
                return -1;

        }
        /*
         * notice that the sending of the file completes asynchronously,
         * we'll get a LWS_CALLBACK_HTTP_FILE_COMPLETION callback when
         * it's done
         */

        break;
    }

    case LWS_CALLBACK_HTTP_BODY:
        if (cmdPtr->maxUpload<=0 || !cmdPtr->onUpload) {
            if (cmdPtr->noWarn==0)
                fprintf(stderr, "Upload disabled: maxUpload=%d, onUpload=%p\n", cmdPtr->maxUpload, cmdPtr->onUpload);
            return -1;
        }
        cmdPtr->stats.uploadLast = pss->stats.uploadLast = time(NULL);
        Jsi_DSAppendLen(&pss->uploadData, inPtr, len);

        ostart = pss->stats.uploadStart;
        if (!pss->stats.uploadStart) {
            cmdPtr->stats.uploadEnd = pss->stats.uploadEnd = 0;
            cmdPtr->stats.uploadStart = pss->stats.uploadStart = time(NULL);
            cmdPtr->stats.uploadCnt++;
            pss->stats.uploadCnt++;
            Jsi_DSSetLength(&pss->uploadData, 0);
        }
        if (!ostart)
            break;

    case LWS_CALLBACK_HTTP_BODY_COMPLETION:
        if (!ostart)
            cmdPtr->stats.uploadEnd = pss->stats.uploadEnd = time(NULL);
        if (cmdPtr->onUpload) {
            /* Pass 2 args: id and bool=true for complete. */
            Jsi_Obj *oarg1;
            Jsi_Value *vpargs, *vargs[2];
            npss = jws_getPss(cmdPtr, wsi, user, 1, 1);
            if (npss != pss)
                fprintf(stderr, "bad pss\n");
            vargs[0] = Jsi_ValueNewNumber(interp, (Jsi_Number)(pss->wid));
            vargs[1] = Jsi_ValueNewNumber(interp, (ostart==0));
            vpargs = Jsi_ValueMakeObject(interp, NULL, oarg1 = Jsi_ObjNewArray(interp, vargs, 2, 0));
            Jsi_IncrRefCount(interp, vpargs);
            
            Jsi_Value *ret = Jsi_ValueNew1(interp);
            Jsi_ValueMakeUndef(interp, &ret);
            rc = Jsi_FunctionInvoke(interp, cmdPtr->onUpload, vpargs, &ret, NULL);
            if (Jsi_InterpGone(interp))
                return JSI_ERROR;

            Jsi_DecrRefCount(interp, vpargs);
            Jsi_DecrRefCount(interp, ret);
            if (rc != JSI_OK) {
                Jsi_LogError("websock bad rcv eval");
                goto errout;
            }
        }        
        lws_return_http_status(wsi, HTTP_STATUS_OK, NULL);
        goto try_to_reuse;

    case LWS_CALLBACK_HTTP_FILE_COMPLETION:
        goto try_to_reuse;

    case LWS_CALLBACK_HTTP_WRITEABLE: {
        lwsl_info("LWS_CALLBACK_HTTP_WRITEABLE\n");

        if (pss->fd == LWS_INVALID_FILE)
            goto try_to_reuse;

        /*
         * we can send more of whatever it is we were sending
         */
        unsigned long amount;
        int sent = 0;
        unsigned char buffer[BUFSIZ*10];
        do {
            int n = sizeof(buffer) - LWS_PRE;
            int m = lws_get_peer_write_allowance(wsi);
            if (m == 0)
                goto later;

            if (m != -1 && m < n)
                n = m;

            n = lws_plat_file_read(wsi, pss->fd,
                           &amount, buffer + LWS_PRE, n);
            if (n < 0) {
                lwsl_err("problem reading file\n");
                goto bail;
            }
            n = (int)amount;
            if (n == 0)
                goto penultimate;
            /*
             * To support HTTP2, must take care about preamble space
             *
             * identification of when we send the last payload frame
             * is handled by the library itself if you sent a
             * content-length header
             */
            m = jws_write(pss, wsi, buffer + LWS_PRE, n, LWS_WRITE_HTTP);
            if (m < 0) {
                lwsl_err("write failed\n");
                /* write failed, close conn */
                goto bail;
            }
            if (m) /* while still active, extend timeout */
                lws_set_timeout(wsi, PENDING_TIMEOUT_HTTP_CONTENT, 5);
            sent += m;

        } while (!lws_send_pipe_choked(wsi) && (sent < 500 * 1024 * 1024));
later:
        lws_callback_on_writable(wsi);
        break;
penultimate:
        lws_plat_file_close(wsi, pss->fd);
        pss->fd = LWS_INVALID_FILE;
        goto try_to_reuse;

bail:
        lws_plat_file_close(wsi, pss->fd);

        return -1;
    }

    default:
        break;
    }

    //if (npss) jws_deletePss(npss);
    return rc;
    
try_to_reuse:
    //if (npss) jws_deletePss(npss);
    if (lws_http_transaction_completed(wsi))
        return -1;

    return 0;
    
errout:
    //if (npss) jws_deletePss(npss);
    return -1;
}

static Jsi_RC jws_recv_callback(Jsi_Interp *interp, jws_CmdObj *cmdPtr, jws_Pss *pss,  const char *inPtr, int nlen)
{
    Jsi_Obj *oarg1;
    Jsi_Value *vpargs, *vargs[2];
    if (nlen<=0)
        return JSI_OK;
    vargs[0] = Jsi_ValueNewNumber(interp, (Jsi_Number)(pss->wid));
    vargs[1]  = Jsi_ValueNewString(interp, inPtr, nlen);
    vpargs = Jsi_ValueMakeObject(interp, NULL, oarg1 = Jsi_ObjNewArray(interp, vargs, 2, 0));
    Jsi_IncrRefCount(interp, vpargs);
    
    Jsi_Value *ret = Jsi_ValueNew1(interp);
    Jsi_ValueMakeUndef(interp, &ret);
    Jsi_RC rc = Jsi_FunctionInvoke(interp, cmdPtr->onRecv, vpargs, &ret, NULL);
    if (Jsi_InterpGone(interp))
        return JSI_ERROR;
    if (rc == JSI_OK && Jsi_ValueIsUndef(interp, ret)==0) {
        /* TODO: handle callback return data??? */
    }
    Jsi_DecrRefCount(interp, vpargs);
    Jsi_DecrRefCount(interp, ret);
    return rc;
}


static int
jws_callback_websock(struct lws *wsi,
      enum lws_callback_reasons reason,
      void *user, void *in, size_t len)
{
    struct lws_context *context = lws_get_context(wsi);

    jws_Pss *pss = (jws_Pss *)user;
    jws_CmdObj *cmdPtr = (jws_CmdObj *)lws_context_user(context);
    Jsi_Interp *interp = cmdPtr->interp;
    char *inPtr = (char*)in;
    int sLen, bSiz, n, rc =0, cnt = 0; /*, result = JSI_OK;*/;
#define LWSPAD (LWS_SEND_BUFFER_PRE_PADDING + LWS_SEND_BUFFER_POST_PADDING)
#define LBUFMAX (BUFSIZ+LWSPAD)
    char buf[LBUFMAX], *bufPtr = buf;
    char *statBuf = NULL;
    int statSize = 0;

    WSSIGASSERT(cmdPtr, OBJ);
    switch (reason) {
    case LWS_CALLBACK_PROTOCOL_INIT:
        if (cmdPtr->debug)
            fprintf(stderr, "WS:CALLBACK_INIT: %p\n", user);
        if (cmdPtr->noWebsock)
            return 1;
        //pss = jws_getPss(cmdPtr, wsi, user, 1, 0);
        break;
        
    case LWS_CALLBACK_FILTER_PROTOCOL_CONNECTION:
        //pss = jws_getPss(cmdPtr, wsi, user, 1, 0);
        if (cmdPtr->debug)
            fprintf(stderr, "WS:CALLBACK_FILTER: %p\n", pss);
        if (cmdPtr->onConnect) {
            int killcon = 0;
            Jsi_Obj *oarg1;
            Jsi_Value *vpargs, *vargs[2], *ret = Jsi_ValueNew1(interp);
            
            vargs[0] = jws_DumpHeaders(cmdPtr, wsi);
            Jsi_IncrRefCount(interp, vargs[0]);
            vpargs = Jsi_ValueMakeObject(interp, NULL, oarg1 = Jsi_ObjNewArray(interp, vargs, 1, 1));
            Jsi_DecrRefCount(interp, vargs[0]);
            Jsi_IncrRefCount(interp, vpargs);
            Jsi_ValueMakeUndef(interp, &ret);
            rc = Jsi_FunctionInvoke(interp, cmdPtr->onConnect, vpargs, &ret, NULL);
            if (Jsi_InterpGone(interp))
                return 1;
            if (rc == JSI_OK && Jsi_ValueIsFalse(interp, ret)) {
                if (cmdPtr->debug)
                    fprintf(stderr, "WS:KILLING CONNECTION: %p\n", pss);
                killcon = 1;
            }

            Jsi_DecrRefCount(interp, vpargs);
            Jsi_DecrRefCount(interp, ret);
            if (rc != JSI_OK) {
                Jsi_LogError("websock bad rcv eval");
                return 1;
            }
            if (killcon)
                return 1;
        }
        break;

    case LWS_CALLBACK_CLIENT_ESTABLISHED:
    case LWS_CALLBACK_ESTABLISHED:
        pss = jws_getPss(cmdPtr, wsi, user, 1, 0);
        if (cmdPtr->debug)
            fprintf(stderr, "WS:CALLBACK_ESTABLISHED: %d,%p\n", pss->wid, pss);
        if (cmdPtr->onOpen) {
            /* Pass 1 args: id. */
            Jsi_Obj *oarg1;
            Jsi_Value *vpargs, *vargs[2];
            vargs[0] = Jsi_ValueNewNumber(interp, (Jsi_Number)(pss->wid));
            vpargs = Jsi_ValueMakeObject(interp, NULL, oarg1 = Jsi_ObjNewArray(interp, vargs, 1, 0));
            Jsi_IncrRefCount(interp, vpargs);
            
            Jsi_Value *ret = Jsi_ValueNew1(interp);
            Jsi_ValueMakeUndef(interp, &ret);
            rc = Jsi_FunctionInvoke(interp, cmdPtr->onOpen, vpargs, &ret, NULL);
            if (Jsi_InterpGone(interp))
                return JSI_ERROR;

            Jsi_DecrRefCount(interp, vpargs);
            Jsi_DecrRefCount(interp, ret);
            if (rc != JSI_OK) 
                return Jsi_LogError("websock bad rcv eval");
        }        
        break;

    case LWS_CALLBACK_CLOSED:
    case LWS_CALLBACK_PROTOCOL_DESTROY:
        pss = (jws_Pss *)user;
        if (cmdPtr->debug)
            fprintf(stderr, "WS:CLOSE: %p\n", pss);
        if (!pss) break;
        if (cmdPtr->onClose) {
            /* Pass 2 args: id and data. */
            Jsi_Obj *oarg1;
            Jsi_Value *vpargs, *vargs[2];
            vargs[0] = Jsi_ValueNewNumber(interp, (Jsi_Number)(pss->wid));
            vpargs = Jsi_ValueMakeObject(interp, NULL, oarg1 = Jsi_ObjNewArray(interp, vargs, 1, 0));
            Jsi_IncrRefCount(interp, vpargs);
            
            Jsi_Value *ret = Jsi_ValueNew1(interp);
            Jsi_ValueMakeUndef(interp, &ret);
            rc = Jsi_FunctionInvoke(interp, cmdPtr->onClose, vpargs, &ret, NULL);
            if (Jsi_InterpGone(interp))
                return JSI_ERROR;

            Jsi_DecrRefCount(interp, vpargs);
            Jsi_DecrRefCount(interp, ret);
            if (rc != JSI_OK) 
                return Jsi_LogError("websock bad rcv eval");
        }        
        jws_deletePss(pss);
        if (cmdPtr->stats.connectCnt<=0 && cmdPtr->onCloseLast) {
            Jsi_FunctionInvokeBool(interp, cmdPtr->onCloseLast, NULL);
            if (Jsi_InterpGone(interp))
                return JSI_ERROR;
        }
        break;
    case LWS_CALLBACK_CLIENT_WRITEABLE:
    case LWS_CALLBACK_SERVER_WRITEABLE:
        pss = jws_getPss(cmdPtr, wsi, user, 0, 0);
        if (!pss) break;
        n=0;
        while (cnt++<100 && pss->stack) {
            char *data = (char*)Jsi_StackUnshift(pss->stack);
            unsigned char *p;
            if (data == NULL)
                break;
            pss->state = PWS_SENT;
            sLen = Jsi_Strlen(data);
            bSiz = sLen + LWSPAD;
            if (bSiz >= (int)LBUFMAX) {
                if (statBuf == NULL) {
                    statSize = bSiz+1+LBUFMAX;
                    statBuf = (char*)Jsi_Malloc(statSize);
                } else if (statSize <= bSiz) {
                    statSize = bSiz+1+LBUFMAX;
                    statBuf = (char*)Jsi_Realloc(statBuf, statSize);
                }
                bufPtr = statBuf;
            }
            // TODO: check output size
            p = (unsigned char *)bufPtr+LWS_SEND_BUFFER_PRE_PADDING;
            memcpy(p, data, sLen);
            Jsi_Free(data);
            n = jws_write(pss, wsi, p, sLen, LWS_WRITE_TEXT);
            if (cmdPtr->debug>=10)
                fprintf(stderr, "WS:CLIENT WRITE(%p): %d=>%d\n", pss, sLen, n);
                                   
            if (n >= 0) {
                cmdPtr->stats.sentCnt++;
                cmdPtr->stats.sentLast = time(NULL);
                pss->stats.sentCnt++;
                pss->stats.sentLast = time(NULL);
            } else {
                lwsl_err("ERROR %d writing to socket\n", n);
                pss->state = PWS_SENDERR;
                pss->stats.sentErrCnt++;
                pss->stats.sentErrLast = time(NULL);
                cmdPtr->stats.sentErrCnt++;
                cmdPtr->stats.sentErrLast = time(NULL);
                rc = 1;
                break;
            }

            // lwsl_debug("tx fifo %d\n", (ringbuffer_head - pss->ringbuffer_tail) & (MAX_MESSAGE_QUEUE - 1));

            /*if (lws_send_pipe_choked(wsi)) {
                    libwebsocket_callback_on_writable(context, wsi);
                    return 0;
            }*/
        }
        if (bufPtr && bufPtr != buf)
            Jsi_Free(bufPtr);
        break;
        
    case LWS_CALLBACK_CLIENT_RECEIVE:
    case LWS_CALLBACK_RECEIVE:
    {
        pss = jws_getPss(cmdPtr, wsi, user, 0, 0);
        if (!pss) break;
        if (cmdPtr->debug>=10)
            fprintf(stderr, "WS:RECV: %p\n", pss);

        //fprintf(stderr, "rx %d\n", (int)len);
        pss->stats.recvCnt++;
        pss->stats.recvLast = time(NULL);
        cmdPtr->stats.recvCnt++;
        cmdPtr->stats.recvLast = time(NULL);

        if (cmdPtr->onRecv ) {
            /* Pass 2 args: id and data. */
            int nlen = len, bsiz = cmdPtr->recvBufSize, rblen = Jsi_DSLength(&pss->recvBuf);
            if (nlen<=0)
                return 0;
            if (bsiz>=0 && nlen >= ((bsiz?bsiz:1024)-1)) { // Websockets buffers a max of 1K.
                if (rblen<=0) {
                    if (bsiz!=0 || rblen || Jsi_JSONParse(NULL, inPtr, NULL, 0) != JSI_OK) {
                        // Buffer if first chunk, user did not override buf size, and not a complete JSON msg.
                        cmdPtr->recvBufCnt++;
                        Jsi_DSAppendLen(&pss->recvBuf, inPtr, len);
                        break;
                    }
                } else {
                    Jsi_DSAppendLen(&pss->recvBuf, inPtr, len);
                    break;
                }
            } else if (rblen) {
                cmdPtr->recvBufCnt--;
                Jsi_DSAppendLen(&pss->recvBuf, inPtr, len);
                nlen = Jsi_DSLength(&pss->recvBuf);
                inPtr = Jsi_DSFreeDup(&pss->recvBuf);
            } else {
                Jsi_DString inStr;
                Jsi_DSInit(&inStr);
                Jsi_DSAppendLen(&inStr, inPtr, len);
                inPtr = Jsi_DSFreeDup(&inStr);
            }
            rc = jws_recv_callback(interp, cmdPtr, pss, inPtr, nlen);
        
            if (rc != JSI_OK) {
                Jsi_LogError("websock bad rcv eval");
                return 1;
            }
        }
        //if (!Jsi_Strlen(Jsi_GetStringResult(interp))) { }
        lws_callback_on_writable_all_protocol(cmdPtr->context, lws_get_protocol(wsi));

        //if (len < 6)
            //break;
        //if (Jsi_Strcmp((const char *)in, "reset\n") == 0)
        //pss->number = 0;
        break;
 
    }
    default:
        break;
    }
    return rc;
}


static Jsi_RC WebSocketConfCmd(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this,
    Jsi_Value **ret, Jsi_Func *funcPtr)
{
    jws_CmdObj *cmdPtr = (jws_CmdObj*)Jsi_UserObjGetData(interp, _this, funcPtr);
  
    if (!cmdPtr) 
        return Jsi_LogError("Apply in a non-websock object");
    return Jsi_OptionsConf(interp, WSOptions, cmdPtr, Jsi_ValueArrayIndex(interp, args, 0), ret, 0);

}

static Jsi_RC WebSocketIdConfCmd(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this,
    Jsi_Value **ret, Jsi_Func *funcPtr)
{
    jws_CmdObj *cmdPtr = (jws_CmdObj*)Jsi_UserObjGetData(interp, _this, funcPtr);
    if (!cmdPtr) 
        return Jsi_LogError("Apply in a non-websock object");
    Jsi_Value *valPtr = Jsi_ValueArrayIndex(interp, args, 0);
    Jsi_Number vid;
    if (Jsi_ValueGetNumber(interp, valPtr, &vid) != JSI_OK || vid < 0) 
        return Jsi_LogError("Expected number id");
    int id = (int)vid;
    jws_Pss *pss = NULL;
    Jsi_HashEntry *hPtr;
    Jsi_HashSearch cursor;
    for (hPtr = Jsi_HashSearchFirst(cmdPtr->pssTable, &cursor);
        hPtr != NULL; hPtr = Jsi_HashSearchNext(&cursor)) {
        jws_Pss* tpss = (jws_Pss*)Jsi_HashValueGet(hPtr);
        WSSIGASSERT(tpss, PWS);
        if (tpss->wid == id && tpss->state != PWS_DEAD) {
            pss = tpss;
            break;
        }
    }

    if (!pss) 
        return Jsi_LogError("No such id: %d", id);
    return Jsi_OptionsConf(interp, WPSOptions, pss, Jsi_ValueArrayIndex(interp, args, 1), ret, 0);
}

static Jsi_RC WebSocketIdsCmd(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this,
    Jsi_Value **ret, Jsi_Func *funcPtr)
{
    jws_CmdObj *cmdPtr = (jws_CmdObj*)Jsi_UserObjGetData(interp, _this, funcPtr);
    if (!cmdPtr) 
        return Jsi_LogError("Apply in a non-websock object");
    Jsi_DString dStr = {"["};
    jws_Pss *pss = NULL;
    Jsi_HashEntry *hPtr;
    Jsi_HashSearch cursor;
    int cnt = 0;
    for (hPtr = Jsi_HashSearchFirst(cmdPtr->pssTable, &cursor);
        hPtr != NULL; hPtr = Jsi_HashSearchNext(&cursor)) {
        pss = (jws_Pss*)Jsi_HashValueGet(hPtr);
        WSSIGASSERT(pss, PWS);
        if (pss->state != PWS_DEAD) {
            Jsi_DSPrintf(&dStr, "%s%d", (cnt++?",":""), pss->wid);
        }
    }
    Jsi_DSAppend(&dStr, "]", NULL);
    Jsi_RC rc = Jsi_JSONParse(interp, Jsi_DSValue(&dStr), ret, 0);
    Jsi_DSFree(&dStr);
    return rc;
}


static Jsi_RC jws_HandlerAdd(Jsi_Interp *interp, jws_CmdObj *cmdPtr, const char *ext, const char *cmd, const char *argStr, int flags)
{
    Jsi_HashEntry *hPtr;
    jws_Hander *hdlPtr;
    Jsi_Value *valPtr = Jsi_ValueNewStringDup(interp, cmd);
    hPtr = Jsi_HashEntryNew(cmdPtr->handlers, ext, NULL);
    if (!hPtr)
        return JSI_ERROR;
    hdlPtr = (jws_Hander *)Jsi_Calloc(1, sizeof(*hdlPtr));
    hdlPtr->val = valPtr;
    hdlPtr->flags = flags;
    if (argStr)
        hdlPtr->arg = Jsi_KeyLookup(interp, argStr);
    Jsi_HashValueSet(hPtr, hdlPtr);
    Jsi_IncrRefCount(interp, valPtr);
    return JSI_OK;
}

#define FN_wshandler JSI_INFO("\
With no args, returns list of handlers.  With one arg, returns value for that handler."\
"Otherwise, sets the handler. When cmd is a string, the call is via Jsi_Main([cmd], arg).\
If a cmd is a function, it is called with a single arg: the file name.")
static Jsi_RC WebSocketHandlerCmd(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this,
    Jsi_Value **ret, Jsi_Func *funcPtr)
{
    jws_CmdObj *cmdPtr = (jws_CmdObj*)Jsi_UserObjGetData(interp, _this, funcPtr);
    Jsi_HashEntry *hPtr;
    jws_Hander *hdlPtr;
    if (!cmdPtr) 
        return Jsi_LogError("Apply in a non-websock object");
    WSSIGASSERT(cmdPtr, OBJ);
    int argc = Jsi_ValueGetLength(interp, args);
    if (argc == 0) {
        Jsi_HashSearch search;
        Jsi_Obj* obj = Jsi_ObjNew(interp);
        for (hPtr = Jsi_HashSearchFirst(cmdPtr->handlers, &search); hPtr; hPtr = Jsi_HashSearchNext(&search)) {
            const char *key = (char*)Jsi_HashKeyGet(hPtr);
            Jsi_Value *val = (Jsi_Value*)Jsi_HashValueGet(hPtr);
            Jsi_ObjInsert(interp, obj, key, val, 0);
        }
        Jsi_ValueMakeObject(interp, ret, obj);
        return JSI_OK;
    }
    if (argc == 1) {
        hPtr = Jsi_HashEntryFind(cmdPtr->handlers, Jsi_ValueArrayIndexToStr(interp, args, 0, NULL));
        if (!hPtr)
            return JSI_OK;
        hdlPtr = (jws_Hander*)Jsi_HashValueGet(hPtr);
        Jsi_ValueReplace(interp, ret, hdlPtr->val);
        return JSI_OK;
    }
    const char *key = Jsi_ValueArrayIndexToStr(interp, args, 0, NULL);
    Jsi_Value *valPtr = Jsi_ValueArrayIndex(interp, args, 1);
    if (Jsi_ValueIsNull(interp, valPtr)) {
        hPtr = Jsi_HashEntryFind(cmdPtr->handlers, key);
        if (!hPtr)
            return JSI_OK;
        hdlPtr = (jws_Hander*)Jsi_HashValueGet(hPtr);
        if (hdlPtr->val)
            Jsi_DecrRefCount(interp, hdlPtr->val);
        Jsi_HashValueSet(hPtr, NULL);
        Jsi_HashEntryDelete(hPtr);
        Jsi_Free(hdlPtr);
        Jsi_ValueMakeStringDup(interp, ret, key);
        return JSI_OK;
    }
    if (Jsi_ValueIsFunction(interp, valPtr)==0 && Jsi_ValueIsString(interp, valPtr)==0) 
        return Jsi_LogError("expected string, function or null");
    Jsi_Value *argPtr = Jsi_ValueArrayIndex(interp, args, 2);
    if (argPtr) {
        if (Jsi_ValueIsNull(interp, argPtr))
            argPtr = NULL;
        else if (!Jsi_ValueIsString(interp, argPtr)) 
            return Jsi_LogError("expected a string");
    }
    hPtr = Jsi_HashEntryNew(cmdPtr->handlers, key, NULL);
    if (!hPtr)
        return JSI_ERROR;
    hdlPtr = (jws_Hander *)Jsi_Calloc(1, sizeof(*hdlPtr));
    Jsi_Value *flagPtr = Jsi_ValueArrayIndex(interp, args, 1);
    Jsi_Number fl = 0;
    if (flagPtr && Jsi_ValueIsNumber(interp, flagPtr))
        Jsi_ValueGetNumber(interp, flagPtr, &fl);
    hdlPtr->val = valPtr;
    if (argPtr) {
        hdlPtr->arg = Jsi_ValueString(interp, argPtr, NULL);
        if (hdlPtr->arg)
            hdlPtr->arg = Jsi_KeyLookup(interp, hdlPtr->arg);
    }
    hdlPtr->flags = fl;
    Jsi_HashValueSet(hPtr, hdlPtr);
    Jsi_IncrRefCount(interp, valPtr);
    return JSI_OK;
}

#define FN_wssend JSI_INFO("\
Send a message to one (or all connections if -1). If not already a string, msg is formatted as JSON prior to the send.")

static Jsi_RC WebSocketSendCmd(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this,
    Jsi_Value **ret, Jsi_Func *funcPtr)
{
    jws_CmdObj *cmdPtr = (jws_CmdObj*)Jsi_UserObjGetData(interp, _this, funcPtr);
    if (!cmdPtr) 
        return Jsi_LogError("Apply in a non-websock object");
    WSSIGASSERT(cmdPtr, OBJ);
    //int len;
    //char *in;
    //Jsi_HashEntry *hPtr;
    //Jsi_HashSearch cursor;
    //Jsi_Obj *objPtr;
    jws_Pss *pss;
    Jsi_HashEntry *hPtr;
    Jsi_HashSearch cursor;
    Jsi_Value *arg = Jsi_ValueArrayIndex(interp, args, 0);
    char *str = Jsi_ValueString(interp, arg, NULL);
    int id = -1, argc = Jsi_ValueGetLength(interp, args);
    Jsi_DString eStr = {};
    if (argc>1) {
        Jsi_Number dnum;
        Jsi_Value *darg = Jsi_ValueArrayIndex(interp, args, 1);
        if (Jsi_ValueGetNumber(interp, darg, &dnum) != JSI_OK) 
            return Jsi_LogError("invalid id");
        id = (int)dnum;
    }
    if (!str) {
        str = (char*)Jsi_ValueGetDString(interp, arg, &eStr, JSI_OUTPUT_JSON);
    }
    for (hPtr = Jsi_HashSearchFirst(cmdPtr->pssTable, &cursor);
        hPtr != NULL; hPtr = Jsi_HashSearchNext(&cursor)) {
        pss = (jws_Pss*)Jsi_HashValueGet(hPtr);
        WSSIGASSERT(pss, PWS);
        if ((id<0 || pss->wid == id) && pss->state != PWS_DEAD) {
            if (!pss->stack)
                pss->stack = Jsi_StackNew();
            Jsi_StackPush(pss->stack, Jsi_Strdup(str));
        }
    }
   
    Jsi_DSFree(&eStr);
    return JSI_OK;
}

static Jsi_RC jws_recv_flush(jws_CmdObj *cmdPtr, jws_Pss *pss)
{
    int nlen = Jsi_DSLength(&pss->recvBuf);
    if (nlen<=0)
        return JSI_OK;
    cmdPtr->recvBufCnt--;
    const char *inPtr = Jsi_DSFreeDup(&pss->recvBuf);
    Jsi_RC rc = jws_recv_callback(cmdPtr->interp, cmdPtr, pss, inPtr, nlen);
    if (rc != JSI_OK) {
        pss->stats.recvErrCnt++;
        pss->stats.recvErrLast = time(NULL);
    }
    return rc;
}

static int jws_Service(jws_CmdObj *cmdPtr)
{
    int n = 0;
    struct timeval tv;

    gettimeofday(&tv, NULL);
    if (cmdPtr->recvBufCnt) { // Flush buffered data.
        jws_Pss *pss = NULL;
        Jsi_HashEntry *hPtr;
        Jsi_HashSearch cursor;
        for (hPtr = Jsi_HashSearchFirst(cmdPtr->pssTable, &cursor);
            hPtr != NULL; hPtr = Jsi_HashSearchNext(&cursor)) {
            pss = (jws_Pss*)Jsi_HashValueGet(hPtr);
            WSSIGASSERT(pss, PWS);
            if (pss->state == PWS_DEAD) continue;
            if (Jsi_DSLength(&pss->recvBuf)<=0) continue;
            int to = cmdPtr->recvBufTimeout;
            if (to>=0 && pss->stats.recvLast && difftime(time(NULL), pss->stats.recvLast)<(double)(to?to:60)) continue;
            jws_recv_flush(cmdPtr, pss);
        }
    }

    /*
     * This provokes the LWS_CALLBACK_SERVER_WRITEABLE for every
     * live websocket connection using the DUMB_INCREMENT protocol,
     * as soon as it can take more packets (usually immediately)
     */

    if (((unsigned int)tv.tv_usec - cmdPtr->oldus) > 50000) {
        lws_callback_on_writable_all_protocol(cmdPtr->context, &cmdPtr->protocols[JWS_PROTOCOL_WEBSOCK]);
        cmdPtr->oldus = tv.tv_usec;
    }

#ifdef EXTERNAL_POLL

    /*
     * this represents an existing server's single poll action
     * which also includes libwebsocket sockets
     */

    n = poll(jws_pollfds, jws_num_pollfds, 50);
    if (n < 0)
        return 0;


    if (n)
        for (n = 0; n < jws_num_pollfds; n++)
            if (jws_pollfds[n].revents)
                /*
                * returns immediately if the fd does not
                * match anything under libwebsockets
                * control
                */
                if (lws_service_fd(context,
                                            &jws_pollfds[n]) < 0)
                    return -1;
#else
    /*
     * If libwebsockets sockets are all we care about,
     * you can use this api which takes care of the poll()
     * and looping through finding who needed service.
     *
     * If no socket needs service, it'll return anyway after
     * the number of ms in the second argument.
     */

    n = lws_service(cmdPtr->context, 50);
#endif
    return n;
}

static Jsi_RC WebSocketUpdateCmd(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this,
    Jsi_Value **ret, Jsi_Func *funcPtr)
{
    jws_CmdObj *cmdPtr = (jws_CmdObj*)Jsi_UserObjGetData(interp, _this, funcPtr);
    if (!cmdPtr) 
        return Jsi_LogError("Apply to non-websock object");
    if (!cmdPtr->noUpdate)
        jws_Service(cmdPtr);
    return JSI_OK;
}

static Jsi_RC jws_websockUpdate(Jsi_Interp *interp, void *data)
{
    jws_CmdObj *cmdPtr = (jws_CmdObj*)data;
    WSSIGASSERT(cmdPtr,OBJ);
    jws_Service(cmdPtr);
    return JSI_OK;
}

static void jws_websocketObjErase(jws_CmdObj *cmdPtr)
{
    if (cmdPtr->interp) {
        if (cmdPtr->event)
            Jsi_EventFree(cmdPtr->interp, cmdPtr->event);
        cmdPtr->event = NULL;
        if (cmdPtr->hasOpts)
            Jsi_OptionsFree(cmdPtr->interp, WSOptions, cmdPtr, 0);
        cmdPtr->hasOpts = 0;
        if (cmdPtr->handlers)
            Jsi_HashDelete(cmdPtr->handlers);
        cmdPtr->handlers = NULL;
        if (cmdPtr->pssTable)
            Jsi_HashDelete(cmdPtr->pssTable);
        cmdPtr->pssTable = NULL;
        Jsi_DSFree(&cmdPtr->dHdrs);
    }
    cmdPtr->interp = NULL;
}

static Jsi_RC jws_websocketObjFree(Jsi_Interp *interp, void *data)
{
    jws_CmdObj *cmdPtr = (jws_CmdObj*)data;
    WSSIGASSERT(cmdPtr,OBJ);
    struct lws_context *ctx = cmdPtr->context;
    jws_websocketObjErase(cmdPtr);
    if (ctx) 
        lws_context_destroy(ctx);
    _JSI_MEMCLEAR(cmdPtr);
    Jsi_Free(cmdPtr);
    return JSI_OK;
}

static bool jws_websocketObjIsTrue(void *data)
{
    //jws_CmdObj *cmdPtr = data;
    return 1;
   /* if (!fo->websockname) return 0;
    else return 1;*/
}

static bool jws_websocketObjEqual(void *data1, void *data2)
{
    return (data1 == data2);
}

static Jsi_RC jws_freeHandlers(Jsi_Interp *interp, Jsi_HashEntry* hPtr, void *ptr) {
    jws_Hander *h = (jws_Hander*)ptr;
    if (!h)
        return JSI_OK;
    if (h->val)
        Jsi_DecrRefCount(interp, h->val);
    if (h->objVar)
        Jsi_DecrRefCount(interp, h->objVar);
    Jsi_Free(h);
    return JSI_OK;
}

static Jsi_RC jws_freePss(Jsi_Interp *interp, Jsi_HashEntry* hPtr, void *ptr) {
    jws_Pss *pss = (jws_Pss*)ptr;
    WSSIGASSERT(pss, PWS);
    if (pss) {
        pss->hPtr = NULL;
        jws_deletePss(pss);
    }
    return JSI_OK;
}

static Jsi_RC WebSocketVersionCmd(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this,
    Jsi_Value **ret, Jsi_Func *funcPtr)
{
    const char *verStr = NULL;
    verStr = lws_get_library_version();
    if (verStr) {
        char buf[100], *cp;
        snprintf(buf, sizeof(buf), "%s", verStr);
        cp = Jsi_Strchr(buf, ' ');
        if (cp) *cp = 0;
        Jsi_ValueMakeStringDup(interp, ret, buf);
    }
    return JSI_OK;
}


#define FN_WebSocket JSI_INFO("\
Create a websocket server/client object.  The server serves out pages to a \
web browser, which can use javascript to upgrade connection to a bidirectional websocket.")
static Jsi_RC WebSocketConstructor(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this,
    Jsi_Value **ret, Jsi_Func *funcPtr);
    

static Jsi_CmdSpec websockCmds[] = {
    { "WebSocket",  WebSocketConstructor, 0,  1, "options:object=void", .help="Create websocket server/client object", .retType=(uint)JSI_TT_USEROBJ, .flags=JSI_CMD_IS_CONSTRUCTOR, .info=FN_WebSocket, .opts=WSOptions },
    { "conf",       WebSocketConfCmd,     0,  1, "options:string|object=void",.help="Configure options", .retType=(uint)JSI_TT_ANY, .flags=0, .info=0, .opts=WSOptions },
    { "handler",    WebSocketHandlerCmd,  0,  4, "extension:string=void, cmd:string|function=void, arg:string|null=void, flags:number=0", 
        .help="Get/Set handler command for an extension", .retType=(uint)JSI_TT_FUNCTION|JSI_TT_ARRAY|JSI_TT_STRING|JSI_TT_UNDEFINED, .flags=0, .info=FN_wshandler },
    { "ids",        WebSocketIdsCmd,      0,  0, "", .help="Return list of ids", .retType=(uint)JSI_TT_ARRAY},
    { "idconf",     WebSocketIdConfCmd,   1,  2, "id:number, options:string|object=void",.help="Configure options for id", .retType=(uint)JSI_TT_ANY, .flags=0, .info=0, .opts=WPSOptions },
    { "send",       WebSocketSendCmd,     1,  2, "data:any, id:number=-1", .help="Send a websocket message to id", .retType=(uint)JSI_TT_VOID, .flags=0, .info=FN_wssend },
    { "update",     WebSocketUpdateCmd,   0,  0, "", .help="Service events for just this websocket", .retType=(uint)JSI_TT_VOID },
    { "version",    WebSocketVersionCmd,  0,  0, "", .help="Runtime library version string", .retType=(uint)JSI_TT_STRING },
    { NULL, 0,0,0,0, .help="Commands for managing WebSocket server/client connections"  }
};


static Jsi_UserObjReg websockobject = {
    "WebSocket",
    websockCmds,
    jws_websocketObjFree,
    jws_websocketObjIsTrue,
    jws_websocketObjEqual
};

static const struct lws_extension jsi_lws_exts[] = {
    {
        "permessage-deflate",
        lws_extension_callback_pm_deflate,
        "permessage-deflate"
    },
    {
        "deflate-frame",
        lws_extension_callback_pm_deflate,
        "deflate_frame"
    },
    { NULL, NULL, NULL /* terminator */ }
};

static Jsi_RC WebSocketConstructor(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this,
    Jsi_Value **ret, Jsi_Func *funcPtr)
{
    jws_CmdObj *cmdPtr;
    Jsi_Value *toacc = NULL;
    Jsi_Value *arg = Jsi_ValueArrayIndex(interp, args, 0);
    
    cmdPtr = (jws_CmdObj*)Jsi_Calloc(1, sizeof(*cmdPtr));
    cmdPtr->sig = JWS_SIG_OBJ;
    cmdPtr->port = 8080;
    //cmdPtr->rootdir = "./";

    cmdPtr->interp = interp;
    cmdPtr->protocols[JWS_PROTOCOL_HTTP].name="http-only";
    cmdPtr->protocols[JWS_PROTOCOL_HTTP].callback=jws_callback_http;
    cmdPtr->protocols[JWS_PROTOCOL_HTTP].per_session_data_size=sizeof(jws_Pss);
    cmdPtr->protocols[JWS_PROTOCOL_WEBSOCK].name="jsi-protocol";
    cmdPtr->protocols[JWS_PROTOCOL_WEBSOCK].callback=jws_callback_websock;
    cmdPtr->protocols[JWS_PROTOCOL_WEBSOCK].per_session_data_size=sizeof(jws_Pss);
    cmdPtr->ietf_version = -1;
    cmdPtr->rx_buffer_size = 0;
    cmdPtr->ws_gid = -1;
    cmdPtr->ws_uid = -1;
    cmdPtr->startTime = time(NULL);
    cmdPtr->hasOpts = (arg != NULL && !Jsi_ValueIsNull(interp,arg));
    if (cmdPtr->hasOpts && Jsi_OptionsProcess(interp, WSOptions, cmdPtr, arg, 0) < 0) {
        cmdPtr->deleted = 1;
        jws_websocketObjFree(interp, cmdPtr);
        //Jsi_EventuallyFree(cmdPtr, destroyjws_CmdObj);
        return JSI_ERROR;
    }
#ifdef LWS_LIBRARY_VERSION_NUMBER
    cmdPtr->version = LWS_LIBRARY_VERSION_NUMBER;
#endif
    cmdPtr->protocols[JWS_PROTOCOL_WEBSOCK].rx_buffer_size=cmdPtr->rx_buffer_size;
    cmdPtr->pssTable = Jsi_HashNew(interp, JSI_KEYS_ONEWORD, jws_freePss);
    cmdPtr->info.port = (cmdPtr->client ? CONTEXT_PORT_NO_LISTEN : cmdPtr->port);


#if !defined(LWS_NO_DAEMONIZE) && !defined(WIN32)
    /* 
     * normally lock path would be /var/lock/lwsts or similar, to
     * simplify getting started without having to take care about
     * permissions or running as root, set to /tmp/.lwsts-lock
     */
    if (cmdPtr->daemonize && lws_daemonize("/tmp/.lwsts-lock")) {
        if (cmdPtr->debug)
            fprintf(stderr, "WS:Failed to daemonize\n");
        jws_websocketObjFree(interp, cmdPtr);
        return JSI_ERROR;
    }
#endif
    cmdPtr->info.user = cmdPtr;
    cmdPtr->info.iface = cmdPtr->interface ? Jsi_ValueString(interp, cmdPtr->interface, NULL) : NULL;
    cmdPtr->info.protocols = cmdPtr->protocols;

    cmdPtr->info.extensions = jsi_lws_exts;

    cmdPtr->info.ssl_cert_filepath = cmdPtr->ssl_cert_filepath;
    cmdPtr->info.ssl_private_key_filepath = cmdPtr->ssl_private_key_filepath;
    cmdPtr->info.gid = cmdPtr->ws_gid;
    cmdPtr->info.uid = cmdPtr->ws_uid;
    cmdPtr->opts = LWS_SERVER_OPTION_SKIP_SERVER_CANONICAL_NAME|LWS_SERVER_OPTION_VALIDATE_UTF8;
    cmdPtr->info.options = cmdPtr->opts;
    cmdPtr->info.max_http_header_pool = 16;
    cmdPtr->info.timeout_secs = 5;
    cmdPtr->info.ssl_cipher_list = "ECDHE-ECDSA-AES256-GCM-SHA384:"
                   "ECDHE-RSA-AES256-GCM-SHA384:"
                   "DHE-RSA-AES256-GCM-SHA384:"
                   "ECDHE-RSA-AES256-SHA384:"
                   "HIGH:!aNULL:!eNULL:!EXPORT:"
                   "!DES:!MD5:!PSK:!RC4:!HMAC_SHA1:"
                   "!SHA1:!DHE-RSA-AES128-GCM-SHA256:"
                   "!DHE-RSA-AES128-SHA256:"
                   "!AES128-GCM-SHA256:"
                   "!AES128-SHA256:"
                   "!DHE-RSA-AES256-SHA256:"
                   "!AES256-GCM-SHA384:"
                   "!AES256-SHA256";

    lws_set_log_level(cmdPtr->debug>1?cmdPtr->debug-1:0, NULL);
    cmdPtr->context = lws_create_context(&cmdPtr->info);
    if (cmdPtr->context == NULL) {
        Jsi_LogError("libwebsocket init failed on port %d (try another port?)", cmdPtr->port);
        jws_websocketObjFree(interp, cmdPtr);
        return JSI_ERROR;
    }

    if (cmdPtr->client) {
        struct lws_client_connect_info lci;
        lci.context = cmdPtr->context;
        lci.address = cmdPtr->address ? Jsi_ValueString(cmdPtr->interp, cmdPtr->address, NULL) : "127.0.0.1";
        lci.port = cmdPtr->port;
        lci.ssl_connection = cmdPtr->use_ssl;
        lci.path = cmdPtr->rootdir?Jsi_ValueString(cmdPtr->interp, cmdPtr->rootdir, NULL):"./";
        lci.host = cmdPtr->cl_host?cmdPtr->cl_host:"localhost";
        lci.origin = cmdPtr->cl_origin?cmdPtr->cl_origin:"localhost";
        lci.protocol = cmdPtr->protocols[JWS_PROTOCOL_WEBSOCK].name;
        lci.ietf_version_or_minus_one = cmdPtr->ietf_version;
        
        if (NULL == lws_client_connect_via_info(&lci))
        {
            Jsi_LogError("websock connect failed");
            jws_websocketObjFree(interp, cmdPtr);
            return JSI_ERROR;
        }
    }

    cmdPtr->event = Jsi_EventNew(interp, jws_websockUpdate, cmdPtr);
    if (Jsi_FunctionIsConstructor(funcPtr)) {
        toacc = _this;
    } else {
        Jsi_Obj *o = Jsi_ObjNew(interp);
        Jsi_PrototypeObjSet(interp, "WebSocket", o);
        Jsi_ValueMakeObject(interp, ret, o);
        toacc = *ret;
    }

    Jsi_Obj *fobj = Jsi_ValueGetObj(interp, toacc);
    if ((cmdPtr->objId = Jsi_UserObjNew(interp, &websockobject, fobj, cmdPtr))<0) {
        jws_websocketObjFree(interp, cmdPtr);
        Jsi_ValueMakeUndef(interp, ret);
        return JSI_ERROR;
    }
    cmdPtr->handlers = Jsi_HashNew(interp, JSI_KEYS_STRING, jws_freeHandlers);
    if (cmdPtr->defHandlers) {
        const char *argStr = NULL;
        if (cmdPtr->debug)
            argStr = "{debug:1}";
        jws_HandlerAdd(interp, cmdPtr, ".jsi",   "Jsi_Jspp",   argStr, 1);
        jws_HandlerAdd(interp, cmdPtr, ".htmli", "Jsi_Htmlpp", argStr, 1);
        jws_HandlerAdd(interp, cmdPtr, ".cssi",  "Jsi_Csspp",  argStr, 1);
    }
    cmdPtr->fobj = fobj;
    return JSI_OK;
}

static Jsi_RC Jsi_DoneWebSocket(Jsi_Interp *interp)
{
    Jsi_UserObjUnregister(interp, &websockobject);
    Jsi_PkgProvide(interp, "WebSocket", -1, NULL);
    return JSI_OK;
}


Jsi_RC Jsi_InitWebSocket(Jsi_Interp *interp, int release)
{
    if (release)
        return Jsi_DoneWebSocket(interp);
    Jsi_Hash *wsys;
    const char *libver = lws_get_library_version();
    int lvlen = sizeof(LWS_LIBRARY_VERSION)-1;
    if (Jsi_Strncmp(libver, LWS_LIBRARY_VERSION, lvlen) || !isspace(libver[lvlen])) 
        return Jsi_LogError("Library version mismatch: %s != %s", LWS_LIBRARY_VERSION, lws_get_library_version());
#if JSI_USE_STUBS
  if (Jsi_StubsInit(interp, 0) != JSI_OK)
    return JSI_ERROR;
#endif
    if (Jsi_PkgProvide(interp, "WebSocket", 1, Jsi_InitWebSocket) != JSI_OK)
        return JSI_ERROR;
    if (!(wsys = Jsi_UserObjRegister(interp, &websockobject))) {
        Jsi_LogBug("Can not init webSocket");
        return JSI_ERROR;
    }

    if (!Jsi_CommandCreateSpecs(interp, websockobject.name, websockCmds, wsys, JSI_CMDSPEC_ISOBJ))
        return JSI_ERROR;
    return JSI_OK;
}

#endif
#endif
