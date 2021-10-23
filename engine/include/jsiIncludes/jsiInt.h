#ifndef __JSIINT_H__
#define __JSIINT_H__

#ifdef JSI_CONFIG_H_FILE
#include JSI_CONFIG_H_FILE
#endif

// Define the defaults
#ifndef JSI__EVENT
#define JSI__EVENT 1
#endif
#ifndef JSI__DEBUG
#define JSI__DEBUG 1
#endif
#ifndef JSI__LOAD
#define JSI__LOAD 1
#endif
#ifndef JSI__SIGNAL
#define JSI__SIGNAL 1
#endif
#ifndef JSI__FILESYS
#define JSI__FILESYS 1
#endif
#ifndef JSI__ZVFS
#define JSI__ZVFS 1
#endif
#ifndef JSI__STUBS
#define JSI__STUBS 1
#endif
#ifndef JSI__THREADS
#define JSI__THREADS 1
#endif
#ifndef JSI__INFO
#define JSI__INFO 1
#endif
#ifndef JSI__CDATA
#define JSI__CDATA 1
#endif
#ifndef JSI__MARKDOWN
#define JSI__MARKDOWN 1
#endif
#ifndef JSI__MATH
#define JSI__MATH 1
#endif
#ifndef JSI__UTF8
#define JSI__UTF8 1
#endif

#if (JSI__STUBS!=1)
#ifndef JSI_OMIT_STUBS
#define JSI_OMIT_STUBS
#endif
#endif
#if (JSI__THREADS!=1)
#define JSI_OMIT_THREADS
#endif
#if (JSI__SIGNAL!=1)
#define JSI_OMIT_SIGNAL
#endif

#if defined(JSI__MD5) && JSI__MD5==0
#define JSI_OMIT_MD5 1
#endif
#if defined(JSI__SHA1) && JSI__SHA1==0
#define JSI_OMIT_SHA1 1
#endif
#if defined(JSI__SHA256) && JSI__SHA256==0
#define JSI_OMIT_SHA256 1
#endif
#if defined(JSI__ENCRYPT) && JSI__ENCRYPT==0
#define JSI_OMIT_ENCRYPT 1
#endif
#if defined(JSI__BASE64) && JSI__BASE64==0
#define JSI_OMIT_BASE64 1
#endif
#if defined(JSI__LOAD) && JSI__LOAD==0
#define JSI_OMIT_LOAD 1
#endif
#if defined(JSI__EVENT) && JSI__EVENT==0
#define JSI_OMIT_EVENT 1
#endif
#if defined(JSI__DEBUG) && JSI__DEBUG==0
#define JSI_OMIT_DEBUG 1
#endif
#if defined(JSI__CDATA) && JSI__CDATA==0
#define JSI_OMIT_CDATA 1
#endif
#if defined(JSI__MARKDOWN) && JSI__MARKDOWN==0
#define JSI_OMIT_MARKDOWN 1
#endif
#if defined(JSI__MATH) && JSI__MATH==0
#define JSI_OMIT_MATH 1
#endif

//#define JSI__MEMDEBUG 1
#if JSI__MEMDEBUG
#define JSI_MEM_DEBUG
#define Assert(n) assert(n)
#else
#define Assert(n)
#endif
#ifndef JSI_OMIT_SIGNATURES
#define JSI_HAS_SIG
#endif
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#define __USE_GNU
#endif
#define VAL_REFCNT
#define VAL_REFCNT2

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <ctype.h>
#include <assert.h>
#include <inttypes.h>
#include <limits.h>

#ifdef __WIN32 /* TODO: support windows signal??? */
#define JSI__MINIZ 1
#define JSI_OMIT_SIGNAL
#endif

#ifndef JSI_AMALGAMATION

#if JSI__REGEX
#include "regex/regex.h"
#else
#include <regex.h>
#endif
#ifdef __WIN32
#include "win/compat.h"
//#include "win/regex.h"
//#include "regex/regex.h"
#else
#define JSI__REGCOMP
#include <sys/time.h>
#endif
#endif

#include <time.h>

#ifndef JSI_IS64BIT
#ifdef __GNUC__
#ifdef __X86_64__
#define JSI_IS64BIT 1
#endif
#else /* GCC */
#if _WIN64 || __amd64__
#define JSI_IS64BIT 1
#endif
#endif /* GCC */
#endif /* JSI_IS64BIT */

#ifndef JSI_IS64BIT
#define JSI_IS64BIT 0
#endif

#define JSMN_FREE(p) Jsi_Free(p)
#define JSMN_MALLOC(l) Jsi_Malloc(l)
#define JSMN_REALLOC(p,l) Jsi_Realloc(p,l)

#define JSI_HAS_SIG /* Signatures to help with debugging */
#ifdef JSI_HAS_SIG
#ifndef SIGASSERT
#define SIGASSERT(s,n) assert((s) && (s)->sig == (uint)JSI_SIG_##n);
#endif
#define SIGINIT(s,n) (s)->sig = JSI_SIG_##n;
#define __VALSIG__ .sig=JSI_SIG_VALUE,
#else
#define SIGASSERT(s,n)
#define SIGINIT(s,n)
#define __VALSIG__
#endif

#ifndef JSI_HAS___PROTO__
#define JSI_HAS___PROTO__ 1  // Enable setting and getting prototypes. 1=set/get funcs, 2=__proto__ assign.
#endif

#ifdef NDEBUG
#define JSI_NOWARN(v) v=v
#else
#define JSI_NOWARN(v)
#endif

#ifdef __FreeBSD__
#define _JSICASTINT(s) (int)(s)
#else
#define _JSICASTINT(s) (s)
#endif

#if 0
#ifndef uint
typedef unsigned int uint;
#endif
#ifndef uchar
typedef unsigned char uchar;
#endif
#endif

#ifndef JSI_AMALGAMATION
#include "jsi.h"
#else
#define JSI_OMIT_STUBS
#endif

#define ALLOC_MOD_SIZE 16      /* Increase allocs by increments of 16. */
#define MAX_ARRAY_LIST 100000  /* Default Max size of an array convertable to list form */
#define MAX_LOOP_COUNT 10000000 /* Limit infinite loops */
#define JSI_MAX_ALLOC_BUF  100000000 /* Limit for dynamic memory allocation hunk */
#define JSI_MAX_SCOPE (BUFSIZ/2)
typedef enum {
    JSI_SIG_ITEROBJ=0xdeadbee1, JSI_SIG_FUNCOBJ, JSI_SIG_SCOPE, JSI_SIG_VALUE,
    JSI_SIG_OBJ, JSI_SIG_USERDATA, JSI_SIG_INTERP, JSI_SIG_PARSER,
    JSI_SIG_FILEOBJ, JSI_SIG_INTERPOBJ, JSI_SIG_FUNC, JSI_SIG_CMDSPECITEM, JSI_SIG_HASH,
    JSI_SIG_HASHENTRY, JSI_SIG_TREE, JSI_SIG_TREEENTRY, JSI_SIG_LIST, JSI_SIG_LISTENTRY,
    JSI_SIG_USER_REG, JSI_SIG_EVENT, JSI_SIG_MAP, JSI_SIG_REGEXP,
    JSI_SIG_ARGTYPE, JSI_SIG_FORINVAR, JSI_SIG_CASELIST, JSI_SIG_CASESTAT,
    JSI_SIG_FASTVAR, JSI_SIG_INTERPSTREVENT, JSI_SIG_ALIASCMD, JSI_SIG_SOCKET, JSI_SIG_SOCKETPSS,
    JSI_SIG_NAMEDATA
} jsi_Sig;

#define Jsi_LogType(fmt,...) Jsi_LogMsg(interp, interp->typeCheck.error?JSI_LOG_ERROR:JSI_LOG_WARN, fmt, ##__VA_ARGS__)

struct jsi_OpCode;

#if  JSI__MEMDEBUG
extern void jsi_VALCHK(Jsi_Value *v);
extern void jsi_OBJCHK(Jsi_Obj *o);
#define VALCHK(val) jsi_VALCHK(val)
#define OBJCHK(val) jsi_OBJCHK(val)
#else
#define VALCHK(val)
#define OBJCHK(val)
#endif

