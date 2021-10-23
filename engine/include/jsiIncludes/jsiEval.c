/* The interpreter evaluation engine for jsi. */
#ifndef JSI_LITE_ONLY
#ifndef JSI_AMALGAMATION
#include "jsiInt.h"
#endif
#include <math.h>

/*#define USE_INLINE*/
#ifdef INLINE_ALL
#define INLINE inline
#else
#define INLINE
#endif

#define _jsi_STACK (interp->Stack)
#define _jsi_STACKIDX(s) interp->Stack[s]
#define _jsi_TOP (interp->Stack[interp->framePtr->Sp-1])
#define _jsi_TOQ (interp->Stack[interp->framePtr->Sp-2])
#define _jsi_THIS (interp->Obj_this)
#define _jsi_THISIDX(s) interp->Obj_this[s]

static Jsi_RC _jsi_LogErr(Jsi_Interp *interp, const char *str) { Jsi_LogMsg(interp, JSI_ERROR, "%s", str); return JSI_ERROR; }

#define _jsi_StrictChk(v) ((strict==0 || !Jsi_NumberIsNaN(v->d.num)) ? JSI_OK : _jsi_LogErr(interp, "value is NaN"))
#define _jsi_StrictChk2(v1,v2)  ((strict==0  || (Jsi_NumberIsNaN(v1->d.num)==0 && Jsi_NumberIsNaN(v2->d.num)==0))  ? JSI_OK : _jsi_LogErr(interp, "value is NaN"))
#define _jsi_StrictUChk(v) ((strict==0 || v->vt != JSI_VT_UNDEF) ? JSI_OK : _jsi_LogErr(interp, "value is undefined"))
#define _jsi_StrictUChk2(v1,v2)  ((strict==0  || (v1->vt != JSI_VT_UNDEF && v2->vt != JSI_VT_UNDEF))  ? JSI_OK : _jsi_LogErr(interp, "value is undefined"))
#define _jsi_StrictUChk3(v1,v2)  ((strict==0  || (v1->vt != JSI_VT_UNDEF || v2->vt == JSI_VT_UNDEF))  ? JSI_OK : _jsi_LogErr(interp, "lhs value undefined in ===/!==") )

static jsi_Pstate* jsiNewParser(Jsi_Interp* interp, char *codeStr, Jsi_Channel fp, int iseval)
{
    bool isNew, cache = 1; //(interp->nocacheOpCodes==0);
    Jsi_HashEntry *hPtr = NULL;
    hPtr = Jsi_HashEntryNew(interp->codeTbl, (void*)codeStr, &isNew);
    if (!hPtr) return NULL;
    jsi_Pstate *ps;

    if (cache && isNew==0 && ((ps = (jsi_Pstate *)Jsi_HashValueGet(hPtr)))) {
        interp->codeCacheHit++;
        return ps;
    }
    ps = jsi_PstateNew(interp);
    ps->eval_flag = iseval;
    if (codeStr)
        jsi_PstateSetString(ps, codeStr);
    else
        jsi_PstateSetFile(ps, fp, 1);
        
    interp->inParse++;
    yyparse(ps);
    interp->inParse--;
    
    if (ps->err_count) {
        if (cache) Jsi_HashEntryDelete(hPtr);
        jsi_PstateFree(ps);
        return NULL;
    }
    if (isNew) {
        if (cache) {
            Jsi_HashValueSet(hPtr, ps);
            ps->hPtr = hPtr;
        } else {
            /* only using caching now. */
            assert(0);
        }
    }
    return ps;
}

/* eval here is diff from Jsi_CmdProc, current scope Jsi_LogWarn should be past to eval */
/* make evaling script execute in the same context */
static Jsi_RC jsiEvalOp(Jsi_Interp* interp, jsi_Pstate *ps, char *program,
                       jsi_ScopeChain *scope, Jsi_Value *currentScope, Jsi_Value *_this, Jsi_Value **ret)
{
    Jsi_RC r = JSI_OK;
    jsi_Pstate *newps = jsiNewParser(interp, program, NULL, 1);
    if (newps) {
        int oef = newps->eval_flag;
        newps->eval_flag = 1;
        interp->ps = newps;
        r = jsi_evalcode(newps, NULL, newps->opcodes, scope, currentScope, _this, ret);
        if (r) {
            Jsi_ValueDup2(interp, &ps->last_exception, newps->last_exception);
        }
        newps->eval_flag = oef;
        interp->ps = ps;
    } else  {
        //Jsi_ValueMakeStringKey(interp, &ps->last_exception, "Syntax Error");
        r = JSI_ERROR;
    }
    return r;
}
                     
static Jsi_Value** ValuesAlloc(Jsi_Interp *interp, int cnt, Jsi_Value**old, int oldsz) {
    int i;
    Jsi_Value **v = (Jsi_Value **)Jsi_Realloc(old, cnt* sizeof(Jsi_Value*));
    for (i=oldsz; i<cnt; i++)
        v[i] = NULL;
    return v;
}

static void jsiSetupStack(Jsi_Interp *interp)
{
    int oldsz = interp->maxStack;
    if (interp->maxStack)
        interp->maxStack += STACK_INCR_SIZE;
    else
        interp->maxStack = STACK_INIT_SIZE;
    _jsi_STACK = ValuesAlloc(interp, interp->maxStack, _jsi_STACK, oldsz);
    _jsi_THIS = ValuesAlloc(interp, interp->maxStack, _jsi_THIS, oldsz); //TODO:!!! use interp->framePtr for this.
}

static void jsiPush(Jsi_Interp* interp, int n) {
    int i = 0;
    do {
        if (!_jsi_STACKIDX(interp->framePtr->Sp))
            _jsi_STACKIDX(interp->framePtr->Sp) = Jsi_ValueNew1(interp);
        if (!_jsi_THISIDX(interp->framePtr->Sp))
            _jsi_THISIDX(interp->framePtr->Sp) = Jsi_ValueNew1(interp);
        if (i++ >= n) break;
        interp->framePtr->Sp++;
    } while (1);
}

/* Before setting a value in the _jsi_STACK/obj, unlink any reference to it. */

static void ClearStack(register Jsi_Interp *interp, int ofs) {
    Jsi_Value **vPtr = &_jsi_STACKIDX(interp->framePtr->Sp-ofs), *v = *vPtr;
    if (!v) return;
#ifndef XX_NEWSTACK
    Jsi_ValueReset(interp, vPtr);
#else
    if (v->refCnt<=1)
        Jsi_ValueReset(interp, vPtr);
    else {
        Jsi_DecrRefCount(interp, v);
        _jsi_STACKIDX(interp->framePtr->Sp-ofs) = Jsi_ValueNew1(interp);
    }
#endif
}

static void ClearThis(register Jsi_Interp *interp, int ofs) {
    Jsi_Value **vPtr = &_jsi_THISIDX(ofs), *v = *vPtr;
    if (!v) return;
#ifndef XX_NEWSTACK
    Jsi_ValueReset(interp, vPtr);
#else
    if (v->refCnt<=1)
        Jsi_ValueReset(interp, vPtr);
    else {
        Jsi_DecrRefCount(interp, v);
        _jsi_THISIDX(ofs) = Jsi_ValueNew1(interp);
    }
#endif
}


static Jsi_RC inline jsi_ValueAssign(Jsi_Interp *interp, Jsi_Value *dst, Jsi_Value* src, int lop)
{
    Jsi_Value *v;
    if (dst->vt != JSI_VT_VARIABLE) {
        if (jsi_IsStrictMode(interp)) 
            return Jsi_LogError("operand not a left value");
    } else {
        v = dst->d.lval;
        SIGASSERT(v, VALUE);
        int strict = jsi_IsStrictMode(interp);
        if (strict && lop == OP_PUSHFUN && interp->curIp[-1].local)
            dst->f.bits.local = 1;
        if (strict && dst->f.bits.local==0) {
            const char *varname = "";
            if (v->f.bits.lookupfailed)
                varname = v->d.lookupFail;
            Jsi_LogType("function created global: \"%s\"", varname);
            dst->f.bits.local=1;
            if (interp->typeCheck.error) {
                if (interp->framePtr->tryDepth) //Fixes tests/strict.js leak in try 
                    Jsi_HashSet(interp->genValueTbl, v, v);
                return JSI_ERROR;
            }
        }
        if (v == src)
            return JSI_OK;
        if (v->f.bits.readonly) {
            if (jsi_IsStrictMode(interp)) 
                return Jsi_LogError("assign to readonly variable");
            return JSI_OK;
        }
        if (Jsi_ValueIsFunction(interp, src))
            Jsi_ValueMove(interp,v, src);
        else
            Jsi_ValueCopy(interp,v, src);
        SIGASSERT(v, VALUE);
#ifdef JSI_MEM_DEBUG
    if (!v->VD.label2)
        v->VD.label2 = "ValueAssign";
#endif
    }
    return JSI_OK;
}

/* pop n values from _jsi_STACK */
static INLINE void jsiPop(Jsi_Interp* interp, int n) {
    int t = n;
    while (t > 0) {
        Assert((interp->framePtr->Sp-t)>=0);
/*        Jsi_Value *v = _jsi_STACKIDX(interp->framePtr->Sp-t);
         if (v->refCnt>1) puts("OO");*/
        ClearStack(interp,t);
        --t;
    }
    interp->framePtr->Sp -= n;
}

/* Convert preceding _jsi_STACK variable(s) into value(s). */
static INLINE void VarDeref(Jsi_Interp* interp, int n) {
    while(interp->framePtr->Sp<n) // Assert and Log may map-out Ops.
        jsiPush(interp, 1);
    int i;
    for (i=1; i<=n; i++) {
        Jsi_Value *vb = _jsi_STACKIDX(interp->framePtr->Sp - i);
        if (vb->vt == JSI_VT_VARIABLE) {
            SIGASSERT(vb->d.lval, VALUE);
            Jsi_ValueCopy(interp, vb, vb->d.lval);
        }
    }
}

#define common_math_opr(opr) {                      \
    VarDeref(interp,2);                                     \
    Jsi_ValueToNumber(interp, _jsi_TOP);     \
    Jsi_ValueToNumber(interp, _jsi_TOQ);     \
    rc = _jsi_StrictChk2(_jsi_TOP, _jsi_TOQ); \
    _jsi_TOQ->d.num = _jsi_TOQ->d.num opr _jsi_TOP->d.num;            \
    jsiPop(interp, 1);                                          \
}

#define common_bitwise_opr(opr) {                       \
    int a, b;                                       \
    VarDeref(interp,2);                                     \
    Jsi_ValueToNumber(interp, _jsi_TOP);     \
    Jsi_ValueToNumber(interp, _jsi_TOQ);     \
    rc = _jsi_StrictChk2(_jsi_TOP, _jsi_TOQ); \
    a = _jsi_TOQ->d.num; b = _jsi_TOP->d.num;                   \
    _jsi_TOQ->d.num = (Jsi_Number)(a opr b);                  \
    jsiPop(interp, 1);                                          \
}

static INLINE Jsi_RC logic_less(Jsi_Interp* interp, int i1, int i2) {
    Jsi_Value *v, *v1 = _jsi_STACK[interp->framePtr->Sp-i1], *v2 = _jsi_STACK[interp->framePtr->Sp-i2], *res = _jsi_TOQ;
    int val = 0, l1 = 0, l2 = 0; 
    bool strict = jsi_IsStrictMode(interp);
    Jsi_RC rc = JSI_OK;
    rc = _jsi_StrictUChk2(v1, v2);
    if (rc != JSI_OK)
        return JSI_ERROR;
    char *s1 = Jsi_ValueString(interp, v1, &l1);
    char *s2 = Jsi_ValueString(interp, v2, &l2);
    Jsi_Number n1, n2;

    if (s1 || s2) {
        char *str;
        if (!(s1 && s2)) {
            v = (s1 ? v2 : v1);
            jsi_ValueToPrimitive(interp, &v);
            Jsi_ValueToString(interp, v, NULL);
            str = Jsi_ValueString(interp, v, (s1?&l2:&l1));
            if (s1) s2 = str; else s1 = str;
        }
        Assert(l1>=0 && l1<=JSI_MAX_ALLOC_BUF);
        Assert(l2>=0 && l2<=JSI_MAX_ALLOC_BUF);
        //int mlen = (l1>l2?l1:l2);
        val = Jsi_Strcmp(s1, s2);
  
        if (val > 0) val = 0;
        else if (val < 0) val = 1;
        else val = (l1 < l2);
        ClearStack(interp,2);
        Jsi_ValueMakeBool(interp, &res, val);
    } else {
        Jsi_ValueToNumber(interp, v1);
        Jsi_ValueToNumber(interp, v2);
        rc = _jsi_StrictChk2(v1,v2);
        if (rc != JSI_OK)
            return JSI_ERROR;
        n1 = v1->d.num; n2 = v2->d.num;
        if (Jsi_NumberIsNaN(n1) || Jsi_NumberIsNaN(n2)) {
            ClearStack(interp,2);
            Jsi_ValueMakeUndef(interp, &res);
        } else {
            val = (n1 < n2);
            ClearStack(interp,2);
            Jsi_ValueMakeBool(interp, &res, val);
        }
    }
    return JSI_OK;
}

