#ifndef JSI_LITE_ONLY
#ifndef JSI_AMALGAMATION
#include "jsiInt.h"
#endif

#ifndef JSI_OMIT_LOAD

#ifdef __WIN32
#define dlsym(l,s) GetProcAddress(l,s)
#define dlclose(l) FreeLibrary(l)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

#ifndef RTLD_NOW
    #define RTLD_NOW 0
#endif
#ifndef RTLD_LOCAL
    #define RTLD_LOCAL 0
#endif

#endif

typedef struct LoadData {
    const char *name;
    const char *fpath;
#ifdef __WIN32
    HMODULE handle;
#else
    void *handle;
#endif
    Jsi_InitProc *onload;
} LoadData;

#ifndef JSI_OMIT_LOAD

/**
 * Note that jsi_LoadLibrary() requires a path to an existing file.
 *
 * If it is necessary to search autoFiles, use Jsi_PkgLoad() instead.
 */
Jsi_RC jsi_LoadLibrary(Jsi_Interp *interp, const char *pathName)
{
#ifdef __WIN32
    HMODULE handle = LoadLibrary(pathName);
#else
    void *handle = dlopen(pathName, RTLD_NOW | RTLD_LOCAL);
#endif
    if (handle == NULL) {
        // FYI: Valgrind shows a mem-leak here.
        Jsi_LogError("loading extension \"%s\": %s", pathName, dlerror());
        return JSI_ERROR;
    }
    /* We use a unique init symbol depending on the extension name.
     * This is done for compatibility between static and dynamic extensions.
     * For extension readline.so, the init symbol is "Jsi_readlineInit"
     */
    const char *pt;
    const char *pkgname;
    int pkgnamelen, n;
    char initsym[100];
    const char *cp = "";
    Jsi_InitProc *onload = NULL;
    Jsi_HashEntry *hPtr;
    LoadData *d;

    pt = Jsi_Strrchr(pathName, '/');
    if (pt) {
        pkgname = pt + 1;
    }
    else {
        pkgname = pathName;
    }
    pt = Jsi_Strchr(pkgname, '.');
    if (pt) {
        pkgnamelen = pt - pkgname;
    }
    else {
        pkgnamelen = Jsi_Strlen(pkgname);
    }
    for (n = 0; n<6 && onload==NULL; n++)
    {
        char ch;
        if (n==2) { /* Skip lib prefix. */
            if (pkgnamelen<4 || Jsi_Strncasecmp(pkgname,"lib",3))
                break;
            pkgname += 3;
            pkgnamelen -= 3;
        }
        if (n==4) { /* Skip lib prefix. */
            if (pkgnamelen<4 || Jsi_Strncasecmp(pkgname,"jsi",3))
                break;
            pkgname += 3;
            pkgnamelen -= 3;
        }
        ch = ((n%2) ? pkgname[0] : toupper(pkgname[0]));
        snprintf(initsym, sizeof(initsym), "Jsi_Init%c%.*s", ch, pkgnamelen-1, pkgname+1);
        cp = initsym+8;
        hPtr = Jsi_HashEntryFind(interp->loadTbl, cp);
        if (hPtr) { /* Already loaded? */
            d = (LoadData*)Jsi_HashValueGet(hPtr);
            if (d && d->handle)
                return JSI_OK;
        }
        onload = (Jsi_InitProc *)dlsym(handle, initsym);
#ifndef JSI_OMIT_STUBS
        if (onload) {
            /* Handle stubs. TODO: handle other libs like sqliteStubsPtr, websocketStubsPtr, etc... */
            struct Jsi_Stubs **vp = (struct Jsi_Stubs**)dlsym(handle, "jsiStubsPtr");
            if (vp) {
                *vp = jsiStubsTblPtr;
            }
        }
#endif
    }
    
    if (onload == NULL)
        Jsi_LogError("No %s symbol found in extension %s", initsym, pathName);
    else if (onload(interp, 0) != JSI_ERROR) {
        bool isNew;
        hPtr = Jsi_HashEntryNew(interp->loadTbl, cp, &isNew);
        if (hPtr && isNew) {
            d = (LoadData*)Jsi_Calloc(1, sizeof(*d));
            d->handle = handle;
            d->name = (char*)Jsi_HashKeyGet(hPtr);
            d->fpath = pathName;
            d->onload = onload;
            Jsi_HashValueSet(hPtr, d);
            return JSI_OK;
        }
        Jsi_LogError("duplicate load");
    }

    if (handle) {
        dlclose(handle);
    }
    return JSI_ERROR;
}
#else
Jsi_RC jsi_LoadLibrary(Jsi_Interp *interp, const char *pathName)
{
    return Jsi_LogError("load not supported");
}
#endif