enum {  jsi_callTraceFuncs = 1, jsi_callTraceCmds = 2, jsi_callTraceNew = 4,
        jsi_callTraceReturn = 8, jsi_callTraceArgs = 16, 
        jsi_callTraceNoTrunc = 32,  jsi_callTraceNoParent = 64,
        jsi_callTraceFullPath = 128, jsi_callTraceBefore = 256
};

/* Scope chain */
typedef struct jsi_ScopeChain_ {
#ifdef JSI_HAS_SIG
    jsi_Sig sig;
#endif
    Jsi_Interp *interp;
    Jsi_Value **chains;  /* values(objects) */
    int chains_cnt;         /* count */
} jsi_ScopeChain;

/* Function obj */
/* a Jsi_FuncObj is a raw function with own scope chain */
struct Jsi_FuncObj {
#ifdef JSI_HAS_SIG
    jsi_Sig sig;
#endif
    Jsi_Interp *interp;
    Jsi_Func *func;
    jsi_ScopeChain *scope;
    Jsi_Value *bindArgs;
    Jsi_Value *bindFunc;
};

typedef int (Jsi_IterProc)(Jsi_IterObj *iterObj, Jsi_Value *val, Jsi_Value *var, int index);

/* Jsi_IterObj, use only in for-in statement */
struct Jsi_IterObj {
#ifdef JSI_HAS_SIG
    jsi_Sig sig;
#endif
    Jsi_Interp *interp;
    const char **keys;
    uint size; 
    uint count;
    uint iter;
    bool isArrayList;            /* If an array list do not store keys. */
    bool isof;
    Jsi_Obj *obj;
    uint cur;                    /* Current array cursor. */
    int depth;                  /* Used to create list of keys. */
    Jsi_IterProc *iterCmd;
};

typedef struct UserObjReg_ { /* Per interp userobj registration. */
#ifdef JSI_HAS_SIG
    jsi_Sig sig;
#endif
    Jsi_UserObjReg *reg;
    Jsi_Hash* hashPtr;
    int idx;
} UserObjReg;

/* User defined object */
typedef struct Jsi_UserObj {
#ifdef JSI_HAS_SIG
    jsi_Sig sig;
#endif
    Jsi_Interp *interp;
    Jsi_Hash *id;
    void *data;
    const char *prefix;
    Jsi_UserObjReg *reg;
    struct UserObjReg_ *ureg;
    uintptr_t idx;
    Jsi_HashEntry* hPtr;
} Jsi_UserObj;

typedef struct {
    int valueCnt;
    int objCnt;
    int valueAllocCnt;
    int objAllocCnt;
#ifdef JSI_MEM_DEBUG
    Jsi_Hash *valueDebugTbl;
    Jsi_Hash *objDebugTbl;
    uint memDebugCallIdx;
#endif
} Jsi_InterpDebug;


/* stack change */
/* 0  nothing change */
/* +1 push */
/* -1 pop */
typedef enum {      /* SC   type of data    comment                             */
    OP_NOP,         /* 0 */
    OP_PUSHNUM,     /* +1   *double         number                              */
    OP_PUSHSTR,     /* +1   *char        string                              */
    OP_PUSHVAR,     /* +1   *FastVar        variable name                       */
    OP_PUSHUND,     /* +1   -               undefined                           */
    OP_PUSHNULL,    /* +1   -               null                                */
    OP_PUSHBOO,     /* +1   int             bool                                */
    OP_PUSHFUN,     /* +1   *Jsi_Func           function                            */
    OP_PUSHREG,     /* +1   *regex_t        regex                               */
    OP_PUSHARG,     /* +1   -               push arguments(cur scope)           */
    OP_PUSHTHS,     /* +1   -               push this                           */
    OP_PUSHTOP,     /* +1   -               duplicate top                       */
    OP_PUSHTOP2,    /* +2   -               duplicate toq and top               */
    OP_UNREF,       /* 0    -               make top be right value             */
    OP_POP,         /* -n   int             pop n elements                      */
    OP_LOCAL,       /* 0    *char        add a var to current scope          */
    OP_NEG,         /* 0    -               make top = - top                    */
    OP_POS,         /* 0    -               make top = + top, (conv to number)  */
    OP_NOT,         /* 0    -               reserve top                         */
    OP_BNOT,        /* 0    -               bitwise not                         */
    OP_ADD,         /* -1   -               all math opr pop 2 elem from stack, */
    OP_SUB,         /* -1   -                calc and push back in to the stack */
    OP_MUL,         /* -1   -                                                   */
    OP_DIV,         /* -1   -                                                   */
    OP_MOD,         /* -1   -                                                   */
    OP_LESS,        /* -1   -               logical opr, same as math opr       */
    OP_GREATER,     /* -1   -                                                   */
    OP_LESSEQU,     /* -1   -                                                   */
    OP_GREATEREQU,  /* -1   -                                                   */
    OP_EQUAL,       /* -1   -                                                   */
    OP_NOTEQUAL,    /* -1   -                                                   */
    OP_STRICTEQU,   /* -1   -                                                   */
    OP_STRICTNEQ,   /* -1   -                                                   */
    OP_BAND,        /* -1   -               bitwise and                         */
    OP_BOR,         /* -1   -               bitwise or                          */
    OP_BXOR,        /* -1   -               bitwise xor                         */
    OP_SHF,         /* -1   int(right)      signed shift left or shift right    */
    OP_INSTANCEOF,  /* -1 */
    OP_ASSIGN,      /* -n   int             if n = 1, assign to lval,           */
                    /*                      n = 2, assign to object member      */
    OP_SUBSCRIPT,   /* -1   -               do subscript TOQ[TOP]               */
    OP_INC,         /* 0    int             data indicate prefix/postfix inc/dec                */
    OP_TYPEOF,      /* 0    obj                                                                 */
    OP_IN,          /* 0    obj                                                                 */
    OP_DEC,         /* 0    int                                                                 */
    OP_KEY,         /* +1   -               push an iter object that contain all key in top     */
    OP_NEXT,        /* -1   -               assign next key to top, make top be res of this opr */
    OP_JTRUE,       /* -1   int             jmp to offset if top is true,                       */
    OP_JFALSE,      /* -1   int             jmp to offset if top is false,                      */
    OP_JTRUE_NP,    /* 0    int             jtrue no pop version                                */
    OP_JFALSE_NP,   /* 0    int             jfalse no pop version                               */
    OP_JMP,         /* 0    int             jmp to offset                                       */
    OP_JMPPOP,      /* -n   *jsi_JmpPopInfo     jmp to offset with pop n                            */
    OP_FCALL,       /* -n+1 int             call func with n args, pop then, make ret to be top */
    OP_NEWFCALL,    /* -n+1 int             same as fcall, call as a constructor                */
    OP_RET,         /* -n   int             n = 0|1, return with arg                            */
    OP_DELETE,      /* -n   int             n = 1, delete var, n = 2, delete object member      */
    OP_CHTHIS,      /* 0,   -               make toq as new 'this'                              */
    OP_OBJECT,      /* -n*2+1   int         create object from stack, and push back in          */
    OP_ARRAY,       /* -n+1 int             create array object from stack, and push back in    */
    OP_EVAL,        /* -n+1 int             eval can not be assign to other var                 */
    OP_STRY,        /* 0    *jsi_TryInfo        push try statment poses Jsi_LogWarn to trylist             */
    OP_ETRY,        /* 0    -               end of try block, jmp to finally                    */
    OP_SCATCH,      /* 0    *char        create new scope, assign to current excption        */
    OP_ECATCH,      /* 0    -               jmp to finally                                      */
    OP_SFINAL,      /* 0    -               restore scope chain create by Scatch                */
    OP_EFINAL,      /* 0    -               end of finally, any unfinish code in catch, do it   */
    OP_THROW,       /* 0    -               make top be last exception, pop trylist till catched*/
    OP_WITH,        /* -1   -               make top be top of scopechain, add to trylist       */
    OP_EWITH,       /* 0    -               pop trylist                                         */
    OP_RESERVED,    /* 0    jsi_ReservedInfo*   reserved, be replaced by iterstat by jmp/jmppop     */
    OP_DEBUG,       /* 0    -               DEBUG OPCODE, output top                            */
    OP_LASTOP       /* 0    -               END OF OPCODE                                       */
} jsi_Eopcode;

typedef enum { jsi_Oplf_none=0, jsi_Oplf_assert=1, jsi_Oplf_debug=2, jsi_Oplf_trace=3, jsi_Oplf_test=4 } jsi_OpLogFlags;