static const char *vprint(Jsi_Value *v)
{
    static char buf[100];
    if (v->vt == JSI_VT_NUMBER) {
        snprintf(buf, 100, "NUM:%" JSI_NUMGFMT " ", v->d.num);
    } else if (v->vt == JSI_VT_BOOL) {
        snprintf(buf, 100, "BOO:%d", v->d.val);
    } else if (v->vt == JSI_VT_STRING) {
        snprintf(buf, 100, "STR:'%s'", v->d.s.str);
    } else if (v->vt == JSI_VT_VARIABLE) {
        snprintf(buf, 100, "VAR:%p", v->d.lval);
    } else if (v->vt == JSI_VT_NULL) {
        snprintf(buf, 100, "NULL");
    } else if (v->vt == JSI_VT_OBJECT) {
        snprintf(buf, 100, "OBJ:%p", v->d.obj);
    } else if (v->vt == JSI_VT_UNDEF) {
        snprintf(buf, 100, "UNDEFINED");
    }
    return buf;
}

typedef enum {
        TL_TRY,
        TL_WITH,
} try_op_type;                            /* type of try */

typedef enum { LOP_NOOP, LOP_THROW, LOP_JMP } last_try_op_t; 

typedef struct TryList {
    try_op_type type;
    union {
        struct {                    /* try data */
            jsi_OpCode *tstart;         /* try start ip */
            jsi_OpCode *tend;           /* try end ip */
            jsi_OpCode *cstart;         /* ...*/
            jsi_OpCode *cend;
            jsi_OpCode *fstart;
            jsi_OpCode *fend;
            int tsp;
            last_try_op_t last_op;              /* what to do after finally block */
                                    /* depend on last jmp code in catch block */
            union {
                jsi_OpCode *tojmp;
            } ld;                   /* jmp out of catch (target)*/
        } td;
        struct {                    /* with data */
            jsi_OpCode *wstart;         /* with start */
            jsi_OpCode *wend;           /* with end */
        } wd;
    } d;
    
    jsi_ScopeChain *scope_save;         /* saved scope (used in catch block/with block)*/
    Jsi_Value *curscope_save;           /* saved current scope */
    struct TryList *next;
    bool inCatch;
    bool inFinal;
} TryList;

/* destroy top of trylist */
#define pop_try(head) _pop_try(interp, &head)
static INLINE void _pop_try(Jsi_Interp* interp, TryList **head)
{
    interp->framePtr->tryDepth--;
    TryList *t = (*head)->next;
    Jsi_Free((*head));
    (*head) = t;
}

#define push_try(head, n) _push_try(interp, &head, n)
static INLINE void _push_try(Jsi_Interp* interp, TryList **head, TryList *n)
{
    interp->framePtr->tryDepth++;
    (n)->next = (*head);
    (*head) = (n);
}

/* restore scope chain */
#define restore_scope() _restore_scope(interp, ps, trylist, \
    &scope, &currentScope, &context_id)
static INLINE void _restore_scope(Jsi_Interp* interp, jsi_Pstate *ps, TryList* trylist,
  jsi_ScopeChain **scope, Jsi_Value **currentScope, int *context_id) {

/* restore_scope(scope_save, curscope_save)*/
    if (*scope != (trylist->scope_save)) {
        jsi_ScopeChainFree(interp, *scope);
        *scope = (trylist->scope_save);
        interp->framePtr->ingsc = *scope;
    }
    if (*currentScope != (trylist->curscope_save)) {
        Jsi_DecrRefCount(interp, *currentScope);
        *currentScope = (trylist->curscope_save); 
        interp->framePtr->incsc = *currentScope;
    }
    *context_id = ps->_context_id++; 
}

#define do_throw(nam) if (_do_throw(interp, ps, &ip, &trylist,&scope, &currentScope, &context_id, (interp->framePtr->Sp?_jsi_TOP:NULL), nam) != JSI_OK) { rc = JSI_ERROR; break; }

static int _do_throw(Jsi_Interp *interp, jsi_Pstate *ps, jsi_OpCode **ipp, TryList **tlp,
     jsi_ScopeChain **scope, Jsi_Value **currentScope, int *context_id, Jsi_Value *top, const char *nam) {
    if (Jsi_InterpGone(interp))
        return JSI_ERROR;
    TryList *trylist = *tlp;
    while (1) {
        if (trylist == NULL) {
            const char *str = (top?Jsi_ValueString(interp, top, NULL):"");
            if (str)
                Jsi_LogError("%s: %s", nam, str);
            return JSI_ERROR;
        }
        if (trylist->type == TL_TRY) {
            int n = interp->framePtr->Sp - trylist->d.td.tsp;
            jsiPop(interp, n);
            if (*ipp >= trylist->d.td.tstart && *ipp < trylist->d.td.tend) {
                *ipp = trylist->d.td.cstart - 1;
                break;
            } else if (*ipp >= trylist->d.td.cstart && *ipp < trylist->d.td.cend) {
                trylist->d.td.last_op = LOP_THROW;
                *ipp = trylist->d.td.fstart - 1;
                break;
            } else if (*ipp >= trylist->d.td.fstart && *ipp < trylist->d.td.fend) {
                _pop_try(interp, tlp);
                trylist = *tlp;
            } else Jsi_LogBug("Throw within a try, but not in its scope?");
        } else {
            _restore_scope(interp, ps, trylist, scope, currentScope, context_id);
            _pop_try(interp, tlp);
            trylist = *tlp;
        }
    }
    return JSI_OK;
}

static TryList *trylist_new(try_op_type t, jsi_ScopeChain *scope_save, Jsi_Value *curscope_save)
{
    TryList *n = (TryList *)Jsi_Calloc(1,sizeof(*n));
    
    n->type = t;
    n->curscope_save = curscope_save;
    /*Jsi_IncrRefCount(interp, curscope_save);*/
    n->scope_save = scope_save;
    
    return n;
}

static void DumpInstr(Jsi_Interp *interp, jsi_Pstate *ps, Jsi_Value *_this,
    TryList *trylist, jsi_OpCode *ip, Jsi_OpCodes *opcodes)
{
    int i;
    char buf[200];
    jsi_code_decode(ip, ip - opcodes->codes, buf, sizeof(buf));
    Jsi_Printf(jsi_Stderr, "%p: %-30.200s : THIS=%s, STACK=[", ip, buf, vprint(_this));
    for (i = 0; i < interp->framePtr->Sp; ++i) {
        Jsi_Printf(jsi_Stderr, "%s%s", (i>0?", ":""), vprint(_jsi_STACKIDX(i)));
    }
    Jsi_Printf(jsi_Stderr, "]");
    if (ip->fname) {
        const char *fn = ip->fname,  *cp = Jsi_Strrchr(fn, '/');
        if (cp) fn = cp+1;
        Jsi_Printf(jsi_Stderr, ", %s:%d", fn, ip->Line);
    }
    Jsi_Printf(jsi_Stderr, "\n");
    TryList *tlt = trylist;
    for (i = 0; tlt; tlt = tlt->next) i++;
    if (ps->last_exception)
        Jsi_Printf(jsi_Stderr, "TL: %d, excpt: %s\n", i, vprint(ps->last_exception));
}

static int cmpstringp(const void *p1, const void *p2)
{
   return Jsi_Strcmp(* (char * const *) p1, * (char * const *) p2);
}

void jsi_SortDString(Jsi_Interp *interp, Jsi_DString *dStr, const char *sep) {
    int argc, i;
    char **argv;
    Jsi_DString sStr;
    Jsi_DSInit(&sStr);
    Jsi_SplitStr(Jsi_DSValue(dStr), &argc, &argv, sep, &sStr);
    qsort(argv, argc, sizeof(char*), cmpstringp);
    Jsi_DSSetLength(dStr, 0);
    for (i=0; i<argc; i++)
        Jsi_DSAppend(dStr, (i?" ":""), argv[i], NULL);
    Jsi_DSFree(&sStr);
}

static void ValueObjDelete(Jsi_Interp *interp, Jsi_Value *target, Jsi_Value *key, int force)
{
    if (target->vt != JSI_VT_OBJECT) return;
    const char *kstr = Jsi_ValueToString(interp, key, NULL);
    Jsi_TreeEntry *hPtr;
    if (!Jsi_ValueIsStringKey(interp, key)) {
        Jsi_MapEntry *hePtr = Jsi_MapEntryFind(target->d.obj->tree->opts.interp->strKeyTbl, kstr);
        if (hePtr)
            kstr = (char*)Jsi_MapKeyGet(hePtr, 0);
    }
    hPtr = Jsi_TreeEntryFind(target->d.obj->tree, kstr);
    if (hPtr == NULL || (hPtr->f.bits.dontdel && !force))
        return;
    Jsi_TreeEntryDelete(hPtr);
}

static void ObjGetNames(Jsi_Interp *interp, Jsi_Obj *obj, Jsi_DString* dStr, int flags) {
    Jsi_TreeEntry *hPtr;
    Jsi_TreeSearch srch;
    Jsi_Value *v;
    int m = 0;
    Jsi_DSInit(dStr);
    if (obj->isarrlist)
        obj = interp->Array_prototype->d.obj;
    for (hPtr=Jsi_TreeSearchFirst(obj->tree, &srch,  JSI_TREE_ORDER_IN, NULL); hPtr; hPtr=Jsi_TreeSearchNext(&srch)) {
        v = (Jsi_Value*)Jsi_TreeValueGet(hPtr);
        if (!v) continue;
        if ((flags&JSI_NAME_FUNCTIONS) && !Jsi_ValueIsFunction(interp,v)) {
            continue;
        }
        if ((flags&JSI_NAME_DATA) && Jsi_ValueIsFunction(interp,v)) {
            continue;
        }

        Jsi_DSAppend(dStr, (m++?" ":""), Jsi_TreeKeyGet(hPtr), NULL);
    }
    Jsi_TreeSearchDone(&srch);
}

static void DumpFunctions(Jsi_Interp *interp, const char *spnam) {
    Jsi_DString dStr;
    Jsi_DSInit(&dStr);
    Jsi_MapEntry *hPtr;
    Jsi_MapSearch search;
    Jsi_CmdSpecItem *csi = NULL;
    Jsi_CmdSpec *cs;
    Jsi_Value *lsf = interp->lastSubscriptFail;
    Jsi_Obj *lso = ((lsf && lsf->vt == JSI_VT_OBJECT)?lsf->d.obj:0);
    const char *varname = NULL;
    int m = 0;
    
    if (lso) {
        spnam = interp->lastSubscriptFailStr;
        if (!spnam) spnam = interp->lastPushStr;
        if (lso->ot == JSI_OT_USEROBJ && lso->d.uobj->reg && lso->d.uobj->interp == interp) {
            cs = lso->d.uobj->reg->spec;
            if (cs)
                goto dumpspec;
        } else if (lso->ot == JSI_OT_FUNCTION) {
            cs = lso->d.fobj->func->cmdSpec;
            if (cs)
                goto dumpspec;
        } else if (lso->ot == JSI_OT_OBJECT) {
            ObjGetNames(interp, lso, &dStr, JSI_NAME_FUNCTIONS);
            Jsi_LogError("'%s', functions are: %s.",
                spnam, Jsi_DSValue(&dStr));
            Jsi_DSFree(&dStr);
            return;
        } else {
            const char *sustr = NULL;
            switch (lso->ot) {
                case JSI_OT_STRING: sustr = "String"; break;
                case JSI_OT_NUMBER: sustr = "Number"; break;
                case JSI_OT_BOOL: sustr = "Boolean"; break;
                default: break;
            }
            if (sustr) {
                hPtr = Jsi_MapEntryFind(interp->cmdSpecTbl, sustr);
                csi = (Jsi_CmdSpecItem*)Jsi_MapValueGet(hPtr);
                cs = csi->spec;
                if (!spnam[0])
                    spnam = sustr;
                goto dumpspec;
            }
        }
    }
    if (!*spnam) {
        for (hPtr = Jsi_MapSearchFirst(interp->cmdSpecTbl, &search, 0);
            hPtr; hPtr = Jsi_MapSearchNext(&search)) {
            csi = (Jsi_CmdSpecItem*)Jsi_MapValueGet(hPtr);
            if (csi->name && csi->name[0])
                Jsi_DSAppend(&dStr, (m++?" ":""), csi->name, NULL);
        }
        Jsi_MapSearchDone(&search);
    }
    
    varname = spnam;
    if ((hPtr = Jsi_MapEntryFind(interp->cmdSpecTbl, spnam))) {
        csi = (Jsi_CmdSpecItem*)Jsi_MapValueGet(hPtr);
        while (csi) {
            int n;
            cs = csi->spec;
dumpspec:
            n = 0;
            while (cs->name) {
                if (n != 0 || !(cs->flags & JSI_CMD_IS_CONSTRUCTOR)) {
                    if (!*cs->name) continue;
                    Jsi_DSAppend(&dStr, (m?" ":""), cs->name, NULL);
                    n++; m++;
                }
                cs++;
            }
            csi = (csi?csi->next:NULL);
        }
        jsi_SortDString(interp, &dStr, " ");
        if (varname)
            spnam = varname;
        else if (interp->lastPushStr && !spnam[0])
            spnam = interp->lastPushStr;
        Jsi_LogError("'%s' sub-commands are: %s.",
            spnam, Jsi_DSValue(&dStr));
        Jsi_DSFree(&dStr);
    } else {
        Jsi_LogError("can not execute expression: '%s' not a function",
            varname ? varname : "");
    }
}

