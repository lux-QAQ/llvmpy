#ifndef PY_ERROR_H  
#define PY_ERROR_H

#include "runtime_common.h"




#ifdef __cplusplus
extern "C"
{
#endif  // __cplusplus

    void py_runtime_error(const char* errorType, int line);
   

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus



#endif  // PY_ERROR_H