Jsi_RC jsi_FreeOneLoadHandle(Jsi_Interp *interp, Jsi_HashEntry *hPtr, void *ptr) {
    LoadData *d = (LoadData*)ptr;
    if (d->onload)
        (*d->onload)(interp, interp==jsiMainInterp?2:1);
#ifndef JSI_OMIT_LOAD
    dlclose(d->handle);
#endif
    Jsi_Free(d);
    return JSI_OK;
}

Jsi_RC Jsi_StubLookup(Jsi_Interp *interp, const char *name, void **ptr)
{
    Jsi_HashEntry *hPtr = Jsi_HashEntryFind(interp->loadTbl, name);
    if (!hPtr)
        return JSI_ERROR;
    LoadData *d = (LoadData*)Jsi_HashValueGet(hPtr);
    if (d==NULL || !d->handle)
        return JSI_ERROR;
    char initsym[100];
    snprintf(initsym, sizeof(initsym), "%sStubsTblPtr", name);
    void **vp = NULL;
#ifndef JSI_OMIT_LOAD
    vp = (void**)dlsym(d->handle, initsym);
#endif
    if (vp == NULL || !*vp)
        return JSI_ERROR;
    *ptr = *vp;
    return JSI_OK;
}

Jsi_RC jsi_LoadLoadCmd(Jsi_Interp *interp, Jsi_Value *args, 
    Jsi_Value *_this, Jsi_Value **ret, Jsi_Func *funcPtr)
{
    Jsi_Value *fval = Jsi_ValueArrayIndex(interp, args, 0);
    char *v = Jsi_Realpath(interp, fval, NULL);
    Jsi_StatBuf fbuf;
    Jsi_RC rc;
    if (v == NULL || Jsi_Stat(interp, fval, &fbuf) || !Jsi_FSNative(interp, fval))
        rc = Jsi_LogError("can not load: %s", (v?v:"null"));
    else
        rc = jsi_LoadLibrary(interp, v);
    if (v)
        Jsi_Free(v);
    return rc;
}

Jsi_RC jsi_LoadUnloadCmd(Jsi_Interp *interp, Jsi_Value *args, 
    Jsi_Value *_this, Jsi_Value **ret, Jsi_Func *funcPtr)
{
    return Jsi_LogError("unload is unimplemented");
}

Jsi_InitProc jsi_InitLoad;

Jsi_RC jsi_InitLoad(Jsi_Interp *interp, int release)
{
    return JSI_OK;
}


#ifdef __WIN32
#undef dlsym
#undef dlclose
#endif

#else
Jsi_RC jsi_LoadLoadCmd(Jsi_Interp *interp, Jsi_Value *args, 
    Jsi_Value *_this, Jsi_Value **ret, Jsi_Func *funcPtr)
{
    Jsi_LogWarn("load is unimplemented");
    return JSI_OK;
}
Jsi_RC jsi_LoadUnloadCmd(Jsi_Interp *interp, Jsi_Value *args, 
    Jsi_Value *_this, Jsi_Value **ret, Jsi_Func *funcPtr)
{
    return Jsi_LogError("unload is unimplemented");
}
#endif