/* Attempt to dynamically load function XX by doing an eval of Jsi_Auto.XX */
/* TODO: prevent infinite loop/recursion. */
Jsi_Value *jsi_LoadFunction(Jsi_Interp *interp, const char *str, Jsi_Value *tret) {
    Jsi_DString dStr = {};
    Jsi_Value *v;
    int i;
    const char *curFile = interp->curFile;
    interp->curFile = "<jsiLoadFunction>";
    for (i=0; i<2; i++) {
        Jsi_DSAppend(&dStr, "Jsi_Auto.", str, NULL);
        Jsi_VarLookup(interp, Jsi_DSValue(&dStr));
        v = Jsi_NameLookup(interp, Jsi_DSValue(&dStr));
        if (v)
            jsi_ValueDebugLabel(v, "jsiLoadFunction","f1");
        Jsi_DSFree(&dStr);
        if (v) {
            const char *cp = Jsi_ValueGetDString(interp, v, &dStr, 0);
            Jsi_DecrRefCount(interp, v);
            v = NULL;
            if (Jsi_EvalString(interp, cp, 0) == JSI_OK) {
                v = Jsi_NameLookup(interp, str);
                if (v)
                    jsi_ValueDebugLabel(v, "jsiLoadFunction","f2");
            }
            Jsi_DSFree(&dStr);
            if (v) {
                tret = v;
                break;
            }
        }
        if (interp->autoLoaded++ || i>0)
            break;
        /*  Index not in memory, so try loading Jsi_Auto from the autoload.jsi file. */
        if (interp->autoFiles == NULL)
            return tret;
        Jsi_Value **ifs = &interp->autoFiles;
        int i, ifn = 1;
        if (Jsi_ValueIsArray(interp, interp->autoFiles)) {
            ifs = interp->autoFiles->d.obj->arr;
            ifn = interp->autoFiles->d.obj->arrCnt;
        }
        for (i=0; i<ifn; i++) {  
            if (Jsi_EvalFile(interp, ifs[i], 0) != JSI_OK)
                break;
            interp->autoLoaded++;
        }
    }
    interp->curFile = curFile;
    return tret;
}

void jsi_TraceFuncCall(Jsi_Interp *interp, Jsi_Func *fstatic, jsi_OpCode *iPtr,
    Jsi_Value *_this, Jsi_Value* args, Jsi_Value *ret)
{
    jsi_OpCode *ip = (iPtr ? iPtr : interp->curIp);
    if (!ip)
        return;
    const char *ff, *fname = ip->fname?ip->fname:"";
    if ((interp->traceCall&jsi_callTraceFullPath)==0 && ((ff=Jsi_Strrchr(fname,'/'))))
        fname = ff+1;
    if (interp->traceHook)
        (*interp->traceHook)(interp, fstatic->name, ip->fname, ip->Line, fstatic->cmdSpec, _this, args, ret);
    else {
        const char *fp = ((interp->traceCall&jsi_callTraceNoParent)?NULL:fstatic->parent);
        if (fp && !*fp)
            fp = NULL;
        Jsi_DString aStr;
        Jsi_DSInit(&aStr);
        Jsi_DString dStr;
        Jsi_DSInit(&dStr);
        Jsi_DString *sPtr = NULL;
        int plen = 0;
        if (ret) {
            sPtr = &dStr;
            Jsi_DSAppend(sPtr, " <-- ", NULL);
            plen = Jsi_DSLength(sPtr);
            Jsi_ValueGetDString(interp, ret, sPtr, 0);
        } else if ((interp->traceCall&jsi_callTraceArgs)) {
            sPtr = &aStr;
            Jsi_ValueGetDString(interp, args, sPtr, JSI_OUTPUT_JSON);
        }
        if (sPtr) {
            if (!(interp->traceCall&jsi_callTraceNoTrunc)) {
                const char *cp0 = Jsi_DSValue(sPtr), *cp1 = Jsi_Strchr(cp0, '\n');
                int nlen = 0, clen = Jsi_DSLength(sPtr);
                if (cp1) {
                    nlen = (cp1-cp0);
                    if (nlen>60) nlen = 60;
                }  else if (clen>60)
                    nlen = 60;
                else nlen = clen;
                if (nlen != clen && clen>plen) {
                    Jsi_DSSetLength(sPtr, nlen);
                    Jsi_DSAppend(sPtr, "...", NULL);
                }
            }
        }
        if (interp->parent && interp->debugOpts.traceCallback) {
            Jsi_DString jStr={}, kStr={}, lStr={};
            Jsi_DSPrintf(&kStr, "[\"%s%s%s\", %s, %s, \"%s\", %d, %d ]",
                (fp?fp:""), (fp?".":""), fstatic->name, 
                (ret?"null":Jsi_JSONQuote(interp, Jsi_DSValue(&aStr),-1, &jStr)),
                (ret?Jsi_JSONQuote(interp, Jsi_DSValue(&dStr),-1, &lStr):"null"),
                 fname, ip->Line, ip->Lofs);
            if (Jsi_CommandInvokeJSON(interp->parent, interp->debugOpts.traceCallback, Jsi_DSValue(&kStr), NULL) != JSI_OK)
                Jsi_Printf(jsi_Stderr, "failed trace call\n");
            Jsi_DSFree(&jStr);
            Jsi_DSFree(&kStr);
            Jsi_DSFree(&lStr);
        } else if ((interp->traceCall&jsi_callTraceBefore))
            Jsi_Printf(jsi_Stderr, "%s:%d %*s#%d: %c %s%s%s(%s) %s\n",
                fname, ip->Line,
                (interp->level-1)*2, "", interp->level,
                (ret?'<':'>'), (fp?fp:""), (fp?".":""), fstatic->name, Jsi_DSValue(&aStr),
            Jsi_DSValue(&dStr));
        else
            Jsi_Printf(jsi_Stderr, "%*s#%d: %c %s%s%s(%s) in %s:%d%s\n", (interp->level-1)*2, "", interp->level,
                (ret?'<':'>'), (fp?fp:""), (fp?".":""), fstatic->name, Jsi_DSValue(&aStr),
            fname, ip->Line, Jsi_DSValue(&dStr));
        Jsi_DSFree(&dStr);
        Jsi_DSFree(&aStr);
    }
}

static INLINE Jsi_RC jsiEvalFunction(register jsi_Pstate *ps, jsi_OpCode *ip, int discard) {
    Jsi_RC excpt_ret = JSI_OK;
    register Jsi_Interp *interp = ps->interp;
    int as_constructor = (ip->op == OP_NEWFCALL);
    int stackargc = (int)(uintptr_t)ip->data;
    const char *oldCurFunc = interp->curFunction;
    VarDeref(interp, stackargc + 1);
    
    int tocall_index = interp->framePtr->Sp - stackargc - 1, adds;
    Jsi_Value *tocall = _jsi_STACKIDX(tocall_index);
    const char *spnam = "";
    //char *lpv = interp->lastPushStr;
    if (tocall->vt == JSI_VT_UNDEF && tocall->f.bits.lookupfailed && tocall->d.lookupFail) {
        spnam = tocall->d.lookupFail;
        tocall->f.bits.lookupfailed = 0;
        tocall = jsi_LoadFunction(interp, spnam, tocall);
        interp->lastPushStr = (char*)spnam;
        interp->curIp = ip;
    }
    if (!Jsi_ValueIsFunction(interp, tocall)) {
       // if (tocall->f.bits.subscriptfailed && tocall->d.lookupFail)
       //     spnam = tocall->d.lookupFail;
        DumpFunctions(interp, spnam);
        excpt_ret = JSI_ERROR;
        goto empty_func;
    }

    if (tocall->d.obj->d.fobj==NULL || tocall->d.obj->d.fobj->func==NULL) {   /* empty function */
empty_func:
        jsiPop(interp, stackargc);
        ClearStack(interp,1);
        Jsi_ValueMakeUndef(interp, &_jsi_TOP);
    } else {
        Jsi_FuncObj *fobj = tocall->d.obj->d.fobj;
        Jsi_Func *fstatic = fobj->func;
        if (fstatic->callback == jsi_NoOpCmd || tocall->d.obj->isNoOp)
            goto empty_func;
        if (!interp->asserts && fstatic->callback == jsi_AssertCmd)
            goto empty_func;
        const char *onam = fstatic->name;
//        if (!onam) // Override blank name with last index.
//            fstatic->name = lpv;
        if (fstatic->name && fstatic->name[0] && fstatic->type == FC_NORMAL)
            interp->curFunction = fstatic->name;
        adds = fstatic->callflags.bits.addargs;
        if (adds && (fstatic->cmdSpec->flags&JSI_CMDSPEC_NONTHIS))
            adds = 0;
        /* create new scope, prepare arguments */
        /* here we shared scope and 'arguments' with the same object */
        /* so that arguments[0] is easier to shared space with first local variable */
        Jsi_Value *fargs = Jsi_ValueNew1(interp);
        Jsi_Obj *ao = Jsi_ObjNewArray(interp, _jsi_STACK+(interp->framePtr->Sp - stackargc), stackargc, 1);
      
        Jsi_ValueMakeObject(interp, &fargs, ao);        
        fargs->d.obj->__proto__ = interp->Object_prototype;          // ecma

        Jsi_Func *pprevActive = interp->prevActiveFunc, *prevActive = interp->prevActiveFunc = interp->activeFunc;
        interp->activeFunc = fstatic;

        excpt_ret = jsi_SharedArgs(interp, fargs, fstatic, 1); /* make arg vars to share arguments */
        fstatic->callflags.bits.addargs = 0;
        jsi_InitLocalVar(interp, fargs, fstatic);
        jsi_SetCallee(interp, fargs, tocall);
        
        jsiPop(interp, stackargc);
    
        Jsi_Value *ntPtr;
        if (_jsi_THISIDX(tocall_index)->vt == JSI_VT_OBJECT) {
            ntPtr = Jsi_ValueDup(interp, _jsi_THISIDX(tocall_index));
            ClearThis(interp, tocall_index);
        } else {
            ntPtr = Jsi_ValueDup(interp, interp->Top_object);
        }
        int calltrc = 0;

        if (as_constructor) {                       /* new Constructor */
            Jsi_Obj *newobj = Jsi_ObjNewType(interp, JSI_OT_OBJECT);
            Jsi_Value *proto = Jsi_ValueObjLookup(interp, tocall, "prototype", 0);
            if (proto && proto->vt == JSI_VT_OBJECT) {
                newobj->__proto__ = proto;
                newobj->clearProto = 1;
                Jsi_IncrRefCount(interp, proto);
            }
            Jsi_ValueReset(interp, &ntPtr);
            Jsi_ValueMakeObject(interp, &ntPtr, newobj);            
            /* TODO: constructor specifics??? */
            calltrc = (interp->traceCall&jsi_callTraceNew);
        }
        if (fstatic->type == FC_NORMAL)
            calltrc = (interp->traceCall&jsi_callTraceFuncs);
        else
            calltrc = (interp->traceCall&jsi_callTraceCmds);
        if (calltrc && fstatic->name)
            jsi_TraceFuncCall(interp, fstatic, ip, ntPtr, fargs, 0);

        Jsi_Value *spretPtr = Jsi_ValueNew1(interp), *spretPtrOld = spretPtr;
        
        double timStart = 0;
        interp->activeFunc = fstatic;
        int docall = (excpt_ret==JSI_OK);
        if (interp->profile || interp->coverage)
            timStart = jsi_GetTimestamp();
        if (fstatic->type == FC_NORMAL) {
            if (docall) {
                excpt_ret = jsi_evalcode(ps, fstatic, fstatic->opcodes, tocall->d.obj->d.fobj->scope, 
                    fargs, ntPtr, &spretPtr);
            }
            interp->funcCallCnt++;
        } else if (!fstatic->callback) {
            Jsi_LogError("can not call:\"%s()\"", fstatic->name);
        } else {
            int oldcf = fstatic->callflags.i;
            fstatic->callflags.bits.iscons = (as_constructor?JSI_CALL_CONSTRUCTOR:0);
            if (fstatic->f.bits.hasattr)
            {
#define SPTR(s) (s?s:"")
                if ((fstatic->f.bits.isobj) && ntPtr->vt != JSI_VT_OBJECT) {
                    excpt_ret = JSI_ERROR;
                    docall = 0;
                    Jsi_LogError("'this' is not object: \"%s()\"", fstatic->name);
                } else if ((!(fstatic->f.bits.iscons)) && as_constructor) {
                    excpt_ret = JSI_ERROR;
                    docall = 0;
                    Jsi_LogError("can not call as constructor: \"%s()\"", fstatic->name);
                } else {
                    int aCnt = Jsi_ValueGetLength(interp, fargs);
                    if (aCnt<(fstatic->cmdSpec->minArgs+adds)) {
                        Jsi_LogError("missing args, expected \"%s(%s)\" ", fstatic->cmdSpec->name, SPTR(fstatic->cmdSpec->argStr));
                        excpt_ret = JSI_ERROR;
                        docall = 0;
                    } else if (fstatic->cmdSpec->maxArgs>=0 && (aCnt>fstatic->cmdSpec->maxArgs+adds)) {
                        Jsi_LogError("extra args, expected \"%s(%s)\" ", fstatic->cmdSpec->name, SPTR(fstatic->cmdSpec->argStr));
                        excpt_ret = JSI_ERROR;
                        docall = 0;
                    }
                }
            }
            if (docall) {
                fstatic->fobj = fobj; // Backlink for bind.
                fstatic->callflags.bits.isdiscard = discard;
                excpt_ret = fstatic->callback(interp, fargs, 
                    ntPtr, &spretPtr, fstatic);
                interp->cmdCallCnt++;
            }
            fstatic->callflags.i = oldcf;
        }
        if (interp->profile || interp->coverage) {
            double timEnd = jsi_GetTimestamp(), timUsed = (timEnd - timStart);;
            assert(timUsed>=0);
            fstatic->allTime += timUsed;
            if (interp->framePtr->evalFuncPtr)
                interp->framePtr->evalFuncPtr->subTime += timUsed;
            else
                interp->subTime += timUsed;
        }
        if (calltrc && (interp->traceCall&jsi_callTraceReturn) && fstatic->name)
            jsi_TraceFuncCall(interp, fstatic, ip, ntPtr, NULL, spretPtr);
        if (!onam)
            fstatic->name = NULL;
        if (docall) {
            fstatic->callCnt++;
            if (excpt_ret == JSI_OK && !as_constructor && fstatic->retType && (interp->typeCheck.all || interp->typeCheck.run))
                excpt_ret = jsi_ArgTypeCheck(interp, fstatic->retType, spretPtr, "returned from", fstatic->name, 0, fstatic, 0);
        }
        interp->prevActiveFunc = pprevActive;
        interp->activeFunc = prevActive;

        if (as_constructor) {
            if (ntPtr->vt == JSI_VT_OBJECT)
                ntPtr->d.obj->constructor = tocall->d.obj;
            if (spretPtr->vt != JSI_VT_OBJECT) {
                Jsi_ValueReset(interp,&spretPtr);
                Jsi_ValueCopy(interp, spretPtr, ntPtr);
            }
        }
        
        jsi_SharedArgs(interp, fargs, fstatic, 0); /* make arg vars to shared arguments */
        Jsi_DecrRefCount(interp, ntPtr);
        ClearStack(interp,1);
        if (spretPtr == spretPtrOld) {
            Jsi_ValueMove(interp, _jsi_TOP, spretPtr);
            Jsi_DecrRefCount(interp, spretPtr);
        } else {
            /*  returning a (non-copied) value reference */
            Jsi_DecrRefCount(interp, _jsi_TOP);
            _jsi_TOP = spretPtr;
        }
        Jsi_DecrRefCount(interp, fargs);
    }
    interp->curFunction = oldCurFunc;
    return excpt_ret;
}

