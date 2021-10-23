/* Jsi commands to access C-data.   http://jsish.org */
#ifndef JSI_LITE_ONLY
#ifndef JSI_AMALGAMATION
#include "jsiInt.h"
#endif
#ifndef JSI_OMIT_CDATA
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <time.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdint.h>
#include <sys/time.h>

#define UdcGet(udf, _this, funcPtr) \
   CDataObj *udf = (typeof(udf))Jsi_UserObjGetData(interp, _this, funcPtr); \
    if (!udf) \
        return Jsi_LogError("CData.%s called with non-CData object", funcPtr->cmdSpec->name);

enum { jsi_CTYP_DYN_MEMORY=(1LL<<32), jsi_CTYP_STRUCT=(1LL<<33), jsi_CTYP_ENUM=(1LL<<34) };

typedef struct {
    JSI_DBDATA_FIELDS  
    Jsi_StructSpec *sl, *keysf;
    Jsi_Map** mapPtr;
    const char *help, *structName, *keyName, *varParam;
    uint flags;
    bool isAlloc;
    Jsi_Map_Type mapType;
    Jsi_Key_Type keyType;
    Jsi_Wide user;
    Jsi_Interp *interp;
    int objId;
    Jsi_Obj *fobj;
} CDataObj;

static Jsi_StructSpec*  jsi_csStructGet(Jsi_Interp *interp, const char *name);
static Jsi_StructSpec*   jsi_csFieldGet(Jsi_Interp *interp, const char *name, Jsi_StructSpec* sl);

static Jsi_EnumSpec*    jsi_csEnumGet(Jsi_Interp *interp, const char *name);
static Jsi_EnumSpec*    jsi_csEnumGetItem(Jsi_Interp *interp, const char *name, Jsi_EnumSpec* sf);
static Jsi_RC     jsi_csStructInit(Jsi_StructSpec* s, uchar* data);
static Jsi_RC CDataOptionsConf(Jsi_Interp *interp, Jsi_OptionSpec *specs,  Jsi_Value *args,
    void *rec, Jsi_Value **ret, int flags, int skipArgs);
static Jsi_RC jsi_csBitGetSet(Jsi_Interp *interp, void *vrec, Jsi_Wide* valPtr, Jsi_OptionSpec *spec, int idx, bool isSet);

static Jsi_StructSpec*  jsi_csStructFields(Jsi_Interp *interp, const char *name) {
    Jsi_StructSpec* sp = jsi_csStructGet(interp, name);
    if (sp)
        return (Jsi_StructSpec*)sp->data;
    return NULL;
}

enum {
    JSI_CS_SIG_NamedData=0xdeaf0100,
    JSI_CS_SIG_Ctx,
};

#define JSI_CS_DSIG(obj) (*(Jsi_Sig*)obj)


/* Traverse hash table and match unique substring. */
Jsi_HashEntry *jsi_csfindInHash(Jsi_Interp *interp, Jsi_Hash * tbl, const char *name)
{
    int len;
    Jsi_HashSearch se;
    Jsi_HashEntry *sentry = 0, *entry = Jsi_HashEntryFind(tbl, name);
    if (entry)
        return entry;
    len = Jsi_Strlen(name);
    entry = Jsi_HashSearchFirst(tbl, &se);
    while (entry) {
        char *ename = (char *) Jsi_HashKeyGet(entry);
        if (!Jsi_Strncmp(name, ename, len)) {
            if (sentry)
                return 0;
            sentry = entry;
        }
        entry = Jsi_HashSearchNext(&se);
    }
    return sentry;
}

/* Traverse enum and match unique substring. */
Jsi_OptionSpec *jsi_csgetEnum(Jsi_Interp *interp, const char *name)
{
    Jsi_HashEntry *entry = jsi_csfindInHash(interp, interp->EnumHash, name);
    return entry ? (Jsi_OptionSpec *) Jsi_HashValueGet(entry) : 0;
}


/************* INITIALIZERS  *********************************************/

/* Init Type hash */
void jsi_csInitType(Jsi_Interp *interp)
{
    if (interp->CTypeHash->numEntries) return;
    bool isNew;
    Jsi_HashEntry *entry;
    const Jsi_OptionType *tl;
    if (!interp->typeInit) {
        int i;
        for (i = JSI_OPTION_BOOL; i!=JSI_OPTION_END; i++) {
            tl = Jsi_OptionTypeInfo((Jsi_OptionId)i);
            entry = Jsi_HashEntryNew(interp->TYPEHash, tl->idName, &isNew);
            if (!isNew)
                Jsi_LogBug("duplicate type: %s", tl->idName);
            Jsi_HashValueSet(entry, (void*)tl);
            if (tl->cName && tl->cName[0])
                Jsi_HashSet(interp->CTypeHash, tl->cName, (void*)tl);
        }
    }
    interp->typeInit = 1;
}

static Jsi_RC jsi_csSetupStruct(Jsi_Interp *interp, Jsi_StructSpec *sl, Jsi_StructSpec *sf, Jsi_OptionType* st) {
    bool isNew;
    int cnt = 0, boffset = 0;
    if (Jsi_HashEntryFind(interp->CTypeHash, sl->name))
        return Jsi_LogError("struct is c-type: %s", sl->name);
    Jsi_HashEntry *entry = Jsi_HashEntryNew(interp->StructHash, sl->name, &isNew);
    if (!isNew)
        return Jsi_LogError("duplicate struct: %s", sl->name);
    Jsi_HashValueSet(entry, sl);
    while (sf && sf->id != JSI_OPTION_END) {
        if (!sf->type)
            sf->type = Jsi_OptionTypeInfo(sf->id);
        if (!sf->type && sf->tname)
            sf->type = Jsi_TypeLookup(interp, sf->tname);
        if (sf->type && sf->type->extData && (sf->type->flags&(jsi_CTYP_ENUM|jsi_CTYP_STRUCT))) {
            Jsi_OptionSpec *es = (typeof(es))sf->type->extData;
            es->value++;
            if ((sf->type->flags&jsi_CTYP_ENUM)) {
                sf->custom = Jsi_Opt_SwitchEnum;
                sf->data = es->extData;
            }
        }
        if (!sf->size) {
            if (!sf->type)
                return Jsi_LogError("unknown id");
            sf->tname = sf->type->cName;
            sf->size = sf->type->size;
            sf->idx = cnt;
            sf->boffset = boffset;
            if (sf->bits) {
                if (sf->bits>64)
                    return Jsi_LogError("bits too large");
                boffset += sf->bits;
                sf->id = JSI_OPTION_CUSTOM;
                sf->custom=Jsi_Opt_SwitchBitfield;
                sf->iniVal.ini_OPT_BITS=&jsi_csBitGetSet;
            } else {
                sf->offset = (boffset+7)/8;
                boffset += sf->type->size*8;
            }
        }
        sf++, cnt++;
    }
    sl->idx = cnt;
    if (!sl->size) 
        sl->size = (boffset+7)/8;
    if (sl->ssig)
        Jsi_HashSet(interp->SigHash, (void*)(uintptr_t)sl->ssig, sl);
    if (!st)
        st = (typeof(st))Jsi_Calloc(1, sizeof(*st));
    st->cName = sl->name;
    st->idName = "CUSTOM";
    st->id = JSI_OPTION_CUSTOM;
    st->size = sl->size;
    st->flags = jsi_CTYP_DYN_MEMORY|jsi_CTYP_STRUCT;
    st->extData = sl;
    Jsi_HashSet(interp->CTypeHash, st->cName, st);
    return JSI_OK;
}

/* Init Struct hash */
static void jsi_csInitStructTables(Jsi_Interp *interp)
{
    Jsi_StructSpec *sf, *sl = interp->statics->structs;
    while (sl  && sl->name) {
        sf = (typeof(sf))sl->data;
        jsi_csSetupStruct(interp, sl, sf, NULL);
        sl++;
    }
}

static Jsi_RC jsi_csSetupEnum(Jsi_Interp *interp, Jsi_EnumSpec *sl, Jsi_EnumSpec *sf, Jsi_OptionType* st) {
    bool isNew;
    int cnt = 0;
    if (Jsi_HashEntryFind(interp->CTypeHash, sl->name))
        return Jsi_LogError("enum is c-type: %s", sl->name);
    Jsi_HashEntry *entry = Jsi_HashEntryNew(interp->EnumHash, sl->name, &isNew);
    if (!isNew)
        return Jsi_LogError("duplicate enum: %s", sl->name);
    Jsi_HashValueSet(entry, sl);
    Jsi_Number val = 0;
    while (sf && sf->id != JSI_OPTION_END) {
        if (!sf->idx) {
            sf->value = val++;
            sf->idx = cnt;
        }
        Jsi_HashSet(interp->EnumItemHash, sf->name, sf);
        sf++, cnt++;
    }
    Jsi_HashSet(interp->EnumHash, sl->name, sl);
    sl->idx = cnt;
    if (!sl->size) 
        sl->size = cnt;
    if (!st)
        st = (typeof(st))Jsi_Calloc(1, sizeof(*st));
    st->cName = sl->name;
    st->idName = "CUSTOM";
    st->id = JSI_OPTION_CUSTOM;
    st->size = sl->size;
    st->flags = jsi_CTYP_DYN_MEMORY|jsi_CTYP_ENUM;
    st->extData = sl;
    Jsi_HashSet(interp->CTypeHash, st->cName, st);
    return JSI_OK;
}

/* Init Enum hash */
void jsi_csInitEnum(Jsi_Interp *interp)
{
    bool isNew;
    Jsi_EnumSpec *sl = interp->statics->enums;
    while (sl && sl->name && sl->id != JSI_OPTION_END) {
        Jsi_HashEntry *entry = Jsi_HashEntryNew(interp->EnumHash, sl->name, &isNew);
        if (!isNew)
            Jsi_LogBug("duplicate enum: %s", sl->name);
        assert(isNew);
        Jsi_HashValueSet(entry, sl);
        sl++;
    }
}