typedef struct jsi_OpCode {
    jsi_Eopcode op;
    void *data;
    unsigned int Line:16;
    unsigned int Lofs:8;
    unsigned char alloc:1;
    unsigned char nodebug:1;
    unsigned char hit:1;
    unsigned char isof:1;
    unsigned char local:1;
    jsi_OpLogFlags logflag:3;
    const char *fname;
} jsi_OpCode;


typedef struct Jsi_OpCodes {
    jsi_OpCode *codes;
    int code_len;
    int code_size;          // Used by malloc.
    
    int expr_counter;           /* context related expr count */
    int lvalue_flag;            /* left value count/flag */
    const char *lvalue_name; /* left value name */
    int line;  // Used in Lemon
#ifdef JSI_MEM_DEBUG
    Jsi_HashEntry *hPtr;
    int id;
#endif
} Jsi_OpCodes;


typedef struct jsi_TryInfo {
    int trylen;
    int catchlen;
    int finallen;
} jsi_TryInfo;

typedef struct jsi_ReservedInfo {
    int type;
    const char *label;
    int topop;
} jsi_ReservedInfo;

typedef struct jsi_JmpPopInfo {
    int off;
    int topop;
} jsi_JmpPopInfo;

#define RES_CONTINUE    1
#define RES_BREAK       2
typedef struct YYLTYPE jsi_Pline;

//void jsi_codes_print(Jsi_OpCodes *ops);
void jsi_code_decode(jsi_OpCode *op, int currentip, char *buf, int bsiz);
const char* jsi_opcode_string(uint opCode);

#ifdef JSI_MEM_DEBUG
typedef struct 
{
    const char *fname;
    int line;
    const char *func;
    const char *label;
    const char *label2;
    const char *label3;
    uint Idx;
    uint flags;
    struct jsi_OpCode *ip;
    int ipLine;
    jsi_Eopcode ipOp;
    const char* ipFname;
    Jsi_HashEntry *hPtr;
    Jsi_Interp *interp;
} jsi_ValueDebug;
#endif

struct Jsi_Obj {
#ifdef JSI_HAS_SIG
    jsi_Sig sig;
#endif
    int refcnt;                 /* reference count */
    Jsi_otype ot:8;             /* object type */
    uint isarrlist:1;           /* Array type. */
    uint isstrkey:1;            /* Key string registered in interp->strKeyTbl (do not free) */
    uint isJSONstr:1;
    uint clearProto:1;          /* Prototype changed, clean it up at exit. */
    uint isNoOp:1;
    uint unused1:1;
    uint unused2:16;
    union {                     /* switched on by value of "ot" */
        int val;
        Jsi_Number num;
        Jsi_String s;
        Jsi_Regex *robj;
        Jsi_FuncObj *fobj;
        Jsi_IterObj *iobj;
        Jsi_UserObj *uobj;
    } d;
    uint arrMaxSize;                 /* Max allocated space for array. */
    uint arrCnt;                     /* Count of actually set keys. */
    Jsi_Value **arr;   /* Array values. */  
    Jsi_Tree *tree;                 /* Tree storage (should be union with array). */
    Jsi_Value *__proto__;           /* TODO: memory leaks when this is changed */
    struct Jsi_Obj *constructor;
#ifdef JSI_MEM_DEBUG
    jsi_ValueDebug VD;
#endif
};

/*#pragma pack(1)*/


struct Jsi_Value {
#ifdef JSI_HAS_SIG
    jsi_Sig sig;
#endif
    int32_t refCnt;
    Jsi_vtype vt:8;             /* value type */
    union {
        uint flag:8;
        struct vflagbit {
            uint readonly:1;
            uint dontenum:1;  /* Dont enumerate. */
            uint dontdel:1;
            uint innershared:1; /* All above used only for objkeys. */
            uint isarrlist:1;
            uint isstrkey:1;    /* Key string registered in interp->strKeyTbl (do not free) */
            uint local:1;       // Used to detect a function creating a global var.
            uint lookupfailed:1;// Indicates failed lookup, string is stored in lookupFail below.
        } bits;
    } f;
    union {                     /* see above */
        int val;
        Jsi_Number num;
        Jsi_String s;
        Jsi_Obj *obj;
        struct Jsi_Value *lval;
        const char *lookupFail;
    } d;
#ifdef JSI_MEM_DEBUG
    jsi_ValueDebug VD;
#endif
};

#ifndef JSI_SMALL_HASH_TABLE
#define JSI_SMALL_HASH_TABLE 0x10
#endif

typedef uintptr_t jsi_Hash;

typedef union jsi_HashKey {
    char string[sizeof(jsi_Hash)];  // STRING, STRUCT
    void *oneWordValue;             // ONEWORD, STRINGKEY
} jsi_HashKey;

typedef struct Jsi_HashEntry {
    jsi_Sig sig;
    int typ; // JSI_MAP_HASH
    struct Jsi_HashEntry *nextPtr;
    Jsi_Hash *tablePtr;
    jsi_Hash hval;
    void* clientData;
    jsi_HashKey key;
} Jsi_HashEntry;


typedef struct Jsi_Hash {
#ifdef JSI_HAS_SIG
    jsi_Sig sig;
#endif
    int typ; // JSI_MAP_HASH
    Jsi_HashEntry **buckets;
    Jsi_HashEntry *staticBuckets[JSI_SMALL_HASH_TABLE];
    int numBuckets;
    int numEntries;
    int rebuildSize;
    jsi_Hash mask;
    unsigned int downShift;
    int keyType;
    Jsi_HashEntry *(*createProc) (Jsi_Hash *tablePtr, const void *key, bool *newPtr);
    Jsi_HashEntry *(*findProc) (Jsi_Hash *tablePtr, const void *key);
    Jsi_MapOpts opts;
} Jsi_Hash;

struct Jsi_Tree {
#ifdef JSI_HAS_SIG
    jsi_Sig sig;
#endif
    int typ; // JSI_MAP_TREE
    //Jsi_Interp *interp;
    Jsi_TreeEntry *root;
    uint numEntries, keyType, epoch;
    struct {
        uint 
            inserting:1, destroyed:1,
            nonredblack:1,  /* Disable red/black handling on insert/delete. */
            internstr:1,    /* STRINGPTR keys are stored in strHash */
            valuesonly:1,   /* Values must be of type JSI_VALUE */
            unused:28;
    } flags;
    Jsi_Hash* strHash;  /* String hash table to use if INTERNSTR; setup on first Create if not defined. */
    Jsi_TreeEntry* (*createProc)(Jsi_Tree *treePtr, const void *key, bool *newPtr);
    Jsi_MapOpts opts;
};

typedef struct Jsi_TreeEntry {
#ifdef JSI_HAS_SIG
    jsi_Sig sig;
#endif
    int typ; // JSI_MAP_TREE
    Jsi_Tree *treePtr;
    struct Jsi_TreeEntry* left;
    struct Jsi_TreeEntry* right;
    struct Jsi_TreeEntry* parent;
    union { /* FLAGS: bottom 16 bits for JSI, upper 16 bits for users. First 7 map to JSI_OM_ above. */
        struct { 
            unsigned int readonly:1, dontenum:1, dontdel:1, innershared:1, isarrlist:1, isstrkey:1, unused:1,
                color:1,
                reserve:8,
                user0:8,
                user1:1, user2:1, user3:1, user4:1, user5:1, user6:1, user7:1, user8:1;
        } bits;
        int flags;
    } f;
    void* value;
    jsi_HashKey key;
} Jsi_TreeEntry;

typedef struct Jsi_Map {  // Wrapped Tree/Hash/List.
    uint sig;
    Jsi_Map_Type typ;
    union {
        Jsi_Hash *hash;
        Jsi_Tree *tree;
        Jsi_List *list;
    } v;
} Jsi_Map;

typedef struct jsi_ArgValue_ {
    char *name;
    uint type;  // or'ed Jsi_otype
    Jsi_Value *defValue;
} jsi_ArgValue;

typedef struct Jsi_ScopeStrs {
    jsi_ArgValue *args;
    int count;
    int _size;  // Used in allocation only.
    int varargs;
    int typeCnt;
    int firstDef;
    int argCnt;
    int retType;
} Jsi_ScopeStrs;

