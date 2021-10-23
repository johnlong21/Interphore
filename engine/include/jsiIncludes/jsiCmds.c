#ifndef JSI_LITE_ONLY
#ifndef JSI_AMALGAMATION
#include "jsiInt.h"
#else
char *strptime(const char *buf, const char *fmt, struct tm *tm);
#endif
#include <errno.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <signal.h>
#include <limits.h>

static Jsi_RC consoleInputCmd(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this,
    Jsi_Value **ret, Jsi_Func *funcPtr)
{
    char buf[1024];
    char *cp, *p = buf;
    buf[0] = 0;
    if (!interp->stdinStr)
        p=fgets(buf, sizeof(buf), stdin);
    else {
        int ilen;
        cp = Jsi_ValueString(interp, interp->stdinStr, &ilen);
        if (!cp || ilen<=0)
            p = NULL;
        else {
            Jsi_Strncpy(buf, cp, sizeof(buf));
            buf[sizeof(buf)-1] = 0;
            p = Jsi_Strchr(buf, '\n');
            if (p) { *p = 0;}
            ilen = Jsi_Strlen(buf);
            p = (cp + ilen + (p?1:0));
            Jsi_ValueMakeStringDup(interp, &interp->stdinStr, p);
            p = buf;
        }
    }
    
    if (p == NULL) {
        Jsi_ValueMakeUndef(interp, ret);
        return JSI_OK;
    }
    if ((p = Jsi_Strchr(buf, '\r'))) *p = 0;
    if ((p = Jsi_Strchr(buf, '\n'))) *p = 0;
    Jsi_ValueMakeStringDup(interp, ret, buf);
    return JSI_OK;
}

typedef struct {
    bool trace;
    bool once;
    bool autoIndex;
    bool isMain;
} SourceData;

static Jsi_OptionSpec SourceOptions[] = {
    JSI_OPT(BOOL,   SourceData, autoIndex,  .help="Look for and load Jsi_Auto.jsi auto-index file" ),
    JSI_OPT(BOOL,   SourceData, isMain, .help="Coerce to true the value of Info.isMain()" ),
    JSI_OPT(BOOL,   SourceData, once,   .help="Source file only if not already sourced (Default: Interp.debugOpts.includeOnce)" ),
    JSI_OPT(BOOL,   SourceData, trace,  .help="Trace include statements (Default: Interp.debugOpts.includeTrace)" ),
    JSI_OPT_END(SourceData, .help="Options for source command")
};


static Jsi_RC SysSourceCmd(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this,
    Jsi_Value **ret, Jsi_Func *funcPtr)
{
    jsi_Pstate *ps = interp->ps;
    Jsi_RC rc = JSI_OK;
    int flags = 0;
    int i, argc;
    SourceData data = {.trace = interp->debugOpts.includeTrace, .once = interp->debugOpts.includeOnce};
    Jsi_Value *v, *va = Jsi_ValueArrayIndex(interp, args, 0);
    Jsi_Value *vo = Jsi_ValueArrayIndex(interp, args, 1);
    if (vo) {
        if (!Jsi_ValueIsObjType(interp, vo, JSI_OT_OBJECT)) { /* Future options. */
            Jsi_LogError("expected options object");
            return JSI_ERROR;
        }
        if (Jsi_OptionsProcess(interp, SourceOptions, &data, vo, 0) < 0) {
            return JSI_ERROR;
        }
        if (data.autoIndex)
            flags |= JSI_EVAL_AUTOINDEX;
    }
    if ((interp->includeDepth+1) > interp->maxIncDepth) 
        return Jsi_LogError("max source depth exceeded");
    if (data.once)
        flags|= JSI_EVAL_ONCE;
    interp->includeCnt++;
    interp->includeDepth++;
    int oisi = interp->isMain;
    interp->isMain = data.isMain;
    const char *sop = (data.once?" <ONCE>":"");
    if (!Jsi_ValueIsArray(interp, va)) {
        v = va;
        if (v && Jsi_ValueIsString(interp,v)) {
            if (data.trace)
                Jsi_LogInfo("sourcing: %s%s", Jsi_ValueString(interp, v, 0), sop);
            rc = Jsi_EvalFile(ps->interp, v, flags);
        } else {
            Jsi_LogError("expected string");
            rc = JSI_ERROR;
        }
        goto done;
    }
    argc = Jsi_ValueGetLength(interp, va);
    for (i=0; i<argc && rc == JSI_OK; i++) {
        v = Jsi_ValueArrayIndex(interp, va, i);
        if (v && Jsi_ValueIsString(interp,v)) {
            if (data.trace)
                Jsi_LogInfo("sourcing: %s%s", Jsi_ValueString(interp, v, 0), sop);
            rc = Jsi_EvalFile(ps->interp, v, flags);
        } else {
            Jsi_LogError("expected string");
            rc = JSI_ERROR;
            break;
        }
    }
done:
    interp->isMain = oisi;
    interp->includeDepth--;
    return rc;
}

static void jsiGetTime(long *seconds, long *milliseconds)
{
    struct timeval tv;

    gettimeofday(&tv, NULL);
    *seconds = tv.tv_sec;
    *milliseconds = tv.tv_usec / 1000;
}

void Jsi_EventFree(Jsi_Interp *interp, Jsi_Event* event) {
    SIGASSERT(event,EVENT);
    if (event->funcVal)
        Jsi_DecrRefCount(interp, event->funcVal);
    if (event->hPtr) {
        Jsi_HashValueSet(event->hPtr, NULL);
        Jsi_HashEntryDelete(event->hPtr);
    }
    _JSI_MEMCLEAR(event);
    Jsi_Free(event);
}

/* Create an event and add to interp event table. */
Jsi_Event* Jsi_EventNew(Jsi_Interp *interp, Jsi_EventHandlerProc *callback, void* data)
{
    Jsi_Event *evPtr;
    while (1) {
        bool isNew;
        uintptr_t id = interp->eventIdx++;
        Jsi_HashEntry *hPtr = Jsi_HashEntryNew(interp->eventTbl, (void*)id, &isNew);
        if (!isNew)
            continue;
        evPtr = (Jsi_Event*)Jsi_Calloc(1, sizeof(*evPtr));
        SIGINIT(evPtr,EVENT);
        evPtr->id = id;
        evPtr->handler = callback;
        evPtr->data = data;
        evPtr->hPtr = hPtr;
        Jsi_HashValueSet(hPtr, evPtr);
        break;
    }
    return evPtr;
}

/* Process events and return count. */
int Jsi_EventProcess(Jsi_Interp *interp, int maxEvents)
{
    Jsi_Event *evPtr;
    Jsi_HashEntry *hPtr;
    Jsi_HashSearch search;
    Jsi_Value* nret = NULL;
    Jsi_RC rc;
    int cnt = 0, newIdx = interp->eventIdx;
    long cur_sec, cur_ms;
    jsiGetTime(&cur_sec, &cur_ms);
    Jsi_Value *vpargs = NULL;

    /*if (Jsi_MutexLock(interp, interp->Mutex) != JSI_OK)
        return JSI_ERROR;*/

    for (hPtr = Jsi_HashSearchFirst(interp->eventTbl, &search);
            hPtr != NULL; hPtr = Jsi_HashSearchNext(&search)) {

        if (!(evPtr = (Jsi_Event*)Jsi_HashValueGet(hPtr)))
            continue;
        SIGASSERT(evPtr,EVENT);
        if ((int)evPtr->id >= newIdx) /* Avoid infinite loop of event creating events. */
            continue;
        switch (evPtr->evType) {
        case JSI_EVENT_SIGNAL:
#ifndef JSI_OMIT_SIGNAL   /* TODO: win signals? */
            if (!jsi_SignalIsSet(interp, evPtr->sigNum))
                continue;
            jsi_SignalClear(interp, evPtr->sigNum);
#endif
            break;
        case JSI_EVENT_TIMER:
            if (cur_sec <= evPtr->when_sec && (cur_sec != evPtr->when_sec || cur_ms < evPtr-> when_ms)) {
                if (evPtr->when_sec && evPtr->when_ms)
                    continue;
            }
            cnt++;
            evPtr->count++;
            break;
        case JSI_EVENT_ALWAYS:
            break;
        default:
            assert(0);
        }

        if (evPtr->handler) {
            rc = evPtr->handler(interp, evPtr->data);
        } else {
            if (vpargs == NULL) {
                vpargs = Jsi_ValueMakeObject(interp, NULL, Jsi_ObjNewArray(interp, NULL, 0, 0));
                Jsi_IncrRefCount(interp, vpargs);
            }
            nret = Jsi_ValueNew1(interp);
            rc = Jsi_FunctionInvoke(interp, evPtr->funcVal, vpargs, &nret, NULL);
        }
        if (interp->deleting) {
            cnt = -1;
            goto bail;
        }
        if (rc != JSI_OK) {
            if (interp->exited) {
                cnt = rc;
                goto bail;
            }
            Jsi_LogError("event function call failure");
        }
        if (evPtr->once) {
            Jsi_EventFree(interp, evPtr);
        } else {
            evPtr->when_sec = cur_sec + evPtr->initialms / 1000;
            evPtr->when_ms = cur_ms + evPtr->initialms % 1000;
            if (evPtr->when_ms >= 1000) {
                evPtr->when_sec++;
                evPtr->when_ms -= 1000;
            }
        }
        if (maxEvents>0 && cnt>=maxEvents)
            break;
    }
bail:
    if (vpargs)
        Jsi_DecrRefCount(interp, vpargs);
    if (nret)
        Jsi_DecrRefCount(interp, nret);
    /*Jsi_MutexUnlock(interp, interp->Mutex);*/

    static int evCnt = 0;
    evCnt++;
    if (interp->parent && interp->busyCallback &&
            (interp->busyCallback[0]==0 || !Jsi_Strcmp(interp->busyCallback, "update")))
        Jsi_EventProcess(interp->parent, maxEvents);
    return cnt;
}

/*
 * \brief: sleep for so many milliseconds with interp mutex unlocked.
 */
Jsi_RC Jsi_Sleep(Jsi_Interp *interp, Jsi_Number dtim) {
    uint utim = 0;
    if (dtim <= 0)
        return JSI_OK;
    Jsi_MutexUnlock(interp, interp->Mutex);
    dtim = (dtim/1e3);
    if (dtim>1) {
        utim = 1e6*(dtim - (Jsi_Number)((int)dtim));
        dtim = (int)dtim;
    } else if (dtim<1) {
        utim = 1e6 * dtim;
        dtim = 0;
    }
    if (utim>0)
        usleep(utim);
    if (dtim>0)
        sleep(dtim);
    if (Jsi_MutexLock(interp, interp->Mutex) != JSI_OK) {
        Jsi_LogBug("could not reget mutex");
        return JSI_ERROR;
    }
    return JSI_OK;
}

#ifndef JSI_OMIT_EVENT
typedef struct {
    int minTime;
    int maxEvents;
    int maxPasses;
    int sleep;
} UpdateData;

static Jsi_OptionSpec UpdateOptions[] = {
    JSI_OPT(INT,    UpdateData, maxEvents,  .help="Maximum number of events to process (or -1 for all)" ),
    JSI_OPT(INT,    UpdateData, maxPasses,  .help="Maximum passes through event queue" ),
    JSI_OPT(INT,    UpdateData, minTime,    .help="Minimum milliseconds before returning, or -1 to loop forever (default is 0)" ),
    JSI_OPT(INT,    UpdateData, sleep,      .help="Time to sleep time (in milliseconds) between event checks. Default is 1" ),
    JSI_OPT_END(UpdateData, .help="Options for update command")
};

#define FN_update JSI_INFO("\
Returns the number of events processed. \
Events are processed until minTime (in milliseconds) is exceeded, or forever if -1. \
The default minTime is 0, meaning return as soon as no events can be processed. \
A positive mintime will result in sleeps between event checks.")
static Jsi_RC SysUpdateCmd(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this,
    Jsi_Value **ret, Jsi_Func *funcPtr)
{
    int maxEvents = -1, hasopts = 0;
    int cnt = 0, lcnt = 0;
    Jsi_RC rc = JSI_OK;
    Jsi_Value *opts = Jsi_ValueArrayIndex(interp, args, 0);
    long cur_sec, cur_ms;
    long start_sec, start_ms;
    jsiGetTime(&start_sec, &start_ms);
    UpdateData udata = {};
    udata.sleep = 1;
    jsi_AddEventHandler(interp); 
       
    if (opts != NULL) {
        Jsi_Number dms = 0;
        if (opts->vt == JSI_VT_OBJECT) {
            hasopts = 1;
            if (Jsi_OptionsProcess(interp, UpdateOptions, &udata, opts, 0) < 0) {
                return JSI_ERROR;
            }
        } else if (opts->vt != JSI_VT_NULL && Jsi_GetNumberFromValue(interp, opts, &dms) != JSI_OK)
            return JSI_ERROR;
        else
            udata.minTime = (unsigned long)dms;
    }
  
    while (1) {
        long long diftime;
        int ne = Jsi_EventProcess(interp, maxEvents);
        if (ne<0)
            break;
        cnt += ne;
        if (Jsi_InterpGone(interp))
            return JSI_ERROR;
        if (udata.minTime==0)
            break;
        jsiGetTime(&cur_sec, &cur_ms);
        if (cur_sec == start_sec)
            diftime = (long long)(cur_ms-start_ms);
        else
            diftime = (cur_sec-start_sec)*1000LL + cur_ms + (1000-start_ms);
        if (udata.minTime>0 && diftime >= (long long)udata.minTime)
            break;
        if (udata.maxPasses && ++lcnt >= udata.maxPasses)
            break;
        if ((rc = Jsi_Sleep(interp, udata.sleep)) != JSI_OK)
            break;
    }
    if (hasopts)
        Jsi_OptionsFree(interp, UpdateOptions, &udata, 0);
    Jsi_ValueMakeNumber(interp, ret, (Jsi_Number)cnt);
    return rc;
}

static Jsi_RC intervalTimer(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this,
    Jsi_Value **ret, Jsi_Func *funcPtr, int once)
{
    bool isNew;
    Jsi_Event *evPtr;
    uintptr_t id;
    Jsi_Number milli;
    long milliseconds, cur_sec, cur_ms;
    Jsi_Value *fv = Jsi_ValueArrayIndex(interp, args, 0);
    Jsi_Value *tv = Jsi_ValueArrayIndex(interp, args, 1);
    
    if (!Jsi_ValueIsFunction(interp, fv)) 
        return Jsi_LogError("expected function");
    if (Jsi_GetNumberFromValue(interp, tv, &milli) != JSI_OK) 
        return Jsi_LogError("expected number");
    milliseconds = (long)milli;
    if (milliseconds < 0)
        milliseconds = 0;
    while (1) {
        id = interp->eventIdx++;
        Jsi_HashEntry *hPtr = Jsi_HashEntryNew(interp->eventTbl, (void*)id, &isNew);
        if (!isNew)
            continue;
        evPtr = (Jsi_Event*)Jsi_Calloc(1, sizeof(*evPtr));
        SIGINIT(evPtr,EVENT);
        evPtr->id = id;
        evPtr->funcVal = fv;
        Jsi_IncrRefCount(interp, fv);
        evPtr->hPtr = hPtr;
        jsiGetTime(&cur_sec, &cur_ms);
        evPtr->initialms = milliseconds;
        evPtr->when_sec = cur_sec + milliseconds / 1000;
        evPtr->when_ms = cur_ms + milliseconds % 1000;
        if (evPtr->when_ms >= 1000) {
            evPtr->when_sec++;
            evPtr->when_ms -= 1000;
        }
        evPtr->once = once;
        Jsi_HashValueSet(hPtr, evPtr);
        break;
    }
    Jsi_ValueMakeNumber(interp, ret, (Jsi_Number)id);
    return JSI_OK;
}

static Jsi_RC setIntervalCmd(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this,
    Jsi_Value **ret, Jsi_Func *funcPtr)
{
    return intervalTimer(interp, args, _this, ret, funcPtr, 0);
}

static Jsi_RC clearIntervalCmd(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this,
    Jsi_Value **ret, Jsi_Func *funcPtr)
{
    Jsi_Number nid;
    uintptr_t id;
    Jsi_Value *tv = Jsi_ValueArrayIndex(interp, args, 0);
    Jsi_HashEntry *hPtr;
    if (Jsi_GetNumberFromValue(interp, tv, &nid) != JSI_OK) 
        return Jsi_LogError("expected number");
    id = (uintptr_t)nid;
    hPtr = Jsi_HashEntryFind(interp->eventTbl, (void*)id);
    if (hPtr == NULL) 
        return Jsi_LogError("id not found: %" PRId64, (Jsi_Wide)id);
    Jsi_HashEntryDelete(hPtr);
    return JSI_OK;
}

static Jsi_RC setTimeoutCmd(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this,
    Jsi_Value **ret, Jsi_Func *funcPtr)
{
    return intervalTimer(interp, args, _this, ret, funcPtr, 1);
}
#endif

static Jsi_RC SysExitCmd(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this,
    Jsi_Value **ret, Jsi_Func *funcPtr)
{
    int err = 0;
    Jsi_Value *v = NULL;
    Jsi_Number n;
    if (Jsi_ValueGetLength(interp, args) > 0) {
        v = Jsi_ValueArrayIndex(interp, args, 0);
        if (v && Jsi_GetNumberFromValue(interp,v, &n) == JSI_OK)
            err = (int)n;
        else 
            return Jsi_LogError("expected number");
    }
    if (interp->onExit && interp->parent && Jsi_FunctionInvokeBool(interp->parent, interp->onExit, v))
        return JSI_OK;
    if (interp->parent == NULL && interp == interp->mainInterp && interp->debugOpts.isDebugger)
        jsi_DoExit(interp, err);
    else {
        interp->exited = 1;
        interp->exitCode = err;
        return JSI_ERROR;
        /* TODO: cleanup events, etc. */
    }
    return JSI_OK;
}


static Jsi_RC parseIntCmd(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this,
    Jsi_Value **ret, Jsi_Func *funcPtr)
{
    Jsi_Wide w = 0;
    Jsi_Number d = 0;

    Jsi_Value *v = Jsi_ValueArrayIndex(interp, args, 0);
    Jsi_Value *bv = Jsi_ValueArrayIndex(interp, args, 1);
    if (!v)
        return JSI_ERROR;
    char *eptr, *str = Jsi_ValueString(interp, v, NULL);
    int base = 0;
    if (!str) {
        if (Jsi_ValueIsNumber(interp, v))
            Jsi_GetNumberFromValue(interp, v, &d);
        else
            goto nanval;
    }
    else if (str[0] == '0' && str[1] == 'x')
        d = (Jsi_Number)strtoll(str, &eptr, 16);
    else if (str[0] == '0' && !bv)
        d = (Jsi_Number)strtoll(str, &eptr, 8);
    else if (base == 0 && !bv)
        d = Jsi_ValueToNumberInt(interp, v, 1);
    else {
        if (str == NULL || JSI_OK != Jsi_GetIntFromValue(interp, bv, &base) || base<2 || base>36)
            return JSI_ERROR;
        d = (Jsi_Number)strtoll(str, &eptr, base);
    }
    if (Jsi_NumberIsNaN(d) || (Jsi_NumberIsFinite(d)==0 && Jsi_GetDoubleFromValue(interp, v, &d) != JSI_OK))
nanval:
        Jsi_ValueDup2(interp, ret, interp->NaNValue);
    else {
        w = (Jsi_Wide)d;
        Jsi_ValueMakeNumber(interp, ret, (Jsi_Number)w);
    }
    return JSI_OK;
}

static Jsi_RC parseFloatCmd(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this,
    Jsi_Value **ret, Jsi_Func *funcPtr)
{
    Jsi_Number n;
    Jsi_Value *v = Jsi_ValueArrayIndex(interp, args, 0);
    Jsi_ValueToNumber(interp, v);
    if (Jsi_GetDoubleFromValue(interp, v, &n) != JSI_OK)
        Jsi_ValueDup2(interp, ret, interp->NaNValue);
    else {
        Jsi_ValueMakeNumber(interp, ret, n);
    }
    return JSI_OK;
}

// Return file path as a Jsi_Value, if it exists.
Jsi_Value * jsi_AccessFile(Jsi_Interp *interp, const char *name, int mode)
{
    //TODO: Optimize by not allocing Jsi_Value if access() fails and not in /zvfs with no other mounts.
    if (0 && access(name, R_OK) <0) {
        if (interp->mountCnt==0)
            return NULL;
        if (interp->selfZvfs && interp->mountCnt==1 && Jsi_Strncmp(name, JSI_ZVFS_DIR, Jsi_Strlen(JSI_ZVFS_DIR)))
            return NULL;
    }
    Jsi_Value *fpath = Jsi_ValueNewStringDup(interp, name); 
    Jsi_IncrRefCount(interp, fpath);
    if (Jsi_Access(interp, fpath, mode) >= 0)
        return fpath;
    Jsi_DecrRefCount(interp, fpath);
    return NULL;
}

static int jsiValidPkgName(const char *n) {
    if (!*n) return 0;
    while (*n) {
        if (*n != '_' && !isalnum(*n))
            return 0;
        n++;
    }
    return 1;
}


static jsi_PkgInfo* jsiPkgGet(Jsi_Interp *interp, const char *name)
{
     return (jsi_PkgInfo*)Jsi_HashGet(interp->packageHash, name, 0);
}