/* Find the enum name in all enums */
Jsi_OptionSpec *jsi_csInitEnumItem(Jsi_Interp *interp)
{
    Jsi_HashEntry *entry;
    bool isNew;
    Jsi_EnumSpec *sl = interp->statics->enums;
    while (sl &&  sl->name && sl->id != JSI_OPTION_END) {
        Jsi_OptionSpec *ei = (typeof(ei))sl->data;
        while (ei->name  && ei->id != JSI_OPTION_END) {
            entry = Jsi_HashEntryNew(interp->EnumItemHash, ei->name, &isNew);
            if (!isNew)
                Jsi_LogBug("duplicate enum item: %s", ei->name);
            Jsi_HashValueSet(entry, ei);
            ei++;
        }
        sl++;
    }
    return 0;
}

Jsi_StructSpec *jsi_csGetStruct(Jsi_Interp *interp, const char *name) {
    return (Jsi_StructSpec *)Jsi_HashGet(interp->StructHash, name, 0);
}

// Format struct key.
static Jsi_Value *jsi_csFmtKeyCmd(Jsi_MapEntry* hPtr, Jsi_MapOpts *opts, int flags)
{
    void *rec = (opts->mapType==JSI_MAP_HASH ? Jsi_HashKeyGet((Jsi_HashEntry*)hPtr): Jsi_TreeKeyGet((Jsi_TreeEntry*)hPtr));
    if (!rec) return NULL;
    CDataObj *cd = (typeof(cd))opts->user;
    assert(cd);
    Jsi_Interp *interp = cd->interp;
    if (!cd->slKey || !cd->slKey)
        return NULL;
    Jsi_Value *v = Jsi_ValueNew1(interp);
    if (Jsi_OptionsConf(interp, (Jsi_OptionSpec*)cd->keysf, rec, NULL, &v, flags) == JSI_OK)
        return v;
    Jsi_DecrRefCount(interp, v);
    return NULL;
}

static Jsi_RC jsi_csCDataNew(Jsi_Interp *interp, const char *name, const char *structName,
    const char *help, const char *varParm) {
    Jsi_DString dStr;
    Jsi_DSInit(&dStr);
    Jsi_DSPrintf(&dStr, "var %s = new CData({structName:\"%s\"", name, structName);
    if (help)
        Jsi_DSPrintf(&dStr, ", help:\"%s\"", help);
    /*if (vd[i].value)
        Jsi_DSPrintf(&dStr, ", arrSize:%u", (uint)vd[i].value);*/
    if (varParm)
        Jsi_DSPrintf(&dStr, ", varParam:\"%s\"", varParm);
    Jsi_DSPrintf(&dStr, "});");
    Jsi_RC rc = Jsi_EvalString(interp, Jsi_DSValue(&dStr), 0);
    Jsi_DSFree(&dStr);
    return rc;
}

Jsi_RC jsi_csInitVarDefs(Jsi_Interp *interp)
{
    Jsi_VarSpec *vd = interp->statics->vars;
    int i;
    for (i=0; vd[i].name; i++) {
        const char *name = vd[i].name;
        const char *structName = vd[i].info;
        const char *help = vd[i].help;
        const char *varParm = (const char*)vd[i].userData;

        if (JSI_OK != jsi_csCDataNew(interp, name, structName, help, varParm))
            return JSI_ERROR;
#if 0        
        Jsi_DString dStr;
        Jsi_DSInit(&dStr);
        Jsi_DSPrintf(&dStr, "var %s = new CData({structName:\"%s\"});", name, structName);
        if (help)
            Jsi_DSPrintf(&dStr, ", help:\"%s\"", help);
        /*if (vd[i].value)
            Jsi_DSPrintf(&dStr, ", arrSize:%u", (uint)vd[i].value);*/
        if (varParm)
            Jsi_DSPrintf(&dStr, ", varParam:\"%s\"", varParm);
        Jsi_DSPrintf(&dStr, "});");
        Jsi_RC rc = Jsi_EvalString(interp, Jsi_DSValue(&dStr), 0);
        Jsi_DSFree(&dStr);
        if (rc != JSI_OK)
            return JSI_ERROR;
#endif
    }
    return JSI_OK;
}

Jsi_StructSpec *jsi_csStructGet(Jsi_Interp *interp, const char *name)
{
    if (!name) return NULL;
    Jsi_StructSpec *sl,*spec = jsi_csGetStruct(interp, name);
    if (spec) return spec;

    Jsi_CData_Static *CData_Strs = interp->statics;
    while (CData_Strs) {
        sl = CData_Strs->structs;
        while (sl->name) {
            if (!Jsi_Strcmp(name, sl->name))
                return sl;
            sl++;
        }
        CData_Strs = CData_Strs->nextPtr;
    }
    return NULL;
}


Jsi_EnumSpec *jsi_csGetEnum(Jsi_Interp *interp, const char *name) {
    return (Jsi_EnumSpec *)Jsi_HashGet(interp->EnumHash, name, 0);
}

/* Traverse enum and match unique substring. */
Jsi_EnumSpec *jsi_csEnumGet(Jsi_Interp *interp, const char *name)
{
    Jsi_EnumSpec *sl, *spec = jsi_csGetEnum(interp, name);
    if (spec) return spec;

    Jsi_CData_Static *CData_Strs = interp->statics;
    while (CData_Strs) {
        sl = CData_Strs->enums;
        while (sl->name) {
            if (!Jsi_Strcmp(name, sl->name))
                return sl;
            sl++;
        }
        CData_Strs = CData_Strs->nextPtr;
    }
    return NULL;
}

/****************************************************************/

/* Traverse from top looking for field in struct, match unique substrings. */
Jsi_StructSpec *jsi_csFieldGet(Jsi_Interp *interp, const char *name, Jsi_StructSpec * sl)
{
    Jsi_StructSpec *sf, *ff = 0, *f = (typeof(f))sl->data;
    int cnt = 0;
    uint len = Jsi_Strlen(name);
    sf = f;
    while (sf->id != JSI_OPTION_END) {
        if (!Jsi_Strncmp(name, sf->name, len)) {
            if (!sf->name[len])
                return sf;
            ff = sf;
            cnt++;
        }
        sf++;
    }
    if (cnt == 1)
        return ff;
    return 0;
}

/* Traverse from top looking for item in enum, match unique substrings. */
Jsi_EnumSpec *jsi_csEnumGetItem(Jsi_Interp *interp, const char *name, Jsi_EnumSpec * el)
{
    Jsi_EnumSpec *ff = 0, *sf;
    int cnt = 0;
    uint len = Jsi_Strlen(name);
    sf = (typeof(sf))el->data;
    if (!sf)
        return 0;
    while (sf->id != JSI_OPTION_END) {
        if (!Jsi_Strncmp(name, sf->name, len)) {
            if (!sf->name[len])
                return sf;
            ff = sf;
            cnt++;
        }
        sf++;
    }
    if (cnt == 1)
        return ff;
    return 0;
}

/******* INIT ***************************************************/


/* Initialize a struct to default values */
static Jsi_RC jsi_csStructInit(Jsi_StructSpec * sl, uchar * data)
{
    /* Jsi_OptionSpec *sf; */
    assert(sl);
    if (!data) {
        fprintf(stderr, "missing data at %s:%d", __FILE__, __LINE__);
        return JSI_ERROR;
    }
    if (sl->custom) {
        memcpy(data, sl->custom, sl->size);
    } else {
        memset(data, 0, sl->size);
    }
    if (sl->ssig)
        *(Jsi_Sig *) data = sl->ssig;
    return JSI_OK;
}

static Jsi_RC CDataEnumNamesCmd(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this,
                               Jsi_Value **ret, Jsi_Func *funcPtr)
{
    
    int argc = Jsi_ValueGetLength(interp, args);
    
    if (argc == 0)
        return Jsi_HashKeysDump(interp, interp->EnumHash, ret, 0);
    char *arg1 = Jsi_ValueString(interp, Jsi_ValueArrayIndex(interp, args, 0), NULL);
    Jsi_EnumSpec *s, *sf;
    if (arg1 == NULL || !(s = (Jsi_EnumSpec*)Jsi_HashGet(interp->EnumHash, arg1, 0)))
        return Jsi_LogError("Unknown enum: %s", arg1);
    Jsi_ValueMakeArrayObject(interp, ret, NULL);
    sf = (typeof(sf))s->data;
    int m = 0;
    while (sf && sf->id != JSI_OPTION_END)
    {
        Jsi_ValueArraySet(interp, *ret, Jsi_ValueNewStringKey(interp, sf->name), m++);
        sf++;
    }
    return JSI_OK;
}

////// ENUM

static Jsi_OptionSpec EnumOptions[] =
{
    JSI_OPT(INT64,      Jsi_EnumSpec, flags,  .help="Flags for enum", jsi_IIOF),
    JSI_OPT(STRKEY,     Jsi_EnumSpec, help,   .help="Description of enum", jsi_IIOF ),
    JSI_OPT(STRKEY,     Jsi_EnumSpec, name,   .help="Name of enum", jsi_IIOF ),
    JSI_OPT(UINT,       Jsi_EnumSpec, idx,    .help="Number of items in enum", jsi_IIRO ),
    JSI_OPT(INT64,      Jsi_EnumSpec, value,  .help="Reference count", jsi_IIRO ),
    JSI_OPT_END(Jsi_EnumSpec, .help="Options for CData enum")
};

static Jsi_OptionSpec EnumFieldOptions[] =
{
    JSI_OPT(INT64,      Jsi_EnumSpec, flags,  .help="Flags for item", jsi_IIOF),
    JSI_OPT(STRKEY,     Jsi_EnumSpec, help,   .help="Desciption of item", jsi_IIOF ),
    JSI_OPT(STRKEY,     Jsi_EnumSpec, name,   .help="Name of item", jsi_IIOF ),
    JSI_OPT(INT64,      Jsi_EnumSpec, value,  .help="Value for item", jsi_IIOF),
    JSI_OPT(UINT,       Jsi_EnumSpec, idx,    .help="Index of item in enum", jsi_IIRO ),
    JSI_OPT_END(Jsi_EnumSpec, .help="Options for CData item")
};

