#ifndef CODEGEN_H
#define CODEGEN_H

// 这个文件至少为了兼容旧的一些代码而存在。实际上，这个文件可有可无。因为它仅仅只是包含了下列这些有文件而已。

// 包含新的头文件结构
#include "CodeGen/PyCodeGen.h"
#include "CodeGen/CodeGenBase.h"
#include "CodeGen/CodeGenExpr.h"
#include "CodeGen/CodeGenStmt.h"
#include "CodeGen/CodeGenModule.h"
#include "CodeGen/CodeGenType.h"
#include "CodeGen/CodeGenRuntime.h"


namespace llvmpy
{
    // 所有类型定义都在上述头文件中，这里不再重复定义

} // namespace llvmpy

#endif // CODEGEN_H