Jsi_Number Jsi_PkgVersion(Jsi_Interp *interp, const char *name, const char **filePtr)
{
    jsi_PkgInfo *ptr = jsiPkgGet(interp, name);
    if (ptr) {
        if (filePtr)
            *filePtr = ptr->loadFile;
        return ptr->version;
    }
    return -1;
}

// Load one package. Note: Currently ver is ignored.
static Jsi_RC jsi_PkgLoadOne(Jsi_Interp *interp, const char *name, const char *path, int len, Jsi_Value **fval, Jsi_Number ver)
{
    bool trace = interp->debugOpts.pkgTrace;
    Jsi_RC rc = JSI_CONTINUE;
    const char *fn;
    const char *ext;
    if (!path)
        return JSI_CONTINUE;
    if (len<0)
        len = Jsi_Strlen(path);
#ifdef __WIN32
    ext = ".dll";
#else
    ext = ".so";
#endif
    Jsi_Value *fpath = NULL;
    Jsi_DString dStr;
    Jsi_DSInit(&dStr);
    int i;
    const char *sf = "";
    if (trace)
        sf = jsi_GetCurFile(interp);
    for (i=0; i<4 && rc == JSI_CONTINUE; i++) {
        const char *pref = (i%2 ? name : ""), *ps = (i%2 ? "/" : "");
        if (i<2) {
#ifdef JSI_OMIT_LOAD
        continue;
#endif
            Jsi_DSSetLength(&dStr, 0);
            Jsi_DSAppendLen(&dStr, path, len);
            Jsi_DSAppend(&dStr, "/", pref, ps, name, ext, NULL);
            fn = Jsi_DSValue(&dStr);
            if ((fpath = jsi_AccessFile(interp, fn, R_OK))) {
                if (trace)
                    Jsi_Printf(jsi_Stderr,"Package-Trace: load('%s') from %s\n", fn, sf);
                rc = jsi_LoadLibrary(interp, fn);
            }
        } else {
            Jsi_DSSetLength(&dStr, 0);
            Jsi_DSAppendLen(&dStr, path, len);
            Jsi_DSAppend(&dStr, "/", pref, ps, name, ".jsi", NULL);
            fn = Jsi_DSValue(&dStr);
            if ((fpath = jsi_AccessFile(interp, fn, R_OK))) {
                if (trace)
                    Jsi_Printf(jsi_Stderr, "Package-Trace: source('%s') from %s\n", fn, sf);
                rc = Jsi_EvalFile(interp, fpath, 0);
            }
        }
    }
    int needErr = 1;
    if (rc == JSI_OK) {
        ver = Jsi_PkgVersion(interp, name, NULL);
        if (ver < 0) {
            rc = Jsi_LogError("package missing provide('%s') in file: %s", name, Jsi_DSValue(&dStr));
            needErr = 0;
        }
    }
    if (rc == JSI_OK) {
        *fval = fpath;
    }
    else if (fpath)
        Jsi_DecrRefCount(interp, fpath);
    if (rc == JSI_ERROR && needErr)
        Jsi_LogError("within require('%s') in file: %s", name, Jsi_DSValue(&dStr));
    Jsi_DSFree(&dStr);
    return rc;
}

// Load package from pkgDirs or executable path.  Note. ver is unused.
static Jsi_RC jsi_PkgLoad(Jsi_Interp *interp, const char *name, Jsi_Number ver) {
    Jsi_RC rc;
    const char *cp = NULL, *path;
    int len;
    uint i;
    Jsi_Value *fval = NULL;
    Jsi_Value *pval = interp->pkgDirs;
    if (pval) {
        Jsi_Obj* obj = pval->d.obj;
        
        for (i=0; i<obj->arrCnt; i++) {
            rc = jsi_PkgLoadOne(interp, name, Jsi_ValueString(interp, obj->arr[i], NULL), -1, &fval, ver);
            if (rc != JSI_CONTINUE)
                goto done;
        }
    }
    // Check executable dir.
    path = jsi_execName;
    if (path)
        cp = Jsi_Strrchr(path, '/');
    if (cp) {
        len = cp-path;
        rc = jsi_PkgLoadOne(interp, name, path, len, &fval, ver);
        if (rc != JSI_CONTINUE)
            goto done;
    }
    // Check script dir.
    if ((path = interp->framePtr->fileName) || (interp->argv0 && (path = Jsi_ValueString(interp, interp->argv0, NULL)))) {
        if ((cp = Jsi_Strrchr(path, '/'))) {
            len = (cp-path);
            rc = jsi_PkgLoadOne(interp, name, path, len, &fval, ver);
            if (rc != JSI_CONTINUE)
                goto done;
        }
    } else if (interp->curDir) { // Interactive mode 
        rc = jsi_PkgLoadOne(interp, name, interp->curDir, Jsi_Strlen(interp->curDir), &fval, ver);
        if (rc != JSI_CONTINUE)
            goto done;
    }
    return JSI_ERROR;
    
done:
    if (rc == JSI_CONTINUE)
        rc = JSI_ERROR;
    if (rc == JSI_OK) {
        Jsi_HashEntry *hPtr = Jsi_HashEntryFind(interp->packageHash, name);
        if (!hPtr)
            rc = JSI_ERROR;
        else {
            jsi_PkgInfo *ptr, *ptr2;
            Jsi_HashEntry *hPtr = Jsi_HashEntryFind(interp->packageHash, name);
            if (hPtr) {
                ptr = (jsi_PkgInfo*)Jsi_HashValueGet(hPtr);
                if (ptr)
                    if (fval) {
                        char *fval2 = Jsi_Realpath(interp, fval, NULL);
                        ptr->loadFile = Jsi_KeyAdd(interp, fval2);
                        if (interp->parent && ptr->initProc) {
                            ptr2 = jsiPkgGet(interp->topInterp, name);
                            if (ptr2)
                                ptr2->loadFile = Jsi_KeyAdd(interp, ptr->loadFile);
                        }
                        Jsi_DecrRefCount(interp, fval);
                        free(fval2);
                    }
            }
        }
    }
    if (rc != JSI_OK && fval)
        Jsi_DecrRefCount(interp, fval);
    return rc;
}


Jsi_Number Jsi_PkgRequire(Jsi_Interp *interp, const char *name, int version)
{
    int ver = 0;
    jsi_PkgInfo *ptr = jsiPkgGet(interp, name), *ptr2 = NULL;
    if (ptr) {
        if (ptr->initProc && ptr->needInit) {
            if ((*ptr->initProc)(interp, 0) != JSI_OK) 
                return -1;
            ptr->needInit = 0;
        }
        return ptr->version;
    } else if ((ptr2 = jsiPkgGet(interp->topInterp, name)) && ptr2->initProc) {
        // C-extension is in topInterp
        if (interp->debugOpts.pkgTrace)
            Jsi_Printf(jsi_Stderr , "Package-Trace: load from topLevel\n");
        ptr = (jsi_PkgInfo*)Jsi_Calloc(1, sizeof(*ptr));
        *ptr = *ptr2;
        if (ptr->loadFile)
            ptr->loadFile = Jsi_KeyAdd(interp, ptr->loadFile);
        ptr->needInit = 1;
        Jsi_HashSet(interp->packageHash, name, ptr);
        if ((*ptr2->initProc)(interp, 0) == JSI_OK && (ptr = jsiPkgGet(interp, name))) {
            ptr->needInit = 0;
            return ptr->version;
        }
    }
    if (interp->pkgReqDepth>interp->maxIncDepth) {
        Jsi_LogError("recursive require('%s')", name);
        return -1;
    }
    
    interp->pkgReqDepth++;
    if (jsi_PkgLoad(interp, name, version) != JSI_OK) {
        //       Jsi_Value *vl = jsi_LoadFunction(interp, name, NULL);
        Jsi_LogError("failed require('%s')", name);
        interp->pkgReqDepth--;
        return -1;
    }
    interp->pkgReqDepth--;
    ver = Jsi_PkgVersion(interp, name, NULL);
    if (ver < 0)
        Jsi_LogError("package not found for require('%s')", name);

    return ver;
}

#define FN_require JSI_INFO("\
With no arguments, returns the list of all loaded packages. \
With one argument, loads the package (if necessary) and returns its version. \
With two arguments, also returns an object containing the version and loadFile, \
but if version argument is less than package version throws an error.")
static Jsi_RC SysRequireCmd(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this,
    Jsi_Value **ret, Jsi_Func *funcPtr)
{
    Jsi_Number n = 1;
    int argc = Jsi_ValueGetLength(interp, args);
    if (argc == 0)
        return Jsi_HashKeysDump(interp, interp->packageHash, ret, 0);
        
    Jsi_Value *vname = Jsi_ValueArrayIndex(interp, args, 0);
    const char *name = NULL;
    if (Jsi_ValueIsString(interp, vname)) {
        name = Jsi_ValueString(interp, vname, NULL);
        if (!jsiValidPkgName(name))
            name = NULL;
    }
    if (!name) 
        return Jsi_LogError("invalid or missing package name");
    if (argc>1) {
        Jsi_Value *v = Jsi_ValueArrayIndex(interp, args, 1);
        if (v && (Jsi_GetDoubleFromValue(interp, v, &n) != JSI_OK || Jsi_NumberIsNaN(n) || n<0)) 
            return Jsi_LogError("bad version: %" JSI_NUMGFMT, n);
    }
    int isMain = interp->isMain;
    interp->isMain = 0;
    Jsi_Number ver = Jsi_PkgRequire(interp, name, n);
    interp->isMain = isMain;
    if (ver < 0)
        return JSI_ERROR;
    if (argc>1) {
        if (ver < n) 
            Jsi_LogWarn("package %s downlevel: %" JSI_NUMGFMT " < %" JSI_NUMGFMT, name, ver, n);
        jsi_PkgInfo *ptr;
        Jsi_HashEntry *hPtr = Jsi_HashEntryFind(interp->packageHash, name);
        if (hPtr && ((ptr = (jsi_PkgInfo*)Jsi_HashValueGet(hPtr)))) {
            return Jsi_JSONParseFmt(interp, ret, "{version:%" JSI_NUMGFMT ", loadFile:\"%s\"}",
                ptr->version, ptr->loadFile?ptr->loadFile:"" );
        }
    }
    Jsi_ValueMakeNumber(interp, ret, ver);
    return JSI_OK;
}

Jsi_RC Jsi_PkgProvide(Jsi_Interp *interp, const char *name, Jsi_Number version, Jsi_InitProc *initProc)
{
    jsi_PkgInfo *ptr;
    Jsi_HashEntry *hPtr = Jsi_HashEntryFind(interp->packageHash, name);
    if (version<0) {
        if (hPtr) {
            ptr = (jsi_PkgInfo*)Jsi_HashValueGet(hPtr);
            Jsi_HashEntryDelete(hPtr);
        }
    } else if (version == 0) 
        return Jsi_LogError("Version must be > 0");
    else {
        if (hPtr) {
            ptr = (jsi_PkgInfo*)Jsi_HashValueGet(hPtr);
            if (ptr && ptr->needInit==0) 
                return Jsi_LogError("package %s already provided from: %s", name, ptr->loadFile?ptr->loadFile:"");
            return JSI_OK;
        }
        if (!jsiValidPkgName(name)) 
            return Jsi_LogError("invalid package name");
        ptr = (jsi_PkgInfo*)Jsi_Calloc(1, sizeof(*ptr));
        ptr->version = version;
        ptr->initProc = initProc;
        if (interp->framePtr->fileName && !initProc)
            ptr->loadFile = Jsi_KeyAdd(interp, interp->framePtr->fileName);
        Jsi_HashSet(interp->packageHash, (void*)name, ptr);
        if (initProc && interp->parent) { // Provide C extensions to topInterp.
            ptr = jsiPkgGet(interp->topInterp, name);
            if (ptr == NULL) {
                if (Jsi_PkgProvide(interp->topInterp, name, version, initProc) != JSI_OK)
                    return JSI_ERROR;
                ptr = jsiPkgGet(interp->topInterp, name);
                if (!ptr)
                    return JSI_ERROR;
                ptr->needInit = 1;
            }
        }

    }
    return JSI_OK;
}

static Jsi_RC SysProvideCmd(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this,
    Jsi_Value **ret, Jsi_Func *funcPtr)
{
    Jsi_Number n = 1;
    Jsi_Value *vname = Jsi_ValueArrayIndex(interp, args, 0);
    const char *name = NULL;
    if (Jsi_ValueIsString(interp, vname)) {
        name = Jsi_ValueString(interp, vname, NULL);
        if (!jsiValidPkgName(name))
            name = NULL;
    }
    if (!name) 
        return Jsi_LogError("invalid or missing package name");

    Jsi_Value *v = Jsi_ValueArrayIndex(interp, args, 1);
    if (v && (Jsi_GetDoubleFromValue(interp, v, &n) != JSI_OK || Jsi_NumberIsNaN(n) || n<=0)) 
        return Jsi_LogError("bad version: %" JSI_NUMGFMT, n);
    return Jsi_PkgProvide(interp, name, n, NULL);
}

Jsi_RC jsi_NoOpCmd(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this,
    Jsi_Value **ret, Jsi_Func *funcPtr)
{
    return JSI_OK;
}

static Jsi_RC isNaNCmd(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this,
    Jsi_Value **ret, Jsi_Func *funcPtr)
{
    Jsi_Number n;
    int rc = 0;
    Jsi_Value *v = Jsi_ValueArrayIndex(interp, args, 0);
    if (Jsi_ValueIsBoolean(interp, v))
        rc = 0;
    else if (!Jsi_ValueIsNumber(interp, v) || Jsi_GetDoubleFromValue(interp, v, &n) != JSI_OK || Jsi_NumberIsNaN(n))
        rc = 1;
    Jsi_ValueMakeBool(interp, ret, rc);
    return JSI_OK;
}
static Jsi_RC isFiniteCmd(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this,
    Jsi_Value **ret, Jsi_Func *funcPtr)
{
    Jsi_Number n;
    int rc = 1;
    Jsi_Value *v = Jsi_ValueArrayIndex(interp, args, 0);
    if (!Jsi_ValueIsNumber(interp, v) || Jsi_GetDoubleFromValue(interp, v, &n) != JSI_OK || !Jsi_NumberIsFinite(n))
        rc = 0;
    Jsi_ValueMakeBool(interp, ret, rc);
    return JSI_OK;
}

/* Returns a url-encoded version of str */
/* IMPORTANT: be sure to free() the returned string after use */
static char *url_encode(char *str) {
  char *pstr = str, *buf = (char*)Jsi_Malloc(Jsi_Strlen(str) * 3 + 1), *pbuf = buf;
  while (*pstr) {
    if (isalnum(*pstr) || *pstr == '-' || *pstr == '_' || *pstr == '.' || *pstr == '~') 
      *pbuf++ = *pstr;
    else if (*pstr == ' ') 
      *pbuf++ = '+';
    else 
      *pbuf++ = '%', *pbuf++ = jsi_toHexChar(*pstr >> 4), *pbuf++ = jsi_toHexChar(*pstr & 15);
    pstr++;
  }
  *pbuf = '\0';
  return buf;
}

/* Returns a url-decoded version of str */
/* IMPORTANT: be sure to free() the returned string after use */
static char *url_decode(char *str) {
  char *pstr = str, *buf = (char*)Jsi_Malloc(Jsi_Strlen(str) + 1), *pbuf = buf;
  while (*pstr) {
    if (*pstr == '%') {
      if (pstr[1] && pstr[2]) {
        *pbuf++ = jsi_fromHexChar(pstr[1]) << 4 | jsi_fromHexChar(pstr[2]);
        pstr += 2;
      }
    } else if (*pstr == '+') { 
      *pbuf++ = ' ';
    } else {
      *pbuf++ = *pstr;
    }
    pstr++;
  }
  *pbuf = '\0';
  return buf;
}

static Jsi_RC EncodeURICmd(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this,
    Jsi_Value **ret, Jsi_Func *funcPtr)
{
    char *cp, *str = Jsi_ValueArrayIndexToStr(interp, args, 0, NULL);
    cp = url_encode(str);
    Jsi_ValueMakeString(interp, ret, cp);
    return JSI_OK;
}

static Jsi_RC DecodeURICmd(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this,
    Jsi_Value **ret, Jsi_Func *funcPtr)
{
    char *cp, *str = Jsi_ValueArrayIndexToStr(interp, args, 0, NULL);
    cp = url_decode(str);
    Jsi_ValueMakeString(interp, ret, cp);
    return JSI_OK;
}

static Jsi_RC SysSleepCmd(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this,
    Jsi_Value **ret, Jsi_Func *funcPtr)
{
    Jsi_Number dtim = 1;
    Jsi_Value *v = Jsi_ValueArrayIndex(interp, args, 0);
    if (v) {
        if (Jsi_GetDoubleFromValue(interp, v, &dtim) != JSI_OK) {
            return JSI_ERROR;
        }
    }
    return Jsi_Sleep(interp, dtim);
}


static Jsi_RC SysGetEnvCmd(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this,
    Jsi_Value **ret, Jsi_Func *funcPtr)
{
    extern char **environ;
    char *cp;
    int i;
    Jsi_Value *v = Jsi_ValueArrayIndex(interp, args, 0);
    if (v != NULL) {
        const char *fnam = Jsi_ValueString(interp, v, NULL);
        if (!fnam) 
            return Jsi_LogError("expected string");
        cp = getenv(fnam);
        if (cp != NULL) {
            Jsi_ValueMakeString(interp, ret, Jsi_Strdup(cp));
        }
        return JSI_OK;
    }
   /* Single object containing result members. */
    Jsi_Value *vres;
    Jsi_Obj  *ores = Jsi_ObjNew(interp);
    Jsi_Value *nnv;
    char *val, nam[200];
    Jsi_ObjIncrRefCount(interp, ores);
    vres = Jsi_ValueMakeObject(interp, NULL, ores);
    Jsi_IncrRefCount(interp, vres);
    
    for (i=0; ; i++) {
        int n;
        cp = environ[i];
        if (cp == 0 || ((val = Jsi_Strchr(cp, '='))==NULL))
            break;
        n = val-cp;
        if (n>=(int)sizeof(nam))
            n = sizeof(nam)-1;
        Jsi_Strncpy(nam, cp, n);
        val = val+1;
        nnv = Jsi_ValueMakeString(interp, NULL, Jsi_Strdup(val));
        Jsi_ObjInsert(interp, ores, nam, nnv, 0);
    }
    Jsi_ValueReplace(interp, ret, vres);
    return JSI_OK;
}

static Jsi_RC SysSetEnvCmd(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this,
                        Jsi_Value **ret, Jsi_Func *funcPtr)
{
    Jsi_Value *v = Jsi_ValueArrayIndex(interp, args, 0);
    const char *fnam = Jsi_ValueString(interp, v, NULL);
    Jsi_Value *vv = Jsi_ValueArrayIndex(interp, args, 1);
    const char *cp = NULL, *fval = (vv?Jsi_ValueString(interp, vv, NULL):NULL);
    int rc = -1;
    if (fnam == 0) 
        return Jsi_LogError("expected string");

    if (fnam[0] != 0) {
#ifndef __WIN32
        if (fval)
            rc = setenv(fnam, fval, 1);
        else
            cp = getenv(fnam);
#else  /* TODO: win setenv */
        if (!fval)
            cp = getenv(fnam);
        else {
            char ebuf[BUFSIZ];
            snprintf(ebuf, sizeof(ebuf), "%s=%s", fnam, fval);
            rc = _putenv(ebuf);
        }
#endif
    }
    if (rc != 0) 
        return Jsi_LogError("setenv failure: %s = %s", fnam, fval);
    if (cp)
        Jsi_ValueMakeStringDup(interp, ret, cp);
    return JSI_OK;
}

static Jsi_RC SysGetPidCmd(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this,
                        Jsi_Value **ret, Jsi_Func *funcPtr)
{
#ifdef __WIN32
    return Jsi_LogError("unsupported");
#else
    Jsi_Number pid = 0;
    bool bv = 0;
    Jsi_Value *arg = Jsi_ValueArrayIndex(interp, args, 0);
    if (arg && Jsi_GetBoolFromValue(interp, arg, &bv) != JSI_OK)
        return JSI_ERROR;
    if (bv)
        pid = (Jsi_Number)getppid();
    else
        pid = (Jsi_Number)getpid();
    Jsi_ValueMakeNumber(interp, ret, pid);
    return JSI_OK;
#endif
}

typedef struct {
    Jsi_Value* inputStr;
    bool bg;
    bool noError;
    bool noTrim;
    bool retCode;
    bool retAll;
    bool retval;
} ExecOpts;

static Jsi_OptionSpec ExecOptions[] = {
    JSI_OPT(BOOL,   ExecOpts, bg,       .help="Run command in background using system() and return OS code" ),
    JSI_OPT(STRING, ExecOpts, inputStr, .help="Use string as input and return OS code" ),
    JSI_OPT(BOOL,   ExecOpts, noError,  .help="Suppress all OS errors" ),
    JSI_OPT(BOOL,   ExecOpts, noTrim,   .help="Do not trim trailing whitespace from output" ),
    JSI_OPT(BOOL,   ExecOpts, retAll,   .help="Return the OS return code and data as an object" ),
    JSI_OPT(BOOL,   ExecOpts, retCode,  .help="Return only the OS return code" ),
    {JSI_OPTION_END}
};