// Eval stack-frame.
typedef struct jsi_Frame {
    int level;
    const char *fileName;
    const char *funcName;
    const char *dirName;
    int line;
    jsi_OpCode *ip;
    int Sp;
    int tryDepth;
    int withDepth;
    jsi_ScopeChain* ingsc;
    Jsi_Value *incsc;
    Jsi_Value *inthis;
    Jsi_OpCodes *opcodes;
    struct jsi_Pstate *ps;
    bool strict;
    int logflag;
    Jsi_Func *evalFuncPtr;
    struct jsi_Frame *parent, *child;
    Jsi_Value *arguments; // Set when arguments are accessed.
} jsi_Frame;

/* Program/parse state(context) */
typedef struct jsi_Pstate {
#ifdef JSI_HAS_SIG
    jsi_Sig sig;
#endif
    int err_count;              /* Jsi_LogError count after parse */
    int eval_flag;              /* 1 if currently executing in an eval function */
    int funcDefs;               /* Count of functions defined. 0 means we can delete this cache (eventually). */
    Jsi_OpCodes *opcodes;    /* Execution codes. */
    struct jsi_Lexer *lexer;        /* seq provider */

    int _context_id;            /* used in FastVar-locating */
    Jsi_Value *last_exception;
    Jsi_Interp *interp;
    Jsi_HashEntry *hPtr;
    Jsi_Hash *argsTbl;
    Jsi_Hash *strTbl;
    Jsi_Hash *fastVarTbl;
    int argType;                // Used during parsing to aggregate type.
    Jsi_ScopeStrs *args;        // Last push.
} jsi_Pstate;


Jsi_ScopeStrs *jsi_ScopeStrsNew(void);
void jsi_ScopeStrsPush(Jsi_Interp *interp, Jsi_ScopeStrs *ss, const char *string, int argType);
void jsi_ScopeStrsFree(Jsi_Interp *interp, Jsi_ScopeStrs *ss);
const char *jsi_ScopeStrsGet(Jsi_ScopeStrs *ss, int i);

void jsi_PstatePush(jsi_Pstate *ps);
void jsi_PstatePop(jsi_Pstate *ps);
void jsi_PstateAddVar(jsi_Pstate *ps, const char *str);
Jsi_ScopeStrs *jsi_ScopeGetVarlist(jsi_Pstate *ps);

void jsi_PstateFree(jsi_Pstate *ps);
jsi_Pstate *jsi_PstateNew(Jsi_Interp *interp);
void jsi_PstateClear(jsi_Pstate *ps);
const char * jsi_PstateGetFilename(jsi_Pstate *ps);
int jsi_PstateSetFile(jsi_Pstate *ps, Jsi_Channel fp, int skipbang);
int jsi_PstateSetString(jsi_Pstate *ps, const char *str);

extern int yyparse(jsi_Pstate *ps);

#ifndef JSI_AMALGAMATION
#include "parser.h"
#endif

typedef struct jsi_ForinVar {
    jsi_Sig sig;
    const char *varname;
    Jsi_OpCodes *local;
    Jsi_OpCodes *lval;
} jsi_ForinVar;


typedef struct jsi_CaseExprStat {
    jsi_Sig sig;
    Jsi_OpCodes *expr;
    Jsi_OpCodes *stat;
    int isdefault;
} jsi_CaseExprStat;


typedef struct jsi_CaseList {
    jsi_Sig sig;
    jsi_CaseExprStat *es;
    int off;
    struct jsi_CaseList *tail;
    struct jsi_CaseList *next;
} jsi_CaseList;


typedef enum {
    LT_NONE,
    LT_FILE,            /* read from file */
    LT_STRING           /* read from a string */
} Jsi_Lexer_Type;

/* jsi_Lexer, where input seq provided */
typedef struct jsi_Lexer {
    Jsi_Lexer_Type ltype;
    union {
        Jsi_Channel fp;           /* LT_FILE, where to read */
        char *str;          /* LT_STRING */
    } d;
    int last_token;         /* last token returned */
    int ungot, unch[100];
    int cur;                /* LT_STRING, current char */
    int cur_line;           /* current line no. */
    int cur_char;           /* current column no. */
    jsi_Pstate *pstate;
} jsi_Lexer;

int yylex (YYSTYPE *yylvalp, YYLTYPE *yyllocp, jsi_Pstate *pstate);
void yyerror(YYLTYPE *yylloc, jsi_Pstate *ps, const char *msg);

typedef struct FastVar_ {
    jsi_Sig sig;
    int context_id:31;
    unsigned int local:1;
    jsi_Pstate *ps;
    struct {
        char *varname;
        struct Jsi_Value *lval;
    } var;
} FastVar;

typedef enum { FC_NORMAL, FC_BUILDIN } Jsi_Func_Type;

/* raw function data, with script function or system Jsi_CmdProc */
struct Jsi_Func {
#ifdef JSI_HAS_SIG
    jsi_Sig sig;
#endif
    Jsi_Func_Type type;                         /* type */
    struct Jsi_OpCodes *opcodes;    /* FC_NORMAL, codes of this function */
    Jsi_CmdProc *callback;            /* FC_BUILDIN, callback */

    Jsi_ScopeStrs *argnames;                 /* FC_NORMAL, argument names */
    Jsi_ScopeStrs *localnames;               /* FC_NORMAL, local var names */
    union {
        uint flags;
        struct {
            uint res:8, hasattr:1, isobj:1 , iscons:1, res2:4;
        } bits;
    } f;
    union {
        uint i;
        struct {
            uint addargs:1 , iscons:1, isdiscard:1, res:5;
        } bits;
    } callflags;
    int refCnt;
    void *privData;                 /* context data given in create. */
    Jsi_CmdSpec *cmdSpec;
    const char *name, *parent;  /* Name for non-anonymous function. */
    uint retType;  /* Type name: or of Jsi_otype*/
    int callCnt;
    const char *script;  /* Script created in. */
    jsi_Pline bodyline; /* Body line info. */
    Jsi_HashEntry *hPtr;
    //bool strict;
    double subTime, allTime;
    Jsi_FuncObj *fobj;
};

typedef struct {
    char *origFile; /* Short file name. */
    char *fileName; /* Fully qualified name. */
    char *dirName;  /* Directory name. */
    int useCnt;
} jsi_FileInfo;

enum {
    STACK_INIT_SIZE=1024, STACK_INCR_SIZE=1024, STACK_MIN_PAD=100,
    JSI_MAX_CALLFRAME_DEPTH=1000, /* default max nesting depth for procs */
    JSI_MAX_EVAL_DEPTH=1000, /* default max nesting depth for eval */
    JSI_MAX_INCLUDE_DEPTH=50,  JSI_MAX_SUBINTERP_DEPTH=10,
    JSI_IS_UTF=1,
    JSI_UTF_CHECKED=2
    /*,JSI_ON_STACK=0x80*/
};

typedef struct InterpStrEvent_ {
#ifdef JSI_HAS_SIG
    jsi_Sig sig;
#endif
    int rc, isExec, tryDepth, errLine;
    const char *errFile;
    Jsi_Value *objData;
    Jsi_DString func;
    Jsi_DString data;
    struct InterpStrEvent_ *next;
    void *mutex;
} InterpStrEvent;

typedef void (*jsiCallTraceProc)(Jsi_Interp *interp, const char *funcName, const char *file, 
    int line, Jsi_CmdSpec* spec, Jsi_Value* _this, Jsi_Value* args, Jsi_Value *ret);

typedef struct {
    const char *file;
    const char *func;
    int line;
    int id;
    int hits;
    bool enabled;
    bool temp;
} jsi_BreakPoint;

typedef struct jsi_PkgInfo {
    Jsi_Number version;
    const char *loadFile;  // Full path of file name loaded.
    Jsi_InitProc *initProc; // For C-extensions.
    bool needInit;  // If a C-extension and _Init func needs calling in this interp.
} jsi_PkgInfo;

