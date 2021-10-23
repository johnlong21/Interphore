#ifndef JSI_LITE_ONLY
#ifndef JSI_AMALGAMATION
#include "jsiInt.h"
#endif
#if JSI__FILESYS==1
#include <errno.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <dirent.h>
#include <signal.h>
#include <unistd.h>
#ifndef __WIN32
#include <pwd.h>
#else
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <fcntl.h>
#endif
#include <limits.h>

#define HAVE_UNISTD_H
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#elif defined(_MSC_VER)
#include <direct.h>
#define F_OK 0
#define W_OK 2
#define R_OK 4
#define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
#define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#endif

#ifndef MAXPATHLEN
#define MAXPATHLEN 1024
#endif

#define GSVal(s) GetStringVal(interp, s)
static const char *GetStringVal(Jsi_Interp *interp, Jsi_Value *s) {
    const char *cp = Jsi_ValueString(interp, s, 0);
    return cp ? cp : "";
}

#define SAFEACCESS(fname, writ)  \
if (interp->isSafe && Jsi_InterpAccess(interp, fname, writ) != JSI_OK) \
        return Jsi_LogError("%s access denied", writ?"write":"read");

Jsi_RC jsi_FileStatCmd(Jsi_Interp *interp, Jsi_Value *fnam, Jsi_Value *_this,
    Jsi_Value **ret, Jsi_Func *funcPtr, int islstat)
{
    int rc;
    Jsi_StatBuf st;
    SAFEACCESS(fnam, 0)
    if (islstat)
        rc = Jsi_Lstat(interp, fnam, &st);
    else
        rc = Jsi_Stat(interp, fnam, &st);
    if (rc != 0) 
        return Jsi_LogError("file not found: %s", GSVal(fnam));
   /* Single object containing result members. */
    Jsi_Value *vres;
    Jsi_Obj  *ores;
    Jsi_Value *nnv;
    vres = Jsi_ValueMakeObject(interp, NULL, ores = Jsi_ObjNew(interp));
    Jsi_IncrRefCount(interp, vres);
#define MKDBL(nam,val) \
    nnv = Jsi_ValueMakeNumber(interp, NULL, (Jsi_Number)val); \
    Jsi_ObjInsert(interp, ores, nam, nnv, 0);
    
    MKDBL("dev",st.st_dev); MKDBL("ino",st.st_ino); MKDBL("mode",st. st_mode);
    MKDBL("nlink",st.st_nlink); MKDBL("uid",st.st_uid); MKDBL("gid",st.st_gid);
    MKDBL("rdev",st.st_rdev);
#ifndef __WIN32
    MKDBL("blksize",st.st_blksize); MKDBL("blocks",st.st_blocks);
#endif
    MKDBL("atime",st.st_atime); MKDBL("mtime",st.st_mtime); MKDBL("ctime",st.st_ctime);    
    MKDBL("size",st.st_size);
    Jsi_ValueDup2(interp, ret, vres);
    Jsi_DecrRefCount(interp, vres);
    return JSI_OK;
}


static Jsi_RC FileStatCmd(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this,
    Jsi_Value **ret, Jsi_Func *funcPtr)
{
    return jsi_FileStatCmd(interp, Jsi_ValueArrayIndex(interp, args, 0), _this, ret, funcPtr, 0);
}

static Jsi_RC FileLstatCmd(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this,
    Jsi_Value **ret, Jsi_Func *funcPtr)
{
    return jsi_FileStatCmd(interp, Jsi_ValueArrayIndex(interp, args, 0), _this, ret, funcPtr, 1);
}

static const char *getFileType(int mode, int lmode)
{
#ifdef S_ISLNK
    if (S_ISLNK(mode) || S_ISLNK(lmode)) {
        return "link";
    }
#endif
    if (S_ISDIR(mode)) {
        return "directory";
    }
#ifdef S_ISCHR
    else if (S_ISCHR(mode)) {
        return "characterSpecial";
    }
#endif
#ifdef S_ISBLK
    else if (S_ISBLK(mode)) {
        return "blockSpecial";
    }
#endif
#ifdef S_ISFIFO
    else if (S_ISFIFO(mode)) {
        return "fifo";
    }
#endif
#ifdef S_ISSOCK
    else if (S_ISSOCK(mode)) {
        return "socket";
    }
#endif
    else if (S_ISREG(mode)) {
        return "file";
    }
    return "unknown";
}

enum { FSS_Exists, FSS_Atime, FSS_Mtime, FSS_Writable, FSS_Readable, FSS_Executable, FSS_Type, 
FSS_Owned, FSS_Isdir, FSS_Isfile };