static INLINE Jsi_RC jsi_PushVar(jsi_Pstate *ps, jsi_OpCode *ip, jsi_ScopeChain *scope, Jsi_Value *currentScope, int context_id) {
    register Jsi_Interp *interp = ps->interp;
    FastVar *fvar = (FastVar *)ip->data;
    SIGASSERT(fvar,FASTVAR);
    Jsi_Value **dvPtr = &_jsi_STACKIDX(interp->framePtr->Sp), *dv = *dvPtr, *v = NULL;
    if (fvar->context_id == context_id && fvar->ps == ps) {
        v = fvar->var.lval;
    } else {
        char *varname = fvar->var.varname;
        v = Jsi_ValueObjLookup(interp, currentScope, varname, 1);
        if (v) {
            fvar->local = 1;
            if (v->vt == JSI_VT_UNDEF) {
                v->d.lookupFail = varname;
                v->f.bits.lookupfailed = 1;
            }
        } else {
            v = jsi_ScopeChainObjLookupUni(scope, varname);
            if (v) 
                fvar->local = 1;
            else {
                /* add to global scope.  TODO: do not define if a right_val??? */
                Jsi_Value *global_scope = scope->chains_cnt > 0 ? scope->chains[0]:currentScope;
                Jsi_Value key = VALINIT, *kPtr = &key; // Note: a string key so no reset needed.
                Jsi_ValueMakeStringKey(interp, &kPtr, varname);
                v = jsi_ValueObjKeyAssign(interp, global_scope, &key, NULL, JSI_OM_DONTENUM);
                if (v->vt == JSI_VT_UNDEF) {
                    v->d.lookupFail = varname;
                    v->f.bits.lookupfailed = 1;
                }
                jsi_ValueDebugLabel(v, "var", varname);
                bool isNew;
                Jsi_HashEntry *hPtr = Jsi_HashEntryNew(interp->varTbl, varname, &isNew);
                if (hPtr && isNew)
                    Jsi_HashValueSet(hPtr, 0);
            }
        }
        
        Jsi_IncrRefCount(interp, v);
        //if (v->vt == JSI_VT_OBJECT && v->d.obj->ot == JSI_OT_OBJECT)
        //    v->f.bits.onstack = 1;  /* Indicate that a double free is required for object. */

    }
    if (dv != v && (dv->vt != JSI_VT_VARIABLE || dv->d.lval != v)) {
        if (dv->vt != JSI_VT_VARIABLE)
            Jsi_ValueReset(interp, dvPtr);
        dv->vt = JSI_VT_VARIABLE;
        SIGASSERT(v, VALUE);
        dv->d.lval = v;
        dv->f.bits.local = (fvar->local);
        //Jsi_IncrRefCount(interp, v);
    }
    SIGASSERT(v, VALUE);
    jsiPush(interp,1);
    return JSI_OK;
}

static INLINE void jsi_PushFunc(jsi_Pstate *ps, jsi_OpCode *ip, jsi_ScopeChain *scope, Jsi_Value *currentScope) {
    /* TODO: now that we're caching ps, may need to reference function ps for context_id??? */
    Jsi_Interp *interp = ps->interp;
    Jsi_FuncObj *fo = jsi_FuncObjNew(interp, (Jsi_Func *)ip->data);
    fo->scope = jsi_ScopeChainDupNext(interp, scope, currentScope);
    Jsi_Obj *obj = Jsi_ObjNewType(interp, JSI_OT_FUNCTION);
    obj->d.fobj = fo;
    
    Jsi_Value *v = _jsi_STACKIDX(interp->framePtr->Sp), *fun_prototype = jsi_ObjValueNew(interp);
    fun_prototype->d.obj->__proto__ = interp->Object_prototype;                
    Jsi_ValueMakeObject(interp, &v, obj);
    Jsi_ValueInsert(interp, v, "prototype", fun_prototype, JSI_OM_DONTDEL|JSI_OM_DONTENUM);
    /* TODO: make own prototype and prototype.constructor */
    
    bool isNew;
    Jsi_HashEntry *hPtr;  Jsi_Value *vv;
    if (interp->framePtr->Sp == 1 && (vv=_jsi_STACKIDX(0))->vt == JSI_VT_VARIABLE) {
        const char *varname = NULL;
        vv = vv->d.lval;
        if (vv && vv->f.bits.lookupfailed && vv->d.lookupFail) {
            varname = vv->d.lookupFail;
            vv->f.bits.lookupfailed = 0;
        }
        if (varname) {
            if (!fo->func->name)
                fo->func->name = varname;
            hPtr = Jsi_HashEntryNew(interp->varTbl, varname, &isNew);
            if (hPtr)
                Jsi_HashValueSet(hPtr, obj);
        }
    }
    hPtr = Jsi_HashEntryNew(interp->funcObjTbl, fo, &isNew);
    if (hPtr && isNew) {
        Jsi_ObjIncrRefCount(interp, obj);
        Jsi_HashValueSet(hPtr, obj);
    }
    jsiPush(interp,1);
}

static Jsi_RC evalSubscript(Jsi_Interp *interp, Jsi_Value *src, Jsi_Value *idx, jsi_OpCode *ip,  jsi_OpCode *end,
    Jsi_Value *currentScope)
{
    Jsi_RC rc = JSI_OK;
    VarDeref(interp,2);
    int isnull;
    if ((isnull=Jsi_ValueIsNull(interp, src)) || Jsi_ValueIsUndef(interp, src)) {
        Jsi_LogError("invalid subscript of %s", (isnull?"null":"undefined"));
        jsiPop(interp, 1);
        return JSI_ERROR;
    }
    Jsi_String *str = jsi_ValueString(src);
    if (str && Jsi_ValueIsNumber(interp, idx)) {
        int bLen, cLen;
        char bbuf[10], *cp = Jsi_ValueString(interp, src, &bLen);
        int n = (int)idx->d.num;
        cLen = bLen;
#if JSI__UTF8
        if (str->flags&JSI_IS_UTF || !(str->flags&JSI_UTF_CHECKED)) {
            cLen = Jsi_NumUtfChars(cp, -1);
            str->flags |= JSI_UTF_CHECKED;
            if (cLen != bLen)
                str->flags |= JSI_IS_UTF;
        }
#endif
        if (n<0 || n>=cLen) {
            Jsi_ValueMakeUndef(interp, &src);
        } else {
            if (cLen != bLen)
                Jsi_UtfGetIndex(cp, n, bbuf);
            else {
                bbuf[0] = cp[n];
                bbuf[1] = 0;
            }
            Jsi_ValueMakeStringDup(interp, &src, bbuf);
        }
        jsiPop(interp, 1);
        return rc;
    }
    Jsi_ValueToObject(interp, src);
    if (interp->hasCallee && (src->d.obj == currentScope->d.obj || (interp->framePtr->arguments && src->d.obj == interp->framePtr->arguments->d.obj))) {
        if (idx->vt == JSI_VT_STRING && Jsi_Strcmp(idx->d.s.str, "callee") == 0) {
            ClearStack(interp,1);
            Jsi_ValueMakeStringKey(interp, &idx, "\1callee\1");
        }
    }
    int bsc = Jsi_ValueIsObjType(interp, src, JSI_OT_NUMBER); // Previous bad subscript.
    if (bsc == 0 && interp->lastSubscriptFail && interp->lastSubscriptFail->vt != JSI_VT_UNDEF)
        Jsi_ValueReset(interp, &interp->lastSubscriptFail);

    if (src->vt != JSI_VT_UNDEF) {
        Jsi_Value res = VALINIT, *resPtr = &res;
        jsi_ValueSubscriptLen(interp, src, idx, &resPtr, (uintptr_t)ip->data);
        //int isfcall = ((ip+3)<end && ip[2].op == OP_FCALL);
        if (res.vt == JSI_VT_UNDEF && bsc == 0) {
            /* eg. so we can list available commands for  "db.xx()" */
            if (idx->vt == JSI_VT_STRING)
                interp->lastSubscriptFailStr = idx->d.s.str;
            Jsi_ValueDup2(interp, &interp->lastSubscriptFail, src);
        }
        ClearStack(interp,2);
        if (resPtr == &res) {
            Jsi_ValueCopy(interp, src, &res); /*TODO: need to rethink this. */
        } else
            Jsi_ValueMove(interp, src, resPtr); 
        if (res.vt == JSI_VT_OBJECT || res.vt == JSI_VT_STRING)  // TODO:*** Undo using ValueCopy twice. ***
            Jsi_ValueReset(interp, &resPtr);
    }
    jsiPop(interp, 1);
    return rc;
}

void jsi_Debugger(void) {
}