typedef struct {
    bool isDebugger; // Set to 1 if we are the debugger, debugging a sub-interp.
    bool noFilter;
    bool doContinue;
    bool forceBreak;
    bool bpLast; // Last break was a breakpoint.
    bool includeOnce;
    bool includeTrace;
    int bpOpCnt;
    int minLevel;
    const char *callback;
    const char *putsCallback;
    const char *msgCallback;
    const char *traceCallback;
    Jsi_Value *callbackFunc;
    int lastLine;
    int lastLevel;
    const char *lastFile;
    bool pkgTrace;
    int breakIdx;
    Jsi_RC (*hook)(struct Jsi_Interp* interp, const char *curFile, int curLine, int curLevel, const char *curFunc, const char *opCode, jsi_OpCode *op, const char *msg);
} Jsi_DebugInterp;

typedef union jsi_numUnion {
    bool       BOOL;
    int            INT;
    uint           UINT;
    int8_t         INT8;
    uint8_t        UINT8;
    int16_t        INT16;
    uint16_t       UINT16;
    int32_t        INT32;
    uint32_t       UINT32;
    int64_t        INT64;
    uint64_t       UINT64;
    Jsi_Number     DOUBLE;
    time_t         TIME_T;
    time_w         TIME_W;
    time_d         TIME_D;
} jsi_numUnion;

typedef struct {
    uint parse:1;
    uint run:1;
    uint all:1;     
    uint error:1;
    uint strict:1;
    uint noundef:1;
    uint nowith:1;
    uint proto:1;
    uint unused:24;
} Jsi_TypeCheck;

typedef enum {
    jsi_AssertModeThrow,
    jsi_AssertModeLog,
    jsi_AssertModePuts
} jsi_AssertMode;

typedef struct {
    uint isSpecified:1; /* User set the option. */
    uint initOnly:1;    /* Allow set only at init, disallowing update/conf. */
    uint readOnly:1;    /* Value can not be set. */
    uint noDupValue:1;  /* Values are not to be duped. */
    uint noClear:1;     /* Values are not to be cleared: watch for memory leaks */
    uint dbDirty:1;     /* Used to limit DB updates. */
    uint dbIgnore:1;    /* Field is not to be used for DB. */
    uint dbRowid:1 ;    /* Field used by DB to store rowid. */
    uint custNoCase:1;  /* Ignore case (eg. for ENUM and BITSET). */
    uint forceInt:1;    /* Force int instead of text for enum/bitset. */
    uint bitsetBool:1;  /* Treat bitset custom field as bool instead of an int. */
    uint timeDateOnly:1;/* Time field is date only. */
    uint timeTimeOnly:1;/* Time field is time only. */
    uint isBits:1;      /* Is a C bit-field. */
    uint fmtString:1;   /* Format value (eg. time) as string. */
    uint fmtNumber:1;   /* Format value (eg. enum) as number. */
    uint fmtHext:1;     /* Format number in hex. */
    uint strict:1;      /* Strict mode. */
    uint fieldSetup:1;  /* Field has been setup. */
    uint coerce:1;      /* Coerce input value to required type. */
    uint noSig:1;       /* No signature. */
    uint enumSpec:1;    /* Enum has spec rather than a list of strings. */
    uint enumUnsigned:1;/* Enum value is unsigned. */
    uint enumExact:1;   /* Enum must be an exact match. */
    uint required:1;    /* Field must be specified (if not IS_UPDATE). */
    uint prefix:1;      /* Allow matching unique prefix of object members. */
    uint isUpdate:1;    /* This is an update/conf (do not reset the specified flags) */
    uint ignoreExtra:1; /* Ignore extra members not found in spec. */
    uint forceStrict:1; /* Override Interp->compat to disable JSI_OPTS_IGNORE_EXTRA. */
    uint verbose:1;     /* Dump verbose options */
    uint userBits:32;
} jsi_OptionFlags;

typedef struct {
    bool file;    // Ouput file:line information: default is at end.
    bool full;    // Show full file path.
    bool func;    // Ouput function at end.
    bool debug;
    bool trace;
    bool test;
    bool noinfo;
    bool nowarn;
    bool noerror;
    bool time;    // Prefix with time
    bool date;    // Prefix with date
    bool before;  // Print file:line before message instead of at end.
    bool isUTC;
    const char* timeFmt;
    Jsi_Value *chan;
} jsi_LogOptions;

extern Jsi_OptionSpec jsi_InterpLogOptions[];

struct Jsi_Interp {
#ifdef JSI_HAS_SIG
    jsi_Sig sig;
#endif
    bool isSafe;
    Jsi_Value *safeReadDirs;
    Jsi_Value *safeWriteDirs;
    Jsi_DebugInterp debugOpts;
    bool deleting;
    bool destroying;
    bool hasEventHdl;
    uint autoLoaded;
    int exited;
    int exitCode;
    int refCount;
    int traceCall;
    int opTrace;
    int memDebug;
    int coverAll, coverHit;
    bool coverage;
    bool profile;
    bool asserts;
    jsi_AssertMode assertMode;
    uint unitTest;
    const char *jsppChars;
    Jsi_Value *jsppCallback;
    bool noStderr;
    bool tracePuts;
    bool isMain;
    bool compat;
    bool mutexUnlock;
    bool noInherit;
    bool noUndef;
    bool fileStrict;  /* Error out on file not found, etc. */
    bool hasCallee;
    bool noReadline;
    bool nocacheOpCodes;
    bool noSubInterps;
    bool privKeys;
    bool subthread;
    bool showColumns;
    bool msgAllowDups;
    bool strict;
    bool protoInit;
    bool isInteractive;
    const char *confArgs;
    Jsi_Value *stdinStr;
    Jsi_Value *stdoutStr;
    Jsi_TypeCheck typeCheck;
    jsi_LogOptions logOpts;
    int typeWarnMax;
    int typeMismatchCnt;
    Jsi_InterpOpts opts;
    Jsi_Value *inopts;
    int evalFlags;
    Jsi_InterpDebug dbStatic;
    Jsi_InterpDebug *dbPtr;
    jsiCallTraceProc traceHook;
    int opCnt;  /* Count of instructions eval'ed */
    int maxOpCnt;
    int maxUserObjs;
    int userObjCnt;
    int funcCnt;
    int level;  /* Nesting level of eval/func/cmd calls. */
    int maxDepth;/* Max allowed eval recursion. */
    int maxIncDepth;
    int includeDepth;
    int includeCnt;
    int maxInterpDepth;
    int interpDepth;
    int pkgReqDepth;
    int didReturn;
    uint codeCacheHit;
    uint funcCallCnt;
    uint cmdCallCnt;
    uintptr_t eventIdx;
#ifdef JSI_MEM_DEBUG
    uint valueDebugIdx;
    Jsi_Hash *codesTbl;
#endif
    jsi_ScopeChain *gsc;
    Jsi_Value *csc;
    struct Jsi_Interp *parent, *topInterp, *mainInterp;
    Jsi_Value *onComplete;
    Jsi_Value *onEval;
    Jsi_Value *onExit;
    Jsi_Value *execZip;
    void (*logHook)(char *buf, va_list va);
    bool istty;
    const char *name;
    Jsi_Value *pkgDirs;
    bool selfZvfs;
    int inParse;
    Jsi_Value *retPtr;       /* Return value from eval */
    jsi_Pstate *ps;
    Jsi_Value *argv0;
    Jsi_Value *args;
    Jsi_Value *console;
    Jsi_Value *scriptFile;  /* Start script returned by info.argv0(). */
    const char *scriptStr;
    const char *curFile;
    const char *curFunction;
    const char *homeDir;
    const char *historyFile;
    char *curDir;
    int maxStack;

    Jsi_Map *strKeyTbl;  /* Global strings table. */
    Jsi_Map *cmdSpecTbl; /* Jsi_CmdSpecs registered. */
    Jsi_Hash *onDeleteTbl;  /* Cleanup funcs to call on interp delete. */
    Jsi_Hash *assocTbl;
    Jsi_Hash *codeTbl; /* Scripts compiled table. */
    Jsi_Hash *eventTbl;
    Jsi_Hash *genValueTbl;
    Jsi_Hash *genObjTbl;
    Jsi_Hash *funcObjTbl;
    Jsi_Hash *funcsTbl;
    Jsi_Hash *bindTbl;
    Jsi_Hash *fileTbl;    // The "source"ed files.
    Jsi_Hash *lexkeyTbl;
    Jsi_Hash *protoTbl;
    Jsi_Hash *regexpTbl;    
    Jsi_Hash *thisTbl;
    Jsi_Hash *userdataTbl;
    Jsi_Hash *varTbl;
    Jsi_Hash *preserveTbl;
    Jsi_Hash *loadTbl;
    Jsi_Hash *staticFuncsTbl; // For debugOpts.typeProto
    Jsi_Hash *breakpointHash;
    Jsi_Hash *packageHash;
    Jsi_Hash *aliasHash;
#ifdef VAL_REFCNT
    Jsi_Value **Stack;
    Jsi_Value **Obj_this;
#else
    Jsi_Value *Stack;
    Jsi_Value *Obj_this;
#endif
            