static Jsi_RC CDataEnumFieldConfCmd(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this,
                              Jsi_Value **ret, Jsi_Func *funcPtr)
{
    
    Jsi_EnumSpec *ei, *sf;
    char *arg1 = Jsi_ValueArrayIndexToStr(interp, args, 0, NULL);
    if (!(sf = jsi_csEnumGet(interp, arg1)))
        return Jsi_LogError("unknown enum item: %s", arg1);
    ei = 0;
    char *arg2 = Jsi_ValueArrayIndexToStr(interp, args, 1, NULL);
    if (!(ei = jsi_csEnumGetItem(interp, arg2, sf)))
        return JSI_OK;

    return CDataOptionsConf(interp, EnumFieldOptions, args, ei, ret, 0, 2);
}

static Jsi_RC CDataEnumConfCmd(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this,
                              Jsi_Value **ret, Jsi_Func *funcPtr)
{
    
    Jsi_EnumSpec *sf;
    char *arg1 = Jsi_ValueArrayIndexToStr(interp, args, 0, NULL);
    if (!(sf = jsi_csEnumGet(interp, arg1)))
        return Jsi_LogError("unknown enum: %s", arg1);
    return CDataOptionsConf(interp, EnumOptions, args, sf, ret, 0, 1);
}


static Jsi_RC CDataEnumUndefineCmd(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this,
                              Jsi_Value **ret, Jsi_Func *funcPtr)
{
    
    char *name = Jsi_ValueArrayIndexToStr(interp, args, 0, NULL);
    Jsi_HashEntry *entry = NULL;
    Jsi_OptionType *st = NULL;
    if (name) {
        entry = Jsi_HashEntryFind(interp->EnumHash, name);
        st = Jsi_TypeLookup(interp, name);
    }
    if (!entry || !st)
        return Jsi_LogError("Unknown enum: %s", name);
    Jsi_EnumSpec *sf, *sl = (typeof(sl))Jsi_HashValueGet(entry);
    if (sl->value)
        return Jsi_LogError("Enum in use");
    Jsi_HashEntryDelete(entry);
    sf = (typeof(sf))sl->data;
    while (sf && sf->id != JSI_OPTION_END) {
        entry = Jsi_HashEntryFind(interp->EnumItemHash, name);
        if (entry)
            Jsi_HashEntryDelete(entry);
        sf++;
    }
    entry = Jsi_HashEntryFind(interp->CTypeHash, name);
    if (entry)
        Jsi_HashEntryDelete(entry);
    Jsi_Free(st);
    return JSI_OK;
}

/* Defines: Handles the "CData.enum.define" subcommand */
static Jsi_RC CDataEnumDefineCmd(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this,
                                Jsi_Value **ret, Jsi_Func *funcPtr)
{
    
    uint flen, i;
    jsi_csInitType(interp);
    Jsi_Value *flds = Jsi_ValueArrayIndex(interp, args, 1);
    if (!flds || !Jsi_ValueIsArray(interp,flds) || (flen=Jsi_ValueGetLength(interp, flds))<1)
        return Jsi_LogError("arg 2 must be a non-empty array");
    Jsi_EnumSpec *sl, *sf, recs[flen+1];
    Jsi_Value *val = Jsi_ValueArrayIndex(interp, args, 0);
    memset(recs, 0, sizeof(recs));
    sl = recs+flen;
    if (!val || Jsi_OptionsProcess(interp, EnumOptions, sl, val, 0) < 0)
        return JSI_ERROR;
    if (jsi_csEnumGet(interp, sl->name))
        return Jsi_LogError("enum already exists: %s", sl->name);
    for (i = 0; i<flen; i++) {
        val = Jsi_ValueArrayIndex(interp, flds, i);
        sf = recs+i;
        if (!val || Jsi_OptionsProcess(interp, EnumFieldOptions, sf, val, 0) < 0)
            return JSI_ERROR;
        if (Jsi_HashGet(interp->EnumItemHash, sf->name, 0))
            return Jsi_LogError("duplicate enum item: %s", sf->name);
    }
    Jsi_OptionType *st = (typeof(st))Jsi_Calloc(1, sizeof(*st) + sizeof(char*)*(flen+1)+sizeof(recs));
    sf = (typeof(sf))((uchar*)st + sizeof(*st));
    sl = sf+flen;
    const char **el = (typeof(el))(sl + 1);
    memcpy(sf, recs, sizeof(recs));
    for (i = 0; i<flen; i++)
        el[i] = sf[i].name;
    sl->id = JSI_OPTION_END;
    sl->data = sf;
    sl->extData = el;
    Jsi_RC rc = jsi_csSetupEnum(interp, sl, sf, st);
    if (rc != JSI_OK)
        Jsi_Free(st);
    return rc;
}

static Jsi_RC CDataEnumValueCmd(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this,
                               Jsi_Value **ret, Jsi_Func *funcPtr)
{
    
    const char *arg1, *arg2;
    Jsi_EnumSpec *ei, *el = 0;
    arg1 = Jsi_ValueArrayIndexToStr(interp, args, 0, NULL);
    arg2 = Jsi_ValueArrayIndexToStr(interp, args, 1, NULL);
    if (!(el = jsi_csEnumGet(interp, arg1))) {
        return JSI_OK;
    }
    if (!(ei = jsi_csEnumGetItem(interp, arg2, el)))
        return JSI_OK;
    Jsi_ValueMakeNumber(interp, ret, ei->value);
    return JSI_OK;
}


/* Return the enum symbol that matches the given integer value. */
static Jsi_RC CDataEnumFindCmd(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this,
                                Jsi_Value **ret, Jsi_Func *funcPtr)
{
    
    Jsi_Value *arg2 = Jsi_ValueArrayIndex(interp, args, 1);
    char *arg1 = Jsi_ValueArrayIndexToStr(interp, args, 0, NULL);
    Jsi_EnumSpec *ei, *el;
    if (!(el = jsi_csEnumGet(interp, arg1)))
        return Jsi_LogError("Unknown enum: %s", arg1);
    Jsi_Wide wval;
    if (Jsi_GetWideFromValue(interp, arg2, &wval) != JSI_OK)
        return JSI_ERROR;

    ei = (typeof(ei))el->data;
    while (ei->name) {
        if (wval == (Jsi_Wide)ei->value) {
            Jsi_ValueMakeStringKey(interp, ret, ei->name);
            return JSI_OK;
        }
        ei++;
    }
    return JSI_OK;
}

static Jsi_RC CDataEnumGetDfn(Jsi_Interp *interp, Jsi_EnumSpec * sl, Jsi_DString *dsPtr)
{
    
    Jsi_EnumSpec *sf;
    Jsi_DString eStr = {};

    Jsi_DSAppend(dsPtr, "{ name: \"", sl->name, "\"", NULL);
    if (sl->flags)
        Jsi_DSPrintf(dsPtr, ", flags:%" PRIx64, sl->flags);
    if (sl->help && sl->help[0]) {
        Jsi_DSAppend(dsPtr, ", label:", Jsi_JSONQuote(interp, sl->help, -1, &eStr), NULL);
        Jsi_DSFree(&eStr);
    }
    sf = (typeof(sf))sl->data;
    Jsi_DSAppend(dsPtr, ", fields:[", NULL);
    while (sf->id != JSI_OPTION_END) {
        Jsi_DSPrintf(dsPtr, " { name:\"%s\", value:%" PRIx64, sf->name, (uint64_t)sf->value);
        if (sf->help && sl->help[0]) {
            Jsi_DSAppend(dsPtr, ", label:", Jsi_JSONQuote(interp, sf->help, -1, &eStr), NULL);
            Jsi_DSFree(&eStr);
        }
        Jsi_DSAppend(dsPtr, "}", NULL);
        sf++;
    }
    Jsi_DSAppend(dsPtr, "]}", NULL);
    return JSI_OK;
}

static Jsi_RC CDataEnumGetCmd(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this,
    Jsi_Value **ret, Jsi_Func *funcPtr)
{
    
    char *name = Jsi_ValueArrayIndexToStr(interp, args, 0, NULL);
    Jsi_EnumSpec *sl = jsi_csEnumGet(interp, name);
    if (!sl)
        return JSI_OK;
    JSI_DSTRING_VAR(dsPtr, 400);
    Jsi_RC rc = CDataEnumGetDfn(interp, sl, dsPtr);
    if (JSI_OK == rc)
        rc = Jsi_JSONParse(interp, Jsi_DSValue(dsPtr), ret, 0);
    Jsi_DSFree(dsPtr);
    return rc;
}

static Jsi_CmdSpec enumCmds[] = {
    {"conf",    CDataEnumConfCmd,    1, 2, "enum:string, options:object|string=void",.help="Configure options for enum", .retType=0, .flags=0, .info=0, .opts=EnumOptions},
    {"define",  CDataEnumDefineCmd,  2, 2, "options:object, fields:array", .help="Create an enum: value of items same as in fieldconf", .retType=0, .flags=0, .info=0, .opts=EnumOptions},
    {"fieldconf",CDataEnumFieldConfCmd,2, 3, "enum:string, field:string, options:object|string=void",.help="Configure options for fields", .retType=0, .flags=0, .info=0, .opts=EnumFieldOptions},
    {"find",    CDataEnumFindCmd,    2, 2, "enum:string, intValue:number", .help="Find item with given value in enum", .retType=(uint)JSI_TT_STRING},
    {"get",     CDataEnumGetCmd,     1, 1, "enum:string", .help="Return enum definition", .retType=(uint)JSI_TT_OBJECT},
    {"names",   CDataEnumNamesCmd,   0, 1, "enum:string=void", .help="Return name list of all enums, or items within one enum", .retType=(uint)JSI_TT_ARRAY},
    {"undefine",CDataEnumUndefineCmd,1, 1, "enum:string",.help="Remove an enum", .retType=0, .flags=0, .info=0, .opts=0},
    {"value",   CDataEnumValueCmd,   2, 2, "enum:string, item:string", .help="Return value for given enum item", .retType=(uint)JSI_TT_NUMBER},
    { NULL,   0,0,0,0, .help="Enum commands" }
};

/************************** STRUCT ******************************/