Jsi_RC _jsi_evalcode(jsi_Pstate *ps, Jsi_OpCodes *opcodes, 
     jsi_ScopeChain *scope, Jsi_Value *currentScope,
     Jsi_Value *_this, Jsi_Value *vret)
{
    register Jsi_Interp* interp = ps->interp;
    jsi_OpCode *ip = &opcodes->codes[0];
    Jsi_RC rc = JSI_OK;
    int curLine = 0;
    int context_id = ps->_context_id++, lop = -1;
    jsi_OpCode *end = &opcodes->codes[opcodes->code_len];
    TryList  *trylist = NULL;
    bool strict = jsi_IsStrictMode(interp);
    const char *curFile = NULL;
    
    if (currentScope->vt != JSI_VT_OBJECT) {
        Jsi_LogBug("Eval: current scope is not a object");
        return JSI_ERROR;
    }
    
    while(ip < end && rc == JSI_OK) {
        int plop = ip->op;

        if (ip->logflag) { // Mask out LogDebug, etc if not enabled.
            interp->curIp = ip;
            switch (ip->logflag) {
                case jsi_Oplf_assert:
                    if (!interp->asserts) {
                        ip++;
                        if (ip->logflag != jsi_Oplf_assert && (ip->op == OP_POP || ip->op == OP_RET))
                            ip++;
                        continue;
                    }
                    break;
                case jsi_Oplf_debug:
                    if (!interp->logOpts.debug && !(interp->framePtr->logflag &(1<<jsi_Oplf_debug))) {
                        ip++;
                        if (ip->logflag != jsi_Oplf_debug && (ip->op == OP_POP || ip->op == OP_RET))
                            ip++;
                        continue;
                    }
                    break;
                case jsi_Oplf_test:
                    if (!interp->logOpts.test && !(interp->framePtr->logflag &(1<<jsi_Oplf_test))) {
                        ip++;
                        if (ip->logflag != jsi_Oplf_test && (ip->op == OP_POP || ip->op == OP_RET))
                            ip++;
                        continue;
                    }
                    break;
                case jsi_Oplf_trace:
                    if (!interp->logOpts.trace && !(interp->framePtr->logflag &(1<<jsi_Oplf_trace))) {
                        ip++;
                        if (ip->logflag != jsi_Oplf_trace && (ip->op == OP_POP || ip->op == OP_RET))
                            ip++;
                        continue;
                    }
                    break;
                default:
                    break;
            }
        }
        if (interp->exited) {
            rc = JSI_ERROR;
            break;
        }
        interp->opCnt++;
        if (interp->maxOpCnt && interp->opCnt > interp->maxOpCnt) {
            rc = Jsi_LogError("Exceeded execution cap: %d", interp->opCnt);
        }
        if (interp->level > interp->maxDepth) {
            rc = Jsi_LogError("Exceeded call depth: %d", interp->level);
            break;
        }
        if (interp->opTrace) {
            DumpInstr(interp, ps, _this, trylist, ip, opcodes);
        }
        if (interp->parent && interp->busyCallback && (interp->opCnt%(interp->busyInterval<=0?100000:interp->busyInterval))==0) {
            // TODO: use actual time interval rather than opCnt.
            if (interp->busyCallback[0]==0 || !Jsi_Strcmp(interp->busyCallback, "update"))
                Jsi_EventProcess(interp->parent, -1);
            else {
                Jsi_DString nStr;
                Jsi_DSInit(&nStr);
                Jsi_DSPrintf(&nStr, "[\"#Interp_%d\", %d]", interp->objId, interp->opCnt);
                if (Jsi_CommandInvokeJSON(interp->parent, interp->busyCallback, Jsi_DSValue(&nStr), NULL) != JSI_OK)
                    rc = JSI_ERROR;
                Jsi_DSFree(&nStr);
            }
        }
        ip->hit=1;
#ifndef USE_STATIC_STACK
        if ((interp->maxStack-interp->framePtr->Sp)<STACK_MIN_PAD)
            jsiSetupStack(interp);
#endif
        jsiPush(interp,0);
        interp->curIp = ip;
        // Fill in line/file info from previous OPs.
        if (!ip->fname) {
            ip->Line = curLine;
            ip->fname = curFile;
        } else {
            curLine = ip->Line;
            curFile = ip->fname;
        }
        if (interp->debugOpts.hook) {
            interp->framePtr->fileName = ip->fname;
            interp->framePtr->line = ip->Line;
            if ((rc = (*interp->debugOpts.hook)(interp, ip->fname, ip->Line, interp->framePtr->level, interp->curFunction, jsi_opcode_string(ip->op), ip, NULL)) != JSI_OK)
                break;
        }

        switch(ip->op) {
            case OP_NOP:
            case OP_LASTOP:
                break;
            case OP_PUSHUND:
                Jsi_ValueMakeUndef(interp, &_jsi_STACKIDX(interp->framePtr->Sp));
                jsiPush(interp,1);
                break;
            case OP_PUSHNULL:
                Jsi_ValueMakeNull(interp, &_jsi_STACKIDX(interp->framePtr->Sp));
                jsiPush(interp,1);
                break;
            case OP_PUSHBOO:
                Jsi_ValueMakeBool(interp, &_jsi_STACKIDX(interp->framePtr->Sp), (uintptr_t)ip->data);
                jsiPush(interp,1);
                break;
            case OP_PUSHNUM:
                Jsi_ValueMakeNumber(interp, &_jsi_STACKIDX(interp->framePtr->Sp), (*((Jsi_Number *)ip->data)));
                jsiPush(interp,1);
                break;
            case OP_PUSHSTR: {
                Jsi_ValueMakeStringKey(interp,&_jsi_STACKIDX(interp->framePtr->Sp), (char*)ip->data);
                interp->lastPushStr = Jsi_ValueString(interp, _jsi_STACKIDX(interp->framePtr->Sp), NULL);
                jsiPush(interp,1);
                break;
            }
            case OP_PUSHVAR: {
                rc = jsi_PushVar(ps, ip, scope, currentScope, context_id);      
                break;
            }
            case OP_PUSHFUN: {
                jsi_PushFunc(ps, ip, scope, currentScope);
                break;
            }
            case OP_NEWFCALL:
                if (interp->maxUserObjs && interp->userObjCnt > interp->maxUserObjs) {
                    rc = Jsi_LogError("Max 'new' count exceeded");
                    break;
                }
            case OP_FCALL: {
                /* TODO: need reliable way to capture func string name to handle unknown functions.*/
                int discard = ((ip+2)<end && ip[1].op == OP_POP);
                if (jsiEvalFunction(ps, ip, discard) != JSI_OK) {        /* throw an execption */
                    do_throw("fcall");

                }
                strict = jsi_IsStrictMode(interp);
                /* TODO: new Function return a function without scopechain, add here */
                break;
            }
            case OP_SUBSCRIPT: {
                rc = evalSubscript(interp, _jsi_TOQ, _jsi_TOP, ip, end, currentScope);
                break;
            }
            case OP_ASSIGN: {
                if ((uintptr_t)ip->data == 1) {
                    VarDeref(interp,1);
                    rc = jsi_ValueAssign(interp, _jsi_TOQ, _jsi_TOP, lop);                    
                    jsiPop(interp,1);
                } else {
                    VarDeref(interp, 3);
                    Jsi_Value *v3 = _jsi_STACKIDX(interp->framePtr->Sp-3);
                    if (v3->vt == JSI_VT_OBJECT) {
                        jsi_ValueObjKeyAssign(interp, v3, _jsi_TOQ, _jsi_TOP, 0);
                        jsi_ValueDebugLabel(_jsi_TOP, "assign", NULL);
                    } else if (strict)
                        Jsi_LogWarn("assign to a non-exist object");
                    ClearStack(interp,3);
                    Jsi_ValueCopy(interp,v3, _jsi_TOP);
                    jsiPop(interp, 2);
                }
                break;
            }
            case OP_PUSHREG: {
                Jsi_Obj *obj = Jsi_ObjNewType(interp, JSI_OT_REGEXP);
                obj->d.robj = (Jsi_Regex *)ip->data;
                Jsi_ValueMakeObject(interp, &_jsi_STACKIDX(interp->framePtr->Sp), obj);
                jsiPush(interp,1);
                break;
            }
            case OP_PUSHARG:
                //Jsi_ValueCopy(interp,_jsi_STACKIDX(interp->framePtr->Sp), currentScope);
                
                if (!interp->framePtr->arguments) {
                    interp->framePtr->arguments = Jsi_ValueNewObj(interp,
                        Jsi_ObjNewArray(interp, currentScope->d.obj->arr, currentScope->d.obj->arrCnt, 0));
                    Jsi_IncrRefCount(interp, interp->framePtr->arguments);
                    if (interp->hasCallee) {
                        Jsi_Value *callee = Jsi_ValueObjLookup(interp, currentScope, "\1callee\1", 0);
                        if (callee)
                            Jsi_ValueInsert(interp, interp->framePtr->arguments, "\1callee\1", callee, JSI_OM_DONTENUM);
                    }
                    // interp->framePtr->arguments->d.obj->__proto__ = interp->Object_prototype; // ecma
                }
                Jsi_ValueCopy(interp,_jsi_STACKIDX(interp->framePtr->Sp), interp->framePtr->arguments);
                jsiPush(interp,1);
                break;
            case OP_PUSHTHS: //TODO: Value copy can cause memory leak!
                Jsi_ValueCopy(interp,_jsi_STACKIDX(interp->framePtr->Sp), _this);
                jsiPush(interp,1);
                break;
            case OP_PUSHTOP:
                Jsi_ValueCopy(interp,_jsi_STACKIDX(interp->framePtr->Sp), _jsi_TOP);
                jsiPush(interp,1);
                break;
            case OP_UNREF:
                VarDeref(interp,1);
                break;
            case OP_PUSHTOP2:
                Jsi_ValueCopy(interp, _jsi_STACKIDX(interp->framePtr->Sp), _jsi_TOQ);
                Jsi_ValueCopy(interp, _jsi_STACKIDX(interp->framePtr->Sp+1), _jsi_TOP);
                jsiPush(interp, 2);
                break;
            case OP_CHTHIS: {
                if (ip->data) {
                    int t = interp->framePtr->Sp - 2;
                    Assert(t>=0);
                    Jsi_Value *v = _jsi_THISIDX(t);
                    ClearThis(interp, t);
                    Jsi_ValueCopy(interp, v, _jsi_TOQ);
                    if (v->vt == JSI_VT_VARIABLE) {
                        Jsi_ValueCopy(interp, v, v->d.lval);
                    }
                    Jsi_ValueToObject(interp, v);
                }
                break;
            }
            case OP_LOCAL: {
                Jsi_Value key = VALINIT, *kPtr = &key; // Note we use a string key so no reset needed.
                Jsi_ValueMakeStringKey(interp, &kPtr, (char*)ip->data);
                jsi_ValueObjKeyAssign(interp, currentScope, kPtr, NULL, JSI_OM_DONTENUM);
                context_id = ps->_context_id++;
                break;
            }
            case OP_POP:
                if ((interp->evalFlags&JSI_EVAL_RETURN) && (ip+1) >= end && 
                (Jsi_ValueIsObjType(interp, _jsi_TOP, JSI_OT_ITER)==0 &&
                Jsi_ValueIsObjType(interp, _jsi_TOP, JSI_OT_FUNCTION)==0)) {
                    /* Interactive and last instruction is a pop: save result. */
                    Jsi_ValueMove(interp, vret,_jsi_TOP); /*TODO***: correct ***/
                    _jsi_TOP->vt = JSI_VT_UNDEF;
                }
                jsiPop(interp, (uintptr_t)ip->data);
                break;
            case OP_NEG:
                VarDeref(interp,1);
                Jsi_ValueToNumber(interp, _jsi_TOP);
                rc = _jsi_StrictChk(_jsi_TOP);
                _jsi_TOP->d.num = -(_jsi_TOP->d.num);
                break;
            case OP_POS:
                VarDeref(interp,1);
                Jsi_ValueToNumber(interp, _jsi_TOP);
                rc = _jsi_StrictChk(_jsi_TOP);
                break;
            case OP_NOT: {
                int val = 0;
                VarDeref(interp,1);
                
                val = Jsi_ValueIsTrue(interp, _jsi_TOP);
                
                ClearStack(interp,1);
                Jsi_ValueMakeBool(interp, &_jsi_TOP, !val);
                break;
            }
            case OP_BNOT: {
                VarDeref(interp,1);
                jsi_ValueToOInt32(interp, _jsi_TOP);
                rc = _jsi_StrictChk(_jsi_TOP);
                _jsi_TOP->d.num = (Jsi_Number)(~((int)_jsi_TOP->d.num));
                break;
            }
            case OP_ADD: {
                VarDeref(interp,2);
                Jsi_Value *v, *v1 = _jsi_TOP, *v2 = _jsi_TOQ;
                int l1, l2;
                if (strict)
                    if (Jsi_ValueIsUndef(interp, v1) || Jsi_ValueIsUndef(interp, v2)) {
                        rc = Jsi_LogError("operand value to + is undefined");
                        break;
                    }
                char *s1 = Jsi_ValueString(interp, v1, &l1);
                char *s2 = Jsi_ValueString(interp, v2, &l2);
                if (s1 || s2) {
                    char *str;
                    if (!(s1 && s2)) {
                        v = (s1 ? v2 : v1);
                        jsi_ValueToPrimitive(interp, &v);
                        Jsi_ValueToString(interp, v, NULL);
                        str = Jsi_ValueString(interp, v, (s1?&l2:&l1));
                        if (s1) s2 = str; else s1 = str;
                    }
                    Assert(l1>=0 && l1<=JSI_MAX_ALLOC_BUF);
                    Assert(l2>=0 && l2<=JSI_MAX_ALLOC_BUF);
                    str = (char*)Jsi_Malloc(l1+l2+1);
                    memcpy(str, s2, l2);
                    memcpy(str+l2, s1, l1);
                    str[l1+l2] = 0;
                    ClearStack(interp,2);
                    Jsi_ValueMakeString(interp, &v2, str);
                } else {
                    Jsi_ValueToNumber(interp, v1);
                    Jsi_ValueToNumber(interp, v2);
                    rc = _jsi_StrictChk2(v1, v2);
                    Jsi_Number n = v1->d.num + v2->d.num;
                    ClearStack(interp,2);
                    Jsi_ValueMakeNumber(interp, &v2, n);
                }
                jsiPop(interp,1);
                break;
            }
            case OP_IN: {
                Jsi_Value *v, *vl;
                const char *cp = NULL;
                Jsi_Number nval;
                VarDeref(interp,2);
                vl = _jsi_TOQ;
                v = _jsi_TOP;
                if (Jsi_ValueIsString(interp,vl))
                    cp = Jsi_ValueGetStringLen(interp, vl, NULL);
                else if (Jsi_ValueIsNumber(interp,vl))
                    Jsi_ValueGetNumber(interp, vl, &nval);
                else {
                    if (strict)
                        Jsi_LogWarn("expected string or number before IN");
                    Jsi_ValueMakeBool(interp, &_jsi_TOQ, 0);
                    jsiPop(interp,1);
                    break;
                }
                
                if (v->vt == JSI_VT_VARIABLE) {
                    v = v->d.lval;
                    SIGASSERT(v, VALUE);
                }
                if (v->vt != JSI_VT_OBJECT || v->d.obj->ot != JSI_OT_OBJECT) {
                    if (strict)
                        Jsi_LogWarn("expected object after IN");
                    Jsi_ValueMakeBool(interp, &_jsi_TOQ, 0);
                    jsiPop(interp,1);
                    break;
                }
                int bval = 0;
                char nbuf[100];
                Jsi_Value *vv;
                Jsi_Obj *obj = v->d.obj;
                if (!cp) {
                    snprintf(nbuf, sizeof(nbuf), "%d", (int)nval);
                    cp = nbuf;
                }
                if (obj->arr) {
                    vv = jsi_ObjArrayLookup(interp, obj, (char*)cp);
                } else {
                    vv = Jsi_TreeObjGetValue(obj, (char*)cp, 1);
                }
                bval = (vv != 0);
                Jsi_ValueMakeBool(interp, &_jsi_TOQ, bval);
                jsiPop(interp,1);
                break;
            }
            case OP_SUB: 
                common_math_opr(-); break;
            case OP_MUL:
                common_math_opr(*); break;
            case OP_DIV:
                common_math_opr(/); break;
            case OP_MOD: {
                VarDeref(interp,2);
                if (!Jsi_ValueIsType(interp,_jsi_TOP, JSI_VT_NUMBER))
                    Jsi_ValueToNumber(interp, _jsi_TOP);
                if (!Jsi_ValueIsType(interp,_jsi_TOQ, JSI_VT_NUMBER))
                    Jsi_ValueToNumber(interp, _jsi_TOQ);
                rc = _jsi_StrictChk2(_jsi_TOP,_jsi_TOQ);
                _jsi_TOQ->d.num = fmod(_jsi_TOQ->d.num, _jsi_TOP->d.num);
                jsiPop(interp,1);
                break;
            }
            case OP_LESS:
                VarDeref(interp,2);
                rc = logic_less(interp,2,1);
                jsiPop(interp,1);
                break;
            case OP_GREATER:
                VarDeref(interp,2);
                rc = logic_less(interp,1,2);
                jsiPop(interp,1);
                break;
            case OP_LESSEQU:
                VarDeref(interp,2);
                rc = logic_less(interp,1,2);
                _jsi_TOQ->d.val = !_jsi_TOQ->d.val;
                jsiPop(interp,1);
                break;
            case OP_GREATEREQU:
                VarDeref(interp,2);
                rc = logic_less(interp,2,1);
                _jsi_TOQ->d.val = !_jsi_TOQ->d.val;
                jsiPop(interp,1);
                break;
            case OP_EQUAL:
            case OP_NOTEQUAL: {
                VarDeref(interp,2);
                int r = Jsi_ValueCmp(interp, _jsi_TOP, _jsi_TOQ, 0);
                r = (ip->op == OP_EQUAL ? !r : r);
                ClearStack(interp,2);
                Jsi_ValueMakeBool(interp, &_jsi_TOQ, r);
                jsiPop(interp,1);
                break;
            }
            case OP_STRICTEQU:
            case OP_STRICTNEQ: {
                int r = 0;
                VarDeref(interp,2);
                rc = _jsi_StrictUChk3(_jsi_TOQ, _jsi_TOP);
                r = !Jsi_ValueIsEqual(interp, _jsi_TOP, _jsi_TOQ);
                r = (ip->op == OP_STRICTEQU ? !r : r);
                ClearStack(interp,2);
                Jsi_ValueMakeBool(interp, &_jsi_TOQ, r);
                jsiPop(interp,1);
                break;
            }
            case OP_BAND: 
                common_bitwise_opr(&); break;
            case OP_BOR:
                common_bitwise_opr(|); break;
            case OP_BXOR:
                common_bitwise_opr(^); break;
            case OP_SHF: {
                VarDeref(interp,2);
                jsi_ValueToOInt32(interp, _jsi_TOQ);
                jsi_ValueToOInt32(interp, _jsi_TOP);
                int t1 = (int)_jsi_TOQ->d.num;
                int t2 = ((unsigned int)_jsi_TOP->d.num) & 0x1f;
                if (ip->data) {                 /* shift right */
                    if ((uintptr_t)ip->data == 2) {   /* unsigned shift */
                        unsigned int t3 = (unsigned int)t1;
                        t3 >>= t2;
                        Jsi_ValueMakeNumber(interp, &_jsi_TOQ, t3);
                    } else {
                        t1 >>= t2;
                        Jsi_ValueMakeNumber(interp, &_jsi_TOQ, t1);
                    }
                } else {
                    t1 <<= t2;
                    Jsi_ValueMakeNumber(interp, &_jsi_TOQ, t1);
                }
                jsiPop(interp,1);
                break;
            }
            case OP_KEY: {
                VarDeref(interp,1);
                if (ip->isof && !Jsi_ValueIsArray(interp, _jsi_TOP)) {
                    rc = Jsi_LogError("operand not an array");
                    break;
                }
                if (_jsi_TOP->vt != JSI_VT_UNDEF && _jsi_TOP->vt != JSI_VT_NULL)
                    Jsi_ValueToObject(interp, _jsi_TOP);
                Jsi_Value *spret = Jsi_ValueNew1(interp);
                jsi_ValueObjGetKeys(interp, _jsi_TOP, spret, ip->isof);
                Jsi_ValueReplace(interp, _jsi_STACK+interp->framePtr->Sp, spret);  
                Jsi_DecrRefCount(interp, spret);  
                jsiPush(interp,1);
                break;
            }
            case OP_NEXT: {
                Jsi_Value *toq = _jsi_TOQ, *top = _jsi_TOP;
                if (toq->vt != JSI_VT_OBJECT || toq->d.obj->ot != JSI_OT_ITER) Jsi_LogBug("next: toq not a iter\n");
                if (top->vt != JSI_VT_VARIABLE) {
                    rc = Jsi_LogError ("invalid for/in left hand-side");
                    break;
                }
                if (strict && top->f.bits.local==0) {
                    const char *varname = "";
                    Jsi_Value *v = top->d.lval;
                    if (v->f.bits.lookupfailed)
                        varname = v->d.lookupFail;

                    rc = Jsi_LogError("function created global: \"%s\"", varname);
                    break;
                }
                
                Jsi_IterObj *io = toq->d.obj->d.iobj;
                if (io->iterCmd) {
                    io->iterCmd(io, top, _jsi_STACKIDX(interp->framePtr->Sp-3), io->iter++);
                } else {
                    while (io->iter < io->count) {
                        if (!io->isArrayList) {
                            if (Jsi_ValueKeyPresent(interp, _jsi_STACKIDX(interp->framePtr->Sp-3), io->keys[io->iter],1)) 
                                break;
                        } else {
                            while (io->cur < io->obj->arrCnt) {
                                if (io->obj->arr[io->cur]) break;
                                io->cur++;
                            }
                            if (io->cur >= io->obj->arrCnt) {
                                /* TODO: Is this really a bug??? */
                                /* Jsi_LogBug("NOT FOUND LIST ARRAY");*/
                                io->iter = io->count;
                                break;
                            } else if (io->obj->arr[io->cur]) {
                                io->cur++;
                                break;
                            }
                        }
                        io->iter++;
                    }
                    if (io->iter >= io->count) {
                        ClearStack(interp,1);
                        Jsi_ValueMakeNumber(interp, &_jsi_TOP, 0);
                    } else {
                        Jsi_Value **vPtr = &_jsi_TOP->d.lval, *v = *vPtr;
                        SIGASSERT(v, VALUE);
                        Jsi_ValueReset(interp, vPtr);
                        if (io->isArrayList) {
                            if (!io->isof)
                                Jsi_ValueMakeNumber(interp, &v, io->cur-1);
                            else if (!io->obj->arr[io->cur-1])
                                Jsi_ValueMakeNull(interp, &v);
                            else
                                Jsi_ValueCopy(interp, v, io->obj->arr[io->cur-1]);
                        } else
                            Jsi_ValueMakeStringKey(interp, &v, io->keys[io->iter]);
                        io->iter++;
                        
                        ClearStack(interp,1);
                        Jsi_ValueMakeNumber(interp, &_jsi_TOP, 1);
                    }
                    break;
                }
            }
            case OP_INC:
            case OP_DEC: {
                int inc = ip->op == OP_INC ? 1 : -1;
                
                if (_jsi_TOP->vt != JSI_VT_VARIABLE) {
                    rc = Jsi_LogError("operand not left value");
                    break;
                }
                Jsi_Value *v = _jsi_TOP->d.lval;
                SIGASSERT(v, VALUE);
                Jsi_ValueToNumber(interp, v);
                rc = _jsi_StrictChk(v);
                v->d.num += inc;
                    
                VarDeref(interp,1);
                if (ip->data) {
                    _jsi_TOP->d.num -= inc;
                }
                break;
            }
            case OP_TYPEOF: {
                const char *typ;
                Jsi_Value *v = _jsi_TOP;
                if (v->vt == JSI_VT_VARIABLE) {
                    v = v->d.lval;
                    SIGASSERT(v, VALUE);
                }
                typ = Jsi_ValueTypeStr(interp, v);
                VarDeref(interp,1);
                Jsi_ValueMakeStringKey(interp, &_jsi_TOP, (char*)typ);
                break;
            }
            case OP_INSTANCEOF: {

                VarDeref(interp,2);
                int bval = Jsi_ValueInstanceOf(interp, _jsi_TOQ, _jsi_TOP);
                jsiPop(interp,1);
                Jsi_ValueMakeBool(interp, &_jsi_TOP, bval);
                break;
            }
            case OP_JTRUE:
            case OP_JFALSE: 
            case OP_JTRUE_NP:
            case OP_JFALSE_NP: {
                VarDeref(interp,1);
                int off = (uintptr_t)ip->data - 1; 
                int r = Jsi_ValueIsTrue(interp, _jsi_TOP);
                
                if (ip->op == OP_JTRUE || ip->op == OP_JFALSE) jsiPop(interp,1);
                ip += ((ip->op == OP_JTRUE || ip->op == OP_JTRUE_NP) ^ r) ? 0 : off;
                break;
            }
            case OP_JMPPOP: 
                jsiPop(interp, ((jsi_JmpPopInfo *)ip->data)->topop);
            case OP_JMP: {
                int off = (ip->op == OP_JMP ? (uintptr_t)ip->data - 1
                            : (uintptr_t)((jsi_JmpPopInfo *)ip->data)->off - 1);

                while (1) {
                    if (trylist == NULL) break;
                    jsi_OpCode *tojmp = ip + off;

                    /* jmp out of a try block, should execute the finally block */
                    /* while jmp out a 'with' block, restore the scope */

                    if (trylist->type == TL_TRY) { 
                        if (tojmp >= trylist->d.td.tstart && tojmp < trylist->d.td.fend) break;
                        
                        if (ip >= trylist->d.td.tstart && ip < trylist->d.td.cend) {
                            trylist->d.td.last_op = LOP_JMP;
                            trylist->d.td.ld.tojmp = tojmp;
                            
                            ip = trylist->d.td.fstart - 1;
                            off = 0;
                            break;
                        } else if (ip >= trylist->d.td.fstart && ip < trylist->d.td.fend) {
                            pop_try(trylist);
                        } else Jsi_LogBug("jmp within a try, but not in its scope?");
                    } else {
                        /* with block */
                        
                        if (tojmp >= trylist->d.wd.wstart && tojmp < trylist->d.wd.wend) break;
                        
                        restore_scope();
                        pop_try(trylist);
                    }
                }
                
                ip += off;
                break;
            }
            case OP_EVAL: {
                int stackargc = (uintptr_t)ip->data;
                VarDeref(interp, stackargc);

                int r = 0;
                Jsi_Value *spPtr = Jsi_ValueNew1(interp);
                if (stackargc > 0) {
                    if (_jsi_STACKIDX(interp->framePtr->Sp - stackargc)->vt == JSI_VT_UNDEF) {
                        Jsi_LogError("undefined value to eval()");
                        goto undef_eval;
                    }
                    char *pro = Jsi_ValueString(interp, _jsi_STACKIDX(interp->framePtr->Sp - stackargc), NULL);
                    if (pro) {
                        pro = Jsi_Strdup(pro);
                        r = jsiEvalOp(interp, ps, pro, scope, currentScope, _this, &spPtr);
                        Jsi_Free(pro);
                    } else {
                        Jsi_ValueCopy(interp, spPtr, _jsi_STACKIDX(interp->framePtr->Sp - stackargc));
                    }
                }
undef_eval:
                jsiPop(interp, stackargc);
                Jsi_ValueCopy(interp, _jsi_STACK[interp->framePtr->Sp], spPtr); /*TODO: is this correct?*/
                Jsi_DecrRefCount(interp, spPtr);
                jsiPush(interp,1);

                if (r) {
                    do_throw("eval");
                }
                break;
            }
            case OP_RET: {
                if (interp->framePtr->Sp>=1 && ip->data) {
                    VarDeref(interp,1);
                    Jsi_ValueMove(interp, vret, _jsi_TOP);
                }
                jsiPop(interp, (uintptr_t)ip->data);
                interp->didReturn = 1;
                if (interp->framePtr->withDepth) {
                    rc=Jsi_LogError("return inside 'with' is not supported in Jsi");
                    goto done;
                }
                if (trylist) {
                    int isTry = 0;
                    while (trylist) {
                        if (trylist->type == TL_TRY && trylist->inCatch)
                            restore_scope();
                        pop_try(trylist);
                    }
                    if (isTry)
                        rc = Jsi_LogError("return inside 'try/catch' is not supported in Jsi");
                    goto done;
                }
                ip = end;
                break;
            }
            case OP_DELETE: {
                int count = (uintptr_t)ip->data;
                if (count == 1) { // Non-standard.
                    if (_jsi_TOP->vt != JSI_VT_VARIABLE)
                        rc = Jsi_LogError("delete a right value");
                    else {
                        Jsi_Value **vPtr = &_jsi_TOP->d.lval, *v = *vPtr;
                        SIGASSERT(v, VALUE);
                        if (v->f.bits.dontdel) {
                            if (strict) rc = Jsi_LogWarn("delete not allowed");
                        } else if (v != currentScope) {
                            Jsi_ValueReset(interp,vPtr);     /* not allow to delete arguments */
                        }
                        else if (strict)
                            Jsi_LogWarn("Delete arguments");
                    }
                    jsiPop(interp,1);
                } else if (count == 2) {
                    VarDeref(interp,2);
                    assert(interp->framePtr->Sp>=2);
                    if (strict) {
                        if (_jsi_TOQ->vt != JSI_VT_OBJECT) Jsi_LogWarn("delete non-object key, ignore");
                        if (_jsi_TOQ->d.obj == currentScope->d.obj) Jsi_LogWarn("Delete arguments");
                    }
                    ValueObjDelete(interp, _jsi_TOQ, _jsi_TOP, 0);
                    
                    jsiPop(interp,2);
                } else Jsi_LogBug("delete");
                break;
            }
            case OP_OBJECT: {
                int itemcount = (uintptr_t)ip->data;
                Assert(itemcount>=0);
                VarDeref(interp, itemcount * 2);
                Jsi_Obj *obj = Jsi_ObjNewObj(interp, _jsi_STACK+(interp->framePtr->Sp-itemcount*2), itemcount*2);
                jsiPop(interp, itemcount * 2 - 1);       /* one left */
                ClearStack(interp,1);
                Jsi_ValueMakeObject(interp, &_jsi_TOP, obj);
                break;
            }
            case OP_ARRAY: {
                int itemcount = (uintptr_t)ip->data;
                Assert(itemcount>=0);
                VarDeref(interp, itemcount);
                Jsi_Obj *obj = Jsi_ObjNewArray(interp, _jsi_STACK+(interp->framePtr->Sp-itemcount), itemcount, 1);
                jsiPop(interp, itemcount - 1);
                ClearStack(interp,1);
                Jsi_ValueMakeObject(interp, &_jsi_TOP, obj);
                break;
            }
            case OP_STRY: {
                jsi_TryInfo *ti = (jsi_TryInfo *)ip->data;
                TryList *n = trylist_new(TL_TRY, scope, currentScope);
                
                n->d.td.tstart = ip;                            /* make every thing pointed to right pos */
                n->d.td.tend = n->d.td.tstart + ti->trylen;
                n->d.td.cstart = n->d.td.tend + 1;
                n->d.td.cend = n->d.td.tend + ti->catchlen;
                n->d.td.fstart = n->d.td.cend + 1;
                n->d.td.fend = n->d.td.cend + ti->finallen;
                n->d.td.tsp = interp->framePtr->Sp;
                n->inCatch=0;
                n->inFinal=0;

                push_try(trylist, n);
                break;
            }
            case OP_ETRY: {             /* means nothing happen go to final */
                if (trylist == NULL || trylist->type != TL_TRY)
                    Jsi_LogBug("Unexpected ETRY opcode??");

                ip = trylist->d.td.fstart - 1;
                break;
            }
            case OP_SCATCH: {
                if (trylist == NULL || trylist->type != TL_TRY) 
                    Jsi_LogBug("Unexpected SCATCH opcode??");

                if (!ip->data) {
                    do_throw("catch");
                } else {
                    trylist->inCatch=1;
                    /* new scope and make var */
                    scope = jsi_ScopeChainDupNext(interp, scope, currentScope);
                    currentScope = jsi_ObjValueNew(interp);
                    interp->framePtr->ingsc = scope;  //TODO: changing frame
                    interp->framePtr->incsc = currentScope;
                    Jsi_IncrRefCount(interp, currentScope);
                    Jsi_Value *excpt = Jsi_ValueNew1(interp);
                    if (ps->last_exception && ps->last_exception->vt != JSI_VT_UNDEF) {
                        Jsi_Value *ple = ps->last_exception;
                        Jsi_ValueCopy(interp, excpt, ple);
                        Jsi_ValueReset(interp, &ps->last_exception);
                    } else if (interp->errMsgBuf[0]) {
                        Jsi_ValueMakeStringDup(interp, &excpt, interp->errMsgBuf);
                        interp->errMsgBuf[0] = 0;
                    }
                    Jsi_ValueInsert(interp, currentScope, (char*)ip->data, excpt, JSI_OM_DONTENUM);
                    Jsi_DecrRefCount(interp, excpt);
                    context_id = ps->_context_id++;
                }
                break;
            }
            case OP_ECATCH: {
                if (trylist == NULL || trylist->type != TL_TRY)
                    Jsi_LogBug("Unexpected ECATCH opcode??");

                trylist->inCatch=0;
                ip = trylist->d.td.fstart - 1;
                break;
            }
            case OP_SFINAL: {
                if (trylist == NULL || trylist->type != TL_TRY)
                    Jsi_LogBug("Unexpected SFINAL opcode??");

                /* restore scatch scope chain */
                trylist->inFinal = 1;
                restore_scope();
                break;
            }
            case OP_EFINAL: {
                if (trylist == NULL || trylist->type != TL_TRY)
                    Jsi_LogBug("Unexpected EFINAL opcode??");

                trylist->inFinal = 0;
                int last_op = trylist->d.td.last_op;
                jsi_OpCode *tojmp = (last_op == LOP_JMP ? trylist->d.td.ld.tojmp : 0);
                
                pop_try(trylist);

                if (last_op == LOP_THROW) {
                    do_throw("finally");
                } else if (last_op == LOP_JMP) {
                    while (1) {
                        if (trylist == NULL) {
                            ip = tojmp;
                            break;
                        }
                        /* same as jmp opcode, see above */
                        if (trylist->type == TL_TRY) {
                            if (tojmp >= trylist->d.td.tstart && tojmp < trylist->d.td.fend) {
                                ip = tojmp;
                                break;
                            }
                            
                            if (ip >= trylist->d.td.tstart && ip < trylist->d.td.cend) {
                                trylist->d.td.last_op = LOP_JMP;
                                trylist->d.td.ld.tojmp = tojmp;
                                
                                ip = trylist->d.td.fstart - 1;
                                break;
                            } else if (ip >= trylist->d.td.fstart && ip < trylist->d.td.fend) {
                                pop_try(trylist);
                            } else Jsi_LogBug("jmp within a try, but not in its scope?");
                        } else {        /* 'with' block */
                            if (tojmp >= trylist->d.wd.wstart && tojmp < trylist->d.wd.wend) {
                                ip = tojmp;
                                break;
                            }
                            restore_scope();
                            pop_try(trylist);
                        }
                    }
                }
                break;
            }
            case OP_THROW: {
                VarDeref(interp,1);
                Jsi_ValueDup2(interp,&ps->last_exception, _jsi_TOP);
                interp->didReturn = 1; /* TODO: could possibly hide _jsi_STACK problem */
                do_throw("throw");
                break;
            }
            case OP_WITH: {
                static int warnwith = 1;
                if (strict && warnwith && interp->typeCheck.nowith) {
                    warnwith = 0;
                    rc = Jsi_LogError("use of with is illegal due to \"use nowith\"");
                    break;
                }
                VarDeref(interp,1);
                Jsi_ValueToObject(interp, _jsi_TOP);
                
                TryList *n = trylist_new(TL_WITH, scope, currentScope);
                
                n->d.wd.wstart = ip;
                n->d.wd.wend = n->d.wd.wstart + (uintptr_t)ip->data;

                push_try(trylist, n);
                interp->framePtr->withDepth++;
                
                /* make expr to top of scope chain */
                scope = jsi_ScopeChainDupNext(interp, scope, currentScope);
                currentScope = Jsi_ValueNew1(interp);
                interp->framePtr->ingsc = scope;
                interp->framePtr->incsc = currentScope;
                Jsi_ValueCopy(interp, currentScope, _jsi_TOP);
                jsiPop(interp,1);
                
                context_id = ps->_context_id++;
                break;
            }
            case OP_EWITH: {
                if (trylist == NULL || trylist->type != TL_WITH)
                    Jsi_LogBug("Unexpected EWITH opcode??");

                restore_scope();
                
                pop_try(trylist);
                interp->framePtr->withDepth--;
                break;
            }
            case OP_DEBUG: {
                jsi_Debugger();
                jsiPush(interp,1);
                break;
            }
            case OP_RESERVED: {
                jsi_ReservedInfo *ri = (jsi_ReservedInfo *)ip->data;
                const char *cmd = ri->type == RES_CONTINUE ? "continue" : "break";
                /* TODO: continue/break out of labeled scope not working. */
                if (ri->label) {
                    Jsi_LogError("%s: label(%s) not found", cmd, ri->label);
                } else {
                    Jsi_LogError("%s must be inside loop(or switch)", cmd);
                }
                rc = JSI_ERROR;
                break;
            }
#ifndef __cplusplus
            default:
                Jsi_LogBug("invalid op ceod: %d", ip->op);
#endif
        }
        lop = plop;
        ip++;
    }
done:
    while (trylist) {
        pop_try(trylist);
    }
    return rc;
}