#define FN_exec JSI_INFO("\
If the command ends with '&', set the 'bg' option to true. \
If the second argument is a string, the 'inputStr' option is set. \
By default, returns the string output, unless the 'bg', 'inputStr', 'retCode' or 'retAll' options are used")
static Jsi_RC SysExecCmd(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this,
    Jsi_Value **ret, Jsi_Func *funcPtr)
{
    if (interp->isSafe) 
        return Jsi_LogError("no exec in safe mode");
    int n, exitCode = 0, hasopts = 0, sLen, sLen2;
    Jsi_RC rc = JSI_OK;
    Jsi_Value *arg = Jsi_ValueArrayIndex(interp, args, 0);
    Jsi_Value *opt = Jsi_ValueArrayIndex(interp, args, 1);
    const char *cp2=NULL, *cp = Jsi_ValueString(interp, arg, &sLen);
    Jsi_DString dStr = {};
    ExecOpts edata = {};
    edata.retval = 1;
    
    if (opt != NULL) {
        if (opt->vt == JSI_VT_OBJECT && opt->d.obj->ot == JSI_OT_OBJECT) {
            hasopts = 1;
            if (Jsi_OptionsProcess(interp, ExecOptions, &edata, opt, 0) < 0) {
                return JSI_ERROR;
            }
            if (edata.retCode)
                edata.retval = 0;
        } else if ((cp2=Jsi_ValueString(interp, opt, &sLen2))) {
            edata.inputStr = opt;
        } else 
            return Jsi_LogError("expected: string?,null|instr?");
    }

    int isbg = 0, ec = 0;
    if (edata.bg || (isbg=((sLen>1 && cp[sLen-1] == '&')))) {
        if (edata.inputStr) {
            rc = Jsi_LogError("inputStr can not used with bg");
            goto done;
        }
        if (!isbg) {
            Jsi_DSAppend(&dStr, cp, " &", NULL);
            cp = Jsi_DSValue(&dStr);
        }
        edata.bg = 1;
        edata.retCode = 1;
        edata.retval = 0;
        exitCode = ((ec=system(cp))>>8);
        Jsi_DSSetLength(&dStr, 0);
    } else if (edata.inputStr) {
        edata.retCode = 1;
        edata.retval = 0;
        cp2 = Jsi_ValueString(interp, edata.inputStr, &sLen2);
        FILE *fp = popen(cp, "w");
        if (!fp) 
            exitCode = errno;
        else {
            while ((n=fwrite(cp2, 1, sLen2, fp))>0) {
                sLen2 -= n;
            }
            exitCode = ((ec=pclose(fp))>>8);
        }
    } else {
        FILE *fp = popen(cp, "r");
        if (!fp) 
            exitCode = errno;
        else {
            char buf[BUFSIZ];;
            while ((n=fread(buf, 1, sizeof(buf), fp))>0)
                Jsi_DSAppendLen(&dStr, buf, n);
            exitCode = ((ec=pclose(fp))>>8);
        }
    }
    if (exitCode && edata.noError==0 && edata.retCode==0 && edata.retAll==0) {
        if (exitCode==ENOENT)
            Jsi_LogError("command not found: %s", cp);
        else
            Jsi_LogError("program exit code (%x)", exitCode);
        rc = JSI_ERROR;
    }
    if (!edata.noTrim) {
        char *cp = Jsi_DSValue(&dStr);
        int iLen = Jsi_DSLength(&dStr);
        while (iLen>0 && isspace(cp[iLen-1]))
            iLen--;
        cp[iLen] = 0;
    }
    if (edata.retAll) {
        Jsi_Obj *nobj = Jsi_ObjNew(interp);
        Jsi_ValueMakeObject(interp, ret, nobj);
        Jsi_Value *cval = Jsi_ValueNewNumber(interp, (Jsi_Number)exitCode);
        Jsi_ObjInsert(interp, nobj, "code", cval, 0);
        cval = Jsi_ValueNewNumber(interp, (Jsi_Number)(ec&0xff));
        Jsi_ObjInsert(interp, nobj, "status", cval, 0);
        cval = Jsi_ValueNew(interp);
        Jsi_ValueFromDS(interp, &dStr, &cval);
        Jsi_ObjInsert(interp, nobj, "data", cval, 0);
        
    } else if (edata.retval)
        Jsi_ValueFromDS(interp, &dStr, ret);
    else {
        Jsi_DSFree(&dStr);
        Jsi_ValueMakeNumber(interp, ret, (Jsi_Number)exitCode);
    }
done:
    if (hasopts)
        Jsi_OptionsFree(interp, ExecOptions, &edata, 0);
    return rc;
}

static Jsi_RC SysPutsCmd_(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this, Jsi_Value **ret,
    Jsi_Func *funcPtr, bool stdErr, jsi_LogOptions *popts, const char *argStr)
{
    int i, cnt = 0, quote = (popts->file);
    const char *fn = NULL;
    Jsi_DString dStr, oStr;
    if (interp->noStderr)
        stdErr = 0;
    Jsi_Chan *chan = (stdErr ? jsi_Stderr : jsi_Stdout);
    Jsi_DSInit(&dStr);
    Jsi_DSInit(&oStr);
    if (popts->chan && !stdErr) {
        Jsi_UserObj *uobj = popts->chan->d.obj->d.uobj;
        jsi_UserObjToName(interp, uobj, &dStr);
        Jsi_Channel nchan = Jsi_FSNameToChannel(interp, Jsi_DSValue(&dStr));
        if (nchan)
            chan = nchan;
        Jsi_DSSetLength(&dStr, 0);
    }
    if (interp->curIp) {
        int didx = 0;
        const char *cp;
        fn = interp->curIp->fname;
        if (fn && !popts->full && (cp=Jsi_Strrchr(fn, '/')))
            fn = cp +1;
        if (popts->time || (didx=popts->date)) {
            if (popts->time && popts->date)
                didx = 2;
            const char *fmts[3] = { "%H-%M-%f", "%F", "%F %H-%M-%f" },
                *fmt = (popts->timeFmt?popts->timeFmt:fmts[didx]);
            Jsi_DatetimeFormat(interp, Jsi_DateTime(), fmt, popts->isUTC, &dStr);
            Jsi_DSAppend(&dStr, ", ", NULL);
        }
        if (popts->file && popts->before) {
            Jsi_DSPrintf(&dStr, "%s:%d: ", fn, interp->curIp->Line);
            if (interp->curIp->Line<1000)
                Jsi_DSAppend(&dStr, (interp->curIp->Line<10?"  ":" "), NULL);
        }
        if (Jsi_DSLength(&dStr)) {
            quote = 1;
        }
        if (quote)
            Jsi_DSAppendLen(&dStr, "\"", 1);
    }
    if (!args)
        Jsi_DSAppend(&dStr, argStr?argStr:"", NULL);
    else {
        int argc = Jsi_ValueGetLength(interp, args);
        for (i = 0; i < argc; ++i) {
            Jsi_Value *v = Jsi_ValueArrayIndex(interp, args, i);
            if (!v) continue;
            if (cnt++)
                Jsi_DSAppendLen(&dStr, " ", 1);
            const char *cp = Jsi_ValueString(interp, v, 0);
            if (cp) {
                Jsi_DSAppend(&dStr, cp, NULL);
                continue;
            }
            Jsi_DSSetLength(&oStr, 0);
            Jsi_ValueGetDString(interp, v, &oStr, 1);
            Jsi_DSAppend(&dStr, Jsi_DSValue(&oStr), NULL);
        }
    }
    if (quote)
        Jsi_DSAppendLen(&dStr, "\"", 1);
    if (popts->file && !popts->before) {
        Jsi_DSPrintf(&dStr, ", %s:%d", fn?fn:"", interp->curIp->Line);
    }
    if (popts->func) {
        // Note: could have looked this up on the stackFrame.
        if (!interp->prevActiveFunc || !((fn=interp->prevActiveFunc->name)))
            fn = "";
        Jsi_DSPrintf(&dStr, ", %s%s", fn[0]?fn:"", fn[0]?"()":"");
    }
    Jsi_DSAppend(&dStr, "\n", NULL);
    chan->interp = interp;
    Jsi_Puts(chan, Jsi_DSValue(&dStr));
    Jsi_DSFree(&dStr);
    Jsi_DSFree(&oStr);
    return JSI_OK;
}

static Jsi_RC SysPrintfCmd(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this, Jsi_Value **ret,
    Jsi_Func *funcPtr)
{
    Jsi_DString dStr;
    Jsi_RC rc = Jsi_FormatString(interp, args, &dStr);
    if (rc != JSI_OK)
        return rc;
    const char *cp;
    (jsi_Stdout)->interp = interp;
    Jsi_Puts(jsi_Stdout, cp = Jsi_DSValue(&dStr));
    if (!Jsi_Strchr(cp, '\n'))
        Jsi_Flush(jsi_Stdout);
    Jsi_DSFree(&dStr);
    return rc;
}

static Jsi_RC consoleLogCmd(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this, Jsi_Value **ret,
    Jsi_Func *funcPtr)
{
    return SysPutsCmd_(interp, args, _this, ret, funcPtr, 1, &interp->logOpts, NULL);
}

#define FN_consolelogf JSI_INFO("\
Also, if there are multiple arguments, and the first argument does not contains a %, \
the second argument is the format, and the first is prepended to the output. \
This command is used with bind/alias to handle LogDebug, LogTrace, ...")
static Jsi_RC consoleLogfCmd(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this, Jsi_Value **ret,
    Jsi_Func *funcPtr)
{
    Jsi_DString dStr;
    int argc = Jsi_ValueGetLength(interp, args);
    Jsi_Value *nargs = args, *pval = Jsi_ValueArrayIndex(interp, args, 0);
    int pLen;
    const char *cp = NULL;
    if (pval) cp = Jsi_ValueString(interp, pval, &pLen);
    if (argc>1 && cp && !Jsi_Strchr(cp, '%')) {
        nargs = Jsi_ValueDup(interp, args);
        Jsi_ValueArrayShift(interp, nargs);
    }
    Jsi_RC rc = Jsi_FormatString(interp, nargs, &dStr);
    if (nargs != args)
        Jsi_DecrRefCount(interp, nargs);
    if (rc != JSI_OK)
        return rc;
    if (nargs != args) {
        int dlen = Jsi_DSLength(&dStr);
        Jsi_DSSetLength(&dStr, dlen+pLen+1);
        char *cd = Jsi_DSValue(&dStr);
        memmove(cd+pLen, cd, dlen+1);
        memcpy(cd, cp, pLen);
    }
    cp = Jsi_DSValue(&dStr);

    rc = SysPutsCmd_(interp, NULL, _this, ret, funcPtr, 1, &interp->logOpts, cp);
    Jsi_DSFree(&dStr);
    return rc;
}

static Jsi_RC consolePutsCmd(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this, Jsi_Value **ret,
    Jsi_Func *funcPtr)
{
    jsi_LogOptions lo = {};
    return SysPutsCmd_(interp, args, _this, ret, funcPtr, 1, (interp->tracePuts?&interp->logOpts:&lo), NULL);
}

#define FN_puts JSI_INFO("\
Each argument is quoted.  Use Interp.logOpts to control source line and/or timestamps output.")
static Jsi_RC SysPutsCmd(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this, Jsi_Value **ret,
    Jsi_Func *funcPtr)
{
    jsi_LogOptions lo = {};
    return SysPutsCmd_(interp, args, _this, ret, funcPtr, 0, (interp->tracePuts?&interp->logOpts:&lo), NULL);
}

static Jsi_RC SysLogCmd(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this, Jsi_Value **ret,
    Jsi_Func *funcPtr)
{
    return SysPutsCmd_(interp, args, _this, ret, funcPtr, 0, &interp->logOpts, NULL);
}

typedef struct {
    jsi_AssertMode mode;
    bool noStderr;
} AssertData;

static Jsi_OptionSpec AssertOptions[] = {
    JSI_OPT(CUSTOM, AssertData, mode,     .help="Action when assertion is false. Default from Interp.assertMode", .flags=0, .custom=Jsi_Opt_SwitchEnum, .data=jsi_AssertModeStrs ),
    JSI_OPT(BOOL,   AssertData, noStderr, .help="Logged msg to stdout. Default from Interp.noStderr" ),
    JSI_OPT_END(AssertData, .help="Options for assert command")
};

static char *jsi_GetCurPSLine(Jsi_Interp *interp) {
    char *cp = NULL;
    int line = interp->curIp->Line;
    if (interp->framePtr->ps &&  (cp=interp->framePtr->ps->lexer->d.str))
        while (line-- > 1)
            if ((cp = Jsi_Strchr(cp, '\n'))) cp++;
    return cp;
}
    
#define FN_assert JSI_INFO("\
Assert does nothing by default, but can be \
enabled with \"use asserts\" or setting Interp.asserts.")
Jsi_RC jsi_AssertCmd(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this,
    Jsi_Value **ret, Jsi_Func *funcPtr)
{
    if (!interp->asserts)
        return JSI_OK;
    int rc = 0;
    Jsi_RC rv = JSI_OK;
    Jsi_Number d;
    Jsi_Value *v = Jsi_ValueArrayIndex(interp, args, 0);
    const char *msg = Jsi_ValueArrayIndexToStr(interp, args, 1, NULL);
    Jsi_Value *opts = Jsi_ValueArrayIndex(interp, args, 2);
    //int hasopts = 0;
    bool b;
    AssertData udata = {
        .mode=interp->assertMode,
        .noStderr=interp->noStderr
    };
    
    if (opts != NULL) {
        if (opts->vt == JSI_VT_OBJECT) {
            //hasopts = 1;
            if (Jsi_OptionsProcess(interp, AssertOptions, &udata, opts, 0) < 0) {
                return JSI_ERROR;
            }
        } else //if (opts->vt != JSI_VT_NULL)
            return Jsi_LogError("arg 3: expected object assert options");
    }

    if (Jsi_ValueGetNumber(interp,v, &d) == JSI_OK)
        rc = (int)d;
    else if (Jsi_ValueGetBoolean(interp,v, &b) == JSI_OK)
        rc = b;
    else if (Jsi_ValueIsFunction(interp, v)) {
        if (!msg) {
        }
        Jsi_Value *vpargs = Jsi_ValueMakeObject(interp, NULL, Jsi_ObjNewArray(interp, NULL, 0, 0));
        Jsi_IncrRefCount(interp, vpargs);
        rc = Jsi_FunctionInvoke(interp, v, vpargs, ret, NULL);
        Jsi_DecrRefCount(interp, vpargs);
        bool b;
        if (Jsi_ValueGetNumber(interp, *ret, &d) == JSI_OK)
            rc = (int)d;
        else if (Jsi_ValueGetBoolean(interp, *ret, &b) == JSI_OK)
            rc = b;
        else 
            return Jsi_LogError("invalid function assert");
    } else 
        return Jsi_LogError("invalid assert");
    if (rc == 0) {
        char mbuf[1024];
        mbuf[0] = 0;
        if (!msg) {
            char *ce, *cp = jsi_GetCurPSLine(interp);
            if (cp) {
                cp = Jsi_Strstr(cp, "assert(");
                while (cp && *cp && isspace(*cp)) cp++;
                if (cp) {
                    Jsi_Strncpy(mbuf, cp, sizeof(mbuf)-1);
                    mbuf[sizeof(mbuf)-1] = 0;
                    ce=Jsi_Strstr(mbuf, ");");
                    if (ce) {
                        ce[1] = 0;
                        msg = mbuf;
                    }
                }
            }
            if (!msg)
                msg = "ASSERT";
        }
        //Jsi_ValueDup2(interp, ret, v);
        if (udata.mode != jsi_AssertModeThrow) {
            Jsi_DString dStr;
            jsi_LogOptions lo = {}, *loPtr = ((udata.mode==jsi_AssertModeLog || interp->tracePuts)?&interp->logOpts:&lo);
            Jsi_DSInit(&dStr);
            const char *imsg = Jsi_DSAppend(&dStr, msg, NULL);
            SysPutsCmd_(interp, NULL, _this, ret, funcPtr, !udata.noStderr, loPtr, imsg);
            Jsi_DSFree(&dStr);
        } else
            rv = Jsi_LogError("%s", msg);
    }
    Jsi_ValueMakeUndef(interp, ret);
    return rv;
}

typedef struct {
    bool utc;
    const char *fmt;
} DateOpts;

static Jsi_OptionSpec DateOptions[] = {
    JSI_OPT(BOOL,   DateOpts, utc, .help="time is in utc" ),
    JSI_OPT(STRKEY, DateOpts, fmt, .help="format string for time" ),
    {JSI_OPTION_END}
};

static const char *timeFmts[] = {
    "%Y-%m-%d %H:%M:%S",
    "%Y-%m-%dT%H:%M:%S",
    "%Y-%m-%d %H:%M:%S",
    "%Y-%m-%d %H:%M",
    "%Y-%m-%dT%H:%M",
    "%Y-%m-%d",
    "%H:%M:%S",
    "%H:%M",
    "%c",
    NULL
};

Jsi_RC Jsi_DatetimeParse(Jsi_Interp *interp, const char *str, const char *fmt, int isUtc, Jsi_Number *datePtr)
{
    char fmt1[BUFSIZ];
    const char *rv = NULL;
    Jsi_Wide n = -1;
    int j = 0, tofs = 0;
    Jsi_RC rc = JSI_OK;
    Jsi_Number ms = 0;
    struct tm tm = {};
    time_t t;
    if (!Jsi_Strcmp("now", str)) {
        struct timeval tv;
        struct timezone tz;
        gettimeofday(&tv, &tz);
        n = tv.tv_sec*1000LL + ((Jsi_Wide)tv.tv_usec)/1000LL;
        if (isUtc) {
            n += 60 * tz.tz_minuteswest;
        }
        if (datePtr)
            *datePtr = (Jsi_Number)n;
        return rc;
    }
    if (fmt && fmt[0]) {
        const char *efp = Jsi_Strstr(fmt, "%f");
        if (efp && efp > fmt && efp[-1] != '%' && (Jsi_Strlen(fmt)+10)<(int)sizeof(fmt1)) {
            snprintf(fmt1, sizeof(fmt1), "%.*s:%%S.", (int)(efp-fmt), fmt);
            fmt = fmt1;
        }
        rv = strptime(str, fmt, &tm);
    } else {
        if (fmt) j++;
        while (timeFmts[j]) {
            rv = strptime(str, timeFmts[j], &tm);
            if (rv != NULL)
                break;
            j++;
        }
    }
    if (!rv)
        rc = Jsi_LogError("parse failed");
    else {
        if (*rv == '.' && isdigit(rv[1]) && isdigit(rv[2]) && isdigit(rv[3])) {
            ms = atof(rv+1);
            rv += 4;
        }
        if (isUtc) {
#ifdef __WIN32
#ifdef JSI_IS64BIT
            t = internal_timegm(&tm);
#else
            t = _mkgmtime(&tm); // TODO: undefined in mingw 64
#endif
#else
            t = timegm(&tm);
#endif
        } else {
            int th, ts;
            char ss[3];
            t = mktime(&tm);
            //TODO: dst
            /*if (tm.tm_isdst)
                t -= 3600;*/
            if (rv[0] == ' ') rv++;
            if (sscanf(rv, "%[+-]%2d:%2d", ss, &th, &ts) == 3) {
                int sign = (rv[0] == '-' ? 1 : -1);
                tofs = (3600*th+60*ts)*sign;
            }
        }
        if (t==-1)
            rc = Jsi_LogError("mktime failed");
        n = (Jsi_Number)(t+tofs)*1000 + ms;
    }
    if (rc == JSI_OK && datePtr)
        *datePtr = (Jsi_Number)n;
    return rc;
}

Jsi_RC Jsi_DatetimeFormat(Jsi_Interp *interp, Jsi_Number num, const char *fmt, int isUtc, Jsi_DString *dStr)
{
    char buf[BUFSIZ], fmt1[BUFSIZ], fmt2[BUFSIZ];
    time_t tt;
    Jsi_RC rc = JSI_OK;
    fmt2[0] = 0;
    
    tt = (time_t)(num/1000);
    if (fmt==NULL)
        fmt = timeFmts[0];
    else if (*fmt == 0) {
        if (!isUtc)
            fmt = timeFmts[1];
        else
            fmt = timeFmts[2];
    }
    
    const char *efp = Jsi_Strstr(fmt, "%f");
    if (efp && efp > fmt && efp[-1] != '%' && ((efp-fmt)+10)<(int)sizeof(fmt1)) {
        int elen = (efp-fmt)+1;
        Jsi_Strncpy(fmt1, fmt, elen);
        Jsi_Strcpy(fmt1+elen, "%S.");
        Jsi_Strcpy(fmt2, efp+2);
        fmt = fmt1;
    } else
        efp = NULL;
    struct tm tm, *tmp=&tm;
#ifdef __WIN32
    if (isUtc)
        tmp = gmtime(&tt);
    else
        tmp = localtime(&tt);
#else
    if (isUtc)
        gmtime_r(&tt, &tm);
    else
        localtime_r(&tt, &tm);
#endif
    buf[0] = 0;
    int rr = strftime(buf, sizeof(buf), fmt, tmp);
       
    if (rr<=0)
        rc = Jsi_LogError("time format error: %d", (int)tt);
    else {
        Jsi_DSAppendLen(dStr, buf, -1);
        if (efp) {
            snprintf(buf, sizeof(buf), "%3.3d", (int)(((Jsi_Wide)num)%1000));
            Jsi_DSAppendLen(dStr, buf[1]=='.'?buf+2:buf, -1);
        }
    }
    if (rc == JSI_OK && fmt2[0]) {
        buf[0] = 0;
        int rr = strftime(buf, sizeof(buf), fmt2, tmp);
           
        if (rr<=0)
            rc = Jsi_LogError("time format error");
        else
            Jsi_DSAppendLen(dStr, buf, -1);
    }
    return rc;
}