    Jsi_Value *Object_prototype;
    Jsi_Value *Function_prototype_prototype;
    Jsi_Value *Function_prototype;
    Jsi_Value *String_prototype;
    Jsi_Value *Number_prototype;
    Jsi_Value *Boolean_prototype;
    Jsi_Value *Array_prototype;
    Jsi_Value *RegExp_prototype;
    Jsi_Value *Date_prototype;
    
    Jsi_Value *NaNValue;
    Jsi_Value *InfValue;
    Jsi_Value *NullValue;
    Jsi_Value *nullFuncArg; /* Efficient call of no-arg func */
    Jsi_Value *nullFuncRet;
    Jsi_Value *autoFiles;
    Jsi_Obj* cleanObjs[4];

    const char *busyCallback;
    int busyInterval;
    int isInCallback;
    int objId;
    Jsi_Value *Top_object;
        Jsi_ScopeStrs *scopes[JSI_MAX_SCOPE];
    int cur_scope;
    int maxArrayList;
    int delRBCnt;
    Jsi_Func *activeFunc;  // Currently active function call.
    Jsi_Func *prevActiveFunc;  // Prev active function call.
    jsi_OpCode *curIp;  /* Used for debug Log msgs. */
    
    char *lastPushStr;  // Used by error handling and Jsi_LogMsg.   TODO: cleanup/rationalize.
    Jsi_Value* lastSubscriptFail;
    const char* lastSubscriptFailStr;

    Jsi_Wide sigmask;
    char errMsgBuf[200];  /* Error message space for when in try. */
    int errLine;
    int errCol;
    const char *errFile;
    Jsi_Mutex* Mutex;
    Jsi_Mutex* QMutex; /* For threads queues */
    void* threadId;
    int threadCnt;
    int threadShrCnt;
    int lockTimeout; /* in milliseconds. */
    uint lockRefCnt;
    int psEpoch;
    int mountCnt;
    Jsi_DString interpEvalQ;
    Jsi_DString interpMsgQ;
    InterpStrEvent *interpStrEvents;

    bool typeInit;
    Jsi_CData_Static *statics;  
    Jsi_Hash *StructHash;
    Jsi_Hash *SigHash;
    Jsi_Hash *EnumHash;
    Jsi_Hash *EnumItemHash;
    Jsi_Hash *CTypeHash;
    Jsi_Hash *TYPEHash;

    const char *recvCallback;
    uint threadErrCnt;  /* Count of bad thread event return codes. */
    uint threadEvalCnt;
    uint threadMsgCnt;
    void *sleepData;
    jsi_PkgInfo *pkgRequiring, *pkgProviding;
    jsi_Pline *parseLine;
    jsi_Frame *framePtr;
    struct jsi_DbVfs **dbVfsPtrPtr;
    double subTime, startTime, funcSelfTime, cmdSelfTime;
};


enum { JSI_REG_GLOB=0x1, JSI_REG_NEWLINE=0x2, JSI_REG_DOT_NEWLINE=0x4, JSI_REG_STATIC=0x100 };

struct Jsi_Regex_ {
#ifdef JSI_HAS_SIG
    jsi_Sig sig;
#endif
    regex_t reg;
    int eflags;
    int flags;
    char *pattern;
    int lastIndex;
};


/* Entries in interp->cmdSpecTbl. */
typedef struct Jsi_CmdSpecItem_ {
#ifdef JSI_HAS_SIG
    jsi_Sig sig;
#endif
    const char *name;  /* Parent cmd. */
    Jsi_CmdSpec *spec;
    Jsi_Value *proto;
    int flags;
    void *privData;
    Jsi_MapEntry *hPtr;
    struct Jsi_CmdSpecItem_ *next; /* TODO: support user-added sub-commands. */
    const char *help;
    const char *info;
    int isCons;
} Jsi_CmdSpecItem;

//extern Jsi_OptionType jsi_OptTypeInfo[];
extern const char* jsi_OptionTypeStr(Jsi_OptionId typ, bool cname);
extern const Jsi_OptionType* Jsi_OptionsStr2Type(const char *str, bool cname);

typedef struct {
    const char *topLink;
    bool getTitle;
    bool returnStr;
    Jsi_DString titleStr;
} jsi_MarkdownOpts;

/* SCOPE */
//typedef struct jsi_ScopeChain jsi_ScopeChain;

extern jsi_ScopeChain* jsi_ScopeChainNew(Jsi_Interp *interp, int cnt); /*STUB = 176*/
extern Jsi_Value* jsi_ScopeChainObjLookupUni(jsi_ScopeChain *sc, char *key); /*STUB = 177*/
extern jsi_ScopeChain* jsi_ScopeChainDupNext(Jsi_Interp *interp, jsi_ScopeChain *sc, Jsi_Value *next); /*STUB = 178*/
extern void jsi_ScopeChainFree(Jsi_Interp *interp, jsi_ScopeChain *sc); /*STUB = 179*/

extern Jsi_Interp *jsiMainInterp; /* The main interp */
extern Jsi_Interp *jsiDelInterp; /* Interp being delete: Used by cleanup callbacks. */
extern void jsi_CmdSpecDelete(Jsi_Interp *interp, void *ptr);
extern void jsi_markdown_to_html(Jsi_DString *input_markdown,  Jsi_DString *output_body, jsi_MarkdownOpts *opts);

Jsi_RC jsi_InitFilesys(Jsi_Interp *interp, int release);
Jsi_RC jsi_InitLexer(Jsi_Interp *interp, int release);
Jsi_RC jsi_InitLoad(Jsi_Interp *interp, int release);
Jsi_RC jsi_InitCmds(Jsi_Interp *interp, int release);
Jsi_RC jsi_InitInterp(Jsi_Interp *interp, int release);
Jsi_RC jsi_InitFileCmds(Jsi_Interp *interp, int release);
Jsi_RC jsi_InitString(Jsi_Interp *interp, int release);
Jsi_RC jsi_InitValue(Jsi_Interp *interp, int release);
Jsi_RC jsi_InitNumber(Jsi_Interp *interp, int release);
Jsi_RC jsi_InitArray(Jsi_Interp *interp, int release);
Jsi_RC jsi_InitBoolean(Jsi_Interp *interp, int release);
Jsi_RC jsi_InitMath(Jsi_Interp *interp, int release);
Jsi_RC jsi_InitProto(Jsi_Interp *interp, int release);
Jsi_RC jsi_InitRegexp(Jsi_Interp *interp, int release);
Jsi_RC jsi_InitJSON(Jsi_Interp *interp, int release);
Jsi_RC Jsi_InitSqlite(Jsi_Interp *interp, int release);
Jsi_RC Jsi_initSqlite(Jsi_Interp *interp, int release);
Jsi_RC Jsi_InitMySql(Jsi_Interp *interp, int release);
Jsi_RC jsi_InitTree(Jsi_Interp *interp, int release);
Jsi_RC Jsi_InitWebSocket(Jsi_Interp *interp, int release);
Jsi_RC Jsi_InitSocket(Jsi_Interp *interp, int release);
Jsi_RC jsi_InitSignal(Jsi_Interp *interp, int release);
Jsi_RC jsi_InitOptions(Jsi_Interp *interp, int release);
Jsi_RC Jsi_InitZvfs(Jsi_Interp *interp, int release);
Jsi_RC jsi_InitCData(Jsi_Interp *interp, int release);
Jsi_RC jsi_execCmd(Jsi_Interp *interp, Jsi_Value *args, Jsi_DString *dStr, Jsi_DString *cStr, int *code);

void jsi_SignalClear(Jsi_Interp *interp, int sigNum);
bool jsi_SignalIsSet(Jsi_Interp *interp, int sigNum);
/* excute opcodes
 * 1. ps, program execution context
 * 2. opcodes, codes to be executed
 * 3. scope, current scopechain, not include current scope
 * 4. currentScope, current scope
 * 5. _this, where 'this' indicated
 * 6. vret, return value
 */