static Jsi_RC CDataStructGetDfn(Jsi_Interp *interp, Jsi_StructSpec * sl, Jsi_DString *dsPtr)
{
    
    Jsi_StructSpec *sf;
    Jsi_DString eStr = {};
    sf = (typeof(sf))sl->data;
    Jsi_DSPrintf(dsPtr, "{ \"name\": \"%s\", \"size\":%d", sl->name, sl->size);
    if (sl->flags)
        Jsi_DSPrintf(dsPtr, ", \"flags\":0x%" PRIx64, sl->flags);
    if (sl->help && sl->help[0]) {
        Jsi_DSAppend(dsPtr, ", \"label\":", Jsi_JSONQuote(interp, sl->help, -1, &eStr), NULL);
        Jsi_DSFree(&eStr);
    }
    if (sl->ssig)
        Jsi_DSPrintf(dsPtr, ", \"sig\":0x%x", sl->ssig);
    Jsi_DSAppend(dsPtr, ", \"fields\":[", NULL);
    while (sf->id != JSI_OPTION_END) {
        Jsi_DSPrintf(dsPtr, " { \"name\":\"%s\",  \"id:\"\"%s\", \"size\":%d, \"bitsize\":%d,"
            "\"offset\":%d, , \"bitoffs\":%d, \"isbit\":%d, \"label\":",
             sf->name, sf->tname, sf->size, sf->bits,
             sf->offset, sf->boffset, sf->flags&JSI_OPT_IS_BITS?1:0 );
        if (sf->help && sl->help[0]) {
            Jsi_DSAppend(dsPtr, Jsi_JSONQuote(interp, sf->help, -1, &eStr), NULL);
            Jsi_DSFree(&eStr);
        } else
            Jsi_DSAppend(dsPtr,"\"\"", NULL);
        Jsi_DSAppend(dsPtr, "}", NULL);
        sf++;
    }
    Jsi_DSAppend(dsPtr, "]}", NULL);
    return JSI_OK;
}

/* Return the structure definition. */
static Jsi_RC CDataStructGetCmd(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this,
                                Jsi_Value **ret, Jsi_Func *funcPtr)
{
    
    char *arg1 = Jsi_ValueArrayIndexToStr(interp, args, 0, NULL);
    Jsi_StructSpec *sl = jsi_csStructGet(interp, arg1);

    if (!sl)
        return Jsi_LogError("unkown struct: %s", arg1);
    Jsi_DString dStr = {};
    Jsi_RC rc = CDataStructGetDfn(interp, sl, &dStr);
    if (JSI_OK == rc)
        rc = Jsi_JSONParse(interp, Jsi_DSValue(&dStr), ret, 0);
    Jsi_DSFree(&dStr);
    return rc;
}

static Jsi_RC CDataStructNamesCmd(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this,
                                 Jsi_Value **ret, Jsi_Func *funcPtr)
{
    
    int argc = Jsi_ValueGetLength(interp, args);

    if (argc == 0)
        return Jsi_HashKeysDump(interp, interp->StructHash, ret, 0);
    char *name = Jsi_ValueArrayIndexToStr(interp, args, 0, NULL);
    Jsi_StructSpec *sf, *sl;
    if (name == NULL || !(sl = jsi_csGetStruct(interp, name)))
        return Jsi_LogError("Unknown struct: %s", name);
    Jsi_ValueMakeArrayObject(interp, ret, NULL);
    sf = (typeof(sf))sl->data;
    int m = 0;
    while (sf && sf->id != JSI_OPTION_END)
    {
        Jsi_ValueArraySet(interp, *ret, Jsi_ValueNewStringKey(interp, sf->name), m++);
        sf++;
    }
    return JSI_OK;
}

static Jsi_RC CDataStructSchemaCmd(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this,
    Jsi_Value **ret, Jsi_Func *funcPtr) {
    char *arg1 = Jsi_ValueArrayIndexToStr(interp, args, 0, NULL);
    Jsi_StructSpec *sf = jsi_csStructFields(interp, arg1);

    if (!sf)
        return Jsi_LogError("unkown struct: %s", arg1);;

    Jsi_DString dStr = {};
    Jsi_OptionsData(interp, (Jsi_OptionSpec*)sf, &dStr, 1);
    Jsi_ValueMakeString(interp, ret, Jsi_DSFreeDup(&dStr));
    Jsi_DSFree(&dStr);
    return JSI_OK;
}

/* Scanning function */
static Jsi_RC jsi_csValueToFieldType(Jsi_Interp *interp, Jsi_OptionSpec* spec, Jsi_Value *inValue, const char *inStr, void *record, Jsi_Wide flags)
{
    if (inStr)
        return JSI_ERROR;
    Jsi_OptionSpec *sp = (typeof(sp))record;

    if (!inStr) {
        if (!inValue || Jsi_ValueIsString(interp, inValue)==0)
            return Jsi_LogError("expected string");
        inStr = Jsi_ValueString(interp, inValue, NULL);
    }
    const Jsi_OptionType* typ = Jsi_TypeLookup(interp, inStr);
    if (!typ)
        return Jsi_LogError("unknown type: %s", inStr);
    sp->id = typ->id;
    sp->type = typ;
    return JSI_OK;
}

/* Printing function. */
static Jsi_RC jsi_csFieldTypeToValue(Jsi_Interp *interp, Jsi_OptionSpec* spec, Jsi_Value **outValue, Jsi_DString *outStr, void *record, Jsi_Wide flags)
{
    if (outStr)
        return JSI_ERROR;
    Jsi_OptionSpec *sp = (typeof(sp))record;
    //const char **s = (const char**)((char*)record + spec->offset);
    const char *s = sp->tname;
    if (outStr)
        Jsi_DSAppend(outStr, s, NULL);
    else if (s)
        Jsi_ValueMakeStringKey(interp, outValue, s);
    else
        Jsi_ValueMakeNull(interp, outValue);
    return JSI_OK;
}

static Jsi_OptionCustom jsi_OptSwitchFieldType = {
    .name="fieldtype", .parseProc=jsi_csValueToFieldType, .formatProc=jsi_csFieldTypeToValue, .freeProc=NULL,
};

static Jsi_OptionSpec StructOptions[] =
{
    JSI_OPT(UINT32, Jsi_StructSpec, crc,     .help="Crc for struct", jsi_IIOF ),
    JSI_OPT(INT64,  Jsi_StructSpec, flags,   .help="Flags for struct", jsi_IIOF ),
    JSI_OPT(STRKEY, Jsi_StructSpec, help,    .help="Struct description", jsi_IIOF ),
    JSI_OPT(UINT32, Jsi_StructSpec, idx,     .help="Number of fields in struct", jsi_IIRO ),
    JSI_OPT(STRKEY, Jsi_StructSpec, name,    .help="Name of struct", jsi_IIOF|JSI_OPT_REQUIRED ),
    JSI_OPT(UINT,   Jsi_StructSpec, size,    .help="Size of struct in bytes", jsi_IIRO ),
    JSI_OPT(UINT32, Jsi_StructSpec, ssig,    .help="Signature for struct", jsi_IIOF),
    JSI_OPT(INT64,  Jsi_StructSpec, value,   .help="Reference count", jsi_IIRO ),
    JSI_OPT_END(Jsi_StructSpec, .help="Options for CData struct create")
};

static Jsi_OptionSpec StructFieldOptions[] =
{
    JSI_OPT(UINT32, Jsi_StructSpec,   bits,   .help="Size of bitfield", jsi_IIOF ),
    JSI_OPT(UINT32, Jsi_StructSpec,   boffset,.help="Bit offset of field within struct", jsi_IIRO ),
    JSI_OPT(INT64,  Jsi_StructSpec,   flags,  .help="Flags for field", jsi_IIOF ),
    JSI_OPT(UINT32, Jsi_StructSpec,   idx,    .help="Index of field in struct", jsi_IIRO ),
    JSI_OPT(STRKEY, Jsi_StructSpec,   help,   .help="Field description", jsi_IIOF ),
    JSI_OPT(STRKEY, Jsi_StructSpec,   info,   .help="Initial value for field", jsi_IIOF ),
    JSI_OPT(STRKEY, Jsi_StructSpec,   name,   .help="Name of field", jsi_IIOF|JSI_OPT_REQUIRED ),
    JSI_OPT(UINT,   Jsi_StructSpec,   offset, .help="Offset of field within struct", jsi_IIRO ),
    JSI_OPT(UINT,   Jsi_StructSpec,   size,   .help="Size of field", jsi_IIOF ),
    JSI_OPT(CUSTOM, Jsi_StructSpec,   type,   .help="Type of field", jsi_IIOF|JSI_OPT_REQUIRED, .custom=&jsi_OptSwitchFieldType,  .data=NULL ),
    JSI_OPT_END(Jsi_StructSpec, .help="Options for CData struct field")
};

static Jsi_RC CDataOptionsConf(Jsi_Interp *interp, Jsi_OptionSpec *specs,  Jsi_Value *args,
    void *rec, Jsi_Value **ret, int flags, int skipArgs)
{
    int argc = Jsi_ValueGetLength(interp, args);
    Jsi_Value *val;
    flags |= JSI_OPTS_IS_UPDATE;

    if (argc == skipArgs)
        return Jsi_OptionsDump(interp, specs, rec, ret, flags);
    val = Jsi_ValueArrayIndex(interp, args, skipArgs);
    Jsi_vtype vtyp = Jsi_ValueTypeGet(val);
    if (vtyp == JSI_VT_STRING) {
        char *str = Jsi_ValueString(interp, val, NULL);
        return Jsi_OptionsGet(interp, specs, rec, str, ret, flags);
    }
    if (vtyp != JSI_VT_OBJECT && vtyp != JSI_VT_NULL)
        return Jsi_LogError("expected string, object, or null");
    if (Jsi_OptionsProcess(interp, specs, rec, val, JSI_OPTS_IS_UPDATE) < 0)
        return JSI_ERROR;
    return JSI_OK;
}


/* Defines: Handles the "CData.struct.conf" subcommand */
static Jsi_RC CDataStructConfCmd(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this,
                                Jsi_Value **ret, Jsi_Func *funcPtr)
{
    
    char *arg1 = Jsi_ValueArrayIndexToStr(interp, args, 0, NULL);
    Jsi_StructSpec *sl = jsi_csStructGet(interp, arg1);
    if (!sl)
        return Jsi_LogError("unknown struct: %s", arg1);
    return CDataOptionsConf(interp, StructOptions, args, sl, ret, 0, 1);
}