/* Get time in milliseconds since Jan 1, 1970 */
Jsi_Number Jsi_DateTime(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    Jsi_Number num = ((Jsi_Number)tv.tv_sec*1000.0 + (Jsi_Number)tv.tv_usec/1000.0);
    return num;
}

double jsi_GetTimestamp(void) {
#ifdef __WIN32
    return Jsi_DateTime()/1000;
#else
    struct timespec tv;
    clock_gettime(CLOCK_MONOTONIC, &tv);
    return (double)tv.tv_sec + (double)tv.tv_nsec/1000000000.0;
#endif
}


static Jsi_RC DateStrptimeCmd(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this,
    Jsi_Value **ret, Jsi_Func *funcPtr)
{
    const char *str = Jsi_ValueArrayIndexToStr(interp, args, 0, NULL);
    Jsi_Value *opt = Jsi_ValueArrayIndex(interp, args, 1);
    Jsi_Number w = 0;
    int isOpt = 0;
    DateOpts opts = {};
    const char *fmt = NULL;
    if (opt != NULL && opt->vt != JSI_VT_NULL && !(fmt = Jsi_ValueString(interp, opt, NULL))) {
        if (Jsi_OptionsProcess(interp, DateOptions, &opts, opt, 0) < 0) {
            return JSI_ERROR;
        }
        isOpt = 1;
        fmt = opts.fmt;
    }
    if (!str)
        str = "now";
    Jsi_RC rc = Jsi_DatetimeParse(interp, str, fmt, opts.utc, &w);
    if (rc != JSI_OK) {
        Jsi_ValueDup2(interp, ret, interp->NaNValue);
        rc = JSI_ERROR;
    } else
        Jsi_ValueMakeNumber(interp, ret, w);
    if (isOpt)
        Jsi_OptionsFree(interp, DateOptions, &opts, 0);
    return rc;
}

#define FN_strftime JSI_INFO("\
Null or no value will use current time.")
static Jsi_RC DateStrftimeCmd(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this,
    Jsi_Value **ret, Jsi_Func *funcPtr)
{
    Jsi_Value* val = Jsi_ValueArrayIndex(interp, args, 0);
    Jsi_Value* opt = Jsi_ValueArrayIndex(interp, args, 1);
    const char *fmt = NULL;//, *cp = NULL;
    DateOpts opts = {};
    Jsi_Number num;
    int isOpt = 0;
    Jsi_RC rc = JSI_OK;
    Jsi_DString dStr;
    Jsi_DSInit(&dStr);
    
    if (val==NULL || Jsi_ValueIsNull(interp, val)) {
        num = 1000.0 * (Jsi_Number)time(NULL);
   /* } else if ((cp=Jsi_ValueString(interp, val, NULL))) {*/
        /* Handle below */
    } else if (Jsi_GetDoubleFromValue(interp, val, &num) != JSI_OK)
        return JSI_ERROR;
        
    if (opt != NULL && opt->vt != JSI_VT_NULL && !(fmt = Jsi_ValueString(interp, opt, NULL))) {
        if (Jsi_OptionsProcess(interp, DateOptions, &opts, opt, 0) < 0) {
            return JSI_ERROR;
        }
        isOpt = 1;
        fmt = opts.fmt;
    }
    const char *errMsg = "time format error";
/*    if (cp) {
        if (isdigit(*cp))
            rc = Jsi_GetDouble(interp, cp, &num);
        else {
            if (Jsi_Strcmp(cp,"now")==0) {
                num = Jsi_DateTime();
            } else {
                rc = JSI_ERROR;
                errMsg = "allowable strings are 'now' or digits";
            }
        }
    }*/
    if (rc == JSI_OK)
        rc = Jsi_DatetimeFormat(interp, num, fmt, opts.utc, &dStr);
    if (rc != JSI_OK)
        Jsi_LogError("%s", errMsg);
    else
        Jsi_ValueMakeStringDup(interp, ret, Jsi_DSValue(&dStr));
    if (isOpt)
        Jsi_OptionsFree(interp, DateOptions, &opts, 0);
    Jsi_DSFree(&dStr);
    return rc;
}

#define FN_infovars JSI_INFO("\
Returns all values, data or function.")
static Jsi_RC InfoVarsCmd(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this,
    Jsi_Value **ret, Jsi_Func *funcPtr)
{
    char *key;
    int n, curlen = 0, isreg = 0, isobj = 0;
    Jsi_HashEntry *hPtr;
    Jsi_HashSearch search;
    Jsi_Value *v;
    Jsi_Obj *nobj = Jsi_ObjNew(interp);
    Jsi_Value *arg = Jsi_ValueArrayIndex(interp, args, 0);
    const char *name = NULL;

    if (arg) {
        if (arg->vt == JSI_VT_STRING)
            name = arg->d.s.str;
        else if (arg->vt == JSI_VT_OBJECT) {
            switch (arg->d.obj->ot) {
                case JSI_OT_STRING: name = arg->d.obj->d.s.str; break;
                case JSI_OT_REGEXP: isreg = 1; break;
                case JSI_OT_FUNCTION:
                case JSI_OT_OBJECT: isobj = 1; break;
                default: return JSI_OK;
            }
        } else
            return JSI_OK;
    }
    if (!name)
        Jsi_ValueMakeArrayObject(interp, ret, nobj);
    if (isobj) {
        Jsi_TreeEntry* tPtr;
        Jsi_TreeSearch search;
        for (tPtr = Jsi_TreeSearchFirst(arg->d.obj->tree, &search, 0, NULL);
            tPtr; tPtr = Jsi_TreeSearchNext(&search)) {
            v = (Jsi_Value*)Jsi_TreeValueGet(tPtr);
            if (v==NULL || Jsi_ValueIsFunction(interp, v)) continue;
    
            n = curlen++;
            key = (char*)Jsi_TreeKeyGet(tPtr);
            Jsi_ObjArraySet(interp, nobj, Jsi_ValueNewStringKey(interp, key), n);
        }
        Jsi_TreeSearchDone(&search);
        return JSI_OK;
    }
    for (hPtr = Jsi_HashSearchFirst(interp->varTbl, &search);
        hPtr != NULL; hPtr = Jsi_HashSearchNext(&search)) {
        if (Jsi_HashValueGet(hPtr))
            continue;
        key = (char*)Jsi_HashKeyGet(hPtr);
        if (name) {
            if (Jsi_Strcmp(name,key))
                continue;
            Jsi_ValueMakeObject(interp, ret, nobj);
            v = Jsi_VarLookup(interp, key);
            Jsi_ObjInsert(interp, nobj, "type", Jsi_ValueNewStringKey(interp,Jsi_ValueTypeStr(interp, v)),0);
            Jsi_ValueMakeObject(interp, ret, nobj);
            return JSI_OK;
        }
        if (isreg) {
            int ismat;
            Jsi_RegExpMatch(interp, arg, key, &ismat, NULL);
            if (!ismat)
                continue;
        }
        n = curlen++;
        Jsi_ObjArraySet(interp, nobj, Jsi_ValueNewStringKey(interp, key), n);
    }
    return JSI_OK;
}

static Jsi_Value *jsiNewConcatValue(Jsi_Interp *interp, const char *c1, const char *c2)
{
    Jsi_DString dStr;
    Jsi_DSInit(&dStr);
    Jsi_DSAppend(&dStr, c1, c2, NULL);
    Jsi_Value *val = Jsi_ValueNewStringKey(interp, Jsi_DSValue(&dStr));
    Jsi_DSFree(&dStr);
    return val;
}

static Jsi_RC InfoFuncDataSub(Jsi_Interp *interp, Jsi_Value *arg, Jsi_Value *_this,
    Jsi_Value **ret, Jsi_Func *funcPtr, int flags, Jsi_Obj *nobj)
{
    const char *key, *name = NULL, *ename;
    int n, curlen = 0, isreg = 0, isobj = 0, isglob = 0, nLen = 0;
    Jsi_HashEntry *hPtr;
    Jsi_HashSearch search;
    Jsi_FuncObj *fo;
    Jsi_Func *func;
    Jsi_Obj *fobj;
    Jsi_Value *val, *nval;
    int isfunc = (flags&1);
    int isdata = (flags&2);
    int addPre = (flags&4);
    Jsi_DString dPre = {};
    
    if (!nobj) {
        nobj = Jsi_ObjNew(interp);
        Jsi_ValueMakeArrayObject(interp, ret, nobj);
    }
    
    if (!arg)
        name = "*";
    else {
        if (arg->vt == JSI_VT_STRING)
            name = Jsi_ValueString(interp, arg, &nLen);
        else if (arg->vt == JSI_VT_OBJECT) {
            switch (arg->d.obj->ot) {
                case JSI_OT_STRING: name = Jsi_ValueString(interp, arg, &nLen); break;
                case JSI_OT_REGEXP: isreg = 1; break;
                case JSI_OT_OBJECT: isobj = 1; break;
                case JSI_OT_FUNCTION:
                    fo = arg->d.obj->d.fobj;
                    if (fo->func->type&FC_BUILDIN)
                        return JSI_OK;
                    goto dumpfunc;
                break;
                default: return JSI_OK;
            }
        } else
            return JSI_OK;
    }
    ename = name;
    if (isobj) {
        Jsi_TreeEntry* tPtr;
        Jsi_TreeSearch search;

dumpobj:
        for (tPtr = Jsi_TreeSearchFirst(arg->d.obj->tree, &search, 0, NULL);
            tPtr; tPtr = Jsi_TreeSearchNext(&search)) {
            Jsi_Value *v = (Jsi_Value*)Jsi_TreeValueGet(tPtr);
            key = (char*)Jsi_TreeKeyGet(tPtr);
            if (v==NULL) continue;
            if (Jsi_ValueIsFunction(interp, v)) {
                if (!isfunc) continue;
            } else {
                if (!isdata) continue;
            }
            if (isreg) {
                int ismat;
                Jsi_RegExpMatch(interp, arg, key, &ismat, NULL);
                if (!ismat)
                    continue;
            } else if (ename) {
                if (isglob) {
                    if (!Jsi_GlobMatch(ename, key, 0))
                        continue;
                } else {
                    if (Jsi_Strcmp(ename, key))
                        continue;
                }
            }
    
            n = curlen++;
            nval = jsiNewConcatValue(interp, (addPre?Jsi_DSValue(&dPre):""), key);
            Jsi_ObjArraySet(interp, nobj, nval, n);
        }
        Jsi_TreeSearchDone(&search);
        Jsi_DSFree(&dPre);
        return JSI_OK;
    }
    
    Jsi_ScopeStrs *sstrs;            
    const char *gs, *gb, *dotStr;
    
    if (name) {
        val = Jsi_NameLookup(interp, name);
        if (val)
            goto dumpvar;
        gs=Jsi_Strchr(name,'*');
        gb=Jsi_Strchr(name,'[');
        dotStr = Jsi_Strrchr(name, '.');
        if (dotStr && ((gs && gs < dotStr) || (gb && gb < dotStr))) 
            return Jsi_LogError("glob must be after last dot");
        isglob = (gs || gb);
        if (!isglob)
            return JSI_OK;
        if (dotStr) {
            if (addPre)
                Jsi_DSAppendLen(&dPre, name, dotStr-ename+1);
            ename = dotStr+1;
            Jsi_DString pStr = {};
            Jsi_DSAppendLen(&pStr, name, dotStr-name);
            val = Jsi_NameLookup(interp, Jsi_DSValue(&pStr));
            Jsi_DSFree(&pStr);
            if (!val)
                return JSI_OK;
            if (Jsi_ValueIsObjType(interp, val, JSI_OT_OBJECT)) {
                arg = val;
                goto dumpobj;
            }
            return JSI_OK;
        } 
    }
        
    for (hPtr = Jsi_HashSearchFirst(interp->varTbl, &search);
        hPtr != NULL; hPtr = Jsi_HashSearchNext(&search)) {
        key = (char*)Jsi_HashKeyGet(hPtr);
        if (!isfunc)
            goto doreg;
        if (!(fobj = (Jsi_Obj*)Jsi_HashValueGet(hPtr)))
            continue;
        if (isfunc && name && isreg==0 && isglob==0) {

            if (Jsi_Strcmp(key, name))
                continue;
            /* Fill object with args and locals of func. */
            fo = fobj->d.fobj; 
            goto dumpfunc;
        }
doreg:
        if (isreg) {
            int ismat;
            Jsi_RegExpMatch(interp, arg, key, &ismat, NULL);
            if (!ismat)
                continue;
        } else if (name) {
            if (isglob) {
                if (!Jsi_GlobMatch(name, key, 0))
                    continue;
            } else {
                if (Jsi_Strcmp(name, key))
                    continue;
            }
        }
        n = curlen++;
        nval = jsiNewConcatValue(interp, (addPre?Jsi_DSValue(&dPre):""), key);
        Jsi_ObjArraySet(interp, nobj, nval, n);
    }
    Jsi_DSFree(&dPre);
    return JSI_OK;

dumpfunc:
{
    nobj = Jsi_ObjNew(interp);
    Jsi_ValueMakeObject(interp, ret, nobj);
    func = fo->func;
    sstrs = func->argnames;
    int sscnt = (sstrs?sstrs->count:0);
    const char *strs[sscnt+1];
    int i;
    for (i=0; i<sscnt; i++)
        strs[i] = sstrs->args[i].name;
    Jsi_DString dStr;
    Jsi_DSInit(&dStr);
    Jsi_Value *aval = Jsi_ValueNewArray(interp, strs, sscnt);
    Jsi_ObjInsert(interp, nobj, "argList", aval, 0);
    sstrs = func->localnames;
    Jsi_Value *lval = Jsi_ValueNewArray(interp, strs, sscnt);
    Jsi_ObjInsert(interp, nobj, "locals", lval, 0);
    jsi_FuncArgsToString(interp, func, &dStr, 1);
    lval = Jsi_ValueNewStringDup(interp, Jsi_DSValue(&dStr));
    Jsi_ObjInsert(interp, nobj, "args", lval, 0);
    Jsi_DSFree(&dStr);
    if (func->retType)
        Jsi_ObjInsert(interp, nobj, "retType", Jsi_ValueNewStringKey(interp, jsi_typeName(interp, func->retType, &dStr)), 0);
    Jsi_DSFree(&dStr);
    if (func->script) {
        lval = Jsi_ValueNewStringKey(interp, func->script);
        Jsi_ObjInsert(interp, nobj, "script", lval, 0);
        int l1 = func->opcodes->codes->Line;
        int l2 = func->bodyline.last_line;
        if (l1>l2) { int lt = l1; l1 = l2; l2 = lt; }
        lval = Jsi_ValueNewNumber(interp, (Jsi_Number)l1);
        Jsi_ObjInsert(interp, nobj, "lineStart", lval, 0);
        lval = Jsi_ValueNewNumber(interp, (Jsi_Number)l2);
        Jsi_ObjInsert(interp, nobj, "lineEnd", lval, 0);
    }
    return JSI_OK;
}
dumpvar:
    if (isfunc && Jsi_ValueIsFunction(interp, val)) {
        fo = val->d.obj->d.fobj;
        goto dumpfunc;
    }
    nobj = Jsi_ObjNew(interp);
    Jsi_ValueMakeObject(interp, ret, nobj);
    Jsi_Value *aval = Jsi_ValueNewStringDup(interp, ename);
    Jsi_ObjInsert(interp, nobj, "name", aval, 0);
    aval = Jsi_ValueNewStringDup(interp, jsi_ValueTypeName(interp, val));
    Jsi_ObjInsert(interp, nobj, "type", aval, 0);
    return JSI_OK;
}

static Jsi_RC InfoLookupCmd(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this,
    Jsi_Value **ret, Jsi_Func *funcPtr)
{
    char *str = Jsi_ValueArrayIndexToStr(interp, args, 0, NULL);
    if (!str)
        return JSI_OK;
    Jsi_Value *val = Jsi_NameLookup(interp, str);
    if (!val)
        return JSI_OK;
    Jsi_ValueDup2(interp, ret, val);
    return JSI_OK;
}


#define FN_infolevel JSI_INFO("\
With no arg, returns the number of the current stack frame level.\
Otherwise returns details on the specified level. \
The topmost level is 1, and 0 is the current level, \
and a negative level translates as relative to the current level.")
static Jsi_RC InfoLevelCmd(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this,
    Jsi_Value **ret, Jsi_Func *funcPtr)
{
    uint argc =  Jsi_ValueGetLength(interp, args);
    if (argc<=0) {
        Jsi_ValueMakeNumber(interp, ret, (Jsi_Number)interp->framePtr->level);
        return JSI_OK;
    }
    Jsi_Number num = 0;
    Jsi_Value *arg = Jsi_ValueArrayIndex(interp, args, 0);
    jsi_Frame *f = interp->framePtr;
    if (Jsi_GetNumberFromValue(interp, arg, &num) != JSI_OK)
        return JSI_ERROR;
    int lev = (int)num;
    if (lev <= 0)
        lev = f->level+lev;
    if (lev <= 0 || lev > f->level) 
        return Jsi_LogError("level %d not between 1 and %d", (int)num, f->level);
    while (f->level != lev  && f->parent)
        f = f->parent;
    char buf[BUFSIZ];
    //int line = (f != interp->framePtr ? f->line : (interp->curIp ? interp->curIp->Line : 0));
    int line = (f->line ? f->line : (interp->curIp ? interp->curIp->Line : 0));
    snprintf(buf, sizeof(buf), "{funcName:\"%s\", fileName:\"%s\", line:%d, level:%d, tryDepth:%d, withDepth:%d}",
        f->funcName?f->funcName:"", f->fileName?f->fileName:"", line, f->level, f->tryDepth, f->withDepth
        );
    
    Jsi_RC rc = Jsi_JSONParse(interp, buf, ret, 0);
    if (rc != JSI_OK)
        return rc;
    Jsi_Func *who = f->evalFuncPtr;
    Jsi_Obj *obj = Jsi_ObjNewArray(interp, NULL, 0, 0);
    Jsi_Value *val = Jsi_ValueMakeArrayObject(interp, NULL, obj);
    if (who && who->localnames) {
        int i;
        for (i = 0; i < who->localnames->count; ++i) {
            const char *argkey = jsi_ScopeStrsGet(who->localnames, i);
            Jsi_Value *v = Jsi_ValueMakeStringKey(interp, NULL, argkey);
            Jsi_ObjArrayAdd(interp, obj, v);

        }
    }
    Jsi_ValueInsert(interp, *ret, "locals", val, 0);
    Jsi_ValueInsert(interp, *ret, "scope", f->incsc, 0);
    Jsi_ValueInsert(interp, *ret, "inthis", f->inthis, 0);
    return rc;
}

static Jsi_RC InfoFilesCmd(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this,
    Jsi_Value **ret, Jsi_Func *funcPtr)
{
    return Jsi_HashKeysDump(interp, interp->fileTbl, ret, 0);
}

static Jsi_RC InfoFuncsCmd(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this,
    Jsi_Value **ret, Jsi_Func *funcPtr)
{
    Jsi_Value *arg = Jsi_ValueArrayIndex(interp, args, 0);
    return InfoFuncDataSub(interp, arg, _this, ret, funcPtr, 1, NULL);
}

#define FN_infodata JSI_INFO("\
Like info.vars(), but does not return function values.")
static Jsi_RC InfoDataCmd(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this,
    Jsi_Value **ret, Jsi_Func *funcPtr)
{
    Jsi_Value *arg = Jsi_ValueArrayIndex(interp, args, 0);
    return InfoFuncDataSub(interp, arg, _this, ret, funcPtr, 2, NULL);
}

static Jsi_RC InfoKeywordsCmd(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this,
    Jsi_Value **ret, Jsi_Func *funcPtr)
{
    if (interp->lexkeyTbl) {
        Jsi_HashKeysDump(interp, interp->lexkeyTbl, ret, 0);
        Jsi_ValueArraySort(interp, *ret, 0);
    }
    return JSI_OK;
}