static Jsi_RC _FileSubstat(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this,
    Jsi_Value **ret, Jsi_Func *funcPtr, int sub)
{
    Jsi_Value *fnam = Jsi_ValueArrayIndex(interp, args, 0);
    int rc;
    Jsi_StatBuf st, lst;
    rc = Jsi_Stat(interp, fnam, &st) | Jsi_Lstat(interp, fnam, &lst);
    if (rc != 0 && sub != FSS_Exists && sub != FSS_Writable && sub != FSS_Readable && sub != FSS_Isdir && sub != FSS_Isfile) {
        if (!interp->fileStrict)
            return JSI_OK;
        return Jsi_LogError("file not found: %s", GSVal(fnam));
    }
    switch (sub) {
        case FSS_Exists: Jsi_ValueMakeBool(interp, ret, (rc == 0)); break;
        case FSS_Atime: Jsi_ValueMakeNumber(interp, ret, st.st_atime); break;
        case FSS_Mtime: Jsi_ValueMakeNumber(interp, ret, st.st_mtime); break;
        case FSS_Writable: Jsi_ValueMakeBool(interp, ret, (Jsi_Access(interp, fnam, W_OK) != -1));  break;
        case FSS_Readable: Jsi_ValueMakeBool(interp, ret, (Jsi_Access(interp, fnam, R_OK) != -1));  break;
        case FSS_Executable:
#ifdef X_OK
            Jsi_ValueMakeBool(interp, ret, Jsi_Access(interp, fnam, X_OK) != -1);
#else
            Jsi_ValueMakeBool(interp, ret, 1);
#endif
            break;
        case FSS_Type: Jsi_ValueMakeStringKey(interp, ret, (char*)getFileType((int)st.st_mode, (int)lst.st_mode)); break;
        case FSS_Owned:
#ifndef __WIN32
            Jsi_ValueMakeBool(interp, ret, rc == 0 && geteuid() == st.st_uid);
#endif
            break;
        case FSS_Isdir: Jsi_ValueMakeBool(interp, ret, rc == 0 && S_ISDIR(st.st_mode));  break;
        case FSS_Isfile: Jsi_ValueMakeBool(interp, ret, rc == 0 && S_ISREG(st.st_mode));  break;
#ifndef __cplusplus
        default:
            assert(0);
#endif
    }
    return JSI_OK;
}
#define MAKE_FSS_SUB(nam) \
static Jsi_RC File##nam##Cmd(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this, \
    Jsi_Value **ret, Jsi_Func *funcPtr) \
{\
    return _FileSubstat(interp, args, _this, ret, funcPtr, FSS_##nam);\
}
MAKE_FSS_SUB(Exists) MAKE_FSS_SUB(Atime) MAKE_FSS_SUB(Writable) MAKE_FSS_SUB(Readable)
MAKE_FSS_SUB(Executable) MAKE_FSS_SUB(Type) MAKE_FSS_SUB(Owned)
MAKE_FSS_SUB(Isdir) MAKE_FSS_SUB(Isfile) MAKE_FSS_SUB(Mtime)
#ifndef __WIN32
#define MKDIR_DEFAULT(PATHNAME) mkdir(PATHNAME, 0755)
#else
#define MKDIR_DEFAULT(PATHNAME) mkdir(PATHNAME)
#endif

static int mkdir_all(Jsi_Interp *interp, Jsi_Value *file)
{
    int ok = 1;
    char *path = Jsi_ValueString(interp, file, NULL);
    if (!path) {
        Jsi_LogError("expected string");
        return -1;
    }

    /* First time just try to make the dir */
    goto first;

    while (ok--) {
        /* Must have failed the first time, so recursively make the parent and try again */
        {
            char *slash = Jsi_Strrchr(path, '/');

            if (slash && slash != path) {
                *slash = 0;
                if (mkdir_all(interp, file) != 0) {
                    return -1;
                }
                *slash = '/';
            }
        }
      first:
        if (MKDIR_DEFAULT(path) == 0) {
            return 0;
        }
        if (errno == ENOENT) {
            /* Create the parent and try again */
            continue;
        }
        /* Maybe it already exists as a directory */
        if (errno == EEXIST) {
            Jsi_StatBuf sb;

            if (Jsi_Stat(interp, file, &sb) == 0 && S_ISDIR(sb.st_mode)) {
                return 0;
            }
            /* Restore errno */
            errno = EEXIST;
        }
        /* Failed */
        break;
    }
    return -1;
}

static Jsi_RC FilePwdCmd(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this,
    Jsi_Value **ret, Jsi_Func *funcPtr)
{
    Jsi_DString dStr = {};
    Jsi_ValueMakeStringDup(interp, ret, Jsi_GetCwd(interp, &dStr));
    Jsi_DSFree(&dStr);
    return JSI_OK;
}

static Jsi_RC FileChdirCmd(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this,
    Jsi_Value **ret, Jsi_Func *funcPtr)
{
    Jsi_Value *path = Jsi_ValueArrayIndex(interp, args, 0);
    if (!path) 
        return Jsi_LogError("expected string");
    int rc = Jsi_Chdir(interp, path);
    if (rc != 0) 
        return Jsi_LogError("can't change to directory \"%s\": %s", GSVal(path), strerror(errno));    
    return JSI_OK;
}

static Jsi_RC FileMkdirCmd(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this,
    Jsi_Value **ret, Jsi_Func *funcPtr)
{
    Jsi_Value *path = Jsi_ValueArrayIndex(interp, args, 0);
    Jsi_Value *vf = Jsi_ValueArrayIndex(interp, args, 1);
    char *spath =  Jsi_ValueString(interp, path,0);
    int rc, force = 0; 

    if (!spath) 
        return Jsi_LogError("expected string");
    if (vf && !Jsi_ValueIsBoolean(interp, vf)) 
        return Jsi_LogError("expected boolean");
    if (vf)
        force = vf->d.val;
    if (force==0)
        rc = MKDIR_DEFAULT(spath);
    else {
        Jsi_Value *npath = Jsi_ValueNewStringDup(interp, spath);
        rc = mkdir_all(interp, npath);
        Jsi_ValueFree(interp, npath);
    }
    if (rc != 0) 
        return Jsi_LogError("can't create directory \"%s\": %s", spath, strerror(errno));    
    return JSI_OK;
}

static Jsi_RC FileTempfileCmd(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this,
    Jsi_Value **ret, Jsi_Func *funcPtr)
{
    char *filename;
#ifndef __WIN32
    Jsi_Value *vt = Jsi_ValueArrayIndex(interp, args, 0);
    const char *tp, *templ = "/tmp/jsiXXXXXX";

    if (vt && (tp = Jsi_ValueString(interp, vt, NULL))) {
        templ = tp;
    }
    filename = Jsi_Strdup(templ);
    int fd = mkstemp(filename);
    if (fd < 0)
        goto fail;
    close(fd);
#else
#ifndef MAX_PATH
#define MAX_PATH 1024
#endif
    char name[MAX_PATH];

    if (!GetTempPath(MAX_PATH, name) || !GetTempFileName(name, "JSI", 0, name)) 
        return Jsi_LogError("failed to get temp file");
    filename = Jsi_Strdup(name);
#endif
    Jsi_ValueMakeString(interp, ret, filename);
    return JSI_OK;
    
#ifndef __WIN32
fail:
    Jsi_LogError("Failed to create tempfile");
    Jsi_Free(filename);
    return JSI_ERROR;
#endif
}

static Jsi_RC FileTruncateCmd(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this,
    Jsi_Value **ret, Jsi_Func *funcPtr)
{   
    Jsi_RC rc;
    Jsi_Value *fname = Jsi_ValueArrayIndex(interp, args, 0);
    Jsi_Value *sizv = Jsi_ValueArrayIndex(interp, args, 1);
    Jsi_Number siz;
    if (Jsi_GetNumberFromValue(interp, sizv, &siz) != JSI_OK)
        return JSI_ERROR;
    SAFEACCESS(fname, 1)
    Jsi_Channel ch = Jsi_Open(interp, fname, "rb+");
    if (!ch)
        return JSI_ERROR;
    rc = (Jsi_Truncate(ch, (unsigned int)siz) == 0 ? JSI_OK : JSI_ERROR);
    Jsi_Close(ch);
    return rc;
}

static Jsi_RC FileChmodCmd(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this,
    Jsi_Value **ret, Jsi_Func *funcPtr)
{
    Jsi_Value *fname = Jsi_ValueArrayIndex(interp, args, 0);
    Jsi_Value *modv = Jsi_ValueArrayIndex(interp, args, 1);
    Jsi_Number fmod;
    if (Jsi_GetNumberFromValue(interp, modv, &fmod) != JSI_OK)
        return JSI_ERROR;
    SAFEACCESS(fname, 1)
    return (Jsi_Chmod(interp, fname, (unsigned int)fmod) == 0 ? JSI_OK : JSI_ERROR);
}

static Jsi_RC FileReadCmd(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this,
    Jsi_Value **ret, Jsi_Func *funcPtr) /* TODO limit. */
{   
    Jsi_Value *fname = Jsi_ValueArrayIndex(interp, args, 0);
    Jsi_Value *vmode = Jsi_ValueArrayIndex(interp, args, 1);
    const char *mode = (vmode ? Jsi_ValueString(interp, vmode, NULL) : NULL);
    SAFEACCESS(fname, 0)

    Jsi_Channel chan = Jsi_Open(interp, fname, (mode ? mode : "rb"));
    int n, sum = 0;
    if (!chan) 
        return Jsi_LogError("failed open for read: %s", GSVal(fname));
    Jsi_DString dStr = {};
    char buf[BUFSIZ];
    while (sum < MAX_LOOP_COUNT && (n = Jsi_Read(chan, buf, sizeof(buf))) > 0) {
        Jsi_DSAppendLen(&dStr, buf, n);
        sum += n;
    }
    Jsi_Close(chan);
    Jsi_ValueMakeDStringObject(interp, ret, &dStr);
    return JSI_OK;
}


static Jsi_RC FileWriteCmd(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this,
    Jsi_Value **ret, Jsi_Func *funcPtr)
{   
    Jsi_Value *fname = Jsi_ValueArrayIndex(interp, args, 0);
    const char *data;
    Jsi_Value *v = Jsi_ValueArrayIndex(interp, args, 1);
    Jsi_Value *vmode = Jsi_ValueArrayIndex(interp, args, 2);
    const char *mode = (vmode ? Jsi_ValueString(interp, vmode, NULL) : NULL);
    Jsi_Channel chan;
    int n, len, cnt = 0, sum = 0;
    SAFEACCESS(fname, 1)
    if (!(data = Jsi_ValueGetStringLen(interp, v, &len))) {
        return JSI_ERROR;
    }
    chan = Jsi_Open(interp, fname, (mode ? mode : "wb+"));
    if (!chan) 
        return Jsi_LogError("failed open for write: %s", GSVal(fname));
    while (cnt < MAX_LOOP_COUNT && len > 0 && (n = Jsi_Write(chan, data, len)) > 0) {
        len -= n;
        sum += n;
        cnt++;
    }
    Jsi_Close(chan);
    /* TODO: handle nulls. */
    Jsi_ValueMakeNumber(interp, ret, (Jsi_Number)sum);
    return JSI_OK;
}

static Jsi_RC FileRenameCmd(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this,
    Jsi_Value **ret, Jsi_Func *funcPtr)
{   
    Jsi_Value *source = Jsi_ValueArrayIndex(interp, args, 0);
    Jsi_Value *dest = Jsi_ValueArrayIndex(interp, args, 1);
    Jsi_Value *vf = Jsi_ValueArrayIndex(interp, args, 2);
    int force = 0; 

    SAFEACCESS(source, 1)
    SAFEACCESS(dest, 1)
    if (vf && !Jsi_ValueIsBoolean(interp, vf)) 
        return Jsi_LogError("expected boolean");
    if (vf)
        force = vf->d.val;
    if (force==0 && Jsi_Access(interp, dest, F_OK) == 0) 
        return Jsi_LogError("error renaming \"%s\" to \"%s\": target exists", GSVal(source), GSVal(dest));

    if (Jsi_Rename(interp, source, dest) != 0) 
        return Jsi_LogError( "error renaming \"%s\" to \"%s\": %s", GSVal(source),
            GSVal(dest), strerror(errno));
    return JSI_OK;
}

static int FileCopy(Jsi_Interp *interp, Jsi_Value* src, Jsi_Value* dest, int force) {
    Jsi_Channel ich = Jsi_Open(interp, src, "rb");
    if (!ich)
        return -1;
    if (force && Jsi_Access(interp, dest, F_OK) == 0) {
        Jsi_Remove(interp, dest, 0);
    }
    Jsi_Channel och = Jsi_Open(interp, dest, "wb+");
    if (!och)
        return -1;
    while (1) {
        char buf[BUFSIZ];
        int n;
        n = Jsi_Read(ich, buf, BUFSIZ);
        if (n<=0)
            break;
        if (Jsi_Write(och, buf, n) != n) {
            Jsi_Close(ich);
            Jsi_Close(och);
            return -1;
        }
    }
    /* TODO: set perms. */
#ifndef __WIN32
    Jsi_StatBuf sb;
    Jsi_Stat(interp, src, &sb);
    Jsi_Chmod(interp, dest, sb.st_mode);
#endif
    Jsi_Close(ich);
    Jsi_Close(och);
    return 0;
}

#define FN_copy JSI_INFO("\
Directories are not handled. \
The third argument if given is a boolean force value \
which if true allows overwrite of an existing file. ")
static Jsi_RC FileCopyCmd(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this,
    Jsi_Value **ret, Jsi_Func *funcPtr)
{
    Jsi_Value *source = Jsi_ValueArrayIndex(interp, args, 0);
    Jsi_Value *dest = Jsi_ValueArrayIndex(interp, args, 1);
    Jsi_Value *vf = Jsi_ValueArrayIndex(interp, args, 2);
    int force = 0; 
    SAFEACCESS(source, 0)
    SAFEACCESS(dest, 1)
    if (vf && !Jsi_ValueIsBoolean(interp, vf)) 
        return Jsi_LogError("expected boolean");
    if (vf)
        force = vf->d.val;
    if (force==0 && Jsi_Access(interp, dest, F_OK) == 0) 
        return Jsi_LogError("error copying \"%s\" to \"%s\": target exists", GSVal(source), GSVal(dest));

    if (FileCopy(interp, source, dest, force) != 0) 
        return Jsi_LogError( "error copying \"%s\" to \"%s\": %s", GSVal(source),
            GSVal(dest), strerror(errno));
    return JSI_OK;
}

#define FN_link JSI_INFO("\
The second argument is the destination file to be created. "\
"If a third bool argument is true, a hard link is created.")
static Jsi_RC FileLinkCmd(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this,
    Jsi_Value **ret, Jsi_Func *funcPtr)
{
    Jsi_Value *source = Jsi_ValueArrayIndex(interp, args, 0);
    Jsi_Value *dest = Jsi_ValueArrayIndex(interp, args, 1);
    Jsi_Value *vf = Jsi_ValueArrayIndex(interp, args, 2);
    int hard = 0; 
    SAFEACCESS(source, 1)
    SAFEACCESS(dest, 1)
    if (vf && !Jsi_ValueIsBoolean(interp, vf)) 
        return Jsi_LogError("expected boolean");
    if (vf)
        hard = vf->d.val;

    if (Jsi_Link(interp, source, dest, hard) != 0) 
        return Jsi_LogError( "error linking \"%s\" to \"%s\": %s", GSVal(source),
            GSVal(dest), strerror(errno));
    return JSI_OK;
}

/* TODO: switch to MatchesInDir */
static Jsi_RC RmdirAll(Jsi_Interp *interp, Jsi_Value *path, int force)
{
    DIR *dir;
    struct dirent *entry;
    char spath[PATH_MAX];
    Jsi_RC erc = JSI_OK;
    char *dirname = Jsi_ValueString(interp, path, 0);
    if (!dirname) 
        return Jsi_LogError("expected string");

    /* TODO: change to Jsi_Scandir */
    dir = opendir(dirname);
    if (dir == NULL) {
        if (force)
            return JSI_OK;
        return Jsi_LogError("opening directory: %s", dirname);
    }

    while ((entry = readdir(dir)) != NULL) {
        if (Jsi_Strcmp(entry->d_name, ".") && Jsi_Strcmp(entry->d_name, "..")) {
            snprintf(spath, (size_t) PATH_MAX, "%s/%s", dirname, entry->d_name);
            Jsi_Value *tpPtr = Jsi_ValueNew1(interp);
            Jsi_ValueMakeStringDup(interp, &tpPtr, spath);
#ifndef __WIN32
            if (entry->d_type == DT_DIR) {
                Jsi_RC rc = RmdirAll(interp, tpPtr, force);
                if (rc != JSI_OK)
                    erc = rc;
            } else 
#endif
            {
                if (Jsi_Remove(interp, tpPtr, force) != 0) {
                    if (force)
                        Jsi_LogError("deleting file: %s", GSVal(tpPtr));
                    erc = JSI_ERROR;
                }
            }
            Jsi_DecrRefCount(interp, tpPtr);
        }
    }
    closedir(dir);
    Jsi_Remove(interp, path, force);
    return erc;
}

static Jsi_RC FileRemoveCmd(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this,
    Jsi_Value **ret, Jsi_Func *funcPtr)
{
    Jsi_Value *source = Jsi_ValueArrayIndex(interp, args, 0);
    Jsi_Value *vf = Jsi_ValueArrayIndex(interp, args, 1);
    const char *spath = GSVal(source);
    int rc, force = 0; 
    SAFEACCESS(source, 1)

    Jsi_StatBuf st;
    if (vf && !Jsi_ValueIsBoolean(interp, vf)) 
        return Jsi_LogError("expected boolean");
    if (vf)
        force = vf->d.val;
    rc = Jsi_Stat(interp, source, &st);
    if (Jsi_Strcmp(spath,"/")==0)
        return JSI_ERROR;
    if (rc != 0) {
        if (force)
            return JSI_OK;
        return Jsi_LogError("error deleting \"%s\": target not found", spath);
    }

    if (Jsi_Remove(interp, source, force) != 0) {
        if (!S_ISDIR(st.st_mode)) {
            if (force)
                return JSI_OK;
            return Jsi_LogError("error deleting \"%s\"", spath);
        }
        if (force==0) 
            return Jsi_LogError("error deleting \"%s\": directory not empty", spath);
        return RmdirAll(interp, source, force);
    }
    return JSI_OK;
}

static Jsi_RC FileReadlinkCmd(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this,
    Jsi_Value **ret, Jsi_Func *funcPtr)
{
    Jsi_Value *path = Jsi_ValueArrayIndex(interp, args, 0);
    SAFEACCESS(path, 1)

    char *linkValue = (char*)Jsi_Malloc(MAXPATHLEN + 1);

    int linkLength = Jsi_Readlink(interp, path, linkValue, MAXPATHLEN);

    if (linkLength == -1) {
        Jsi_Free(linkValue);
        return Jsi_LogError("couldn't readlink \"%s\": %s", GSVal(path), strerror(errno));
    }
    linkValue[linkLength] = 0;
    Jsi_ValueMakeString(interp, ret, linkValue);
    return JSI_OK;
}

static Jsi_RC FileIsRelativeCmd(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this,
    Jsi_Value **ret, Jsi_Func *funcPtr)
{
    const char *path = Jsi_ValueString(interp, Jsi_ValueArrayIndex(interp, args, 0), NULL);
    if (path == NULL) 
        return Jsi_LogError("expected string");
    bool isRel = 1;
#if defined(__MINGW32__) || defined(_MSC_VER)
    if ((path[0] && path[1] == ':') || (path[0] == '\\'))
        isRel = 0;
    else
#endif
        isRel = (path[0] != '/');
    Jsi_ValueMakeBool(interp, ret, isRel);
    return JSI_OK;
}

static Jsi_RC FileDirnameCmd(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this,
    Jsi_Value **ret, Jsi_Func *funcPtr)
{
    Jsi_Value *val = Jsi_ValueArrayIndex(interp, args, 0);
    const char *path = Jsi_ValueString(interp, val, NULL);
    if (!path) 
        return Jsi_LogError("expected string");
    const char *p = Jsi_Strrchr(path, '/');

    if (!p && path[0] == '.' && path[1] == '.' && path[2] == '\0') {
        Jsi_ValueMakeStringKey(interp, ret, "..");
    } else if (!p) {
        Jsi_ValueMakeStringKey(interp, ret, ".");
    }
    else if (p == path) {
        Jsi_ValueMakeStringKey(interp, ret, "/");
    }
#if defined(__MINGW32__) || defined(_MSC_VER)
    else if (p[-1] == ':') {
        Jsi_ValueMakeString(interp, ret, jsi_SubstrDup(path, 0, p-path+1));
    }
#endif
    else {
        Jsi_ValueMakeString(interp, ret, jsi_SubstrDup(path, 0, p-path));
    }
    return JSI_OK;
}

static Jsi_RC FileRootnameCmd(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this,
    Jsi_Value **ret, Jsi_Func *funcPtr)
{
    const char *path = Jsi_ValueString(interp, Jsi_ValueArrayIndex(interp, args, 0), NULL);
    if (path == NULL) 
        return Jsi_LogError("expected string");
    const char *lastSlash = Jsi_Strrchr(path, '/');
    const char *p = Jsi_Strrchr(path, '.');

    if (p == NULL || (lastSlash != NULL && lastSlash > p)) {
        Jsi_ValueMakeStringDup(interp, ret, path);
    }
    else {
        Jsi_ValueMakeString(interp, ret, jsi_SubstrDup(path,0, p-path));
    }
    return JSI_OK;
}
static Jsi_RC FileExtensionCmd(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this,
    Jsi_Value **ret, Jsi_Func *funcPtr)
{
    const char *path = Jsi_ValueString(interp, Jsi_ValueArrayIndex(interp, args, 0), NULL);
    if (path == NULL) 
        return Jsi_LogError("expected string");

    const char *lastSlash = Jsi_Strrchr(path, '/');
    const char *p = Jsi_Strrchr(path, '.');

    if (p == NULL || (lastSlash != NULL && lastSlash >= p)) {
        p = "";
    }
    Jsi_ValueMakeStringDup(interp, ret, p);
    return JSI_OK;

}
static Jsi_RC FileTailCmd(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this,
    Jsi_Value **ret, Jsi_Func *funcPtr)
{
    const char *path = Jsi_ValueString(interp, Jsi_ValueArrayIndex(interp, args, 0), NULL);
    if (path == NULL) 
        return Jsi_LogError("expected string");
    const char *lastSlash = Jsi_Strrchr(path, '/');

    if (lastSlash) {
        Jsi_ValueMakeStringDup(interp, ret, lastSlash+1);
    }
    else {
        Jsi_ValueMakeStringDup(interp, ret, path);
    }
    return JSI_OK;
}

static Jsi_RC FileRealpathCmd(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this,
    Jsi_Value **ret, Jsi_Func *funcPtr)
{
    Jsi_Value *path = Jsi_ValueArrayIndex(interp, args, 0);
    char *newname = Jsi_FileRealpath(interp, path, NULL);
    if (newname)
        Jsi_ValueMakeString(interp, ret, (char*)newname);
    return JSI_OK;

}

static Jsi_RC FileJoinCmd(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this,
    Jsi_Value **ret, Jsi_Func *funcPtr)
{
    Jsi_Value *p2 = Jsi_ValueArrayIndex(interp, args, 1);
    const char *path1 = Jsi_ValueString(interp, Jsi_ValueArrayIndex(interp, args, 0), NULL);
    const char *path2 = Jsi_ValueString(interp, p2, NULL);
    if (path1 == NULL || path2 == NULL) 
        return Jsi_LogError("expected string");

    char *newname;
    if (*path2 == '/' || *path2 == '~')
        newname = Jsi_FileRealpath(interp, p2, NULL);
    else {
        Jsi_DString dStr = {};
        Jsi_DSAppend(&dStr, path1, "/", path2, NULL);
        Jsi_Value *tpPtr = Jsi_ValueNew1(interp);
        Jsi_ValueMakeString(interp, &tpPtr, Jsi_DSFreeDup(&dStr));
        newname = Jsi_FileRealpath(interp, tpPtr, NULL);
        Jsi_DecrRefCount(interp, tpPtr);
    }
    if (newname ==  NULL)
        return JSI_ERROR;
    Jsi_ValueMakeString(interp, ret, (char*)newname);
    return JSI_OK;
}

static Jsi_RC FileSizeCmd(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this,
    Jsi_Value **ret, Jsi_Func *funcPtr)
{
    Jsi_Value *path = Jsi_ValueArrayIndex(interp, args, 0);
    int rc;
    Jsi_StatBuf st;
    SAFEACCESS(path, 0)
    if (Jsi_ValueArrayIndex(interp, args, 1))
        rc = Jsi_Lstat(interp, path, &st);
    else
        rc = Jsi_Stat(interp, path, &st);
    if (rc != 0) 
        return Jsi_LogError("bad file");
    Jsi_ValueMakeNumber(interp, ret, (Jsi_Number)st.st_size);
    return JSI_OK;
}

typedef struct {
    int limit;            /* Max number of results to return. */
    bool recurse;        /* Recurse into directories. */
    bool tails;          /* Return tail of path. */
    int maxDepth;       /* For recursive */
    int flags;
    bool retCount;
    int cnt;            /* Actual count. */
    int discardCnt;
    int maxDiscard;
    const char *types;   /* File types to include */
    const char *noTypes; /* File types to exclude */
    Jsi_Value *filter;    /* Function that returns true to keep. */
    Jsi_Value *dirFilter; /* Function that returns true to recurse into dir. */
    Jsi_Value *dir;
    const char *dirStr;
    int dirLen;
    const char *prefix;
} GlobData;

const char *globRetValues[] = { "file", "dir", "both", "count" };
static Jsi_OptionSpec GlobOptions[] = {
    JSI_OPT(VALUE,  GlobData, dir,      .help="The start directory: this path will not be prepended to results"),
    JSI_OPT(INT,    GlobData, maxDepth, .help="Maximum directory depth to recurse into"),
    JSI_OPT(INT,    GlobData, maxDiscard,.help="Maximum number of items to discard before giving up"),
    JSI_OPT(FUNC,   GlobData, dirFilter,.help="Filter function for directories, returning false to discard", .flags=0, .custom=0, .data=(void*)"dir:string"),
    JSI_OPT(FUNC,   GlobData, filter,   .help="Filter function to call with each file, returning false to discard", .flags=0, .custom=0, .data=(void*)"file:string"),
    JSI_OPT(INT,    GlobData, limit,    .help="The maximum number of results to return/count"),
    JSI_OPT(STRKEY, GlobData, noTypes,  .help="Filter files to exclude these \"types\""),
    JSI_OPT(STRKEY, GlobData, prefix,   .help="String prefix to add to each file in list"),
    JSI_OPT(BOOL,   GlobData, recurse,  .help="Recurse into sub-directories"),
    JSI_OPT(BOOL,   GlobData, retCount, .help="Return only the count of matches"),
    JSI_OPT(BOOL,   GlobData, tails,    .help="Returned only tail of path"),
    JSI_OPT(STRKEY, GlobData, types,    .help="Filter files to include type: one or more of chars 'fdlpsbc' for file, directory, link, etc"),
    JSI_OPT_END(GlobData)
};

static Jsi_RC SubGlobDirectory(Jsi_Interp *interp, Jsi_Obj *obj, char *zPattern, Jsi_Value *pattern,
  int isreg, const char *path, GlobData *opts, int deep)
{
    struct dirent **namelist;
    Jsi_RC rc = JSI_OK;
    int i, n, flags = opts->flags;
    bool bres = 0;
    Jsi_DString tStr = {};
    Jsi_Value *rpPath = Jsi_ValueNew1(interp);
    const char *spath = path;
    if (!spath || !spath[0])
        spath = ".";
    Jsi_ValueMakeStringDup(interp, &rpPath, spath);
    
    if ((n=Jsi_Scandir(interp, rpPath, &namelist, 0, 0)) < 0) {
        if (opts->recurse && deep) return JSI_OK;
        Jsi_LogError("bad directory");
        Jsi_DecrRefCount(interp, rpPath);
        return JSI_ERROR;
    }

    for (i=0; i<n && rc == JSI_OK; i++)
    {
        int ftyp;
        const char *z = namelist[i]->d_name;
        if (*z == '.') {
            if (!(flags&JSI_FILE_TYPE_HIDDEN))
                continue;
            else if ((z[1] == 0 || (z[1] == '.' && z[2] == 0)))
                continue;
        }
#ifdef __WIN32
        /* HACK in scandir(): if is directory then inode set to 1. */
        ftyp = (namelist[i]->d_ino? DT_DIR : DT_REG);
#else
        ftyp = namelist[i]->d_type;
#endif
        if (ftyp == DT_DIR) {
            if (opts->recurse && (opts->maxDepth<=0 || (deep+1) <= opts->maxDepth)) {
                const char *zz;
                Jsi_DString sStr = {};
                Jsi_DSAppend(&sStr, path, "/",  z, NULL);
                if (opts->dirFilter && Jsi_ValueIsFunction(interp, opts->dirFilter)) {
                    if (!(bres=Jsi_FunctionInvokeBool(interp, opts->dirFilter,
                        Jsi_ValueNewStringDup(interp, Jsi_DSValue(&sStr)))))
                        continue;
                    else if (bres!=0 && bres!=1)
                        rc = JSI_ERROR;
                }
                if (opts->types && Jsi_Strchr(opts->types, 'd')) {
                    opts->cnt++;
                    if (!opts->retCount) {
                        Jsi_DString pStr;
                        Jsi_DSInit(&pStr);
                        if (opts->prefix)
                            Jsi_DSAppend(&pStr, opts->prefix, z, NULL);
                        if (!opts->tails)
                            Jsi_DSAppend(&pStr, path, (path[0]?"/":""), NULL);
                        Jsi_DSAppend(&pStr, z, NULL);
                        zz = Jsi_DSValue(&pStr);
                        if (opts->dirLen && Jsi_Strlen(zz)>=(uint)opts->dirLen) {
                            zz += opts->dirLen;
                            if (zz[0] == '/') zz++;
                        }
                        Jsi_ObjArrayAdd(interp, obj, Jsi_ValueNewStringDup(interp, zz));
                        Jsi_DSFree(&pStr);
                    }
                }
                zz = Jsi_DSValue(&sStr);
                rc = SubGlobDirectory(interp, obj, zPattern, pattern,
                    isreg, zz, opts, deep+1);
                Jsi_DSFree(&sStr);
                if (opts->limit>0 && opts->cnt >= opts->limit)
                    goto done;
                if (opts->maxDiscard>0 && opts->discardCnt >= opts->maxDiscard)
                    goto done;
                if (rc != JSI_OK)
                    goto done;
            }
            if (opts->types==0 && opts->noTypes && (!(flags&JSI_FILE_TYPE_DIRS)))
                continue;
        } else {
            if (!(flags&JSI_FILE_TYPE_FILES))
                continue;
        }
        // TODO: sanity check types/noTypes.
        if (opts->types) {
            const char *cp = opts->types;
            int mat = 0;
            while (cp && *cp && !mat) {
                mat = 0;
                switch (*cp) {
                    case 'd': mat = (ftyp == DT_DIR); break;
                    case 'f': mat = (ftyp == DT_REG); break;
#ifndef __WIN32
                    case 'p': mat = (ftyp == DT_FIFO); break;
                    case 's': mat = (ftyp == DT_SOCK); break;
                    case 'l': mat = (ftyp == DT_LNK); break;
                    case 'c': mat = (ftyp == DT_CHR); break;
                    case 'b': mat = (ftyp == DT_BLK); break;
#endif
                }
                cp++;
            }
            if (!mat)
                continue;
        }
        if (opts->noTypes) {
            const char *cp = opts->noTypes;
            int mat = 0;
            while (cp && *cp && !mat) {
                mat = 0;
                switch (*cp) {
                    case 'd': mat = (ftyp == DT_DIR); break;
                    case 'f': mat = (ftyp == DT_REG); break;
#ifndef __WIN32
                    case 'p': mat = (ftyp == DT_FIFO); break;
                    case 's': mat = (ftyp == DT_SOCK); break;
                    case 'l': mat = (ftyp == DT_LNK); break;
                    case 'c': mat = (ftyp == DT_CHR); break;
                    case 'b': mat = (ftyp == DT_BLK); break;
#endif
                }
                cp++;
            }
            if (mat)
                continue;
        }

        if (isreg) {
           int ismat;
            Jsi_RegExpMatch(interp, pattern, z, &ismat, NULL);
            if (!ismat)
                continue;
        } else if (zPattern != NULL && Jsi_GlobMatch(zPattern, z, 0) == 0)
            continue;
        if (opts->filter && Jsi_ValueIsFunction(interp, opts->filter)) {
            if (!(bres=Jsi_FunctionInvokeBool(interp, opts->filter,
                Jsi_ValueNewStringDup(interp, z)))) {
                opts->discardCnt++;
                continue;
            } else if (bres!=0 && bres!=1)
                rc = JSI_ERROR;
        }
        opts->cnt++;
        if (!opts->retCount) {
            Jsi_DSInit(&tStr);
            if (opts->prefix)
                Jsi_DSAppend(&tStr, opts->prefix, z, NULL);
            if (!opts->tails)
                Jsi_DSAppend(&tStr, path, (path[0]?"/":""), NULL);
            Jsi_DSAppend(&tStr, z, NULL);
            z = Jsi_DSValue(&tStr);
            if (opts->dirLen && Jsi_Strlen(z)>=(uint)opts->dirLen) {
                z += opts->dirLen;
                if (z[0] == '/') z++;
            }
            rc = Jsi_ObjArrayAdd(interp, obj, Jsi_ValueNewStringDup(interp, z));
            Jsi_DSFree(&tStr);
        }
        if (opts->limit>0 && opts->cnt >= opts->limit)
            break;
        if (opts->maxDiscard>0 && opts->discardCnt >= opts->maxDiscard)
            break;
            
    }

done:
    if (rpPath)
        Jsi_DecrRefCount(interp, rpPath);
    if (namelist) {
        while (--n >= 0)
            Jsi_Free(namelist[n]);
        Jsi_Free(namelist);
    }
    return rc;     
}
#define FN_glob JSI_INFO("\
With no arguments (or null) returns all files/directories in current directory. \
The first argument can be a pattern (either a glob or regexp) of the files to return. \
When the second argument is a function, it is called with each path, and filter on false. \
Otherwise second arugment must be a set of options.")
static Jsi_RC FileGlobCmd(Jsi_Interp *interp, Jsi_Value *args, Jsi_Value *_this,
    Jsi_Value **ret, Jsi_Func *funcPtr)
{
    int fo = 1, isOpts = 0;
    Jsi_RC rc = JSI_OK;
    Jsi_Value *pat = Jsi_ValueArrayIndex(interp, args, 0);
    Jsi_Value *arg = Jsi_ValueArrayIndex(interp, args, 1);
    Jsi_Value *dir = NULL;
    Jsi_Value *tvPtr = Jsi_ValueNew1(interp);
    Jsi_Value *pvPtr = Jsi_ValueNew1(interp);
    GlobData Data = {};
    Jsi_Obj *obj = NULL;
    int len, isreg = 0;
    char *zPattern = NULL;
    const char *dcp;
    Jsi_DString dStr;
    Jsi_DSInit(&dStr);
    
    Data.flags = JSI_FILE_TYPE_FILES;

    if (arg)
        switch (arg->vt) {
        case JSI_VT_NULL: break;
        case JSI_VT_OBJECT:
        {
            Jsi_Obj *sobj = arg->d.obj;
            switch (sobj->ot) {
                case JSI_OT_OBJECT:
                    if (Jsi_ValueIsFunction(interp, arg)) {
                        Data.filter = arg;
                        break;
                    } else if (!sobj->isarrlist) {
                        isOpts = 1;
                        break;
                    }
                default: fo = 0;
            }
            if (fo) break;
        }
        default:
            rc = Jsi_LogError("arg2 must be a function, options or null");
            goto done;
    }
    if (isOpts && Jsi_OptionsProcess(interp, GlobOptions, &Data, arg, 0) < 0) {
        rc = JSI_ERROR;
        goto done;
    }
    if (Data.dir) {
        dcp = Data.dirStr = Jsi_ValueString(interp, Data.dir, &Data.dirLen);
        if (*dcp == '~') {
            dcp = Jsi_FileRealpath(interp, Data.dir, NULL);
            if (!dcp)
                dcp = Data.dirStr;
            else
                Data.dirLen = Jsi_Strlen(dcp);
        }
        Jsi_DSAppend(&dStr, dcp, NULL);
    }
    
    if (pat == NULL || Jsi_ValueIsNull(interp, pat)) {
        Jsi_ValueMakeStringKey(interp, &pvPtr, "*");
        pat = pvPtr;
        Data.flags = JSI_FILE_TYPE_DIRS|JSI_FILE_TYPE_FILES;
    } else if (Jsi_ValueIsString(interp, pat)) {
        char *cpp, *pstr = Jsi_ValueString(interp, pat, NULL);
        if (pstr && ((cpp=Jsi_Strrchr(pstr, '/')))) {
            Jsi_DString tStr = {};
            Jsi_DSAppend(&tStr, cpp+1, NULL);
            Jsi_ValueMakeStringKey(interp,  &pvPtr, Jsi_DSValue(&tStr));
            pat = pvPtr;
            Jsi_DSFree(&tStr);

            if (Jsi_DSLength(&dStr))
                Jsi_DSAppend(&tStr, "/", NULL);
            Jsi_DSAppendLen(&dStr, pstr, (cpp-pstr));
        }
    }

    dcp = Jsi_DSValue(&dStr);
    
    Jsi_ValueString(interp, dir, NULL);
    if (*dcp == '~') {
        dcp = Jsi_FileRealpath(interp, dir, NULL);
        if (dcp) {
            Jsi_ValueMakeString(interp, &tvPtr, dcp);
            dir = tvPtr;
        }
    }
    
    if (interp->isSafe) {

        if (Data.dir && Jsi_DSLength(&dStr)<=0)
            dir = Data.dir;
        else if (Jsi_DSLength(&dStr)<=0)
            Jsi_DSAppend(&dStr, ".", NULL);
        
        if (!dir) {
            Jsi_ValueMakeStringKey(interp, &tvPtr, Jsi_DSValue(&dStr));
            dir = tvPtr;
        }

        if (Jsi_InterpAccess(interp, dir, 0) != JSI_OK) {
            rc = Jsi_LogError("read access denied");
            goto done;
        }
    }
   // if (!Data.dir && Jsi_DSLength(&dStr)<=0)
    //    Jsi_DSAppend(&dStr, ".", NULL);
    if (!(isreg=Jsi_ValueIsObjType(interp, pat, JSI_OT_REGEXP)))
        zPattern = Jsi_ValueString(interp, pat, &len);
 /*   pathPtr = Jsi_ValueString(interp, dir, &len);
    if (!pathPtr) { rc = JSI_ERROR; goto done; }
    if (len && pathPtr[len-1] == '/')
        len--; */
        
    if (!Data.retCount) {
        obj = Jsi_ObjNew(interp);
        Jsi_ValueMakeArrayObject(interp, ret, obj);
    }
    rc = SubGlobDirectory(interp, obj, zPattern, pat, isreg, Jsi_DSValue(&dStr), &Data, 0);
    if (rc != JSI_OK)
        Jsi_ValueMakeUndef(interp, ret);   
    else if (Data.retCount)
        Jsi_ValueMakeNumber(interp, ret, (Jsi_Number)Data.cnt);

done:
    if (pvPtr)
        Jsi_DecrRefCount(interp, pvPtr);
    if (tvPtr)
        Jsi_DecrRefCount(interp, tvPtr);
    if (isOpts)
        Jsi_OptionsFree(interp, GlobOptions, &Data, 0);
    return rc;
}

static Jsi_CmdSpec fileCmds[] = {
    { "atime",      FileAtimeCmd,       1,  1, "file:string",  .help="Return file Jsi_Access time", .retType=(uint)JSI_TT_NUMBER },
    { "chdir",      FileChdirCmd,       1,  1, "file:string",  .help="Change current directory" },
    { "chmod",      FileChmodCmd,       2,  2, "file:string, mode:string",  .help="Set file permissions" },
    { "copy",       FileCopyCmd,        2,  3, "src:string, dest:string, force:boolean=false",  .help="Copy a file to destination", .retType=0, .flags=0, .info=FN_copy },
    { "dirname",    FileDirnameCmd,     1,  1, "file:string",  .help="Return directory path", .retType=(uint)JSI_TT_STRING },
    { "executable", FileExecutableCmd,  1,  1, "file:string",  .help="Return true if file is executable", .retType=(uint)JSI_TT_BOOLEAN },
    { "exists",     FileExistsCmd,      1,  1, "file:string",  .help="Return true if file exists", .retType=(uint)JSI_TT_BOOLEAN },
    { "extension",  FileExtensionCmd,   1,  1, "file:string",  .help="Return file extension", .retType=(uint)JSI_TT_STRING },
    { "join",       FileJoinCmd,        2,  2, "path:string, path:string",  .help="Join two file realpaths, or just second if an absolute path", .retType=(uint)JSI_TT_STRING },
    { "isdir",      FileIsdirCmd,       1,  1, "file:string",  .help="Return true if file is a directory", .retType=(uint)JSI_TT_BOOLEAN },
    { "isfile",     FileIsfileCmd,      1,  1, "file:string",  .help="Return true if file is a normal file", .retType=(uint)JSI_TT_BOOLEAN },
    { "isrelative", FileIsRelativeCmd,  1,  1, "file:string",  .help="Return true if file path is relative", .retType=(uint)JSI_TT_BOOLEAN },
    { "glob",       FileGlobCmd,        0,  2, "pattern:regexp|string|null='*', options:function|object|null=void", .help="Return list of files in dir with optional pattern match", .retType=(uint)JSI_TT_ARRAY, .flags=0, .info=FN_glob, .opts=GlobOptions },
    { "link",       FileLinkCmd,        2,  3, "src:string, dest:string, ishard:boolean=false",  .help="Link a file", .retType=0, .flags=0, .info=FN_link },
    { "lstat",      FileLstatCmd,       1,  1, "file:string",  .help="Return status info for file", .retType=(uint)JSI_TT_OBJECT },
    { "mkdir",      FileMkdirCmd,       1,  1, "file:string",  .help="Create a directory" },
    { "mtime",      FileMtimeCmd,       1,  1, "file:string",  .help="Return file modified time", .retType=(uint)JSI_TT_NUMBER },
    { "owned",      FileOwnedCmd,       1,  1, "file:string",  .help="Return true if file is owned by user", .retType=(uint)JSI_TT_BOOLEAN },
    { "pwd",        FilePwdCmd,         0,  0, "",  .help="Return current directory", .retType=(uint)JSI_TT_STRING },
    { "remove",     FileRemoveCmd,      1,  2, "file:string, force:boolean=false",  .help="Delete a file or direcotry" },
    { "rename",     FileRenameCmd,      2,  3, "src:string, dest:string, force:boolean=false",  .help="Rename a file, with possible overwrite" },
    { "read",       FileReadCmd,        1,  2, "file:string, mode:string='rb'",  .help="Read a file", .retType=(uint)JSI_TT_STRING },
    { "readable",   FileReadableCmd,    1,  1, "file:string",  .help="Return true if file is readable", .retType=(uint)JSI_TT_BOOLEAN },
    { "readlink",   FileReadlinkCmd,    1,  1, "file:string",  .help="Read file link destination", .retType=(uint)JSI_TT_STRING },
    { "realpath",   FileRealpathCmd,    1,  1, "file:string",  .help="Return absolute file name minus .., ./ etc", .retType=(uint)JSI_TT_STRING },
    { "rootname",   FileRootnameCmd,    1,  1, "file:string",  .help="Return file name minus extension", .retType=(uint)JSI_TT_STRING },
    { "size",       FileSizeCmd,        1,  1, "file:string",  .help="Return size for file", .retType=(uint)JSI_TT_NUMBER },
    { "stat",       FileStatCmd,        1,  1, "file:string",  .help="Return status info for file", .retType=(uint)JSI_TT_OBJECT },
    { "tail",       FileTailCmd,        1,  1, "file:string",  .help="Return file name minus dirname", .retType=(uint)JSI_TT_STRING },
    { "tempfile",   FileTempfileCmd,    1,  1, "file:string",  .help="Create a temp file", .retType=(uint)JSI_TT_ANY },
    { "truncate",   FileTruncateCmd,    2,  2, "file:string, size:number",  .help="Truncate file" },
    { "type",       FileTypeCmd,        1,  1, "file:string",  .help="Return type of file", .retType=(uint)JSI_TT_STRING },
    { "write",      FileWriteCmd,       2,  3, "file:string, str:string, mode:string='wb+'",  .help="Write a file", .retType=(uint)JSI_TT_NUMBER },
    { "writable",   FileWritableCmd,    1,  1, "file:string",  .help="Return true if file is writable", .retType=(uint)JSI_TT_BOOLEAN },
    { NULL, 0,0,0,0, .help="Commands for accessing the filesystem" }
};

Jsi_RC jsi_InitFileCmds(Jsi_Interp *interp, int release)
{
    if (!release)
        Jsi_CommandCreateSpecs(interp, "File",   fileCmds,   NULL, 0);
    return JSI_OK;
}
#endif
#endif