static Jsi_RC CDataStructUndefineCmd(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this,
                              Jsi_Value **ret, Jsi_Func *funcPtr)
{
    
    char *name = Jsi_ValueArrayIndexToStr(interp, args, 0, NULL);
    Jsi_HashEntry *entry = NULL;
    if (name)
        entry = Jsi_HashEntryFind(interp->StructHash, name);
    if (!entry)
        return Jsi_LogError("Unknown struct: %s", name);
    Jsi_StructSpec *sl = (typeof(sl))Jsi_HashValueGet(entry);
    if (sl->value)
        return Jsi_LogError("Struct in use");
    Jsi_HashEntryDelete(entry);
    entry = Jsi_HashEntryFind(interp->CTypeHash, name);
    if (entry)
        Jsi_HashEntryDelete(entry);
    return JSI_OK;
}

/* Defines: Handles the "CData.define.create" subcommand */
static Jsi_RC CDataStructDefineCmd(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this,
                                Jsi_Value **ret, Jsi_Func *funcPtr)
{
    
    uint flen, i;
    jsi_csInitType(interp);
    Jsi_Value *flds = Jsi_ValueArrayIndex(interp, args, 1);
    if (!flds || !Jsi_ValueIsArray(interp,flds) || (flen=Jsi_ValueGetLength(interp, flds))<1)
        return Jsi_LogError("arg 2 must be a non-empty array");
    Jsi_StructSpec *sl, *sf, recs[flen+1];
    Jsi_Value *val = Jsi_ValueArrayIndex(interp, args, 0);
    memset(recs, 0, sizeof(recs));
    sl = recs+flen;
    if (!val || Jsi_OptionsProcess(interp, StructOptions, sl, val, 0) < 0)
        return JSI_ERROR;
    if (jsi_csStructGet(interp, sl->name))
        return Jsi_LogError("struct already exists: %s", sl->name);
    for (i = 0; i<flen; i++) {
        val = Jsi_ValueArrayIndex(interp, flds, i);
        sf = recs+i;
        if (!val || Jsi_OptionsProcess(interp, StructFieldOptions, sf, val, 0) < 0)
            return JSI_ERROR;
    }
    Jsi_OptionType *st = (typeof(st))Jsi_Calloc(1, sizeof(*st) + sizeof(recs));
    sf = (typeof(sf))((uchar*)st + sizeof(*st));
    sl = sf+flen;
    memcpy(sf, recs, sizeof(recs));
    sl->id = JSI_OPTION_END;
    sl->data = sf;
    Jsi_RC rc = jsi_csSetupStruct(interp, sl, sf, st);
    if (rc != JSI_OK)
        Jsi_Free(st);
    return rc;
}

/* Defines: Handles the "CData.struct.fieldconf" subcommand */
static Jsi_RC CDataStructFieldConfCmd(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this,
                                Jsi_Value **ret, Jsi_Func *funcPtr)
{
    
    char *arg1 = Jsi_ValueArrayIndexToStr(interp, args, 0, NULL);
    Jsi_StructSpec *sf, *sl = jsi_csStructGet(interp, arg1);
    if (!sl)
        return Jsi_LogError("unknown struct: %s", arg1);
    char *arg2 = Jsi_ValueArrayIndexToStr(interp, args, 1, NULL);
    if (!arg2 || !(sf = jsi_csFieldGet(interp, arg2, sl)))
        return Jsi_LogError("unknown field: %s", arg2);
    return CDataOptionsConf(interp, StructFieldOptions, args, sf, ret, 0, 2);
}

static Jsi_CmdSpec structCmds[] =
{
    {"conf",      CDataStructConfCmd,   1, 2, "struct:string, options:object|string=void", .help="Configure options for struct", .retType=0, .flags=0, .info=0, .opts=StructOptions},
    {"define",    CDataStructDefineCmd, 2, 2, "options:object, fields:array", .help="Create a struct: field values same as in fieldconf", .retType=0, .flags=0, .info=0, .opts=StructOptions},
    {"fieldconf", CDataStructFieldConfCmd,2,3,"struct:string, field:string, options:object|string=void", .help="Configure options for fields", .retType=0, .flags=0, .info=0, .opts=StructFieldOptions},
    {"get",       CDataStructGetCmd,    1, 2, "struct, options:object=void", .help="Return the struct definition", .retType=(uint)JSI_TT_OBJECT},
    {"names",     CDataStructNamesCmd,  0, 1, "struct:string=void", .help="Return name list of all structs, or fields for one struct", .retType=(uint)JSI_TT_ARRAY},
    {"schema",    CDataStructSchemaCmd, 1, 1, "", .help="Return database schema for struct", .retType=(uint)JSI_TT_STRING },
    {"undefine",  CDataStructUndefineCmd,1, 1, "name:string",.help="Remove a struct", .retType=0, .flags=0, .info=0, .opts=0},
    {NULL}
};



static Jsi_RC jsi_csGetKey(Jsi_Interp *interp, CDataObj *cd, Jsi_Value *arg, void **kPtr, size_t ksize, int anum)
{
    void *kBuf = *kPtr;
    *kPtr = NULL;
    if (!arg)
        return JSI_OK;
    Jsi_Number nval = 0;
    switch (cd->keyType) {
        case JSI_KEYS_STRING:
        case JSI_KEYS_STRINGKEY:
            *kPtr = (void*)Jsi_ValueString(interp, arg, NULL);
            if (!*kPtr)
                return Jsi_LogError("arg %d: expected string key", anum);
            break;
        case JSI_KEYS_ONEWORD:
            if (Jsi_ValueGetNumber(interp, arg, &nval) != JSI_OK)
                return Jsi_LogError("arg %d: expected number key", anum);
            *kPtr = (void*)(uintptr_t)nval;
            break;
        default: {
            if (!cd->slKey) {
badkey:
                return Jsi_LogError("arg %d: expected struct key", anum);
            }
            if (arg->vt == JSI_VT_OBJECT && arg->d.obj->ot == JSI_OT_OBJECT) {
                if (cd->slKey->size>ksize || !kBuf)
                    goto badkey;
                memset(kBuf, 0, cd->slKey->size);
                if (Jsi_OptionsConf(interp, (Jsi_OptionSpec*)cd->keysf, kBuf, arg, NULL, 0) != JSI_OK)
                    return JSI_ERROR;
                *kPtr = kBuf;
            } else
                return Jsi_LogError("arg %d: expected object key", anum);
        }
    }
    return JSI_OK;
}

#define FN_dataGetN JSI_INFO("If given a name argument, gets data for the named field. \
Otherwise gets data for all fields in struct.")
static Jsi_RC CDataGetCmd(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this,
                             Jsi_Value **ret, Jsi_Func *funcPtr)
{
    UdcGet(cd, _this, funcPtr);
    uchar *dptr = NULL;
    Jsi_Value *karg = Jsi_ValueArrayIndex(interp, args, 0);
    char kbuf[BUFSIZ];
    void *key = kbuf;
    bool isNull = Jsi_ValueIsNull(interp, karg);
    if (isNull) {
        if (cd->mapPtr || cd->arrSize>1)
            return Jsi_LogError("null key used with c-array/map");
    } else {
        if (!cd->mapPtr && cd->arrSize<=0)
            return Jsi_LogError("must be array/map");
        if (JSI_OK != jsi_csGetKey(interp, cd, karg, &key, sizeof(kbuf), 1))
        return JSI_ERROR;
    }

    dptr = (uchar*)cd->data;
    if (isNull) {
    } else if (cd->mapPtr) {
        Jsi_MapEntry *mPtr = Jsi_MapEntryFind(*cd->mapPtr, key);
        if (mPtr)
            dptr = (uchar*)Jsi_MapValueGet(mPtr);
        else
            return Jsi_LogError("arg 1: key not found [%s]", Jsi_ValueToString(interp, karg, NULL));
    } /*else if (!cd->arrSize)
        return Jsi_LogError("arg 2: expected a c-array or map");*/
    else {
        uint kind = (intptr_t)key;
        if (kind>=cd->arrSize)
            return Jsi_LogError("array index out of bounds: %d not in 0,%d", kind, cd->arrSize-1);

        dptr = ((uchar*)cd->data) + cd->sl->size*kind;
        if (cd->isPtrs)
            dptr = ((uchar*)cd->data) + sizeof(void*)*kind;
        else if (cd->isPtr2) {
            dptr = (uchar*)(*(void**)dptr);
            dptr += sizeof(void*)*kind;
        }
    }
    int argc = Jsi_ValueGetLength(interp, args);
    if (argc != 2 && argc != 1)
        return Jsi_LogError("expected 1 or 2 args");
    Jsi_Value *arg2 = Jsi_ValueArrayIndex(interp, args, 1);
    return Jsi_OptionsConf(interp, (Jsi_OptionSpec*)cd->sf, dptr, arg2, ret, 0);
}