extern Jsi_RC jsi_evalcode(jsi_Pstate *ps, Jsi_Func *func, Jsi_OpCodes *opcodes, 
        jsi_ScopeChain *scope, Jsi_Value *currentScope,
        Jsi_Value *_this,
        Jsi_Value **vret);
        
typedef Jsi_RC (*Jsi_Constructor)(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this,
    Jsi_Value **ret, int flags, void *privData);
extern Jsi_RC jsi_SharedArgs(Jsi_Interp *interp, Jsi_Value *args, Jsi_Func *func, int alloc);
extern void jsi_SetCallee(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *tocall);
extern Jsi_RC jsi_AssertCmd(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this,
    Jsi_Value **ret, Jsi_Func *funcPtr);
extern Jsi_RC jsi_NoOpCmd(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this,
    Jsi_Value **ret, Jsi_Func *funcPtr);
extern Jsi_RC jsi_InterpInfo(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this,
    Jsi_Value **ret, Jsi_Func *funcPtr);
    
enum {StrKeyAny = 0, StrKeyFunc = 0x1, StrKeyCmd = 0x2, StrKeyVar = 0x2};

//char* jsi_KeyLookup(Jsi_Interp *interp, const char *str);
char* jsi_KeyFind(Jsi_Interp *interp, const char *str, int nocreate, int *isKey);
void jsi_InitLocalVar(Jsi_Interp *interp, Jsi_Value *arguments, Jsi_Func *who);
Jsi_Value *jsi_GlobalContext(Jsi_Interp *interp);
void jsi_AddEventHandler(Jsi_Interp *interp);

extern const char *jsi_ObjectTypeName(Jsi_Interp *interp, Jsi_otype otyp);
extern const char *jsi_ValueTypeName(Jsi_Interp *interp, Jsi_Value *val);
extern const char *jsi_TypeName(Jsi_Interp *interp, Jsi_ttype otyp);
extern Jsi_RC jsi_ObjectToStringCmd(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this,
    Jsi_Value **ret, Jsi_Func *funcPtr);
extern Jsi_RC jsi_HasOwnPropertyCmd(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this,
    Jsi_Value **ret, Jsi_Func *funcPtr);

extern const char *jsi_GetHomeDir(Jsi_Interp *interp);
extern Jsi_RC jsi_RegExpValueNew(Jsi_Interp *interp, const char *regtxt, Jsi_Value *ret);
extern void jsi_DumpOptionSpecs(Jsi_Interp *interp, Jsi_Obj *nobj, Jsi_OptionSpec* spec);
extern Jsi_Func *jsi_FuncMake(jsi_Pstate *pstate, Jsi_ScopeStrs *args, Jsi_OpCodes *ops, jsi_Pline *line, const char *name);
extern Jsi_Func *jsi_FuncNew(Jsi_Interp *interp);
extern void jsi_FreeOpcodes(Jsi_OpCodes *ops);
extern void jsi_DelAssocData(Jsi_Interp *interp, void *data);

extern void jsi_UserObjFree (Jsi_Interp *interp, Jsi_UserObj *uobj);
extern bool jsi_UserObjIsTrue (Jsi_Interp *interp, Jsi_UserObj *uobj);
extern Jsi_RC jsi_UserObjDump   (Jsi_Interp *interp, const char *argStr, Jsi_Obj *obj);
extern Jsi_RC jsi_UserObjDelete (Jsi_Interp *interp, void *data);
extern void jsi_UserObjToName(Jsi_Interp *interp, Jsi_UserObj *uobj, Jsi_DString *dStr);
extern Jsi_Obj *jsi_UserObjFromName(Jsi_Interp *interp, const char *name);

extern Jsi_RC Zvfs_Mount( Jsi_Interp *interp, Jsi_Value *archive, Jsi_Value *mount, Jsi_Value **ret);
extern Jsi_Value* jsi_ObjArraySetDup(Jsi_Interp *interp, Jsi_Obj *obj, Jsi_Value *value, int arrayindex);
extern void jsi_ValueObjSet(Jsi_Interp *interp, Jsi_Value *target, const char *key, Jsi_Value *value, int flags, int isstrkey);
extern const char* jsi_ValueSubscript(Jsi_Interp *interp, Jsi_Value *target, Jsi_Value *key, Jsi_Value **ret, int right_val);
extern void jsi_ValueSubscriptLen(Jsi_Interp *interp, Jsi_Value *target, Jsi_Value *key, Jsi_Value **ret, int right_val);
extern Jsi_Value* jsi_ValueObjKeyAssign(Jsi_Interp *interp, Jsi_Value *target, Jsi_Value *key, Jsi_Value *value, int flag);
extern void jsi_ValueObjGetKeys(Jsi_Interp *interp, Jsi_Value *target, Jsi_Value *ret, bool isof);
extern Jsi_Value* jsi_ObjArrayLookup(Jsi_Interp *interp, Jsi_Obj *obj, const char *key);
extern void jsi_ValueInsertArray(Jsi_Interp *interp, Jsi_Value *target, int index, Jsi_Value *val, int flags);
extern Jsi_Value* jsi_ProtoObjValueNew1(Jsi_Interp *interp, const char *name);
extern Jsi_Value* jsi_ProtoValueNew(Jsi_Interp *interp, const char *name, const char *parent);
extern Jsi_Value* jsi_ObjValueNew(Jsi_Interp *interp);
extern Jsi_Value* Jsi_ValueDup(Jsi_Interp *interp, Jsi_Value *v);
extern int jsi_ValueToOInt32(Jsi_Interp *interp, Jsi_Value *v);
extern Jsi_RC jsi_FreeOneLoadHandle(Jsi_Interp *interp, Jsi_HashEntry *hPtr, void *handle);
extern Jsi_Value* jsi_MakeFuncValue(Jsi_Interp *interp, Jsi_CmdProc *callback, const char *name, Jsi_Value** toVal, Jsi_CmdSpec *cspec);
extern Jsi_Value* jsi_MakeFuncValueSpec(Jsi_Interp *interp, Jsi_CmdSpec *cmdSpec, void *privData);
extern bool jsi_FuncArgCheck(Jsi_Interp *interp, Jsi_Func *f, const char *argStr);
extern bool jsi_CommandArgCheck(Jsi_Interp *interp, Jsi_CmdSpec *cmdSpec, Jsi_Func *f, const char *parent);
extern Jsi_RC jsi_FileStatCmd(Jsi_Interp *interp, Jsi_Value *fnam, Jsi_Value *_this,
    Jsi_Value **ret, Jsi_Func *funcPtr, int islstat);
extern Jsi_RC jsi_LoadLoadCmd(Jsi_Interp *interp, Jsi_Value *args, 
    Jsi_Value *_this, Jsi_Value **ret, Jsi_Func *funcPtr);
extern Jsi_RC jsi_LoadUnloadCmd(Jsi_Interp *interp, Jsi_Value *args, 
    Jsi_Value *_this, Jsi_Value **ret, Jsi_Func *funcPtr);
extern void jsi_ValueToPrimitive(Jsi_Interp *interp, Jsi_Value **vPtr);
extern Jsi_RC jsi_HashFree(Jsi_Interp *interp, Jsi_HashEntry *hPtr, void *ptr);
extern Jsi_RC jsi_evalStrFile(Jsi_Interp* interp, Jsi_Value *path, char *str, int flags, int level);
extern Jsi_RC jsi_FuncArgsToString(Jsi_Interp *interp, Jsi_Func *f, Jsi_DString *dStr, int flags);
extern Jsi_Value *jsi_LoadFunction(Jsi_Interp *interp, const char *str, Jsi_Value *tret);

extern Jsi_IterObj *jsi_IterObjNew(Jsi_Interp *interp, Jsi_IterProc *iterProc);
extern void jsi_IterObjFree(Jsi_IterObj *iobj);
extern Jsi_FuncObj *jsi_FuncObjNew(Jsi_Interp *interp, Jsi_Func *func);
extern void jsi_FuncObjFree(Jsi_FuncObj *fobj);
extern Jsi_RC jsi_ArglistFree(Jsi_Interp *interp, Jsi_HashEntry *hPtr, void *ptr);
extern void jsi_FuncFree(Jsi_Interp *interp, Jsi_Func *func);

