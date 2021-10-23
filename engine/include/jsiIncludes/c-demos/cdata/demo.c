#ifdef BUILD_SHARED_LIB
#include "jsi.h"
#else
#include "jsiOne.c"
#endif
#define JSI_CDATA_IMPL
#include "demo0.h"
#include "demo1.h"
#include "demo2.h"

Jsi_InitProc Jsi_InitDemo;

Jsi_RC Jsi_InitDemo(Jsi_Interp *interp, int release)
{
    if (release)
        return JSI_OK;
    if (jsi_c_init_demo0(interp) && jsi_c_init_demo1(interp)
        && jsi_c_init_demo2(interp))
        return Jsi_PkgProvide(interp, "demo", 1.0, Jsi_InitDemo);
    return JSI_ERROR;
}

//typedef enum { xxA=0x7fffffffffffffffLL, xxB=0x7fffffffffffffffLL, xxC=-1LL  } xx;

#ifndef BUILD_SHARED_LIB
int main(int argc, char *argv[])
{
//    double d = -1;
    //printf("EE: %lld, %lld, %lld, %zd\n", (long long)xxA, (long long)xxB, (long long)xxC, sizeof(xx));
    Jsi_InterpOpts opts = {.argc=argc, .argv=argv, .initProc=Jsi_InitDemo};
    Jsi_Interp *interp = Jsi_InterpNew(&opts);
    if (!interp)
        return 1;
    bar.bbit++;
    foo.val1 = 3.2;
    //Jsi_MapSet(bars, (void*)1, "abc");
    //Jsi_MapSet(bars, (void*)2, "def");
    //Jsi_MapSet(bStr, "Hello", "xyz");
    Jsi_Main(&opts);
    
    // User code here ...
    
    Jsi_InterpDelete(interp);
    return(0);
}

#endif