static Jsi_RC InfoOptionsCmd(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this,
    Jsi_Value **ret, Jsi_Func *funcPtr)
{
    uint i, argc =  Jsi_ValueGetLength(interp, args);
    Jsi_DString dStr = {};
    const char *str;
    bool bv = 0;
    if (argc && Jsi_GetBoolFromValue(interp, Jsi_ValueArrayIndex(interp, args, 0), &bv) != JSI_OK)
        return JSI_ERROR;
    Jsi_DSAppend(&dStr, "[", NULL);
    for (i=1; (str = jsi_OptionTypeStr((Jsi_OptionId)i, bv));i++)
        Jsi_DSAppend(&dStr, (i>1?", ":""), "\"", str, "\"", NULL);
    Jsi_DSAppend(&dStr, "]", NULL);
    Jsi_JSONParse(interp, Jsi_DSValue(&dStr), ret, 0);
    return JSI_OK;
}
static Jsi_RC InfoVersionCmd(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this,
    Jsi_Value **ret, Jsi_Func *funcPtr)
{
    Jsi_Value *arg = Jsi_ValueArrayIndex(interp, args, 0);
    char buf[BUFSIZ];
    const char *cp;
    if (!arg)
        Jsi_ValueMakeNumber(interp, ret, Jsi_Version());
    else if ((cp=Jsi_ValueString(interp, arg, NULL))) {
        snprintf(buf, sizeof(buf), "%d%s%d%s%d",
            JSI_VERSION_MAJOR, cp, JSI_VERSION_MINOR, cp, JSI_VERSION_RELEASE);
        Jsi_ValueMakeStringDup(interp, ret, buf);
    } else if (!Jsi_ValueIsTrue(interp, arg))
        return Jsi_LogError("expected string or bool");
    else {
        snprintf(buf, sizeof(buf),
            "{major:%d, minor:%d, release:%d}",
            JSI_VERSION_MAJOR, JSI_VERSION_MINOR, JSI_VERSION_RELEASE);
        
        return Jsi_JSONParse(interp, buf, ret, 0);
    }
    return JSI_OK;
}


static Jsi_RC InfoIsMainCmd (Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this,
    Jsi_Value **ret, Jsi_Func *funcPtr)
{
    int isi = (interp->isMain);
    if (isi == 0) {
        const char *c2 = interp->curFile;
        Jsi_Value *v1 = interp->argv0;
        if (c2 && v1 && Jsi_ValueIsString(interp, v1)) {
            char *c1 = Jsi_ValueString(interp, v1, NULL);
            isi = (c1 && !Jsi_Strcmp(c1,c2));
        }
    }
    //if (interp->noIsMain)
        //isi = 0;
    Jsi_ValueMakeBool(interp, ret, isi);
    return JSI_OK;
}

static Jsi_RC InfoArgv0Cmd(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this,
    Jsi_Value **ret, Jsi_Func *funcPtr)
{
    if (interp->argv0)
        Jsi_ValueDup2(interp, ret, interp->argv0);
     return JSI_OK;
}

static Jsi_RC InfoExecZipCmd(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this,
    Jsi_Value **ret, Jsi_Func *funcPtr)
{
    if (interp->execZip)
        Jsi_ValueDup2(interp, ret, interp->execZip);
     return JSI_OK;
}

#ifndef JSI_OMIT_DEBUG
static Jsi_RC DebugAddCmd(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this,
    Jsi_Value **ret, Jsi_Func *funcPtr)
{
    if (!interp->breakpointHash)
        interp->breakpointHash = Jsi_HashNew(interp, JSI_KEYS_STRING, jsi_HashFree);
    int argc = Jsi_ValueGetLength(interp, args);
    jsi_BreakPoint *bptr, bp = {};
    Jsi_Number vnum;
    if (argc>1 && Jsi_ValueGetBoolean(interp, Jsi_ValueArrayIndex(interp, args, 1), &bp.temp) != JSI_OK) 
        return Jsi_LogError("bad boolean");
    Jsi_Value *v = Jsi_ValueArrayIndex(interp, args, 0);
    if (Jsi_ValueGetNumber(interp, v, &vnum) == JSI_OK) {
        bp.line = (int)vnum;
        bp.file = interp->curFile;
    } else {
        const char *val = Jsi_ValueArrayIndexToStr(interp, args, 0, NULL);
        const char *cp;
        
        if (isdigit(val[0])) {
            if (Jsi_GetInt(interp, val, &bp.line, 0) != JSI_OK) 
                return Jsi_LogError("bad number");
            bp.file = interp->curFile;
        } else if ((cp = Jsi_Strchr(val, ':'))) {
            if (Jsi_GetInt(interp, cp+1, &bp.line, 0) != JSI_OK) 
                return Jsi_LogError("bad number");
            Jsi_DString dStr = {};
            Jsi_DSAppendLen(&dStr, val, cp-val);
            bp.file = Jsi_KeyAdd(interp, Jsi_DSValue(&dStr));
            Jsi_DSFree(&dStr);
        } else {
            bp.func = Jsi_KeyAdd(interp, val);
        }
    }
    if (bp.line<=0 && !bp.func) 
        return Jsi_LogError("bad number");
    char nbuf[100];
    bp.id = ++interp->debugOpts.breakIdx;
    bp.enabled = 1;
    snprintf(nbuf, sizeof(nbuf), "%d", bp.id);
    bptr = (jsi_BreakPoint*)Jsi_Malloc(sizeof(*bptr));
    *bptr = bp;
    Jsi_HashSet(interp->breakpointHash, (void*)nbuf, bptr);
    Jsi_ValueMakeNumber(interp, ret, (Jsi_Number)bp.id);
    return JSI_OK;
}

static Jsi_RC DebugRemoveCmd_(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this,
    Jsi_Value **ret, Jsi_Func *funcPtr, int op)
{
    Jsi_Value *val = Jsi_ValueArrayIndex(interp, args, 0);
    if (interp->breakpointHash)
    {
        int num;
        char nbuf[100];
        if (Jsi_GetIntFromValue(interp, val, &num) != JSI_OK) 
            return Jsi_LogError("bad number");
        
        snprintf(nbuf, sizeof(nbuf), "%d", num);
        Jsi_HashEntry *hPtr = Jsi_HashEntryFind(interp->breakpointHash, nbuf);
        jsi_BreakPoint* bptr;
        if (hPtr && (bptr = (jsi_BreakPoint*)Jsi_HashValueGet(hPtr))) {
            switch (op) {
                case 1: bptr->enabled = 0; break;
                case 2: bptr->enabled = 1; break;
                default:
                    Jsi_HashEntryDelete(hPtr);
            }
            return JSI_OK;
        }
    }
    return Jsi_LogError("unknown breakpoint");
}

static Jsi_RC DebugRemoveCmd(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this,
    Jsi_Value **ret, Jsi_Func *funcPtr)
{
    return DebugRemoveCmd_(interp, args, _this, ret, funcPtr, 0);
}

static Jsi_RC DebugEnableCmd(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this,
    Jsi_Value **ret, Jsi_Func *funcPtr)
{
    Jsi_Value *val = Jsi_ValueArrayIndex(interp, args, 1);
    bool bval;
    if (Jsi_ValueGetBoolean(interp, val, &bval) != JSI_OK)
        return JSI_ERROR;
    return DebugRemoveCmd_(interp, args, _this, ret, funcPtr, bval ? 2 : 1);
}

static Jsi_RC DebugInfoCmd(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this,
    Jsi_Value **ret, Jsi_Func *funcPtr)
{
    if (!interp->breakpointHash) {
        Jsi_ValueMakeArrayObject(interp, ret, NULL);
        return JSI_OK;
    }
    int argc = Jsi_ValueGetLength(interp, args);
    if (argc == 0)
        return Jsi_HashKeysDump(interp, interp->breakpointHash, ret, 0);
    Jsi_Value *val = Jsi_ValueArrayIndex(interp, args, 0);
    int num;
    char nbuf[100];
    if (Jsi_GetIntFromValue(interp, val, &num) != JSI_OK) 
        return Jsi_LogError("bad number");
    
    snprintf(nbuf, sizeof(nbuf), "%d", num);
    Jsi_HashEntry *hPtr = Jsi_HashEntryFind(interp->breakpointHash, nbuf);
    if (!hPtr) 
        return Jsi_LogError("unknown breakpoint");
    jsi_BreakPoint* bp = (jsi_BreakPoint*)Jsi_HashValueGet(hPtr);
    if (!bp) return JSI_ERROR;
    Jsi_DString dStr = {};
    if (bp->func)
        Jsi_DSPrintf(&dStr, "{id:%d, type:\"func\", func:\"%s\", hits:%d, enabled:%s, temporary:%s}",
         bp->id, bp->func, bp->hits, bp->enabled?"true":"false", bp->temp?"true":"false");
    else
        Jsi_DSPrintf(&dStr, "{id:%d, type:\"line\", file:\"%s\", line:%d, hits:%d, enabled:%s}",
            bp->id, bp->file?bp->file:"", bp->line, bp->hits, bp->enabled?"true":"false");
    Jsi_RC rc = Jsi_JSONParse(interp, Jsi_DSValue(&dStr), ret, 0);
    Jsi_DSFree(&dStr);
    return rc;
}
#endif

static Jsi_RC InfoScriptCmd(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this,
    Jsi_Value **ret, Jsi_Func *funcPtr)
{
    Jsi_Value *arg = Jsi_ValueArrayIndex(interp, args, 0);
    int isreg = 0;
    const char *name = NULL;

    if (!arg) {
        name = jsi_GetCurFile(interp);
    } else {
        if (arg->vt == JSI_VT_OBJECT) {
            switch (arg->d.obj->ot) {
                case JSI_OT_FUNCTION: name = arg->d.obj->d.fobj->func->script; break;
                case JSI_OT_REGEXP: isreg = 1; break;
                default: break;
            }
        } else
            return JSI_OK;
    }
    if (isreg) {
        Jsi_HashEntry *hPtr;
        Jsi_HashSearch search;
        int curlen = 0, n;
        const char *key;
        Jsi_Obj *nobj = Jsi_ObjNew(interp);
        Jsi_ValueMakeArrayObject(interp, ret, nobj);
        for (hPtr = Jsi_HashSearchFirst(interp->fileTbl, &search);
            hPtr != NULL; hPtr = Jsi_HashSearchNext(&search)) {
            key = (const char*)Jsi_HashKeyGet(hPtr);
            if (isreg) {
                int ismat;
                Jsi_RegExpMatch(interp, arg, key, &ismat, NULL);
                if (!ismat)
                    continue;
            }
            n = curlen++;
            Jsi_ObjArraySet(interp, nobj, Jsi_ValueNewStringKey(interp, key), n);
        }
        return JSI_OK;
    }
    if (name)
        Jsi_ValueMakeStringDup(interp, ret, name);
    return JSI_OK;
}

static Jsi_RC InfoScriptDirCmd(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this,
    Jsi_Value **ret, Jsi_Func *funcPtr)
{
    const char *path = interp->curDir;
    if (path)
        Jsi_ValueMakeString(interp, ret, Jsi_Strdup(path));
    return JSI_OK;
}

static int isBigEndian()
{
    union { unsigned short s; unsigned char c[2]; } uval = {0x0102};
    return uval.c[0] == 1;
}

static Jsi_RC InfoPlatformCmd(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this,
    Jsi_Value **ret, Jsi_Func *funcPtr)
{
    char buf[BUFSIZ];
    size_t crcSizes[] = { sizeof(int), sizeof(void*), sizeof(time_t), sizeof(Jsi_Number), (size_t)isBigEndian() };
#ifdef __WIN32
    const char *os="win", *platform = "win";
#elif defined(__FreeBSD__)
    const char *os="freebsd", *platform = "unix";
#else
    const char *os="linux", *platform = "unix";
#endif
#ifndef JSI_OMIT_THREADS
    int thrd = 1;
#else
    int thrd = 0;
#endif
    if (Jsi_FunctionReturnIgnored(interp, funcPtr))
        return JSI_OK;
    snprintf(buf, sizeof(buf),
        "{os:\"%s\", platform:\"%s\", hasThreads:%s, pointerSize:%zu, timeSize:%zu "
        "intSize:%zu, wideSize:%zu, numberSize:%zu, crc:%x, isBigEndian:%s, confArgs:\"%s\"}",
        os, platform, thrd?"true":"false", sizeof(void*), sizeof(time_t), sizeof(int),
        sizeof(Jsi_Wide), sizeof(Jsi_Number), Jsi_Crc32(0, (uchar*)crcSizes, sizeof(crcSizes)),
        isBigEndian()? "true" : "false", interp->confArgs);
        
    return Jsi_JSONParse(interp, buf, ret, 0);
}

static Jsi_RC InfoNamedCmd(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this,
    Jsi_Value **ret, Jsi_Func *funcPtr)
{
    Jsi_Obj *nobj;
    char *argStr = NULL;
    Jsi_Value *arg = Jsi_ValueArrayIndex(interp, args, 0);
    if (arg  && (argStr = (char*)Jsi_ValueString(interp, arg, NULL)) == NULL) 
        return Jsi_LogError("expected string");
    if (argStr && argStr[0] == '#') {
        Jsi_Obj *obj = jsi_UserObjFromName(interp, argStr);
        if (!obj) 
            return Jsi_LogError("Unknown object: %s", argStr);
        Jsi_ValueMakeObject(interp, ret, obj);
        return JSI_OK;
    }
    nobj = Jsi_ObjNew(interp);
    Jsi_ValueMakeArrayObject(interp, ret, nobj);
    return jsi_UserObjDump(interp, argStr, nobj);
}


const char* Jsi_OptionsData(Jsi_Interp *interp, Jsi_OptionSpec *specs, Jsi_DString *dStr, bool schema) {
    const Jsi_OptionSpec *send;
    for (send = specs; send->id>=JSI_OPTION_BOOL && send->id<JSI_OPTION_END; send++) ;
    if (!send)
        return NULL;
    if (schema) {
        char tbuf[100];
        int i = 0;
        if (send->name)
            Jsi_DSAppend(dStr, "  -- '", send->name, "\': ", NULL);
        if (send->help)
            Jsi_DSAppend(dStr,  send->help, NULL);
        Jsi_DSAppend(dStr, "\n", NULL);
        for (; specs->id>=JSI_OPTION_BOOL && specs->id<JSI_OPTION_END; specs++) {
            const char *tstr = NULL;
            if (specs->flags&JSI_OPT_DB_DIRTY)
                continue;
            if (!Jsi_Strcmp(specs->name, "rowid"))
                continue;
            switch(specs->id) {
            case JSI_OPTION_BOOL: tstr = "BOOLEAN"; break;
            case JSI_OPTION_TIME_T: tstr = "TIMESTAMP"; break;
            case JSI_OPTION_TIME_D:
            case JSI_OPTION_TIME_W:
                if (specs->flags & JSI_OPT_TIME_TIMEONLY)
                    tstr = "TIME";
                else if (specs->flags & JSI_OPT_TIME_TIMEONLY)
                    tstr = "DATE";
                else
                    tstr = "DATETIME";
                break;
            case JSI_OPTION_SIZE_T:
            case JSI_OPTION_SSIZE_T:
            case JSI_OPTION_INTPTR_T:
            case JSI_OPTION_UINTPTR_T:
            case JSI_OPTION_UINT:
            case JSI_OPTION_INT:
            case JSI_OPTION_ULONG:
            case JSI_OPTION_LONG:
            case JSI_OPTION_USHORT:
            case JSI_OPTION_SHORT:
            case JSI_OPTION_UINT8:
            case JSI_OPTION_UINT16:
            case JSI_OPTION_UINT32:
            case JSI_OPTION_UINT64:
            case JSI_OPTION_INT8:
            case JSI_OPTION_INT16:
            case JSI_OPTION_INT32:
            case JSI_OPTION_INT64: tstr = "INT"; break;
            case JSI_OPTION_FLOAT:
            case JSI_OPTION_NUMBER:
            case JSI_OPTION_LDOUBLE:
            case JSI_OPTION_DOUBLE: tstr = "FLOAT"; break;
            
            case JSI_OPTION_STRBUF:
                tstr = tbuf;
                snprintf(tbuf, sizeof(tbuf), "VARCHAR(%d)", specs->size);;
                break;
            case JSI_OPTION_DSTRING: 
            case JSI_OPTION_STRKEY:
            case JSI_OPTION_STRING:
                tstr = "TEXT";
                break;
            case JSI_OPTION_CUSTOM:
                if (specs->custom == Jsi_Opt_SwitchEnum || specs->custom == Jsi_Opt_SwitchBitset) {
                    if (specs->flags&JSI_OPT_FORCE_INT)
                        tstr = "INT";
                    else
                        tstr = "TEXT";
                } else
                    tstr = "";
                break;
            case JSI_OPTION_VALUE: /* Unsupported. */
            case JSI_OPTION_VAR:
            case JSI_OPTION_OBJ:
            case JSI_OPTION_ARRAY:
            case JSI_OPTION_FUNC:
            case JSI_OPTION_USEROBJ:
            case JSI_OPTION_REGEXP:
                break;
#ifdef __cplusplus
            case JSI_OPTION_END:
#else
            default:
#endif
                Jsi_LogBug("invalid option type: %d", specs->id);
            }
            
            if (!tstr) continue;
            Jsi_DSAppend(dStr, (i?"\n ,":"  "), specs->name, " ", (tstr[0]?tstr:specs->tname), NULL);

            const char *cp;
            if ((cp=specs->userData)) {
                const char *udrest = NULL;
                if (!Jsi_Strcmp(cp, "DEFAULT")) {
                    if ((specs->id == JSI_OPTION_TIME_D || specs->id <= JSI_OPTION_TIME_T)) {
                        if (specs->flags & JSI_OPT_TIME_DATEONLY)
                            udrest = "(round((julianday('now') - 2440587.5)*86400000))";
                        else if (specs->flags & JSI_OPT_TIME_TIMEONLY)
                            udrest = "(round((julianday('now')-julianday('now','start of day') - 2440587.5)*86400000))";
                        else
                            udrest = "(round((julianday('now') - 2440587.5)*86400000.0))";
                    } else if (specs->id <= JSI_OPTION_TIME_T) {
                        if (specs->flags & JSI_OPT_TIME_DATEONLY)
                            udrest = "(round((julianday('now') - 2440587.5)*86400))";
                        else if (specs->flags & JSI_OPT_TIME_TIMEONLY)
                            udrest = "(round((julianday('now')-julianday('now','start of day') - 2440587.5)*86400))";
                        else
                            udrest = "(round((julianday('now') - 2440587.5)*86400.0))";
                    }
                }
                Jsi_DSAppend(dStr, " ", cp, udrest, NULL);
            }
            if (specs->help)
                Jsi_DSAppend(dStr, " -- ", specs->help, NULL);
            if (specs->id==JSI_OPTION_END)
                break;
            i++;
        }
        if (send->userData)
            Jsi_DSAppend(dStr,  send->userData, NULL);
        Jsi_DSAppend(dStr, "\n  -- MD5=", NULL);

    } else {
        int i = 0;
        Jsi_DSAppend(dStr, "\n/* ", send->name, ": ", NULL);
        if (send->help)
            Jsi_DSAppend(dStr,  send->help, NULL);
        Jsi_DSAppend(dStr, " */\ntypedef struct ",  send->name, " {", NULL);
        for (; specs->id>=JSI_OPTION_BOOL && specs->id<JSI_OPTION_END; specs++) {
            int nsz = 0;
            const char *tstr = NULL;
            switch(specs->id) {
            case JSI_OPTION_STRBUF: nsz=1;
            case JSI_OPTION_BOOL:
            case JSI_OPTION_TIME_T:
            case JSI_OPTION_TIME_W:
            case JSI_OPTION_TIME_D:;
            case JSI_OPTION_SIZE_T:
            case JSI_OPTION_SSIZE_T:
            case JSI_OPTION_INTPTR_T:
            case JSI_OPTION_UINTPTR_T:
            case JSI_OPTION_INT:
            case JSI_OPTION_UINT:
            case JSI_OPTION_LONG:
            case JSI_OPTION_ULONG:
            case JSI_OPTION_SHORT:
            case JSI_OPTION_USHORT:
            case JSI_OPTION_UINT8:
            case JSI_OPTION_UINT16:
            case JSI_OPTION_UINT32:
            case JSI_OPTION_UINT64:
            case JSI_OPTION_INT8:
            case JSI_OPTION_INT16:
            case JSI_OPTION_INT32:
            case JSI_OPTION_INT64:
            case JSI_OPTION_FLOAT:
            case JSI_OPTION_DOUBLE:
            case JSI_OPTION_LDOUBLE:
            case JSI_OPTION_NUMBER:
            case JSI_OPTION_DSTRING:
            case JSI_OPTION_STRKEY: 
            case JSI_OPTION_STRING:
                tstr = jsi_OptionTypeStr(specs->id, 1); 
                break;
            case JSI_OPTION_CUSTOM:
                if (specs->custom == Jsi_Opt_SwitchEnum || specs->custom == Jsi_Opt_SwitchBitset) {
                    tstr = "int";
                } else {
                    tstr = "char";
                    nsz = 1;
                }
                break;
            case JSI_OPTION_VALUE: /* Unsupported. */
            case JSI_OPTION_VAR:
            case JSI_OPTION_OBJ:
            case JSI_OPTION_ARRAY:
            case JSI_OPTION_REGEXP:
            case JSI_OPTION_FUNC:
            case JSI_OPTION_USEROBJ:
                break;
#ifdef __cplusplus
            case JSI_OPTION_END:
#else
            default:
#endif
                Jsi_LogBug("invalid option type: %d", specs->id);
            }
            
            if (!tstr) continue;
            Jsi_DSAppend(dStr, "\n    ", tstr, " ", specs->name, NULL);
            if (nsz) {
                Jsi_DSPrintf(dStr, "[%d]", specs->size);
            }
            Jsi_DSAppend(dStr, ";", NULL);
            if (specs->help)
                Jsi_DSAppend(dStr, " /* ", specs->help, " */", NULL);
            if (specs->id==JSI_OPTION_END)
                break;
            i++;
        }
        Jsi_DSAppend(dStr, "\n};\n// MD5=", NULL);

    }
    char buf[33];
    Jsi_CryptoHash(buf, Jsi_DSValue(dStr), Jsi_DSLength(dStr), Jsi_CHash_MD5, 0, 0);
    //Jsi_Md5Str(buf, Jsi_DSValue(dStr), -1);
    Jsi_DSAppend(dStr, buf, NULL);

    return Jsi_DSValue(dStr);
}