#ifndef _JSI_MEMCLEAR
#ifndef JSI_MEM_DEBUG
#define _JSI_MEMCLEAR(ptr)
#else
#define _JSI_MEMCLEAR(ptr) memset(ptr, 0, sizeof(*ptr)) /* To aid debugging memory.*/
#endif
#endif

#define MAX_SUBREGEX    256
#define JSI__LONG_LONG
#define UCHAR(s) (unsigned char)(s)
#define StrnCpy(d,s) Jsi_Strncpy(d,s,sizeof(d)), d[sizeof(d)-1] = 0
extern char* jsi_SubstrDup(const char *a, int start, int len);
extern int jsi_typeGet(Jsi_Interp *interp , const char *tname);
extern const char *jsi_typeName(Jsi_Interp *interp, int typ, Jsi_DString *dStr);
extern Jsi_RC jsi_ArgTypeCheck(Jsi_Interp *interp, int typ, Jsi_Value *arg, const char *p1, const char *p2, int index, Jsi_Func *func, bool isdefault);
extern void jsi_FuncCallCheck(jsi_Pstate *p, jsi_Pline *line, int argc, bool isNew, const char *name, const char *namePre, Jsi_OpCodes *argCodes);
extern Jsi_RC jsi_RunFuncCallCheck(Jsi_Interp *interp, Jsi_Func *func, int argc, const char *name, jsi_Pline *line, Jsi_OpCodes *argCodes);
extern Jsi_ScopeStrs *jsi_ArgsOptAdd(jsi_Pstate *pstate, Jsi_ScopeStrs *a);
extern Jsi_ScopeStrs *jsi_argInsert(jsi_Pstate *pstate, Jsi_ScopeStrs *a, const char *name, Jsi_Value *defValue, jsi_Pline *lPtr);
extern Jsi_ScopeStrs* jsi_ParseArgStr(Jsi_Interp *interp, const char *argStr);
extern Jsi_RC jsi_LoadLibrary(Jsi_Interp *interp, const char *pathName);
extern Jsi_Value* jsi_AccessFile(Jsi_Interp *interp, const char *name, int mode);
extern double jsi_GetTimestamp(void);
extern const char *jsi_GetCurFile(Jsi_Interp *interp);
extern void jsi_TypeMismatch(Jsi_Interp* interp);
extern void jsi_SortDString(Jsi_Interp *interp, Jsi_DString *dStr, const char *sep);
extern const char* jsi_GetDirective(Jsi_Interp *interp, Jsi_OpCodes *ops, const char *str);
extern Jsi_Value* jsi_CommandCreate(Jsi_Interp *interp, const char *name, Jsi_CmdProc *cmdProc, void *privData, int flags, Jsi_CmdSpec *cspec);
extern int jsi_GetDefaultType(const char *cp);
extern Jsi_RC jsi_ParseTypeCheckStr(Jsi_Interp *interp, const char *str);
extern Jsi_Interp *jsi_DoExit(Jsi_Interp *interp, int rc);
extern Jsi_RC jsi_SysMarkdownCmd(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this, Jsi_Value **ret, Jsi_Func *funcPtr);
extern Jsi_RC jsi_CDataDataSetCmdSub(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this,
                             Jsi_Value **ret, Jsi_Func *funcPtr, int flags);
extern char jsi_toHexChar(char code);
extern char jsi_fromHexChar(char ch);

typedef enum {
    _JSI_CDATA_INFO=0,
    _JSI_CDATA_GET=1,
    _JSI_CDATA_SET=2,
    _JSI_CDATA_SIZE=3,
    _JSI_CDATA_SCHEMA=4,
    _JSI_CDATA_STRUCT=6
} jsi_cdatasub;

//extern Jsi_RC jsi_cdataMapsubCmd(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this,
//    Jsi_Value **ret, Jsi_Func *funcPtr, jsi_cdatasub sub);

#define jsi_ValueString(pv) (pv->vt == JSI_VT_STRING ? &pv->d.s : \
  ((pv->vt == JSI_VT_OBJECT && pv->d.obj->ot == JSI_OT_STRING) ? &pv->d.obj->d.s : NULL))
                 
#define jsi_IsStrictMode(interp) (interp->strict || interp->framePtr->strict)

#ifdef JSI_MEM_DEBUG
#define jsi_ValueDebugUpdate(interp, v, tbl, file, line, func) jsi_ValueDebugUpdate_(interp, &v->VD, v, interp->dbPtr->tbl, file, line, func)
#define jsi_ValueDebugLabel(v,l1,l2) jsi_ValueDebugLabel_(&v->VD,l1,l2)

#define Jsi_ValueNew(interp) jsi_ValueNew(interp, __FILE__, __LINE__,__PRETTY_FUNCTION__)
#define Jsi_ValueNew1(interp) jsi_ValueNew1(interp, __FILE__, __LINE__,__PRETTY_FUNCTION__)
#define Jsi_ValueDup(interp,v) jsi_ValueDup(interp, v,__FILE__, __LINE__,__PRETTY_FUNCTION__)
#define Jsi_ObjNew(interp) jsi_ObjNew(interp, __FILE__, __LINE__,__PRETTY_FUNCTION__)

extern Jsi_Value *jsi_ValueNew(Jsi_Interp *interp, const char *fname, int line, const char *func);
extern Jsi_Value *jsi_ValueNew1(Jsi_Interp *interp, const char *fname, int line, const char *func);
extern Jsi_Value *jsi_ValueDup(Jsi_Interp *interp, Jsi_Value *ov, const char *fname, int line, const char *func);
extern Jsi_Obj *jsi_ObjNew(Jsi_Interp *interp, const char *fname, int line, const char *func);
extern void jsi_ValueDebugLabel_(jsi_ValueDebug *vd, const char *l1, const char *l2);
extern void jsi_ValueDebugUpdate_(Jsi_Interp *interp, jsi_ValueDebug *vd, void *v, Jsi_Hash* tbl, const char *fname, int line, const char *func);
extern void jsi_DebugValue(Jsi_Value* v, const char *reason, uint idx, Jsi_Interp *interp);
extern void jsi_DebugObj(Jsi_Obj* o, const char *reason, uint idx, Jsi_Interp *interp);

#define jsi_DebugValueCallIdx() ++interp->dbPtr->memDebugCallIdx
#define VALINIT { __VALSIG__ .refCnt=1, .vt=JSI_VT_UNDEF, .f={.flag=JSI_OM_ISSTATIC}, .d={}, .VD={.fname=__FILE__, .line=__LINE__,.func=__PRETTY_FUNCTION__}  }
#else
#define VALINIT { __VALSIG__ .refCnt=1, .vt=JSI_VT_UNDEF, .f={.flag=JSI_OM_ISSTATIC}  }
#define jsi_ValueDebugUpdate(interp, vd, v, tbl, file, line, func)
#define jsi_ValueDebugLabel(v,l1,l2)
#define jsi_DebugValue(v,r,i,t)
#define jsi_DebugObj(o,r,i,t)
#define jsi_DebugValueCallIdx() 0
#define jsi_ValueDebugLabel_(v,l1,l2)
#endif

#define DECL_VALINIT(n) Jsi_Value n = VALINIT

void jsi_TraceFuncCall(Jsi_Interp *interp, Jsi_Func *func, jsi_OpCode *iPtr, 
    Jsi_Value *_this, Jsi_Value* args, Jsi_Value *ret);

struct Jsi_Stubs;
extern struct Jsi_Stubs *jsiStubsTblPtr;
extern char *jsi_execName;
extern const char *jsi_AssertModeStrs[];
extern Jsi_Chan jsi_StdChans[3];
extern Jsi_CmdSpec cDataArrayCmds[];
#define jsi_Stdin jsi_StdChans
#define jsi_Stdout jsi_StdChans+1
#define jsi_Stderr jsi_StdChans+2

#if JSI__SANITIZE
#define Jsi_Malloc(sz) malloc(sz)
#define Jsi_Calloc(nm, sz) calloc(nm,sz)
#define Jsi_Realloc(ptr, sz) realloc(ptr,sz)
#define Jsi_Free(ptr) free(ptr)
#endif

#define jsi_IIOF .flags=JSI_OPT_INIT_ONLY
#define jsi_IIRO .flags=JSI_OPT_READ_ONLY

#endif /* __JSIINT_H__ */