// Bottom-most eval() routine creates stack frame.
Jsi_RC jsi_evalcode(jsi_Pstate *ps, Jsi_Func *func, Jsi_OpCodes *opcodes, 
         jsi_ScopeChain *scope, Jsi_Value *fargs,
         Jsi_Value *_this,
         Jsi_Value **vret)
{
    Jsi_Interp *interp = ps->interp;
    if (interp->exited)
        return JSI_ERROR;
    Jsi_RC rc;
    jsi_Frame frame = *interp->framePtr;
    frame.parent = interp->framePtr;
    interp->framePtr = &frame;
    frame.parent->child = interp->framePtr = &frame;
    frame.ps = ps;
    frame.ingsc = scope;
    frame.incsc = fargs;
    frame.inthis = _this;
    frame.opcodes = opcodes;
    frame.fileName = interp->curFile;
    frame.funcName = interp->curFunction;
    frame.dirName = interp->curDir;
    frame.logflag = frame.parent->logflag;
    frame.level = frame.parent->level+1;
    frame.evalFuncPtr = func;
    frame.arguments = NULL;
   // if (func && func->strict)
    //    frame.strict = 1;
    if (interp->curIp)
        frame.parent->line = interp->curIp->Line;
    frame.ip = interp->curIp;
    interp->refCount++;
    interp->level++;
    Jsi_IncrRefCount(interp, fargs);
    rc = _jsi_evalcode(ps, opcodes, scope, fargs, _this, *vret);
    Jsi_DecrRefCount(interp, fargs);
    if (interp->didReturn == 0 && !interp->exited) {
        if ((interp->evalFlags&JSI_EVAL_RETURN)==0)
            Jsi_ValueMakeUndef(interp, vret);
        /*if (interp->framePtr->Sp != oldSp) //TODO: at some point after memory refs???
            Jsi_LogBug("Stack not balance after execute script");*/
    }
    if (frame.arguments)
        Jsi_DecrRefCount(interp, frame.arguments);
    interp->didReturn = 0;
    interp->refCount--;
    interp->level--;
    interp->framePtr = frame.parent;
    interp->framePtr->child = NULL;
    interp->curIp = frame.ip;
    if (interp->exited)
        rc = JSI_ERROR;
    return rc;
}