static Jsi_RC InfoExecutableCmd(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this,
    Jsi_Value **ret, Jsi_Func *funcPtr)
{
    if (jsi_execName == NULL)
        Jsi_ValueMakeNull(interp, ret);
    else
        Jsi_ValueMakeStringKey(interp, ret, jsi_execName);
    return JSI_OK;
}

#define FN_infoevent JSI_INFO("\
With no args, returns list of all outstanding events.  With one arg, returns info\
for the given event id.")

#ifndef JSI_OMIT_EVENT
static Jsi_RC InfoEventCmd(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this,
    Jsi_Value **ret, Jsi_Func *funcPtr)
{
    uint n = 0;
    int nid;
    Jsi_HashEntry *hPtr;
    Jsi_HashSearch search;
    Jsi_Obj *nobj;
    Jsi_Value *arg = Jsi_ValueArrayIndex(interp, args, 0);
    
    if (arg && Jsi_GetIntFromValue(interp, arg, &nid) != JSI_OK)
        return JSI_ERROR;
    if (!arg) {
        nobj = Jsi_ObjNew(interp);
        Jsi_ValueMakeArrayObject(interp, ret, nobj);
    }

    for (hPtr = Jsi_HashSearchFirst(interp->eventTbl, &search);
        hPtr != NULL; hPtr = Jsi_HashSearchNext(&search)) {
        uintptr_t id;
        id = (uintptr_t)Jsi_HashKeyGet(hPtr);
        if (!arg) {
            Jsi_ObjArraySet(interp, nobj, Jsi_ValueNewNumber(interp, (Jsi_Number)id), n);
            n++;
        } else if (id == (uint)nid) {
            Jsi_Event *evPtr = (Jsi_Event *)Jsi_HashValueGet(hPtr);
            Jsi_DString dStr;
            if (!evPtr) return JSI_ERROR;
            Jsi_DSInit(&dStr);
            switch (evPtr->evType) {
                case JSI_EVENT_SIGNAL:
                    Jsi_DSPrintf(&dStr, "{ type:\"signal\", sigNum:%d, count:%u, builtin:%s }", 
                        evPtr->sigNum, evPtr->count, (evPtr->handler?"true":"false") );
                    break;
                case JSI_EVENT_TIMER: {
             
                    long cur_sec, cur_ms;
                    uint64_t ms;
                    jsiGetTime(&cur_sec, &cur_ms);
                    ms = (evPtr->when_sec*1000LL + evPtr->when_ms) - (cur_sec * 1000LL + cur_ms);
                    Jsi_DSPrintf(&dStr, "{ type:\"timer\", once:%s, when:%" PRId64 ", count:%u, initial:%" PRId64 ", builtin:%s }",
                        evPtr->once?"true":"false", (Jsi_Wide)ms, evPtr->count, (Jsi_Wide)evPtr->initialms, (evPtr->handler?"true":"false") );
                    break;
                }
                case JSI_EVENT_ALWAYS:
                    Jsi_DSPrintf(&dStr, "{ type:\"always\", count:%u, builtin:%s }", 
                        evPtr->count, (evPtr->handler?"true":"false") );
                    break;
            }
            Jsi_RC rc = Jsi_JSONParse(interp, Jsi_DSValue(&dStr), ret, 0);
            Jsi_DSFree(&dStr);
            return rc;
        }
    }
    return JSI_OK;
}

static Jsi_RC eventInfoCmd(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this,
    Jsi_Value **ret, Jsi_Func *funcPtr)
{
    return InfoEventCmd(interp, args, _this, ret, funcPtr);
}
#endif

static Jsi_RC InfoErrorCmd(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this,
    Jsi_Value **ret, Jsi_Func *funcPtr)
{
    Jsi_Obj *nobj = Jsi_ObjNew(interp);
    Jsi_ValueMakeObject(interp, ret, nobj);
    Jsi_ValueInsert( interp, *ret, "file", Jsi_ValueNewStringKey(interp, interp->errFile?interp->errFile:""), 0);
    Jsi_ValueInsert(interp, *ret, "line", Jsi_ValueNewNumber(interp, interp->errLine), 0);
    return JSI_OK;
}

void jsi_DumpCmdSpec(Jsi_Interp *interp, Jsi_Obj *nobj, Jsi_CmdSpec* spec, const char *name)
{
    Jsi_ObjInsert(interp, nobj, "minArgs", Jsi_ValueNewNumber(interp, spec->minArgs),0);
    Jsi_ObjInsert(interp, nobj, "maxArgs", Jsi_ValueNewNumber(interp, spec->maxArgs),0);
    if (spec->help)
        Jsi_ObjInsert(interp, nobj, "help", Jsi_ValueNewStringKey(interp, spec->help),0);
    if (spec->info)
        Jsi_ObjInsert(interp, nobj, "info", Jsi_ValueNewStringKey(interp, spec->info),0);
    Jsi_ObjInsert(interp, nobj, "args", Jsi_ValueNewStringKey(interp, spec->argStr?spec->argStr:""),0);
    Jsi_DString dStr;
    Jsi_DSInit(&dStr);
    Jsi_ObjInsert(interp, nobj, "retType", Jsi_ValueNewStringKey(interp, jsi_typeName(interp, spec->retType, &dStr)), 0);
    Jsi_DSFree(&dStr);
    Jsi_ObjInsert(interp, nobj, "name", Jsi_ValueNewStringKey(interp, name?name:spec->name),0);
    Jsi_ObjInsert(interp, nobj, "type", Jsi_ValueNewStringKey(interp, "command"),0);
    Jsi_ObjInsert(interp, nobj, "flags", Jsi_ValueNewNumber(interp, spec->flags),0);
    if (spec->opts) {
        Jsi_Obj *sobj = Jsi_ObjNewType(interp, JSI_OT_ARRAY);
        Jsi_Value *svalue = Jsi_ValueMakeObject(interp, NULL, sobj);
        jsi_DumpOptionSpecs(interp, sobj, spec->opts);
        Jsi_ObjInsert(interp, nobj, "options", svalue, 0);
    }
}
void jsi_DumpCmdItem(Jsi_Interp *interp, Jsi_Obj *nobj, Jsi_CmdSpecItem* csi, const char *name)
{
    Jsi_ObjInsert(interp, nobj, "name", Jsi_ValueNewStringKey(interp, name), 0);
    Jsi_ObjInsert(interp, nobj, "type", Jsi_ValueNewStringKey(interp, "object"),0);
    Jsi_ObjInsert(interp, nobj, "constructor", Jsi_ValueNewBoolean(interp, csi && csi->isCons), 0);
    if (csi && csi->help)
        Jsi_ObjInsert(interp, nobj, "help", Jsi_ValueNewStringDup(interp, csi->help),0);
    if (csi && csi->info)
        Jsi_ObjInsert(interp, nobj, "info", Jsi_ValueNewStringDup(interp, csi->info),0);
    if (csi && csi->isCons && csi->spec->argStr)
        Jsi_ObjInsert(interp, nobj, "args", Jsi_ValueNewStringDup(interp, csi->spec->argStr),0);
    Jsi_DString dStr;
    Jsi_DSInit(&dStr);
    Jsi_ObjInsert(interp, nobj, "retType", Jsi_ValueNewStringKey(interp, jsi_typeName(interp, csi->spec->retType, &dStr)), 0);
    Jsi_DSFree(&dStr);
    if (csi && csi->isCons && csi->spec->opts) {
        Jsi_Obj *sobj = Jsi_ObjNewType(interp, JSI_OT_ARRAY);
        Jsi_Value *svalue = Jsi_ValueMakeObject(interp, NULL, sobj);
        jsi_DumpOptionSpecs(interp, sobj, csi->spec->opts);
        Jsi_ObjInsert(interp, nobj, "options", svalue, 0);
    }
}


typedef struct {
    bool full;
    bool constructor;
} InfoCmdsData;

static Jsi_OptionSpec InfoCmdsOptions[] = {
    JSI_OPT(BOOL,   InfoCmdsData, full,  .help="Return full path" ),
    JSI_OPT(BOOL,   InfoCmdsData, constructor, .help="Do not exclude constructor" ),
    JSI_OPT_END(InfoCmdsData, .help="Options for Info.cmds")
};

static Jsi_RC InfoCmdsCmdSub(Jsi_Interp *interp, Jsi_Value *arg, Jsi_Value *_this,
    Jsi_Value **ret, Jsi_Func *funcPtr, int all, int sort, Jsi_Obj *nobj, int add)
{
    const char *key, *name = NULL, *cp;
    int tail = (add&2);
    int curlen = 0, icnt = 0, isglob = 0, dots = 0, gotFirst = 0, ninc;
    Jsi_MapEntry *hPtr = NULL;
    Jsi_MapSearch search;
    Jsi_CmdSpecItem *csi = NULL;
    Jsi_DString dStr;
    const char *skey;
    Jsi_CmdSpec *spec = NULL;
    
    Jsi_DSInit(&dStr);
    int isreg = 0;

    if (!arg)
        name = "*";
    else {
        if (arg->vt == JSI_VT_STRING)
            name = arg->d.s.str;
        else if (arg->vt == JSI_VT_OBJECT) {
            switch (arg->d.obj->ot) {
                case JSI_OT_STRING: name = arg->d.obj->d.s.str; break;
                case JSI_OT_REGEXP: isreg = 1; break;
                case JSI_OT_FUNCTION:
                    spec = arg->d.obj->d.fobj->func->cmdSpec;
                    if (!spec)
                        return JSI_OK;
                    skey = spec->name;
                    goto dumpfunc;
                    break;
                default: return JSI_OK;
            }
        } else
            return JSI_OK;
    }
    ninc = 0;
    isglob = (isreg == 0 && name && (Jsi_Strchr(name,'*') || Jsi_Strchr(name,'[')));
    if (isglob) {
        cp = name;
        while (*cp) { if (*cp == '.') dots++; cp++; }
    }
    while (1) {
        if (++icnt == 1) {
           /* if (0 && name)
                hPtr = Jsi_MapEntryFind(interp->cmdSpecTbl, "");
            else */ {
                hPtr = Jsi_MapSearchFirst(interp->cmdSpecTbl, &search, 0);
                gotFirst = 1;
            }
        } else if (icnt == 2 && name && !gotFirst)
            hPtr = Jsi_MapSearchFirst(interp->cmdSpecTbl, &search, 0);
        else
            hPtr = Jsi_MapSearchNext(&search);
        if (!hPtr)
            break;
        csi = (Jsi_CmdSpecItem*)Jsi_MapValueGet(hPtr);
        key = (const char *)Jsi_MapKeyGet(hPtr, 0);
        if (isglob && dots == 0 && *key && Jsi_GlobMatch(name, key, 0) && !Jsi_Strchr(key, '.')) {
                Jsi_ObjArraySet(interp, nobj, Jsi_ValueNewStringKey(interp, key), curlen++);
                ninc++;
        }
        if (isglob == 0 && name && key[0] && Jsi_Strcmp(name, key)==0) {
            skey = (char*)name;
            spec = NULL;
            goto dumpfunc;
        }
        assert(csi);
        do {
            int i;
            for (i=0; csi->spec[i].name; i++) {
                Jsi_DSSetLength(&dStr, 0);
                spec = csi->spec+i;
                if (i==0 && spec->flags&JSI_CMD_IS_CONSTRUCTOR && !all) /* ignore constructor name. */
                    continue;
                if (key[0])
                    Jsi_DSAppend(&dStr, key, ".", NULL);
                Jsi_DSAppend(&dStr, spec->name, NULL);
                skey = Jsi_DSValue(&dStr);
                if (isglob) {
                    if (!(((key[0]==0 && dots == 0) || (key[0] && dots == 1)) &&
                        Jsi_GlobMatch(name, skey, 0)))
                    continue;
                }
                if (name && isglob == 0 && Jsi_Strcmp(name,skey) == 0) {
dumpfunc:
                    nobj = Jsi_ObjNew(interp);
                    Jsi_ValueMakeObject(interp, ret, nobj);
                    if (spec == NULL)
                        jsi_DumpCmdItem(interp, nobj, csi, skey);
                    else
                        jsi_DumpCmdSpec(interp, nobj, spec, skey);
                    Jsi_DSFree(&dStr);
                    return JSI_OK;
                    
                } else if (isglob == 0 && isreg == 0)
                    continue;
                if (isreg) {
                    int ismat;
                    Jsi_RegExpMatch(interp, arg, skey, &ismat, NULL);
                    if (!ismat)
                        continue;
                }
                Jsi_ObjArraySet(interp, nobj, Jsi_ValueNewStringKey(interp, tail?spec->name:skey), curlen++);
                ninc++;
            }
            csi = csi->next;
        } while (csi);
    }
    Jsi_DSFree(&dStr);
    if (sort && (*ret)->vt != JSI_VT_UNDEF)
        Jsi_ValueArraySort(interp, *ret, 0);
    return JSI_OK;    
}

// TODO: does not show loaded commands.
static Jsi_RC InfoCmdsCmd(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this,
    Jsi_Value **ret, Jsi_Func *funcPtr)
{
    InfoCmdsData data = {};
    Jsi_Value *vo = Jsi_ValueArrayIndex(interp, args, 1);
    if (vo) {
        if (!Jsi_ValueIsObjType(interp, vo, JSI_OT_OBJECT)) /* Future options. */
            return Jsi_LogError("expected options object");
        if (Jsi_OptionsProcess(interp, InfoCmdsOptions, &data, vo, 0) < 0) {
            return JSI_ERROR;
        }
    }

    Jsi_Value *arg = Jsi_ValueArrayIndex(interp, args, 0);
    Jsi_Obj *nobj = Jsi_ObjNew(interp);
    Jsi_ValueMakeArrayObject(interp, ret, nobj);
    int ff = (data.full ? 0 : 2);
    return InfoCmdsCmdSub(interp, arg, _this, ret, funcPtr, data.constructor, 1, nobj, ff);
}


static Jsi_RC InfoMethodsCmd(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this,
    Jsi_Value **ret, Jsi_Func *funcPtr)
{
    Jsi_Value *arg = Jsi_ValueArrayIndex(interp, args, 0);
    Jsi_RC rc = InfoFuncDataSub(interp, arg, _this, ret, funcPtr, 1, NULL);
    if (rc == JSI_OK && Jsi_ValueIsArray(interp, *ret)) {
        Jsi_Obj *nobj = (*ret)->d.obj;
        InfoCmdsCmdSub(interp, arg, _this, ret, funcPtr, 0, 1, nobj, 3);
    }
    return rc;
}

static Jsi_RC InfoCompletionsCmd(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this,
    Jsi_Value **ret, Jsi_Func *funcPtr)
{
    int slen, argc = Jsi_ValueGetLength(interp, args);
    Jsi_Value *arg1 = Jsi_ValueArrayIndex(interp, args, 0);
    char *key, *substr, *str = Jsi_ValueString(interp, arg1, &slen);
    int start = 0, end = slen-1;
    if (!str)
        return JSI_ERROR;
    if (argc>1) {
        Jsi_Number n1;
        if (Jsi_GetNumberFromValue(interp, Jsi_ValueArrayIndex(interp, args, 1), &n1) != JSI_OK)
            return JSI_ERROR;
        start = (int)n1;
        if (start<0) start = 0;
        if (start>=slen) start=slen-1;
    }
    if (argc>2) {
        Jsi_Number n2;
        if (Jsi_GetNumberFromValue(interp, Jsi_ValueArrayIndex(interp, args, 2), &n2) != JSI_OK)
            return JSI_ERROR;
        end = (int)n2;
        if (end<0) end = slen-1;
        if (end>=slen) end=slen-1;
    }

    Jsi_DString dStr = {};
    Jsi_DSAppendLen(&dStr, str?str+start:"", end-start+1);
    Jsi_DSAppend(&dStr, "*", NULL);
    substr = Jsi_DSValue(&dStr);
    Jsi_Value *arg = Jsi_ValueNewStringDup(interp, substr);
    Jsi_IncrRefCount(interp, arg);
    
    Jsi_Obj *nobj = Jsi_ObjNew(interp);
    Jsi_ValueMakeArrayObject(interp, ret, nobj);

    Jsi_RC rc = InfoCmdsCmdSub(interp, arg, _this, ret, funcPtr, 0, 0, nobj, 0);
    if (rc != JSI_OK || (Jsi_ValueIsArray(interp, *ret) && (*ret)->d.obj->arrCnt>0))
        goto done;
    rc = InfoFuncDataSub(interp, arg, _this, ret, funcPtr, 0x7, nobj);
    if (rc != JSI_OK || (Jsi_ValueIsArray(interp, *ret) && (*ret)->d.obj->arrCnt>0))
        goto done;
    if (str) {
        substr[end-start+1] = 0;
        slen = Jsi_Strlen(substr);
    }
    if (str == NULL || !Jsi_Strchr(substr, '.')) {
        Jsi_HashEntry *hPtr;
        Jsi_HashSearch search;
        int n = Jsi_ValueGetLength(interp, *ret);
        for (hPtr = Jsi_HashSearchFirst(interp->lexkeyTbl, &search);
            hPtr != NULL; hPtr = Jsi_HashSearchNext(&search)) {
            key = (char*)Jsi_HashKeyGet(hPtr);
            if (str == NULL || !Jsi_Strncmp(substr, key, slen))
                Jsi_ValueArraySet(interp, *ret, Jsi_ValueNewStringKey(interp, key), n++);
        }
    }
    Jsi_ValueArraySort(interp, *ret, 0);
done:
    Jsi_DSFree(&dStr);
    Jsi_DecrRefCount(interp, arg);
    return rc;
}

Jsi_Value * Jsi_LookupCS(Jsi_Interp *interp, const char *name, int *ofs)
{
    const char *csdot = Jsi_Strrchr(name, '.');
    int len;
    *ofs = 0;
    if (csdot == name)
        return NULL;
    if (!csdot)
        return interp->csc;
    if (!csdot[1])
        return NULL;
    Jsi_DString dStr;
    Jsi_DSInit(&dStr);
    Jsi_DSAppendLen(&dStr, name, len=csdot-name);
    Jsi_Value *cs = Jsi_NameLookup(interp, Jsi_DSValue(&dStr));
    Jsi_DSFree(&dStr);
    *ofs = len+1;
    return cs;
}

static void ValueObjDeleteStr(Jsi_Interp *interp, Jsi_Value *target, const char *key, int force)
{
    const char *kstr = key;
    if (target->vt != JSI_VT_OBJECT) return;

    Jsi_TreeEntry *hPtr;
    Jsi_MapEntry *hePtr = Jsi_MapEntryFind(target->d.obj->tree->opts.interp->strKeyTbl, key);
    if (hePtr)
        kstr = (const char*)Jsi_MapKeyGet(hePtr, 0);
    hPtr = Jsi_TreeEntryFind(target->d.obj->tree, kstr);
    if (hPtr == NULL || ( hPtr->f.bits.dontdel && !force))
        return;
    Jsi_TreeEntryDelete(hPtr);
}

Jsi_RC Jsi_CommandDelete(Jsi_Interp *interp, const char *name) {
    int ofs;
    Jsi_Value *fv = Jsi_NameLookup(interp, name);
    Jsi_Value *cs = Jsi_LookupCS(interp, name, &ofs);
    if (cs) {
        const char *key = name + ofs;
        ValueObjDeleteStr(interp, cs, key, 1);
    }
    if (fv)
        Jsi_DecrRefCount(interp, fv);
    return JSI_OK;
}


Jsi_Value * jsi_CommandCreate(Jsi_Interp *interp, const char *name, Jsi_CmdProc *cmdProc, void *privData, int flags, Jsi_CmdSpec *cspec)
{
    Jsi_Value *n = NULL;
    const char *csdot = Jsi_Strrchr(name, '.');
    if (0 && csdot) {
        Jsi_LogBug("commands with dot unsupported: %s", name);
        return NULL;
    }
    if (csdot == name) {
        name = csdot+1;
        csdot = NULL;
    }
    if (!csdot) {
        n = jsi_MakeFuncValue(interp, cmdProc, name, NULL, cspec);
        Jsi_IncrRefCount(interp, n);
        Jsi_ObjDecrRefCount(interp, n->d.obj);
        Jsi_Func *f = n->d.obj->d.fobj->func;
        f->privData = privData;
        f->name = Jsi_KeyAdd(interp, name);
        Jsi_ValueInsertFixed(interp, interp->csc, f->name, n);
        return n;
    }
    Jsi_DString dStr;
    Jsi_DSInit(&dStr);
    Jsi_DSAppendLen(&dStr, name, csdot-name);
    Jsi_Value *cs = Jsi_NameLookup(interp, Jsi_DSValue(&dStr));
    Jsi_DSFree(&dStr);
    if (cs) {
    
        n = jsi_MakeFuncValue(interp, cmdProc, csdot+1, NULL, cspec);
        Jsi_IncrRefCount(interp, n);
        Jsi_ObjDecrRefCount(interp, n->d.obj);
        Jsi_Func *f = n->d.obj->d.fobj->func;
        
        f->name = Jsi_KeyAdd(interp, csdot+1);
        f->privData = privData;
        Jsi_ValueInsertFixed(interp, cs, f->name, n);
        //Jsi_ObjDecrRefCount(interp, n->d.obj);
    }
    return n;
}

