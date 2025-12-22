#ifndef cpuH
#define cpuH

#include <string>
#include <vector>

typedef struct { const char *label; int (*execute)( ); } SBenchmarkTask;

struct SCpuContext
{
    std::string platform;
    std::string modelName;
    int coreCount;
    std::vector< SBenchmarkTask* > tasks;

    unsigned long long pi_depth;
    int duration_sec;
    std::string freq_sys_path;
};

extern SCpuContext g_CpuCtx;

extern char g_GlobalErrorBuffer[ 128 ];
extern unsigned long long volatile g_CurrentStep;
extern unsigned long long volatile g_TotalSteps;

#endif