Jsi_RC jsi_JsPreprocessLine(Jsi_Interp* interp, char *buf, size_t bsiz, uint ilen, int jOpts[4], int lineNo) {
    if (interp->unitTest&1 && buf[0]==';' && buf[1] && buf[2]) {
        // Wrap ";XXX;" in a puts("XXX ==> ", XXX)
        if (!jOpts[0]) {
            if (!Jsi_Strcmp(buf, "=!EXPECTSTART!=\n") || !Jsi_Strcmp(buf, "=!INPUTSTART!=\n") ) {
                return JSI_OK;
            }
        } else {
            if (!Jsi_Strcmp(buf, "=!EXPECTEND!=\n") || !Jsi_Strcmp(buf, "=!INPUTEND!=\n")) {
                jOpts[0] = 0;
                return JSI_OK;
            }
        }
        if (buf[ilen-1]=='\n' && buf[ilen-2]==';' && (2*ilen+12)<bsiz) {
            if (Jsi_Strchr(buf, '"')) {
                return Jsi_LogError("double-quote is illegal in unitTest on line %d: %s", lineNo, buf);
            }
            char ubuf[bsiz], *ucp = ubuf;
            buf[ilen-=2] = 0;
            Jsi_Strcpy(ubuf, buf+1);
            while (*ucp && isspace(*ucp)) ucp++;
            if (ilen>2 && ucp[0]=='\'' && ubuf[ilen-2]=='\'')
                snprintf(buf, bsiz, "puts(\"%s\");\n", ucp);
            else
                snprintf(buf, bsiz, "puts(\"%s ==> \", %s);\n", ucp, ucp);
        }
    }
    else if (interp->jsppCallback) {
        Jsi_DString dStr = {};
        buf[ilen-1] = 0; // Remove newline for length of call.
        Jsi_Value *inStr = Jsi_ValueNewStringDup(interp, buf);
        Jsi_IncrRefCount(interp, inStr);
        if (Jsi_FunctionInvokeString(interp, interp->jsppCallback, inStr, &dStr) != JSI_OK) {
            Jsi_DSFree(&dStr);
            Jsi_DecrRefCount(interp, inStr);
            return JSI_ERROR;
        }
        Jsi_DecrRefCount(interp, inStr);
        Jsi_DSAppendLen(&dStr, "\n", 1);
        Jsi_Strncpy(buf, Jsi_DSValue(&dStr), bsiz);
        buf[bsiz-1] = 0;
        Jsi_DSFree(&dStr);
    }
    return JSI_OK;
}