Jsi_Value * Jsi_CommandCreate(Jsi_Interp *interp, const char *name, Jsi_CmdProc *cmdProc, void *privData)
{
    Jsi_Value *v = jsi_CommandCreate(interp, name, cmdProc, privData, 1, 0);
    if (v)
        Jsi_HashSet(interp->genValueTbl, v, v);
    return v;
}

// Sanity check builtin args signature.
bool jsi_CommandArgCheck(Jsi_Interp *interp, Jsi_CmdSpec *cmdSpec, Jsi_Func *f, const char *parent)
{
    bool rc = 1;
    Jsi_DString dStr = {};
    Jsi_DSAppend(&dStr, cmdSpec->argStr, NULL);
    const char *elips = Jsi_Strstr(cmdSpec->argStr, "...");
    if (cmdSpec->maxArgs<0 && !elips)
        Jsi_DSAppend(&dStr, ", ...", NULL);
    int aCnt = 0, i = -1;
    char *cp = Jsi_DSValue(&dStr);
    while (cp[++i]) if (cp[i]==',') aCnt++;
    
    if (cmdSpec->maxArgs>=0) {
        aCnt++;
        if (cmdSpec->minArgs<0 || cmdSpec->minArgs>cmdSpec->maxArgs || 
            cmdSpec->minArgs>aCnt || cmdSpec->maxArgs>aCnt || elips) {
            rc = 0;
        }                    
    } else {
        if (cmdSpec->minArgs<0 || cmdSpec->minArgs>aCnt) {
            rc = 0;
        }                    
    }
    Jsi_DSFree(&dStr);
    if (!rc) {
        jsi_TypeMismatch(interp);
        Jsi_LogWarn("inconsistent arg string for \"%s.%s(%s)\" [%d,%d]",
            parent, cmdSpec->name, cmdSpec->argStr, cmdSpec->minArgs, cmdSpec->maxArgs);
    }
    if (f->argnames==NULL && cmdSpec->argStr) {
        // At least give a clue where the problem is.
        const char *ocfile = interp->curFile;
        jsi_Pline *opl = interp->parseLine, pline;
        interp->parseLine = &pline;
        pline.first_line = 1;
        interp->curFile = cmdSpec->name;
        f->argnames = jsi_ParseArgStr(interp, cmdSpec->argStr);
        interp->curFile = ocfile;
        interp->parseLine = opl;
    }
    return rc;
}

static Jsi_Value *CommandCreateWithSpec(Jsi_Interp *interp, Jsi_CmdSpec *cSpec, int idx, Jsi_Value *proto, void *privData, const char *parent)
{
    Jsi_CmdSpec *cmdSpec = cSpec+idx;
    int iscons = (cmdSpec->flags&JSI_CMD_IS_CONSTRUCTOR);
    Jsi_Value *func = NULL;
    Jsi_Func *f;
    if (cmdSpec->name)
        Jsi_KeyAdd(interp, cmdSpec->name);
    if (cmdSpec->proc == NULL)
    {
        func = jsi_ProtoObjValueNew1(interp, cmdSpec->name);
        Jsi_ValueInsertFixed(interp, NULL, cmdSpec->name, func);
        f = func->d.obj->d.fobj->func;
    } else {
    
        func = jsi_MakeFuncValueSpec(interp, cmdSpec, privData);
    #ifdef JSI_MEM_DEBUG
        func->VD.label = "CMDspec";
        func->VD.label2 = cmdSpec->name;
    #endif
        //Jsi_IncrRefCount(interp, func);
        //Jsi_HashSet(interp->genValueTbl, func, func);
        Jsi_ValueInsertFixed(interp, (iscons?NULL:proto), cmdSpec->name, func);
        f = func->d.obj->d.fobj->func;
    
        if (cmdSpec->name)
            f->name = cmdSpec->name;
        f->f.flags = (cmdSpec->flags & JSI_CMD_MASK);
        f->f.bits.hasattr = 1;
        if (iscons) {
            f->f.bits.iscons = 1;
            Jsi_ValueInsertFixed(interp, func, "prototype", proto);
            Jsi_PrototypeObjSet(interp, "Function", Jsi_ValueGetObj(interp, func));
        }
    }
    func->d.obj->d.fobj->func->parent = parent;
    if (cmdSpec->argStr && interp->typeCheck.all)
        jsi_CommandArgCheck(interp, cmdSpec, f, parent);

    f->retType = cmdSpec->retType;
    return func;
}


Jsi_Value *Jsi_CommandCreateSpecs(Jsi_Interp *interp, const char *name, Jsi_CmdSpec *cmdSpecs,
    void *privData, int flags)
{
    int i = 0;
    Jsi_Value *proto;
    if (!cmdSpecs[0].name)
        return NULL;
    if (!name)
        name = cmdSpecs[0].name;
    name = Jsi_KeyAdd(interp, name);
    if (flags & JSI_CMDSPEC_PROTO) {
        proto = (Jsi_Value*)privData;
        privData = NULL;
        i++;
    } else if (name[0] == 0)
        proto = NULL;
    else {
        if (!(flags & JSI_CMDSPEC_ISOBJ)) {
            proto = jsi_CommandCreate(interp, name, NULL, privData, 0, cmdSpecs);
        } else {
            proto = jsi_ProtoValueNew(interp, name, NULL);
        }
    }
    for (; cmdSpecs[i].name; i++)
        CommandCreateWithSpec(interp,  cmdSpecs, i, proto, privData, name);

    bool isNew;
    Jsi_MapEntry *hPtr = Jsi_MapEntryNew(interp->cmdSpecTbl, name, &isNew);
    if (!hPtr || !isNew) {
        Jsi_LogBug("failed cmdspec register: %s", name);
        return NULL;
    }
    Jsi_CmdSpecItem *op, *p = (Jsi_CmdSpecItem*)Jsi_Calloc(1,sizeof(*p));
    SIGINIT(p,CMDSPECITEM);
    p->spec = cmdSpecs;
    p->flags = flags;
    p->proto = proto;
    p->privData = privData;
    p->name = (const char*)Jsi_MapKeyGet(hPtr, 0);
    p->hPtr = hPtr;
    Jsi_CmdSpec *csi = cmdSpecs;
    p->isCons = (csi && csi->flags&JSI_CMD_IS_CONSTRUCTOR);
    while (csi->name)
        csi++;
    p->help = csi->help;
    p->info = csi->info;
    if (!isNew) {
        op = (Jsi_CmdSpecItem*)Jsi_MapValueGet(hPtr);
        p->next = op;
    }
    Jsi_MapValueSet(hPtr, p);
    return proto;
}

void jsi_CmdSpecDelete(Jsi_Interp *interp, void *ptr)
{
    Jsi_CmdSpecItem *cs = (Jsi_CmdSpecItem*)ptr;
    Jsi_CmdSpec *p;
    SIGASSERT(cs,CMDSPECITEM);
    //return;
    while (cs) {
        p = cs->spec;
        Jsi_Value *proto = cs->proto;
        if (proto)
            Jsi_DecrRefCount(interp, proto);
        while (0 && p && p->name) {
            /*Jsi_Value *proto = p->proto;
            if (proto)
                Jsi_DecrRefCount(interp, proto);*/
            p++;
        }
        ptr = cs;
        cs = cs->next;
        Jsi_Free(ptr);
    }
}

static Jsi_RC SysTimesCmd(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this,
    Jsi_Value **ret, Jsi_Func *funcPtr)
{
    Jsi_RC rc = JSI_OK;
    int i, n=1, argc = Jsi_ValueGetLength(interp, args);
    Jsi_Value *func = Jsi_ValueArrayIndex(interp, args, 0);
    Jsi_Wide diff, start, end;
    if(!Jsi_ValueIsFunction(interp, func))
        return Jsi_LogError("expected function");
    if (argc > 1 && Jsi_GetIntFromValue(interp, Jsi_ValueArrayIndex(interp, args, 1), &n) != JSI_OK)
        return JSI_ERROR;
    if (n<=0) 
        return Jsi_LogError("count not > 0: %d", n);
    struct timeval tv;
    gettimeofday(&tv, NULL);
    start = (Jsi_Wide) tv.tv_sec * 1000000 + tv.tv_usec;
    for (i=0; i<n; i++) {
        rc = Jsi_FunctionInvoke(interp, func, NULL, ret, NULL);
    }
    gettimeofday(&tv, NULL);
    end = (Jsi_Wide) tv.tv_sec * 1000000 + tv.tv_usec;
    diff = (end - start);
    Jsi_ValueMakeNumber(interp, ret, (Jsi_Number)diff);
    return rc;
}

static Jsi_RC SysFormatCmd(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this,
    Jsi_Value **ret, Jsi_Func *funcPtr)
{
    Jsi_DString dStr;
    Jsi_RC rc = Jsi_FormatString(interp, args, &dStr);
    if (rc != JSI_OK)
        return rc;
    Jsi_ValueMakeString(interp, ret, Jsi_Strdup(Jsi_DSValue(&dStr)));
    Jsi_DSFree(&dStr);
    return JSI_OK;
}

static Jsi_RC quoteCmd(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this,
    Jsi_Value **ret, Jsi_Func *funcPtr)
{
    Jsi_DString dStr = {};
    Jsi_Value *arg = Jsi_ValueArrayIndex(interp, args, 0);
    const char *str = Jsi_ValueGetDString(interp, arg, &dStr, 1);
    Jsi_ValueMakeString(interp, ret, Jsi_Strdup(str));
    Jsi_DSFree(&dStr);
    return JSI_OK;
}
// Karl Malbrain's compact CRC-32. See "A compact CCITT crc16 and crc32 C implementation that balances processor cache usage against speed": http://www.geocities.com/malbrain/
uint32_t Jsi_Crc32(uint32_t crc, const void *ptr, size_t buf_len)
{
    static const uint32_t s_crc32[16] = {
        0x00000000, 0x1db71064, 0x3b6e20c8, 0x26d930ac, 0x76dc4190, 0x6b6b51f4, 0x4db26158, 0x5005713c,
        0xedb88320, 0xf00f9344, 0xd6d6a3e8, 0xcb61b38c, 0x9b64c2b0, 0x86d3d2d4, 0xa00ae278, 0xbdbdf21c
    };
    uint32_t crcu32 = (uint32_t)crc;
    uchar *uptr = (uchar *)ptr;
    if (!uptr) return 0;
    crcu32 = ~crcu32;
    while (buf_len--) {
        uchar b = *uptr++;
        crcu32 = (crcu32 >> 4) ^ s_crc32[(crcu32 & 0xF) ^ (b & 0xF)];
        crcu32 = (crcu32 >> 4) ^ s_crc32[(crcu32 & 0xF) ^ (b >> 4)];
    }
    return ~crcu32;
}

static Jsi_RC SysCrc32Cmd(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this,
    Jsi_Value **ret, Jsi_Func *funcPtr)
{
    int ilen;
    char *inbuffer = Jsi_ValueArrayIndexToStr(interp, args, 0, &ilen);
    Jsi_Number crc = 0;
    int argc = Jsi_ValueGetLength(interp, args);
    if (argc>1)
        Jsi_ValueGetNumber(interp, Jsi_ValueArrayIndex(interp, args, 1), &crc);
    Jsi_ValueMakeNumber(interp, ret, (Jsi_Number)Jsi_Crc32((uint32_t)crc, (uchar*)inbuffer, ilen));
    return JSI_OK;
}