#define FN_dataSet JSI_INFO("Sets data value for given a name argument.")
static Jsi_RC CDataSetCmd(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this,
                             Jsi_Value **ret, Jsi_Func *funcPtr)
{
    UdcGet(cd, _this, funcPtr);
    uchar *dptr = NULL;
    Jsi_Value *arg = Jsi_ValueArrayIndex(interp, args, 0);
    char kbuf[BUFSIZ];
    void *key = kbuf;
    bool isNull = Jsi_ValueIsNull(interp, arg);
    if (isNull) {
        if (cd->mapPtr || cd->arrSize)
            return Jsi_LogError("null key used with c-array/map");
    } else {
        if (!cd->mapPtr && cd->arrSize<=0)
            return Jsi_LogError("must be array/map");
        if (JSI_OK != jsi_csGetKey(interp, cd, arg, &key, sizeof(kbuf), 1))
            return JSI_ERROR;
    }

    dptr = (uchar*)cd->data;
    if (isNull) {
    } else if (cd->mapPtr) {
        Jsi_MapEntry *mPtr = Jsi_MapEntryFind(*cd->mapPtr, key);
        if (mPtr)
            dptr = (uchar*)Jsi_MapValueGet(mPtr);
        else {
            bool isNew;
            if (cd->maxSize && Jsi_MapSize(*cd->mapPtr)>=cd->maxSize)
                return Jsi_LogError("map would exceeded maxSize: %d", cd->maxSize);
            if (!cd->noAuto)
                mPtr = Jsi_MapEntryNew(*cd->mapPtr, key, &isNew);
            if (!mPtr)
                return Jsi_LogError("arg 1: key not found [%s]", Jsi_ValueToString(interp, arg, NULL));
            Jsi_StructSpec *sl = cd->sl;
            dptr = (uchar*)Jsi_Calloc(1, sl->size);
            Jsi_MapValueSet(mPtr, dptr);
            jsi_csStructInit(sl, dptr);
        }
    } else if (!cd->arrSize)
        return Jsi_LogError("expected a c-array or map");
    else {
        uint kind = (uintptr_t)key;
        if (kind>=cd->arrSize)
            return Jsi_LogError("array index out of bounds: %d not in 0,%d", kind, cd->arrSize-1);
        dptr = ((uchar*)cd->data) + cd->sl->size*kind;
        if (cd->isPtrs)
            dptr = ((uchar*)cd->data) + sizeof(void*)*kind;
        else if (cd->isPtr2)
            dptr = (uchar*)(*(void**)dptr) + sizeof(void*)*kind;
    }
    int argc = Jsi_ValueGetLength(interp, args);
    Jsi_Value *arg2 = Jsi_ValueArrayIndex(interp, args, 1);
    if (argc == 2) {
        if (!Jsi_ValueIsObjType(interp, arg2, JSI_OT_OBJECT))
            return Jsi_LogError("arg 3: last must be an object with 3 args");
        return Jsi_OptionsConf(interp, (Jsi_OptionSpec*)cd->sf, dptr, arg2, ret, 0);
    } else if (argc != 3)
        return Jsi_LogError("expected 2 or 3 args");
    const char *cp;
    if (!(cp = Jsi_ValueString(interp, arg2, NULL)))
        return Jsi_LogError("with 3 args, string expected for arg 3");
    Jsi_Value *arg3 = Jsi_ValueArrayIndex(interp, args, 2);
    return Jsi_OptionsSet(interp, (Jsi_OptionSpec*)cd->sf, dptr, cp, arg3, 0);
}

static Jsi_RC CDataInfoCmd(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this,
                              Jsi_Value **ret, Jsi_Func *funcPtr)
{
    UdcGet(cd, _this, funcPtr);
    Jsi_StructSpec *sl = cd->sl;
    Jsi_DString dStr= {};
    const char *sptr = Jsi_DSPrintf(&dStr, "{struct:\"%s\", label:\"%s\"}", sl->name, cd->help?cd->help:"");
    Jsi_RC rc = JSI_ERROR;
    if (!sptr)
        return Jsi_LogError("format failed");
    else
        rc = Jsi_JSONParse(interp, sptr, ret, 0);
    Jsi_DSFree(&dStr);
    if (rc != JSI_OK)
        return rc;
    Jsi_Obj *sobj;
    Jsi_Value *svalue;
    if (cd->sf) {
        sobj = Jsi_ObjNewType(interp, JSI_OT_ARRAY);
        svalue = Jsi_ValueMakeObject(interp, NULL, sobj);
        jsi_DumpOptionSpecs(interp, sobj,(Jsi_OptionSpec*) cd->sf);
        sobj = (*ret)->d.obj;
        Jsi_ObjInsert(interp, sobj, "spec", svalue, 0);
    }
    if (cd->slKey) {
        sobj = Jsi_ObjNewType(interp, JSI_OT_ARRAY);
        svalue = Jsi_ValueMakeObject(interp, NULL, sobj);
        jsi_DumpOptionSpecs(interp, sobj, (Jsi_OptionSpec*)cd->slKey);
        sobj = (*ret)->d.obj;
        Jsi_ObjInsert(interp, sobj, "keySpec", svalue, 0);
    }    return JSI_OK;
}


const char *csMapTypeStrs[] = { "none", "hash", "tree",  "list", NULL };
const char *csKeyTypeStrs[] = { "string", "strkey", "number",  NULL };

static Jsi_OptionSpec CDataOptions[] = {
    JSI_OPT(UINT,     CDataObj, arrSize, .help="If an array, its size in elements", jsi_IIOF ),
    JSI_OPT(UINT,     CDataObj, flags,   .help="Flags", jsi_IIOF|JSI_OPT_FMT_HEX ),
    JSI_OPT(STRKEY,   CDataObj, help,    .help="Description of data", jsi_IIOF ),
    JSI_OPT(STRKEY,   CDataObj, keyName, .help="Key struct, for key struct maps", jsi_IIOF ),
    JSI_OPT(CUSTOM,   CDataObj, keyType, .help="Key id", jsi_IIOF|JSI_OPT_COERCE, .custom=Jsi_Opt_SwitchEnum, .data=csKeyTypeStrs),
    JSI_OPT(CUSTOM,   CDataObj, mapType, .help="If a map, its type", jsi_IIOF, .custom=Jsi_Opt_SwitchEnum, .data=csMapTypeStrs),
    JSI_OPT(UINT,     CDataObj, maxSize, .help="Limit the array size or number of keys in a map" ),
//    JSI_OPT(STRKEY,   CDataObj, name,    .help="Name of data", jsi_IIOF|JSI_OPT_REQUIRED ),
    JSI_OPT(BOOL,     CDataObj, noAuto,  .help="Disable automatic key-add to map in 'setN'"),
    JSI_OPT(STRKEY,   CDataObj, structName,  .help="Struct used for storing data", jsi_IIOF|JSI_OPT_REQUIRED ),
    JSI_OPT(INT64,    CDataObj, user,    .help="User data" ),
    JSI_OPT(STRKEY,   CDataObj, varParam,.help="Param for maps/array vars", jsi_IIOF ),
    JSI_OPT_END(CDataObj, .help="Options for CData named")
};


static bool jsi_csBitSetGet(int isSet, uchar *tbuf, int bits, Jsi_UWide *valPtr) {
    union bitfield *bms = (union bitfield *)tbuf;
    Jsi_UWide val = *valPtr;
    union bitfield {
        Jsi_UWide b1:1; Jsi_UWide b2:2; Jsi_UWide b3:3; Jsi_UWide b4:4; Jsi_UWide b5:5; Jsi_UWide b6:6;
        Jsi_UWide b7:7; Jsi_UWide b8:8; Jsi_UWide b9:9; Jsi_UWide b10:10; Jsi_UWide b11:11; Jsi_UWide b12:12;
        Jsi_UWide b13:13; Jsi_UWide b14:14; Jsi_UWide b15:15; Jsi_UWide b16:16; Jsi_UWide b17:17; 
        Jsi_UWide b18:18; Jsi_UWide b19:19; Jsi_UWide b20:20; Jsi_UWide b21:21; Jsi_UWide b22:22;
        Jsi_UWide b23:23; Jsi_UWide b24:24; Jsi_UWide b25:25; Jsi_UWide b26:26; Jsi_UWide b27:27;
        Jsi_UWide b28:28; Jsi_UWide b29:29; Jsi_UWide b30:30; Jsi_UWide b31:31; Jsi_UWide b32:32;
        Jsi_UWide b33:33; Jsi_UWide b34:34; Jsi_UWide b35:35; Jsi_UWide b36:36; Jsi_UWide b37:37;
        Jsi_UWide b38:38; Jsi_UWide b39:39; Jsi_UWide b40:40; Jsi_UWide b41:41; Jsi_UWide b42:42;
        Jsi_UWide b43:43; Jsi_UWide b44:44; Jsi_UWide b45:45; Jsi_UWide b46:46; Jsi_UWide b47:47;
        Jsi_UWide b48:48; Jsi_UWide b49:49; Jsi_UWide b50:50; Jsi_UWide b51:51; Jsi_UWide b52:52;
        Jsi_UWide b53:53; Jsi_UWide b54:54; Jsi_UWide b55:55; Jsi_UWide b56:56; Jsi_UWide b57:57;
        Jsi_UWide b58:58; Jsi_UWide b59:59; Jsi_UWide b60:60; Jsi_UWide b61:61; Jsi_UWide b62:62;
        Jsi_UWide b63:63; Jsi_UWide b64:64;
    };
    if (isSet) {
        switch (bits) {
    #define CBSN(n) \
            case n: bms->b##n = val; return (bms->b##n == val)
           CBSN(1); CBSN(2); CBSN(3); CBSN(4); CBSN(5); CBSN(6); CBSN(7); CBSN(8);
           CBSN(9); CBSN(10); CBSN(11); CBSN(12); CBSN(13); CBSN(14); CBSN(15); CBSN(16);
           CBSN(17); CBSN(18); CBSN(19); CBSN(20); CBSN(21); CBSN(22); CBSN(23); CBSN(24);
           CBSN(25); CBSN(26); CBSN(27); CBSN(28); CBSN(29); CBSN(30); CBSN(31); CBSN(32);
           CBSN(33); CBSN(34); CBSN(35); CBSN(36); CBSN(37); CBSN(38); CBSN(39); CBSN(40);
           CBSN(41); CBSN(42); CBSN(43); CBSN(44); CBSN(45); CBSN(46); CBSN(47); CBSN(48);
           CBSN(49); CBSN(50); CBSN(51); CBSN(52); CBSN(53); CBSN(54); CBSN(55); CBSN(56);
           CBSN(57); CBSN(58); CBSN(59); CBSN(60); CBSN(61); CBSN(62); CBSN(63); CBSN(64);
        }
        assert(0);
    }
    switch (bits) {
#define CBGN(n) \
        case n: val = bms->b##n; break
       CBGN(1); CBGN(2); CBGN(3); CBGN(4); CBGN(5); CBGN(6); CBGN(7); CBGN(8);
       CBGN(9); CBGN(10); CBGN(11); CBGN(12); CBGN(13); CBGN(14); CBGN(15); CBGN(16);
       CBGN(17); CBGN(18); CBGN(19); CBGN(20); CBGN(21); CBGN(22); CBGN(23); CBGN(24);
       CBGN(25); CBGN(26); CBGN(27); CBGN(28); CBGN(29); CBGN(30); CBGN(31); CBGN(32);
       CBGN(33); CBGN(34); CBGN(35); CBGN(36); CBGN(37); CBGN(38); CBGN(39); CBGN(40);
       CBGN(41); CBGN(42); CBGN(43); CBGN(44); CBGN(45); CBGN(46); CBGN(47); CBGN(48);
       CBGN(49); CBGN(50); CBGN(51); CBGN(52); CBGN(53); CBGN(54); CBGN(55); CBGN(56);
       CBGN(57); CBGN(58); CBGN(59); CBGN(60); CBGN(61); CBGN(62); CBGN(63); CBGN(64);
       default: assert(0);
    }
    *valPtr = val;
    return 1;
}