Jsi_RC jsi_evalStrFile(Jsi_Interp* interp, Jsi_Value *path, char *str, int flags, int level)
{
    Jsi_Channel tinput = NULL, input = Jsi_GetStdChannel(interp, 0);
    Jsi_Value *npath = path;
    Jsi_RC rc = JSI_ERROR;
    const char *ustr = NULL;
    if (Jsi_MutexLock(interp, interp->Mutex) != JSI_OK)
        return rc;
    int oldSp, uskip = 0, fncOfs = 0;
    int oldef = interp->evalFlags;
    jsi_Pstate *oldps = interp->ps;
    const char *oldFile = interp->curFile;
    char *origFile = Jsi_ValueString(interp, path, NULL);
    const char *fname = origFile;
    char *oldDir = interp->curDir;
    char dirBuf[PATH_MAX];
    jsi_Pstate *ps = NULL;
    bool oldStrict = interp->framePtr->strict;
    
    oldSp = interp->framePtr->Sp;
    dirBuf[0] = 0;
    Jsi_DString dStr = {};
    Jsi_DSInit(&dStr);

    if (str == NULL) {
        if (fname != NULL) {
            char *cp;
            if (!Jsi_Strcmp(fname,"-"))
                input = Jsi_GetStdChannel(interp, 0);
            else {
    
                /* Use translated FileName. */
                if (interp->curDir && fname[0] != '/' && fname[0] != '~') {
                    char dirBuf2[PATH_MAX], *np;
                    snprintf(dirBuf, sizeof(dirBuf), "%s/%s", interp->curDir, fname);
                    if ((np=Jsi_FileRealpathStr(interp, dirBuf, dirBuf2)) == NULL) {
                        Jsi_LogError("Can not open '%s'", fname);
                        goto bail;
                    }
                    npath = Jsi_ValueNewStringDup(interp, np);
                    Jsi_IncrRefCount(interp, npath);
                    fname = Jsi_ValueString(interp, npath, NULL);
                    if (flags&JSI_EVAL_ARGV0) {
                        interp->argv0 = Jsi_ValueNewStringDup(interp, np);
                        Jsi_IncrRefCount(interp, interp->argv0);
                    }
                } else {
                    if (flags&JSI_EVAL_ARGV0) {
                        interp->argv0 = Jsi_ValueNewStringDup(interp, fname);
                        Jsi_IncrRefCount(interp, interp->argv0);
                    }
                }
                
                tinput = input = Jsi_Open(interp, npath, "r");
                if (!input) {
                    //Jsi_LogError("Can not open '%s'", fname);
                    goto bail;
                }
            }
            bool isNew;
            Jsi_HashEntry *hPtr;
            jsi_FileInfo *fi = NULL;
            hPtr = Jsi_HashEntryNew(interp->fileTbl, fname, &isNew);
            if (isNew == 0 && hPtr) {
                if ((flags & JSI_EVAL_ONCE)) {
                    rc = JSI_OK;
                    goto bail;
                }
                fi = (jsi_FileInfo *)Jsi_HashValueGet(hPtr);
                if (!fi) goto bail;
                interp->curFile = fi->fileName;
                interp->curDir = fi->dirName;
                
            } else {
                fi = (jsi_FileInfo *)Jsi_Calloc(1,sizeof(*fi));
                if (!fi) goto bail;
                Jsi_HashValueSet(hPtr, fi);
                fi->origFile = (char*)Jsi_KeyAdd(interp, origFile);
                interp->curFile = fi->fileName = (char*)Jsi_KeyAdd(interp, fname);
                char *dfname = Jsi_Strdup(fname);
                if ((cp = Jsi_Strrchr(dfname,'/')))
                    *cp = 0;
                interp->curDir = fi->dirName = (char*)Jsi_KeyAdd(interp, dfname);
                Jsi_Free(dfname);
            }
            if (!input->fname)
                input->fname = interp->curFile;

            int cnt = 0, noncmt = 0, jppOpts[4]={};
            uint ilen;
            char buf[8192];
            while (cnt<MAX_LOOP_COUNT) {
                if (!Jsi_Gets(input, buf, sizeof(buf)))
                    break;
                if (++cnt==1 && (!(flags&JSI_EVAL_NOSKIPBANG)) && (buf[0] == '#' && buf[1] == '!')) {
                    interp->typeCheck.run = 1;
                    Jsi_DSAppend(&dStr, "\n", NULL);
                    uskip=1;
                    continue;
                }
                if (!noncmt) {
                    int bi;
                    if (!buf[0] || (buf[0] == '/' && buf[1] == '/'))
                        goto cont;
                    for (bi=0; buf[bi]; bi++) if (!isspace(buf[bi])) break;
                    if (!buf[bi])
                        goto cont;
                }
                if (!noncmt++)
                    fncOfs = Jsi_DSLength(&dStr)-uskip;
                const char *jpp = interp->jsppChars;
                uint jlen;
                if (jpp || interp->unitTest)
                    ilen = Jsi_Strlen(buf);
                if ((interp->unitTest && buf[0]==';' && buf[1] && buf[2]) || (jpp && buf[0] && jpp[0] == buf[0] && ilen>2 && (buf[ilen-2]==jpp[0]
                    || ((jlen=Jsi_Strlen(jpp))<(ilen-3) && !Jsi_Strncmp(jpp, buf+ilen-jlen-1, jlen))))) {
                    if (jsi_JsPreprocessLine(interp, buf, sizeof(buf), ilen, jppOpts, cnt) != JSI_OK)
                        goto bail;
                }
cont:
                Jsi_DSAppend(&dStr, buf,  NULL);
            }
            if (cnt>=MAX_LOOP_COUNT)
                Jsi_LogError("source file too large");
            str = Jsi_DSValue(&dStr);

        }
        if (interp->curDir && (flags&JSI_EVAL_AUTOINDEX))
            Jsi_AddAutoFiles(interp, interp->curDir);
    }
    ustr = str + fncOfs;
    // See if "use XXX" is on first non // or empty line (or second if there is a #! on first line)
    if (ustr && *ustr && !Jsi_Strncmp(ustr+uskip, "\"use ", 5)) {
        ustr += 5+uskip;
        const char *cpe = ustr;
        while (*cpe && *cpe != '\"' && *cpe != '\n' && (isalpha(*cpe) || *cpe ==',' || *cpe =='!')) cpe++;
        if (*cpe == '\"') {
            Jsi_DString cStr;
            Jsi_DSInit(&cStr);
            cpe = Jsi_DSAppendLen(&cStr, ustr, (cpe-ustr));
            rc = jsi_ParseTypeCheckStr(interp, cpe);
            Jsi_DSFree(&cStr);
            if (rc != JSI_OK)
                goto bail;
        }
    }

    /* TODO: cleanup interp->framePtr->Sp stuff. */
    oldSp = interp->framePtr->Sp;
    // Evaluate code.
    rc = JSI_OK;
    ps = jsiNewParser(interp, str, input, 0);
    interp->evalFlags = flags;
    if (!ps)
        rc = JSI_ERROR;
    else {
        Jsi_ValueMakeUndef(interp, &interp->retPtr);
        interp->ps = ps;
        Jsi_Value *retPtr = interp->retPtr;
        bool strict = (jsi_GetDirective(interp, ps->opcodes, "use strict")!=NULL);
        const char *cext;
        if (!strict && fname && ((cext=Jsi_Strstr(fname,".jsi"))) && !cext[4])
            strict = 1;
        if (strict) {
            interp->framePtr->strict = 1;
            if (interp->framePtr->level<=1)
                interp->strict = 1;
        }
        const char *curFile = interp->curFile;
        if (!path && !Jsi_Strncmp(str, "Jsi_Main(", 9))
            interp->curFile = "<Jsi_Main>";

        if (level <= 1)
            rc = jsi_evalcode(ps, NULL, ps->opcodes, interp->gsc, interp->csc, interp->csc, &retPtr);
        else {
            jsi_Frame *fptr = interp->framePtr;
            while (fptr && fptr->level != level)
                fptr = fptr->parent;
            if (!fptr)
                rc = JSI_ERROR;
            else
                rc = jsi_evalcode(ps, NULL, ps->opcodes, fptr->ingsc, fptr->incsc, fptr->inthis, &retPtr);
        }
        interp->curFile = curFile;
        if (rc != JSI_OK)
            rc = JSI_ERROR;
        else
            Jsi_ValueDup2(interp, &oldps->last_exception, ps->last_exception); //TODO: dup even if null?
        interp->ps = oldps;
        interp->evalFlags = oldef;
    }
    
bail:
    interp->framePtr->strict = oldStrict;
    interp->curFile = oldFile;
    interp->curDir = oldDir;
    interp->framePtr->Sp = oldSp;
    if (path != npath)
        Jsi_DecrRefCount(interp, npath);
    Jsi_DSFree(&dStr);
    if (tinput)
        Jsi_Close(tinput);
    Jsi_MutexUnlock(interp, interp->Mutex);
    if (interp->exited && interp->level <= 0)
    {
        rc = JSI_EXIT;
        if (!interp->parent)
            Jsi_InterpDelete(interp);
    }

    return rc;
}

Jsi_RC Jsi_EvalFile(Jsi_Interp* interp, Jsi_Value *fname, int flags)
{
    int isnull;
    if ((isnull=Jsi_ValueIsNull(interp, fname)) || Jsi_ValueIsUndef(interp, fname)) 
        return Jsi_LogError("invalid file eval %s", (isnull?"null":"undefined"));
    return jsi_evalStrFile(interp, fname, NULL, flags, 0);
}

Jsi_RC Jsi_EvalString(Jsi_Interp* interp, const char *str, int flags)
{
    return jsi_evalStrFile(interp, NULL, (char*)str, flags, 0);
}

#undef _jsi_THIS
#undef _jsi_STACK
#undef _jsi_STACKIDX
#undef _jsi_THISIDX
#undef _jsi_TOP
#undef _jsi_TOQ

#endif