#ifndef JSI_OMIT_BASE64
static const char b64ev[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static int atend;

static int
b64getidx(const char *buffer, int len, int *posn) {
    char c;
    char *idx;
    if (atend) return -1;
    do {
    if ((*posn)>=len) {
        atend = 1;
        return -1;
    }
    c = buffer[(*posn)++];
    if (c<0 || c=='=') {
        atend = 1;
        return -1;
    }
    idx = Jsi_Strchr(b64ev, c);
    } while (!idx);
    return idx - b64ev; 
} 

static void B64DecodeDStr(const char *inbuffer, int ilen, Jsi_DString *dStr)
{
    int olen, pos;
    char outbuffer[3];
    int c[4];
    
    pos = 0; 
    atend = 0;
    while (!atend) {
        if (inbuffer[pos]=='\n' ||inbuffer[pos]=='\r') { pos++; continue; }
        c[0] = b64getidx(inbuffer, ilen, &pos);
        c[1] = b64getidx(inbuffer, ilen, &pos);
        c[2] = b64getidx(inbuffer, ilen, &pos);
        c[3] = b64getidx(inbuffer, ilen, &pos);

        olen = 0;
        if (c[0]>=0 && c[1]>=0) {
            outbuffer[0] = ((c[0]<<2)&0xfc)|((c[1]>>4)&0x03);
            olen++;
            if (c[2]>=0) {
                outbuffer[1] = ((c[1]<<4)&0xf0)|((c[2]>>2)&0x0f);
                olen++;
                if (c[3]>=0) {
                    outbuffer[2] = ((c[2]<<6)&0xc0)|((c[3])&0x3f);
                    olen++;
                }
            }
        }

        if (olen>0)
            Jsi_DSAppendLen(dStr, outbuffer, olen);
        olen = 0;
    }
    if (olen>0)
        Jsi_DSAppendLen(dStr, outbuffer, olen);
}

static Jsi_RC B64Decode(Jsi_Interp *interp, char *inbuffer, int ilen, Jsi_Value **ret)
{
    Jsi_DString dStr;
    Jsi_DSInit(&dStr);
    B64DecodeDStr(inbuffer, ilen, &dStr);
    Jsi_ValueMakeString(interp, ret, Jsi_DSFreeDup(&dStr));
    return JSI_OK;
}

static Jsi_RC
B64EncodeDStr(const char *ib, int ilen, Jsi_DString *dStr)
{
    int i=0, pos=0;
    char c[74];
    
    while (pos<ilen) {
#define P(n,s) ((pos+n)>ilen?'=':b64ev[s])
        c[i++]=b64ev[(ib[pos]>>2)&0x3f];
        c[i++]=P(1,((ib[pos]<<4)&0x30)|((ib[pos+1]>>4)&0x0f));
        c[i++]=P(2,((ib[pos+1]<<2)&0x3c)|((ib[pos+2]>>6)&0x03));
        c[i++]=P(3,ib[pos+2]&0x3f);
        if (i>=72) {
            c[i++]='\n';
            c[i]=0;
            Jsi_DSAppendLen(dStr, c, i);
            i=0;
        }
        pos+=3;
    }
    if (i) {
        /*    c[i++]='\n';*/
        c[i]=0;
        Jsi_DSAppendLen(dStr, c, i);
        i=0;
    }
    return JSI_OK;
}

static Jsi_RC
B64Encode(Jsi_Interp *interp, char *ib, int ilen, Jsi_Value **ret)
{
    Jsi_DString dStr;
    Jsi_DSInit(&dStr);
    B64EncodeDStr(ib, ilen, &dStr);
    Jsi_ValueMakeString(interp, ret, Jsi_DSFreeDup(&dStr));
    return JSI_OK;
}

Jsi_RC Jsi_Base64(const char *str, int len, Jsi_DString *dStr, bool decode)
{
    if (len<0)
        len = Jsi_Strlen(str);
    if (decode)
        B64DecodeDStr(str, len, dStr);
    else
        B64EncodeDStr(str, len, dStr);
    return JSI_OK;
}

static Jsi_RC B64DecodeCmd(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this,
    Jsi_Value **ret, Jsi_Func *funcPtr)
{
    int ilen;
    char *inbuffer = Jsi_ValueArrayIndexToStr(interp, args, 0, &ilen);
    return B64Decode(interp, inbuffer, ilen, ret);
}

static Jsi_RC B64EncodeCmd(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this,
    Jsi_Value **ret, Jsi_Func *funcPtr)
{
    int ilen;
    char *inbuffer = Jsi_ValueArrayIndexToStr(interp, args, 0, &ilen);
    return B64Encode(interp, inbuffer, ilen, ret);
}

static Jsi_RC SysBase64Cmd(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this,
    Jsi_Value **ret, Jsi_Func *funcPtr)
{
    Jsi_Value *arg = Jsi_ValueArrayIndex(interp, args, 1);
    bool dec = 0;
    if (arg && Jsi_GetBoolFromValue(interp, arg, &dec) != JSI_OK) 
        return Jsi_LogError("expected bool");
    if (dec)
        return B64DecodeCmd(interp, args, _this, ret, funcPtr);
    else
        return B64EncodeCmd(interp, args, _this, ret, funcPtr);
}

static Jsi_RC SysHexStrCmd(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this,
    Jsi_Value **ret, Jsi_Func *funcPtr)
{
    Jsi_Value *arg = Jsi_ValueArrayIndex(interp, args, 1);
    bool dec = 0;
    if (arg && Jsi_GetBoolFromValue(interp, arg, &dec) != JSI_OK) 
        return Jsi_LogError("expected bool");
    int len;
    const char *str = Jsi_ValueArrayIndexToStr(interp, args, 0, &len);
    if (!str || len<=0)
        return Jsi_LogError("expected string");
    char *dest = (char*)Jsi_Malloc(dec?(len/2+1):(len*2+1));
    if (dec)
        Jsi_ToHexStr((const uchar*)str, len, dest);
    else
        Jsi_FromHexStr(str, (uchar*)dest);
    Jsi_ValueMakeString(interp, ret, dest);
    return JSI_OK;
}
#endif


static const char *hashTypeStrs[] = { "sha256", "sha1", "md5", "sha3_224", "sha3_384", "sha3_512", "sha3_256", NULL };

typedef struct {
    uint hashcash;
    Jsi_Value *file;
    Jsi_CryptoHashType type;
    bool noHex;
} HashOpts;


static Jsi_OptionSpec HashOptions[] = {
    JSI_OPT(STRING,  HashOpts, file,      .help="Read data from file and append to str" ),
    JSI_OPT(UINT,    HashOpts, hashcash,  .help="Search for a hash with this many leading zero bits by appending :nonce (Proof-Of-Work)" ),
    JSI_OPT(BOOL,    HashOpts, noHex,     .help="Return binary digest, without conversion to hex chars" ),
    JSI_OPT(CUSTOM,  HashOpts, type,      .help="Type of hash", .flags=0, .custom=Jsi_Opt_SwitchEnum, .data=hashTypeStrs ),
    {JSI_OPTION_END}
};

static Jsi_RC SysHashCmd(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this,
    Jsi_Value **ret, Jsi_Func *funcPtr)
{
    int n, hasopts = 0;
    const char *cp = NULL;
    Jsi_RC rc = JSI_OK;
    Jsi_DString dStr = {};
    HashOpts edata = {};
    char zbuf[1024];
    
    Jsi_Value *arg = Jsi_ValueArrayIndex(interp, args, 0);
    if (!arg || !Jsi_ValueIsString(interp, arg))
        return Jsi_LogError("arg 1: expected string");
        
    cp=Jsi_ValueString(interp, arg, &n);
    Jsi_DSAppendLen(&dStr, cp, n);
    Jsi_Value *opt = Jsi_ValueArrayIndex(interp, args, 1);
    if (opt) {
        if (opt->vt != JSI_VT_OBJECT || opt->d.obj->ot != JSI_OT_OBJECT)
            return Jsi_LogError("expected options object");
        if (Jsi_OptionsProcess(interp, HashOptions, &edata, opt, 0) < 0)
            return JSI_ERROR;
        hasopts = 1;
    }
    
    if (edata.file) {
        Jsi_Channel in = Jsi_Open(interp, edata.file, "rb");
        
        if( in==0 ) {
            rc = Jsi_LogError("unable to open file");
            goto done;
        }
        for(;;) {
            int n;
            n = Jsi_Read(in, zbuf, sizeof(zbuf));
            if( n<=0 ) break;
            Jsi_DSAppendLen(&dStr, zbuf, n);;
        }
        Jsi_Close(in);
    }
    cp = Jsi_DSValue(&dStr);
    n = Jsi_DSLength(&dStr);
    zbuf[0] = 0;
    Jsi_CryptoHash(zbuf, cp, n, edata.type, edata.hashcash, edata.noHex);
        
done:
    Jsi_DSFree(&dStr);
    if (hasopts)
        Jsi_OptionsFree(interp, HashOptions, &edata, 0);
    if (rc == JSI_OK)
        Jsi_ValueMakeStringDup(interp, ret, zbuf);
    return rc;

}


#ifndef JSI_OMIT_ENCRYPT

#define FN_encrypt JSI_INFO("\
Keys that are not 16 bytes use the MD5 hash of the key.")
static Jsi_RC SysEncryptCmd_(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this,
    Jsi_Value **ret, Jsi_Func *funcPtr, bool decrypt)
{
    int ilen, klen;
    Jsi_RC rc = JSI_OK;
    const char *key, *str = Jsi_ValueArrayIndexToStr(interp, args, 0, &ilen);
    Jsi_DString dStr = {};
    if (!str || !ilen) 
        return Jsi_LogError("data must be a non-empty string");
    if (decrypt && (ilen%4) != 1) {
        rc = Jsi_LogError("data length incorrect and can not be decrypted");
        goto done;
    }
    key = Jsi_ValueArrayIndexToStr(interp, args, 1, &klen);
    Jsi_DSAppendLen(&dStr, str, ilen);
    rc = Jsi_Encrypt(interp, &dStr, key, klen, decrypt);

    if (rc == JSI_OK)
        Jsi_ValueMakeDStringObject(interp, ret, &dStr);
done:
    return rc;
}
static Jsi_RC SysEncryptCmd(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this,
    Jsi_Value **ret, Jsi_Func *funcPtr)
{
    return SysEncryptCmd_(interp, args, _this, ret, funcPtr, 0);
}
static Jsi_RC SysDecryptCmd(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this,
    Jsi_Value **ret, Jsi_Func *funcPtr)
{
    return SysEncryptCmd_(interp, args, _this, ret, funcPtr, 1);
}
#endif

#ifndef JSI_OMIT_MARKDOWN

Jsi_OptionSpec MarkdownCmdsOptions[] = {
    JSI_OPT(STRKEY, jsi_MarkdownOpts, topLink, "Web server root url" ),
    JSI_OPT(BOOL,   jsi_MarkdownOpts, returnStr,  "Return just HTML string instead of an object" ),
    JSI_OPT(BOOL,   jsi_MarkdownOpts, getTitle, "Extract title from first H1 in page" ),
    JSI_OPT_END(jsi_MarkdownOpts, .help="Options for markdown command")
};

Jsi_RC jsi_SysMarkdownCmd(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this,
    Jsi_Value **ret, Jsi_Func *funcPtr)
{
    int blen;
    const char *str = Jsi_ValueArrayIndexToStr(interp, args, 0, &blen);
    Jsi_Value *opts = Jsi_ValueArrayIndex(interp, args, 1);
    Jsi_DString bStr = {};
    Jsi_DString rStr = {};
    jsi_MarkdownOpts data = {};
    if (str)
        Jsi_DSAppendLen(&bStr, str, blen);
    if (opts && Jsi_OptionsProcess(interp, MarkdownCmdsOptions, &data, opts, 0) < 0)
        return JSI_ERROR;
    jsi_markdown_to_html(&bStr, &rStr, &data);
    if (data.returnStr)
        Jsi_ValueMakeDStringObject(interp, ret, &rStr);
    else {
        Jsi_Obj *nobj = Jsi_ObjNew(interp);
        Jsi_ValueMakeObject(interp, ret, nobj);
        Jsi_Value *cval = Jsi_ValueNew(interp);
        Jsi_ValueFromDS(interp, &data.titleStr, &cval);
        Jsi_ObjInsert(interp, nobj, "title", cval, 0);
        Jsi_ValueFromDS(interp, &rStr, &cval);
        Jsi_ObjInsert(interp, nobj, "data", cval, 0);
    }
    Jsi_DSFree(&bStr);
    return JSI_OK;
}
#endif

static Jsi_RC SysFromCharCodeCmd(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this,
    Jsi_Value **ret, Jsi_Func *funcPtr)
{
    char unibuf[BUFSIZ+1];
    int i = 1;    
    Jsi_Value *v = Jsi_ValueArrayIndex(interp, args, 0);
    if (!v) return Jsi_LogError("expected number");

    Jsi_ValueToNumber(interp, v);
#if JSI_UTF8
    i = Jsi_UniCharToUtf(v->d.num, unibuf);
#else
    unibuf[0] = (char) v->d.num;
#endif
    unibuf[i] = 0;
    Jsi_ValueMakeStringDup(interp, ret, unibuf);
    return JSI_OK;
}
/* Commands visible at the toplevel. */

static Jsi_CmdSpec consoleCmds[] = {
    { "assert", jsi_AssertCmd,      1,  3, "expr:boolean|number|function, msg:string=void, options:object=void",  .help="Same as System.assert()", .retType=(uint)JSI_TT_VOID, .flags=0, .info=0, .opts=AssertOptions},
    { "input",  consoleInputCmd,    0,  0, "", .help="Read input from the console", .retType=(uint)JSI_TT_STRING|JSI_TT_VOID },
    { "log",    consoleLogCmd,      1, -1, "val, ...", .help="Same as puts, but goes to stderr and includes file:line", .retType=(uint)JSI_TT_VOID, .flags=0 },
    { "logf",   consoleLogfCmd,     1, -1, "format:string, ...", .help="Same as printf, but goes to stderr and includes file:line and newline", .retType=(uint)JSI_TT_VOID, .flags=0, .info=FN_consolelogf },
    { "puts",   consolePutsCmd,     1, -1, "val, ...", .help="Output one or more values to stderr", .retType=(uint)JSI_TT_VOID, .flags=0, .info=FN_puts },
    { NULL, 0,0,0,0,  .help="Console input and output" }
};

#ifndef JSI_OMIT_EVENT
static Jsi_CmdSpec eventCmds[] = {
    { "clearInterval",clearIntervalCmd, 1,  1, "id:number", .help="Delete an event (created with setInterval/setTimeout)", .retType=(uint)JSI_TT_VOID },
    { "info",       eventInfoCmd,       1,  1, "id:number", .help="Return info for the given event id", .retType=(uint)JSI_TT_OBJECT },
    { "names",      eventInfoCmd,       0,  0, "", .help="Return list event ids (created with setTimeout/setInterval)", .retType=(uint)JSI_TT_ARRAY },
    { "setInterval",setIntervalCmd,     2,  2, "callback:function, millisecs:number", .help="Setup recurring function to run every given millisecs", .retType=(uint)JSI_TT_NUMBER },
    { "setTimeout", setTimeoutCmd,      2,  2, "callback:function, millisecs:number", .help="Setup function to run after given millisecs", .retType=(uint)JSI_TT_NUMBER },
    { "update",     SysUpdateCmd,    0,  1, "options:number|object=void", .help="Service all events, eg. setInterval/setTimeout", .retType=(uint)JSI_TT_NUMBER, .flags=0, .info=FN_update, .opts=UpdateOptions },
    { NULL, 0,0,0,0,  .help="Event management" }
};
#endif

#ifndef JSI_OMIT_DEBUG
static Jsi_CmdSpec debugCmds[] = {
    { "add",        DebugAddCmd,    1,  2, "val:string|number, temp:boolean=false", .help="Add a breakpoint for line, file:line or func", .retType=(uint)JSI_TT_NUMBER },
    { "remove",     DebugRemoveCmd, 1,  1, "id:number", .help="Remove breakpoint", .retType=(uint)JSI_TT_VOID },
    { "enable",     DebugEnableCmd, 2,  2, "id:number, on:boolean", .help="Enable/disable breakpoint", .retType=(uint)JSI_TT_VOID },
    { "info",       DebugInfoCmd,   0,  1, "id:number=void", .help="Return info about one breakpoint, or list of bp numbers", .retType=(uint)JSI_TT_OBJECT|JSI_TT_ARRAY },
    { NULL, 0,0,0,0,  .help="Debugging management" }
};
#endif

static Jsi_CmdSpec infoCmds[] = {
    { "argv0",      InfoArgv0Cmd,       0,  0, "", .help="Return initial start script file name", .retType=(uint)JSI_TT_STRING|JSI_TT_VOID },
    { "cmds",       InfoCmdsCmd,        0,  2, "val:string|regexp='*', options:object=void", .help="Return details or list of matching commands", .retType=(uint)JSI_TT_ARRAY|JSI_TT_OBJECT, .flags=0, .info=0, .opts=InfoCmdsOptions },
    { "completions",InfoCompletionsCmd,  1,  3, "str:string, start:number=0, end:number=void", .help="Return command completions on portion of string from start to end", .retType=(uint)JSI_TT_ARRAY },
    { "data",       InfoDataCmd,        0,  1, "val:string|regexp|object=void", .help="Return list of matching data (non-functions)", .retType=(uint)JSI_TT_ARRAY, .flags=0, .info=FN_infodata },
    { "error",      InfoErrorCmd,       0,  0, "", .help="Return file and line number of error (used inside catch)", .retType=(uint)JSI_TT_OBJECT },
#ifndef JSI_OMIT_EVENT
    { "event",      InfoEventCmd,       0,  1, "id:number=void", .help="List events or info for 1 event (setTimeout/setInterval)", .retType=(uint)JSI_TT_ARRAY|JSI_TT_OBJECT, .flags=0, .info=FN_infoevent },
#endif
    { "executable", InfoExecutableCmd,  0,  0, "", .help="Return name of executable", .retType=(uint)JSI_TT_STRING },
    { "execZip",    InfoExecZipCmd,     0,  0, "", .help="If executing a .zip file, return file name", .retType=(uint)JSI_TT_STRING|JSI_TT_VOID },
    { "files",      InfoFilesCmd,       0,  0, "", .help="Return list of all sourced files", .retType=(uint)JSI_TT_ARRAY },
    { "funcs",      InfoFuncsCmd,       0,  1, "string|regexp|object=void", .help="Return details or list of matching functions", .retType=(uint)JSI_TT_ARRAY|JSI_TT_OBJECT },
    { "interp",     jsi_InterpInfo,     0,  1, "interp:userobj=void", .help="Return info on given or current interp", .retType=(uint)JSI_TT_OBJECT },
    { "isMain",     InfoIsMainCmd,      0,  0, "", .help="Return true if current script was the main script invoked from command-line", .retType=(uint)JSI_TT_BOOLEAN },
    { "keywords",   InfoKeywordsCmd,    0,  0, "", .help="Return list of reserved jsi keywords", .retType=(uint)JSI_TT_ARRAY },
    { "level",      InfoLevelCmd,       0,  1, "level:number=void", .help="Return current level or details of a call-stack frame", .retType=(uint)JSI_TT_ARRAY|JSI_TT_OBJECT|JSI_TT_NUMBER, .flags=0, .info=FN_infolevel },
    { "lookup",     InfoLookupCmd,      1,  1, "name:string", .help="Given string name, lookup and return value (eg. function)", .retType=(uint)JSI_TT_ANY },
    { "methods",    InfoMethodsCmd,     1,  1, "val:string|regexp", .help="Return functions and commands", .retType=(uint)JSI_TT_ARRAY|JSI_TT_OBJECT },
    { "named",      InfoNamedCmd,       0,  1, "name:string=void", .help="Returns command names for builtin Objects (eg. 'File', 'Interp'), sub-Object names, or the named object", .retType=(uint)JSI_TT_ARRAY|JSI_TT_USEROBJ },
    { "options",    InfoOptionsCmd,     0,  1, "ctype:boolean=false", .help="Return Option type name, or with true the C type)", .retType=(uint)JSI_TT_ARRAY },
    { "platform",   InfoPlatformCmd,    0,  0, "", .help="N/A. Returns general platform information for JSI", .retType=(uint)JSI_TT_OBJECT  },
    { "script",     InfoScriptCmd,      0,  1, "func:function|regexp=void", .help="Get current script file name, or file containing function", .retType=(uint)JSI_TT_STRING|JSI_TT_ARRAY|JSI_TT_VOID },
    { "scriptDir",  InfoScriptDirCmd,   0,  0, "", .help="Get directory of current script", .retType=(uint)JSI_TT_STRING|JSI_TT_VOID },
    { "vars",       InfoVarsCmd,        0,  1, "val:string|regexp|object=void", .help="Return details or list of matching variables", .retType=(uint)JSI_TT_ARRAY|JSI_TT_OBJECT, .flags=0, .info=FN_infovars },
    { "version",    InfoVersionCmd,     0,  1, "boolStr:string|boolean=false", .help="Return JSI version double, string or object when boolStr==true", .retType=(uint)JSI_TT_NUMBER|JSI_TT_OBJECT|JSI_TT_STRING  },
    { NULL, 0,0,0,0, .help="Commands for inspecting internal state information in JSI"  }
};

static Jsi_CmdSpec utilCmds[] = {
#ifndef JSI_OMIT_BASE64
    { "base64",     SysBase64Cmd,    1,  2, "val:string, decode:boolean=false",.help="Base64 encode/decode a string", .retType=(uint)JSI_TT_STRING },
    { "hexStr",     SysHexStrCmd,    1,  2, "val:string, decode:boolean=false",.help="Hex encode/decode a string", .retType=(uint)JSI_TT_STRING },
#endif
    { "crc32",      SysCrc32Cmd,     1,  2, "val:string, crcSeed=0",.help="Calculate 32-bit CRC", .retType=(uint)JSI_TT_NUMBER },
#ifndef JSI_OMIT_ENCRYPT
    { "decrypt",    SysDecryptCmd,   2,  2, "val:string, key:string", .help="Decrypt data using BTEA encryption", .retType=(uint)JSI_TT_STRING, .flags=0, .info=FN_encrypt },
    { "encrypt",    SysEncryptCmd,   2,  2, "val:string, key:string", .help="Encrypt data using BTEA encryption", .retType=(uint)JSI_TT_STRING, .flags=0, .info=FN_encrypt },
#endif
    { "fromCharCode",SysFromCharCodeCmd, 1, 1, "code:number", .help="Return char with given character code", .retType=(uint)JSI_TT_STRING, .flags=JSI_CMDSPEC_NONTHIS},
    { "getenv",     SysGetEnvCmd,    0,  1, "name:string=void", .help="Get one or all environment", .retType=(uint)JSI_TT_STRING|JSI_TT_OBJECT|JSI_TT_VOID  },
    { "getpid",     SysGetPidCmd,    0,  1, "parent:boolean=false", .help="Get process/parent id", .retType=(uint)JSI_TT_NUMBER },
#ifndef JSI_OMIT_MARKDOWN
    { "markdown",   jsi_SysMarkdownCmd,  1,  2, "val:string, options:object=void",.help="Render markdown", .retType=(uint)(JSI_TT_STRING|JSI_TT_OBJECT), .flags=0, .info=0, .opts=MarkdownCmdsOptions },
#endif
    { "setenv",     SysSetEnvCmd,    1,  2, "name:string, value:string=void", .help="Set/get an environment var"  },
    { "hash",       SysHashCmd,      1,  2, "val:string, options|object=void", .help="Return hash (default SHA256) of string/file", .retType=(uint)JSI_TT_STRING, .flags=0, .info=0, .opts=HashOptions},
    { "times",      SysTimesCmd,     1,  2, "callback:function, count:number=1", .help="Call function count times and return execution time in microseconds", .retType=(uint)JSI_TT_NUMBER },
    { NULL, 0,0,0,0, .help="Utilities commands"  }
};

static Jsi_CmdSpec sysCmds[] = {
    { "assert", jsi_AssertCmd,       1,  3, "expr:boolean|number|function, msg:string=void, options:object=void",  .help="Throw or output msg if expr is false", .retType=(uint)JSI_TT_VOID, .flags=0, .info=FN_assert, .opts=AssertOptions },
#ifndef JSI_OMIT_EVENT
    { "clearInterval",clearIntervalCmd,1,1, "id:number", .help="Delete event id returned from setInterval/setTimeout/info.events()", .retType=(uint)JSI_TT_VOID },
#endif
    { "decodeURI",  DecodeURICmd,    1,  1, "val:string", .help="Decode an HTTP URL", .retType=(uint)JSI_TT_STRING },
    { "encodeURI",  EncodeURICmd,    1,  1, "val:string", .help="Encode an HTTP URL", .retType=(uint)JSI_TT_STRING },
    { "exec",       SysExecCmd,      1,  2, "val:string, options:string|object=void", .help="Execute an OS command", .retType=(uint)JSI_TT_ANY, .flags=0, .info=FN_exec, .opts=ExecOptions},
    { "exit",       SysExitCmd,      0,  1, "code:number=0", .help="Exit the current interpreter", .retType=(uint)JSI_TT_VOID },
    { "format",     SysFormatCmd,    1, -1, "format:string, ...", .help="Printf style formatting: adds %q and %S", .retType=(uint)JSI_TT_STRING },
    { "isFinite",   isFiniteCmd,     1,  1, "val", .help="Return true if is a finite number", .retType=(uint)JSI_TT_BOOLEAN },
    { "isNaN",      isNaNCmd,        1,  1, "val", .help="Return true if not a number", .retType=(uint)JSI_TT_BOOLEAN },
#ifndef JSI_OMIT_LOAD
    { "load",       jsi_LoadLoadCmd, 1,  1, "shlib:string", .help="Load a shared executable and invoke its _Init call", .retType=(uint)JSI_TT_VOID },
#endif
    { "log",        SysLogCmd,       1, -1, "val, ...", .help="Same as puts, but includes file:line", .retType=(uint)JSI_TT_VOID, .flags=0 },
    { "noOp",       jsi_NoOpCmd,     0, -1, "", .help="A No-Op. A zero overhead command call that is useful for debugging" },
    { "parseInt",   parseIntCmd,     1,  2, "val:any, base:number=10", .help="Convert string to an integer", .retType=(uint)JSI_TT_NUMBER },
    { "parseFloat", parseFloatCmd,   1,  1, "val", .help="Convert string to a double", .retType=(uint)JSI_TT_NUMBER },
    { "printf",     SysPrintfCmd,    1, -1, "format:string, ...", .help="Formatted output to stdout", .retType=(uint)JSI_TT_VOID, .flags=0, .info=FN_puts },
    { "provide",    SysProvideCmd,   1,  2, "name:string, version:number=1", .help="Provide a package for use with require", .retType=(uint)JSI_TT_VOID },
    { "puts",       SysPutsCmd,      1, -1, "val, ...", .help="Output one or more values to stdout", .retType=(uint)JSI_TT_VOID, .flags=0, .info=FN_puts },
    { "quote",      quoteCmd,        1,  1, "val:string", .help="Return quoted string", .retType=(uint)JSI_TT_STRING },
    { "require",    SysRequireCmd,   0,  2, "name:string=void, version:number=1", .help="Load/query packages", .retType=(uint)JSI_TT_NUMBER|JSI_TT_OBJECT|JSI_TT_ARRAY, .flags=0, .info=FN_require },
    { "sleep",      SysSleepCmd,     0,  1, "secs:number=1.0",  .help="sleep for N milliseconds, minimum .001", .retType=(uint)JSI_TT_VOID },
#ifndef JSI_OMIT_EVENT
    { "setInterval",setIntervalCmd,  2,  2, "callback:function, ms:number", .help="Setup recurring function to run every given millisecs", .retType=(uint)JSI_TT_NUMBER },
    { "setTimeout", setTimeoutCmd,   2,  2, "callback:function, ms:number", .help="Setup function to run after given millisecs", .retType=(uint)JSI_TT_NUMBER },
#endif
    { "source",     SysSourceCmd,    1,  2, "val:string|array, options:object=void",  .help="Load and evaluate source files", .retType=(uint)JSI_TT_VOID, .flags=0, .info=0, .opts=SourceOptions},
    { "strftime",   DateStrftimeCmd, 0,  2, "num:number=null, options:string|object=void",  .help="Format numeric time (in ms) to a string", .retType=(uint)JSI_TT_STRING, .flags=0, .info=FN_strftime, .opts=DateOptions },
    { "strptime",   DateStrptimeCmd, 0,  2, "val:string=void, options:string|object=void",  .help="Parse time from string and return time (in ms) since 1970", .retType=(uint)JSI_TT_NUMBER, .flags=0, .info=0, .opts=DateOptions },
#ifndef JSI_OMIT_LOAD
    { "unload",     jsi_LoadUnloadCmd, 1, 1,  "shlib:string", .help="Unload a shared executable and invoke its _Done call", .retType=(uint)JSI_TT_VOID },
#endif
#ifndef JSI_OMIT_EVENT
    { "update",     SysUpdateCmd,    0,  1, "options:number|object=void", .help="Service all events, eg. setInterval/setTimeout", .retType=(uint)JSI_TT_NUMBER, .flags=0, .info=FN_update, .opts=UpdateOptions },
#endif
    { NULL, 0,0,0,0, .help="Builtin system commands. All are callable from the either the top level or as System.XXX()" }
};

Jsi_RC jsi_InitCmds(Jsi_Interp *interp, int release)
{
    if (release) return JSI_OK;
    interp->console = Jsi_CommandCreateSpecs(interp, "console", consoleCmds, NULL, 0);
    Jsi_IncrRefCount(interp, interp->console);
    Jsi_ValueInsertFixed(interp, interp->console, "args", interp->args);
        
    Jsi_CommandCreateSpecs(interp, "",       sysCmds,    NULL, 0);
    Jsi_CommandCreateSpecs(interp, "System", sysCmds,    NULL, 0);
    Jsi_CommandCreateSpecs(interp, "Info",   infoCmds,   NULL, 0);
    Jsi_CommandCreateSpecs(interp, "Util",   utilCmds,   NULL, 0);
#ifndef JSI_OMIT_EVENT
    Jsi_CommandCreateSpecs(interp, "Event",  eventCmds,  NULL, 0);
#endif
#ifndef JSI_OMIT_DEBUG
    Jsi_CommandCreateSpecs(interp, "Debug",  debugCmds,  NULL, 0);
#endif
    return JSI_OK;
}
#endif