static Jsi_RC jsi_csBitGetSet(Jsi_Interp *interp, void *vrec,  Jsi_Wide* vPtr, Jsi_OptionSpec *spec, int idx, bool isSet) {
    Jsi_UWide *valPtr = (typeof(valPtr))vPtr;
    int bits = spec->bits;
    int boffs = spec->boffset;
    if (bits<1 || bits>64) return JSI_ERROR;
    int ofs = (boffs/8);
    int bo = (boffs%8); // 0 if byte-aligned
    int Bsz = ((bits+bo+7)/8);
    uchar *rec = (uchar*)vrec;
#ifdef __SIZEOF_INT128__
    typedef unsigned __int128 utvalType;
#else
    typedef Jsi_UWide utvalType;
#endif
    utvalType tbuf[2] = {};
    uchar sbuf[20], *bptr = (uchar*)tbuf;
    memcpy(tbuf, rec+ofs, Bsz);
    Jsi_UWide mval;
    Jsi_UWide amask = ((1LL<<(bits-1))-1LL);
    utvalType tval = 0, kval = 0, lmask;
    if (bo) { // If not byte aligned, get tval and shift
        bptr = sbuf;
        kval = tval = *(typeof(tval)*)tbuf;
        tval >>= bo;
        mval = (Jsi_UWide)tval;
        *(Jsi_UWide*)bptr = mval;
    } else
         mval = *valPtr;
        
    if (!isSet) { // Get value.
        if (!jsi_csBitSetGet(0, bptr, bits, &mval))
            return JSI_ERROR;
        *valPtr = mval;
        return JSI_OK;
    }
    
    if (!jsi_csBitSetGet(1, bptr, bits, &mval))
        return JSI_ERROR;
    if (bo) {
        tval = (typeof(tval))mval;
        lmask=(amask<<bo);
        kval &= ~lmask;
        tval <<= bo;
        tval = (kval | tval);
        *(typeof(tval)*)tbuf = tval;
    }
    memcpy(rec+ofs, tbuf, Bsz);

    return JSI_OK;    
}

static Jsi_RC CDataConfCmd(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this,
                              Jsi_Value **ret, Jsi_Func *funcPtr)
{
    UdcGet(cd, _this, funcPtr);
    return CDataOptionsConf(interp, CDataOptions, args, cd, ret, 0, 0);
}
 
static Jsi_RC CDataNamesCmd(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this,
    Jsi_Value **ret, Jsi_Func *funcPtr) {
    UdcGet(cd, _this, funcPtr);
    if (cd->mapType != JSI_MAP_NONE)
        return Jsi_MapKeysDump(interp, *cd->mapPtr, ret, 0);
    return Jsi_LogError("not a map");;
}

static Jsi_RC CDataUnsetCmd(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this,
                              Jsi_Value **ret, Jsi_Func *funcPtr)
{
    UdcGet(cd, _this, funcPtr);
    Jsi_Value *arg2 = Jsi_ValueArrayIndex(interp, args, 1);   
    char kbuf[BUFSIZ];
    void *key = kbuf;
    if (JSI_OK != jsi_csGetKey(interp, cd, arg2, &key, sizeof(kbuf), 2)) {
        return JSI_ERROR;
    }
    uchar *dptr = NULL;
    Jsi_MapEntry *mPtr = Jsi_MapEntryFind(*cd->mapPtr, key);
    if (mPtr)
        dptr = (uchar*)Jsi_MapValueGet(mPtr);
    if (!dptr) {
        if (cd->keyType != JSI_KEYS_ONEWORD)
            return Jsi_LogError("no data in map: %s", (char*)key);
        else
            return Jsi_LogError("no data in map: %p", key);
        return JSI_ERROR;
    }
    Jsi_Free(dptr);
    Jsi_MapEntryDelete(mPtr);
    return JSI_OK;

}

static Jsi_RC CDataConstructor(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this,
    Jsi_Value **ret, Jsi_Func *funcPtr);
    
/* Defines: Handles the "Data" subcommand */
static Jsi_CmdSpec cdataCmds[] =
{
    {"CData",     CDataConstructor,1, 1, "options:object=void",.help="Create new map/array of structs", .retType=(uint)JSI_TT_USEROBJ, .flags=JSI_CMD_IS_CONSTRUCTOR, .info=0, .opts=CDataOptions},
    {"conf",      CDataConfCmd,    0, 1, "options:object|string=void",.help="Configure options for c-data", .retType=0, .flags=0, .info=0, .opts=CDataOptions},
    {"info",      CDataInfoCmd,    0, 0, "", .help="Return info for data", .retType=(uint)JSI_TT_OBJECT},
    {"names",     CDataNamesCmd,   0, 0, "", .help="Return keys for map", .retType=(uint)JSI_TT_ARRAY },
    {"get"   ,    CDataGetCmd,     1, 2, "key:string|number|object|null, field:string=void", .help="Get nth value of map/array", .retType=(uint)JSI_TT_ANY},
    {"set",       CDataSetCmd,     2, 3, "key:string|number|object|null, field:object|string, value:any=void", .help="Set nth value of map/array", .retType=(uint)JSI_TT_ANY},
    {"unset",     CDataUnsetCmd,   0, 1, "key:string|number|object=void", .help="Remove entry from map/array", .retType=(uint)JSI_TT_ANY},
    {NULL}
};


static Jsi_OptionSpec TypeOptions[] = {
    JSI_OPT(STRKEY,   Jsi_OptionType, idName,  .help="The id name: usually upcased cName", jsi_IIOF ),
    JSI_OPT(STRKEY,   Jsi_OptionType, cName,   .help="C type name", jsi_IIOF ),
    JSI_OPT(STRKEY,   Jsi_OptionType, help,    .help="Description of id", jsi_IIOF ),
    JSI_OPT(STRKEY,   Jsi_OptionType, fmt,     .help="Printf format for id", jsi_IIOF ),
    JSI_OPT(STRKEY,   Jsi_OptionType, xfmt,    .help="Hex printf format for id", jsi_IIOF ),
    JSI_OPT(INT64,    Jsi_OptionType, flags,   .help="Flags for id", jsi_IIOF ),
    JSI_OPT(INT,      Jsi_OptionType, size,    .help="Size for id", jsi_IIOF ),
    JSI_OPT(INT64,    Jsi_OptionType, user,    .help="User data" ),
    JSI_OPT_END(Jsi_OptionType, .help="Options for CData id")
};

static Jsi_RC CDataTypeConfCmd(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this,
                              Jsi_Value **ret, Jsi_Func *funcPtr)
{
    
    char *arg1 = Jsi_ValueArrayIndexToStr(interp, args, 0, NULL);
    Jsi_OptionType *nd = NULL;
    jsi_csInitType(interp);
    if (arg1)
        nd = (typeof(nd))Jsi_TypeLookup(interp, arg1);
    if (!nd)
        return Jsi_LogError("Unknown type: %s", arg1);
    return CDataOptionsConf(interp, TypeOptions, args, nd, ret, 0, 1);
}

static Jsi_RC CDataTypeNamesCmd(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this,
                              Jsi_Value **ret, Jsi_Func *funcPtr)
{
    
    jsi_csInitType(interp);
    int argc = Jsi_ValueGetLength(interp, args);
    return Jsi_HashKeysDump(interp, (argc?interp->CTypeHash:interp->TYPEHash), ret, 0);
}

/* Defines: Handles the "Type" subcommand */
static Jsi_CmdSpec typeCmds[] =
{
    //{"aliases",   CDataTypeAliasCmd,   0, 0, "", .help="Return type aliases", .retType=(uint)JSI_TT_ARRAY},
    {"conf",      CDataTypeConfCmd,    1, 2, "typ:string, options:object|string=void",.help="Configure options for type", .retType=0, .flags=0, .info=0, .opts=TypeOptions},
    {"names",     CDataTypeNamesCmd,   0, 1, "ctype=false", .help="Return type names", .retType=(uint)JSI_TT_ARRAY},
    {NULL}
};

static Jsi_RC jsi_csTypeFree(Jsi_Interp *interp, Jsi_HashEntry *hPtr, void *ptr) {
    if (!ptr) return JSI_OK;
    Jsi_OptionType *type = (typeof(type))ptr;
    if (type && type->extData && (type->flags&(jsi_CTYP_ENUM|jsi_CTYP_STRUCT)))
        ((Jsi_OptionSpec*)type->extData)->value++;
    if (type->flags&jsi_CTYP_DYN_MEMORY)
        Jsi_Free(ptr);
    return JSI_OK;
}

static Jsi_RC jsi_csMapFree(Jsi_Interp *interp, Jsi_MapEntry *hPtr, void *ptr) {
    if (!ptr) return JSI_OK;
    Jsi_Free(ptr);
    return JSI_OK;
}

static Jsi_RC jsi_csNewCData(Jsi_Interp *interp, CDataObj *cd, int flags) {

    Jsi_StructSpec *slKey = NULL, *keySpec = NULL, *sf = cd->sf, *sl = cd->sl;
    
    if (!sf)
        cd->sf = sf = jsi_csStructFields(interp, cd->structName);
    if (!sl)
        sl = cd->sl = jsi_csGetStruct(interp, cd->structName);
    if (!sf)
        return Jsi_LogError("missing struct/fields: %s", cd->structName);
    
    if (cd->keyName) {
        slKey = keySpec = jsi_csGetStruct(interp, cd->keyName);
        if (slKey == NULL)
            return Jsi_LogError("unknown key struct: %s", cd->keyName);
    }

    const char *vparm = cd->varParam;
    if (vparm && vparm[0]) {
        char parm[200] = {}, *parms=parm, *ep;
        int plen = Jsi_Strlen(vparm);
        if (plen>=2 && vparm[0] == '[' && vparm[plen-1]==']') {
            snprintf(parm, sizeof(parm), "%.*s", plen-2, vparm+1);
            int sz = 0;
            if (parm[0] && isdigit(parm[0])) {
                sz=strtoul(parm, &ep, 0);
                if (*ep || sz<=0)
                    return Jsi_LogError("bad array size: %s", vparm);
                cd->arrSize = sz;
            } else {
                Jsi_EnumSpec *ei = (typeof(ei))Jsi_HashGet(interp->EnumItemHash, parm, 0);
                if (!ei || (sz=ei->value)<=0)
                    return Jsi_LogError("bad array enum: %s", vparm);
            }
            
        } else if (plen>=2 && vparm[0] == '{' && vparm[plen-1]=='}') {
            snprintf(parm, sizeof(parm), "%.*s", plen-2, vparm+1);
            if (parms[0]) {
                const char *ktn = NULL;
                if (*parms == '#') {
                     cd->mapType = JSI_MAP_HASH;
                     parms++;
                }
                if (*parms == '0') {
                    cd->keyType = JSI_KEYS_ONEWORD;
                    if (parms[1])
                        return Jsi_LogError("Trailing junk: %s", vparm);
                } else if (parms[0] == '@') {
                    slKey = jsi_csGetStruct(interp, ktn=(parms+1));
                    if (!slKey)
                        return Jsi_LogError("unknown key struct: %s", ktn);
                    cd->keyName = slKey->name;
                } else if (parms[0])
                        return Jsi_LogError("Trailing junk: %s", vparm);
            }

        } else
            return Jsi_LogError("expected either {} or []: %s", vparm);
    
    }
    cd->sl->value++;
 
    if (cd->keyName) {
        cd->slKey = jsi_csGetStruct(interp, cd->keyName);
        if (!cd->slKey)
            return Jsi_LogError("unknown key struct: %s", cd->keyName);
        cd->keysf = jsi_csStructFields(interp, cd->keyName);
        cd->keyType = (Jsi_Key_Type)slKey->size;
        cd->slKey->value++;
    }
    
    if (cd->mapType != JSI_MAP_NONE) {
        cd->data = (typeof(cd->data))Jsi_MapNew(interp, cd->mapType, cd->keyType, jsi_csMapFree);
        cd->mapPtr = (Jsi_Map**)&cd->data;
        cd->isAlloc = 1;
        if (cd->slKey) {
            Jsi_MapOpts mo;
            Jsi_MapConf(*cd->mapPtr, &mo, 0);
            mo.fmtKeyProc = jsi_csFmtKeyCmd;
            mo.user = (void*)cd;
            Jsi_MapConf(*cd->mapPtr, &mo, 1);
        }
    } else {
        uint sz = (!cd->arrSize ? 1 : cd->arrSize);
        cd->keyType = JSI_KEYS_ONEWORD;
        cd->isAlloc = 1;
        cd->data = (typeof(cd->data))Jsi_Calloc(sz, cd->sl->size);
    }

    return JSI_OK;
}

static Jsi_RC jsi_csObjFree(Jsi_Interp *interp, void *data)
{
    CDataObj *cd = (CDataObj *)data;
    cd->sl->value--;
    if (cd->slKey)
        cd->slKey->value--;
    if (cd->isAlloc) {
        if (cd->mapPtr && *cd->mapPtr) Jsi_MapDelete(*cd->mapPtr);
        else if (cd->data) Jsi_Free(cd->data);
    }
    Jsi_Free(cd);
    return JSI_OK;
}

static bool jsi_csObjIsTrue(void *data)
{
    CDataObj *fo = (CDataObj *)data;
    if (!fo->fobj) return JSI_OK;
    else return 1;
}

static bool jsi_csObjEqual(void *data1, void *data2)
{
    return (data1 == data2);
}

static Jsi_UserObjReg cdataobject = {
    "CData",
    cdataCmds,
    jsi_csObjFree,
    jsi_csObjIsTrue,
    jsi_csObjEqual
};

static Jsi_RC CDataConstructor(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this,
    Jsi_Value **ret, Jsi_Func *funcPtr)
{
    Jsi_Value *that = _this;
    Jsi_Obj *nobj;
    
    if (!Jsi_FunctionIsConstructor(funcPtr)) {
        Jsi_Obj *o = Jsi_ObjNew(interp);
        Jsi_PrototypeObjSet(interp, "CData", o);
        Jsi_ValueMakeObject(interp, ret, o);
        that = *ret;
    }

    CDataObj *cd = (typeof(cd))Jsi_Calloc(1,sizeof(*cd));
    cd->interp = interp;
    Jsi_Value *val = Jsi_ValueArrayIndex(interp, args, 0);
    Jsi_vtype vtyp = Jsi_ValueTypeGet(val);
    if (vtyp != JSI_VT_OBJECT && vtyp != JSI_VT_NULL) {
        Jsi_LogError("expected object, or null");
        goto errout;
    }
    if (Jsi_OptionsProcess(interp, CDataOptions, cd, val, 0) < 0)
        goto errout;

    if (JSI_OK != jsi_csNewCData(interp, cd, JSI_OPT_NO_SIG))
        goto errout;

    nobj = (Jsi_Obj*)Jsi_ValueGetObj(interp, that);
    cd->objId = Jsi_UserObjNew(interp, &cdataobject, nobj, cd);
    if (cd->objId<0) {
        goto errout;
    }
    cd->fobj = nobj;
    return JSI_OK;
    
errout:
    Jsi_OptionsFree(interp, CDataOptions, cd, 0);
    Jsi_Free(cd);
    return JSI_ERROR;

}

// Globals

static Jsi_RC jsi_DoneCData(Jsi_Interp *interp)
{
    if (!interp->SigHash) return JSI_OK;
    Jsi_HashDelete(interp->SigHash);
    Jsi_HashDelete(interp->StructHash);
    Jsi_HashDelete(interp->EnumHash);
    Jsi_HashDelete(interp->EnumItemHash);
    Jsi_HashDelete(interp->TYPEHash);
    Jsi_HashDelete(interp->CTypeHash);
    return JSI_OK;
}

Jsi_RC jsi_InitCData(Jsi_Interp *interp, int release)
{
    if (release) return jsi_DoneCData(interp);
#if JSI_USE_STUBS
    if (Jsi_StubsInit(interp, 0) != JSI_OK)
        return JSI_ERROR;
#endif

    Jsi_Hash *fsys = Jsi_UserObjRegister(interp, &cdataobject);
    if (!fsys)
        return Jsi_LogBug("Can not init cdata");

    interp->SigHash      = Jsi_HashNew(interp, JSI_KEYS_ONEWORD, NULL);
    interp->StructHash   = Jsi_HashNew(interp, JSI_KEYS_STRING, NULL);
    interp->EnumHash     = Jsi_HashNew(interp, JSI_KEYS_STRING, NULL);
    interp->EnumItemHash = Jsi_HashNew(interp, JSI_KEYS_STRING, NULL);
    interp->CTypeHash    = Jsi_HashNew(interp, JSI_KEYS_STRING, jsi_csTypeFree);
    interp->TYPEHash     = Jsi_HashNew(interp, JSI_KEYS_STRING, NULL);

    Jsi_CommandCreateSpecs(interp, cdataobject.name,  cdataCmds,  fsys, JSI_CMDSPEC_ISOBJ);
    Jsi_CommandCreateSpecs(interp, "CEnum",  enumCmds,   NULL, 0);
    Jsi_CommandCreateSpecs(interp, "CStruct",structCmds, NULL, 0);
    Jsi_CommandCreateSpecs(interp, "CType",  typeCmds,   NULL, 0);

    if (Jsi_PkgProvide(interp, cdataobject.name, 1, jsi_InitCData) != JSI_OK)
        return JSI_ERROR;
    return JSI_OK;
}

/* Initialize a struct to default values */
Jsi_RC Jsi_CDataStructInit(Jsi_Interp *interp, uchar* data, const char *sname)
{
    Jsi_StructSpec * sl = jsi_csStructGet(interp, sname);
    if (!sl)
        return Jsi_LogError("unknown struct: %s", sname);
    return jsi_csStructInit(sl, data);
}

Jsi_CDataDb *Jsi_CDataLookup(Jsi_Interp *interp, const char *name) {
    CDataObj *cd = (typeof(cd))Jsi_UserObjDataFromVar(interp, name);
    if (!cd)
        return NULL;
    return (Jsi_CDataDb*)cd;
}

Jsi_RC Jsi_CDataRegister(Jsi_Interp *interp, Jsi_CData_Static *statics)
{
    Jsi_RC rc = JSI_OK;
    if (statics) {
        if (interp->statics)
            statics->nextPtr = interp->statics;
        interp->statics = statics;
        jsi_csInitType(interp);
        jsi_csInitStructTables(interp);
        jsi_csInitEnum(interp);
        jsi_csInitEnumItem(interp);
        rc = jsi_csInitVarDefs(interp);
    }
    return rc;
}

/* Traverse types and match unique substring. */
Jsi_OptionType *Jsi_TypeLookup(Jsi_Interp *interp, const char *name)
{
    int isup = 1;
    const char *cp = name;
    while (*cp && isup) {
        if (!isupper(*cp)) isup=0;
        cp++;
    }
    return (Jsi_OptionType*)Jsi_HashGet((isup?interp->TYPEHash:interp->CTypeHash), name, 0);
}

#else // JSI_OMIT_CDATA

Jsi_CDataDb *Jsi_CDataLookup(Jsi_Interp *interp, const char *name) { return NULL; }
Jsi_RC Jsi_CDataRegister(Jsi_Interp *interp, Jsi_CData_Static *statics) { return JSI_ERROR; }
Jsi_RC Jsi_CDataStructInit(Jsi_Interp *interp, uchar* data, const char *sname) { return JSI_ERROR; }
Jsi_OptionType *Jsi_TypeLookup(Jsi_Interp *interp, const char *name) { return NULL; }

#endif // JSI_OMIT_CDATA
#endif // JSI_LITE_ONLY
