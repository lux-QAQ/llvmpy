#include "TypeOperations.h"
#include "ObjectLifecycle.h"
#include "CodeGen/PyCodeGen.h"  // 添加这一行，确保有完整类型定义
#include "CodeGen/CodeGenRuntime.h"
//#include "codegen.h"
#include "TypeIDs.h"
#include "lexer.h"
#include "ObjectType.h"  // 确保包含 ObjectType.h

#include <iostream>
#include <sstream>
#include <unordered_set>

namespace llvmpy
{

//===----------------------------------------------------------------------===//
// 类型操作注册表实现
//===----------------------------------------------------------------------===//

// 单例模式
TypeOperationRegistry& TypeOperationRegistry::getInstance()
{
    static TypeOperationRegistry instance;
    return instance;
}

TypeOperationRegistry::TypeOperationRegistry()
{
    // 在构造函数中初始化所有内置操作
    initializeBuiltinOperations();
}

void TypeOperationRegistry::initializeBuiltinOperations()
{
    // 注册整数操作
    registerBinaryOp(TOK_PLUS, PY_TYPE_INT, PY_TYPE_INT, PY_TYPE_INT, "py_object_add", true);
    registerBinaryOp(TOK_MINUS, PY_TYPE_INT, PY_TYPE_INT, PY_TYPE_INT, "py_object_subtract", true);
    registerBinaryOp(TOK_MUL, PY_TYPE_INT, PY_TYPE_INT, PY_TYPE_INT, "py_object_multiply", true);
    registerBinaryOp(TOK_DIV, PY_TYPE_INT, PY_TYPE_INT, PY_TYPE_DOUBLE, "py_object_divide", true);  // Integer division results in float in Python 3
    registerBinaryOp(TOK_MOD, PY_TYPE_INT, PY_TYPE_INT, PY_TYPE_INT, "py_object_modulo", true);
    // Add comparison operators for int
    registerBinaryOp(TOK_LT, PY_TYPE_INT, PY_TYPE_INT, PY_TYPE_BOOL, "py_object_compare", true);
    registerBinaryOp(TOK_GT, PY_TYPE_INT, PY_TYPE_INT, PY_TYPE_BOOL, "py_object_compare", true);
    registerBinaryOp(TOK_EQ, PY_TYPE_INT, PY_TYPE_INT, PY_TYPE_BOOL, "py_object_compare", true);
    registerBinaryOp(TOK_NEQ, PY_TYPE_INT, PY_TYPE_INT, PY_TYPE_BOOL, "py_object_compare", true);
    registerBinaryOp(TOK_LE, PY_TYPE_INT, PY_TYPE_INT, PY_TYPE_BOOL, "py_object_compare", true);
    registerBinaryOp(TOK_GE, PY_TYPE_INT, PY_TYPE_INT, PY_TYPE_BOOL, "py_object_compare", true);

    // 注册浮点数操作
    registerBinaryOp(TOK_PLUS, PY_TYPE_DOUBLE, PY_TYPE_DOUBLE, PY_TYPE_DOUBLE, "py_object_add", true);
    registerBinaryOp(TOK_MINUS, PY_TYPE_DOUBLE, PY_TYPE_DOUBLE, PY_TYPE_DOUBLE, "py_object_subtract", true);
    registerBinaryOp(TOK_MUL, PY_TYPE_DOUBLE, PY_TYPE_DOUBLE, PY_TYPE_DOUBLE, "py_object_multiply", true);
    registerBinaryOp(TOK_DIV, PY_TYPE_DOUBLE, PY_TYPE_DOUBLE, PY_TYPE_DOUBLE, "py_object_divide", true);
    // Add comparison operators for double
    registerBinaryOp(TOK_LT, PY_TYPE_DOUBLE, PY_TYPE_DOUBLE, PY_TYPE_BOOL, "py_object_compare", true);
    registerBinaryOp(TOK_GT, PY_TYPE_DOUBLE, PY_TYPE_DOUBLE, PY_TYPE_BOOL, "py_object_compare", true);
    registerBinaryOp(TOK_EQ, PY_TYPE_DOUBLE, PY_TYPE_DOUBLE, PY_TYPE_BOOL, "py_object_compare", true);
    registerBinaryOp(TOK_NEQ, PY_TYPE_DOUBLE, PY_TYPE_DOUBLE, PY_TYPE_BOOL, "py_object_compare", true);
    registerBinaryOp(TOK_LE, PY_TYPE_DOUBLE, PY_TYPE_DOUBLE, PY_TYPE_BOOL, "py_object_compare", true);
    registerBinaryOp(TOK_GE, PY_TYPE_DOUBLE, PY_TYPE_DOUBLE, PY_TYPE_BOOL, "py_object_compare", true);

    // 注册混合数值类型操作 (整数和浮点数)
    registerBinaryOp(TOK_PLUS, PY_TYPE_INT, PY_TYPE_DOUBLE, PY_TYPE_DOUBLE, "py_object_add", true);
    registerBinaryOp(TOK_PLUS, PY_TYPE_DOUBLE, PY_TYPE_INT, PY_TYPE_DOUBLE, "py_object_add", true);
    registerBinaryOp(TOK_MINUS, PY_TYPE_INT, PY_TYPE_DOUBLE, PY_TYPE_DOUBLE, "py_object_subtract", true);
    registerBinaryOp(TOK_MINUS, PY_TYPE_DOUBLE, PY_TYPE_INT, PY_TYPE_DOUBLE, "py_object_subtract", true);
    registerBinaryOp(TOK_MUL, PY_TYPE_INT, PY_TYPE_DOUBLE, PY_TYPE_DOUBLE, "py_object_multiply", true);
    registerBinaryOp(TOK_MUL, PY_TYPE_DOUBLE, PY_TYPE_INT, PY_TYPE_DOUBLE, "py_object_multiply", true);
    registerBinaryOp(TOK_DIV, PY_TYPE_INT, PY_TYPE_DOUBLE, PY_TYPE_DOUBLE, "py_object_divide", true);
    registerBinaryOp(TOK_DIV, PY_TYPE_DOUBLE, PY_TYPE_INT, PY_TYPE_DOUBLE, "py_object_divide", true);
    // Add comparison operators for mixed types
    registerBinaryOp(TOK_LT, PY_TYPE_INT, PY_TYPE_DOUBLE, PY_TYPE_BOOL, "py_object_compare", true);
    registerBinaryOp(TOK_LT, PY_TYPE_DOUBLE, PY_TYPE_INT, PY_TYPE_BOOL, "py_object_compare", true);
    registerBinaryOp(TOK_GT, PY_TYPE_INT, PY_TYPE_DOUBLE, PY_TYPE_BOOL, "py_object_compare", true);
    registerBinaryOp(TOK_GT, PY_TYPE_DOUBLE, PY_TYPE_INT, PY_TYPE_BOOL, "py_object_compare", true);
    registerBinaryOp(TOK_EQ, PY_TYPE_INT, PY_TYPE_DOUBLE, PY_TYPE_BOOL, "py_object_compare", true);
    registerBinaryOp(TOK_EQ, PY_TYPE_DOUBLE, PY_TYPE_INT, PY_TYPE_BOOL, "py_object_compare", true);
    registerBinaryOp(TOK_NEQ, PY_TYPE_INT, PY_TYPE_DOUBLE, PY_TYPE_BOOL, "py_object_compare", true);
    registerBinaryOp(TOK_NEQ, PY_TYPE_DOUBLE, PY_TYPE_INT, PY_TYPE_BOOL, "py_object_compare", true);
    registerBinaryOp(TOK_LE, PY_TYPE_INT, PY_TYPE_DOUBLE, PY_TYPE_BOOL, "py_object_compare", true);
    registerBinaryOp(TOK_LE, PY_TYPE_DOUBLE, PY_TYPE_INT, PY_TYPE_BOOL, "py_object_compare", true);
    registerBinaryOp(TOK_GE, PY_TYPE_INT, PY_TYPE_DOUBLE, PY_TYPE_BOOL, "py_object_compare", true);
    registerBinaryOp(TOK_GE, PY_TYPE_DOUBLE, PY_TYPE_INT, PY_TYPE_BOOL, "py_object_compare", true);

    // 添加：注册幂运算
    registerBinaryOp(TOK_POWER, PY_TYPE_INT, PY_TYPE_INT, PY_TYPE_ANY, "py_object_power", true);  // Result can be int or double
    registerBinaryOp(TOK_POWER, PY_TYPE_DOUBLE, PY_TYPE_DOUBLE, PY_TYPE_DOUBLE, "py_object_power", true);
    registerBinaryOp(TOK_POWER, PY_TYPE_INT, PY_TYPE_DOUBLE, PY_TYPE_DOUBLE, "py_object_power", true);
    registerBinaryOp(TOK_POWER, PY_TYPE_DOUBLE, PY_TYPE_INT, PY_TYPE_DOUBLE, "py_object_power", true);

    // 注册字符串操作
    registerBinaryOp(TOK_PLUS, PY_TYPE_STRING, PY_TYPE_STRING, PY_TYPE_STRING, "py_object_add", true);
    registerBinaryOp(TOK_MUL, PY_TYPE_STRING, PY_TYPE_INT, PY_TYPE_STRING, "py_object_multiply", true);
    registerBinaryOp(TOK_MUL, PY_TYPE_INT, PY_TYPE_STRING, PY_TYPE_STRING, "py_object_multiply", true);
    // Add comparison operators for string
    registerBinaryOp(TOK_LT, PY_TYPE_STRING, PY_TYPE_STRING, PY_TYPE_BOOL, "py_object_compare", true);
    registerBinaryOp(TOK_GT, PY_TYPE_STRING, PY_TYPE_STRING, PY_TYPE_BOOL, "py_object_compare", true);
    registerBinaryOp(TOK_EQ, PY_TYPE_STRING, PY_TYPE_STRING, PY_TYPE_BOOL, "py_object_compare", true);
    registerBinaryOp(TOK_NEQ, PY_TYPE_STRING, PY_TYPE_STRING, PY_TYPE_BOOL, "py_object_compare", true);
    registerBinaryOp(TOK_LE, PY_TYPE_STRING, PY_TYPE_STRING, PY_TYPE_BOOL, "py_object_compare", true);
    registerBinaryOp(TOK_GE, PY_TYPE_STRING, PY_TYPE_STRING, PY_TYPE_BOOL, "py_object_compare", true);

    // 注册列表操作
    registerBinaryOp(TOK_PLUS, PY_TYPE_LIST, PY_TYPE_LIST, PY_TYPE_LIST, "py_object_add", true);
    registerBinaryOp(TOK_MUL, PY_TYPE_LIST, PY_TYPE_INT, PY_TYPE_LIST, "py_object_multiply", true);
    registerBinaryOp(TOK_MUL, PY_TYPE_INT, PY_TYPE_LIST, PY_TYPE_LIST, "py_object_multiply", true);
    // Add comparison operators for list (usually identity or element-wise, handled by runtime)
    registerBinaryOp(TOK_EQ, PY_TYPE_LIST, PY_TYPE_LIST, PY_TYPE_BOOL, "py_object_compare", true);
    registerBinaryOp(TOK_NEQ, PY_TYPE_LIST, PY_TYPE_LIST, PY_TYPE_BOOL, "py_object_compare", true);
    // LT, GT, LE, GE are generally not defined for lists in the same way as numbers/strings

    // 注册类型转换
    registerTypeConversion(PY_TYPE_INT, PY_TYPE_DOUBLE, "py_convert_int_to_double", 1);
    registerTypeConversion(PY_TYPE_DOUBLE, PY_TYPE_INT, "py_convert_double_to_int", 2);
    registerTypeConversion(PY_TYPE_INT, PY_TYPE_BOOL, "py_convert_to_bool", 1);
    registerTypeConversion(PY_TYPE_DOUBLE, PY_TYPE_BOOL, "py_convert_to_bool", 1);
    registerTypeConversion(PY_TYPE_STRING, PY_TYPE_BOOL, "py_convert_to_bool", 2);

    // 注册 any 类型的兼容性
    registerTypeCompatibility(PY_TYPE_ANY, PY_TYPE_INT, true);
    registerTypeCompatibility(PY_TYPE_ANY, PY_TYPE_DOUBLE, true);
    registerTypeCompatibility(PY_TYPE_ANY, PY_TYPE_BOOL, true);
    registerTypeCompatibility(PY_TYPE_ANY, PY_TYPE_STRING, true);
    registerTypeCompatibility(PY_TYPE_ANY, PY_TYPE_LIST, true);
    registerTypeCompatibility(PY_TYPE_ANY, PY_TYPE_DICT, true);

    // 注册 any 类型的转换
    registerTypeConversion(PY_TYPE_ANY, PY_TYPE_INT, "py_convert_any_to_int", 1);
    registerTypeConversion(PY_TYPE_INT, PY_TYPE_ANY, "py_convert_to_any", 1);
    registerTypeConversion(PY_TYPE_ANY, PY_TYPE_DOUBLE, "py_convert_any_to_double", 1);
    registerTypeConversion(PY_TYPE_DOUBLE, PY_TYPE_ANY, "py_convert_to_any", 1);
    registerTypeConversion(PY_TYPE_ANY, PY_TYPE_BOOL, "py_convert_any_to_bool", 1);
    registerTypeConversion(PY_TYPE_BOOL, PY_TYPE_ANY, "py_convert_to_any", 1);
    registerTypeConversion(PY_TYPE_ANY, PY_TYPE_STRING, "py_convert_any_to_string", 1);
    registerTypeConversion(PY_TYPE_STRING, PY_TYPE_ANY, "py_convert_to_any", 1);

    registerTypeConversion(PY_TYPE_INT, PY_TYPE_ANY, "py_convert_int_to_any", 1);
    registerTypeConversion(PY_TYPE_DOUBLE, PY_TYPE_ANY, "py_convert_double_to_any", 1);

    // 注册类型兼容性
    registerTypeCompatibility(PY_TYPE_INT, PY_TYPE_DOUBLE, true);
    registerTypeCompatibility(PY_TYPE_INT, PY_TYPE_BOOL, true);
    registerTypeCompatibility(PY_TYPE_DOUBLE, PY_TYPE_BOOL, true);
    registerTypeCompatibility(PY_TYPE_STRING, PY_TYPE_BOOL, true);

    // 注册类型提升规则 (操作数类型混合时的结果类型)
    registerTypePromotion(PY_TYPE_INT, PY_TYPE_DOUBLE, TOK_PLUS, PY_TYPE_DOUBLE);
    registerTypePromotion(PY_TYPE_INT, PY_TYPE_DOUBLE, TOK_MINUS, PY_TYPE_DOUBLE);
    registerTypePromotion(PY_TYPE_INT, PY_TYPE_DOUBLE, TOK_MUL, PY_TYPE_DOUBLE);
    registerTypePromotion(PY_TYPE_INT, PY_TYPE_DOUBLE, TOK_DIV, PY_TYPE_DOUBLE);
    // Add promotions for comparison operators if needed (result is always bool)
    registerTypePromotion(PY_TYPE_INT, PY_TYPE_DOUBLE, TOK_LT, PY_TYPE_BOOL);
    registerTypePromotion(PY_TYPE_INT, PY_TYPE_DOUBLE, TOK_GT, PY_TYPE_BOOL);
    registerTypePromotion(PY_TYPE_INT, PY_TYPE_DOUBLE, TOK_EQ, PY_TYPE_BOOL);
    registerTypePromotion(PY_TYPE_INT, PY_TYPE_DOUBLE, TOK_NEQ, PY_TYPE_BOOL);
    registerTypePromotion(PY_TYPE_INT, PY_TYPE_DOUBLE, TOK_LE, PY_TYPE_BOOL);
    registerTypePromotion(PY_TYPE_INT, PY_TYPE_DOUBLE, TOK_GE, PY_TYPE_BOOL);

    // 注册容器类型操作
    registerBinaryOp(TOK_PLUS, PY_TYPE_LIST, PY_TYPE_LIST, PY_TYPE_LIST, "py_object_add", true);
    registerBinaryOp(TOK_MUL, PY_TYPE_LIST, PY_TYPE_INT, PY_TYPE_LIST, "py_object_multiply", true);
    registerBinaryOp(TOK_MUL, PY_TYPE_INT, PY_TYPE_LIST, PY_TYPE_LIST, "py_object_multiply", true);

    // 注册列表基础类型操作 - 使其与列表类型兼容
    registerBinaryOp(TOK_PLUS, PY_TYPE_LIST_BASE, PY_TYPE_LIST, PY_TYPE_LIST, "py_object_add", true);
    registerBinaryOp(TOK_PLUS, PY_TYPE_LIST, PY_TYPE_LIST_BASE, PY_TYPE_LIST, "py_object_add", true);
    registerBinaryOp(TOK_PLUS, PY_TYPE_LIST_BASE, PY_TYPE_LIST_BASE, PY_TYPE_LIST_BASE, "py_object_add", true);
    registerBinaryOp(TOK_MUL, PY_TYPE_LIST_BASE, PY_TYPE_INT, PY_TYPE_LIST_BASE, "py_object_multiply", true);
    registerBinaryOp(TOK_MUL, PY_TYPE_INT, PY_TYPE_LIST_BASE, PY_TYPE_LIST_BASE, "py_object_multiply", true);

    // 注册字典基础类型操作
    registerBinaryOp(TOK_PLUS, PY_TYPE_DICT_BASE, PY_TYPE_DICT, PY_TYPE_DICT, "py_object_add", true);            // Note: Dict addition isn't standard Python
    registerBinaryOp(TOK_PLUS, PY_TYPE_DICT, PY_TYPE_DICT_BASE, PY_TYPE_DICT, "py_object_add", true);            // Note: Dict addition isn't standard Python
    registerBinaryOp(TOK_PLUS, PY_TYPE_DICT_BASE, PY_TYPE_DICT_BASE, PY_TYPE_DICT_BASE, "py_object_add", true);  // Note: Dict addition isn't standard Python

    // 注册指针类型操作
    registerTypeCompatibility(PY_TYPE_PTR_INT, PY_TYPE_INT, true);
    registerTypeCompatibility(PY_TYPE_PTR_DOUBLE, PY_TYPE_DOUBLE, true);
    registerTypeCompatibility(PY_TYPE_PTR, PY_TYPE_ANY, true);
    registerTypeCompatibility(PY_TYPE_ANY, PY_TYPE_PTR, true);

    // 注册列表类型兼容性
    registerTypeCompatibility(PY_TYPE_LIST, PY_TYPE_LIST_BASE, true);
    registerTypeCompatibility(PY_TYPE_LIST_BASE, PY_TYPE_LIST, true);

    // 注册字典类型兼容性
    registerTypeCompatibility(PY_TYPE_DICT, PY_TYPE_DICT_BASE, true);
    registerTypeCompatibility(PY_TYPE_DICT_BASE, PY_TYPE_DICT, true);

    // 注册函数类型兼容性
    registerTypeCompatibility(PY_TYPE_FUNC, PY_TYPE_FUNC_BASE, true);
    registerTypeCompatibility(PY_TYPE_FUNC_BASE, PY_TYPE_FUNC, true);

    // 注册指针类型转换
    registerTypeConversion(PY_TYPE_PTR_INT, PY_TYPE_INT, "py_extract_int", 1);
    registerTypeConversion(PY_TYPE_INT, PY_TYPE_PTR_INT, "py_wrap_int", 1);
    registerTypeConversion(PY_TYPE_PTR_DOUBLE, PY_TYPE_DOUBLE, "py_extract_double", 1);
    registerTypeConversion(PY_TYPE_DOUBLE, PY_TYPE_PTR_DOUBLE, "py_wrap_double", 1);

    // 容器类型转换
    registerTypeConversion(PY_TYPE_LIST_BASE, PY_TYPE_LIST, "py_convert_list_base_to_list", 1);
    registerTypeConversion(PY_TYPE_LIST, PY_TYPE_LIST_BASE, "py_convert_list_to_list_base", 1);
    registerTypeConversion(PY_TYPE_DICT_BASE, PY_TYPE_DICT, "py_convert_dict_base_to_dict", 1);
    registerTypeConversion(PY_TYPE_DICT, PY_TYPE_DICT_BASE, "py_convert_dict_to_dict_base", 1);

    // any类型与容器类型的转换
    registerTypeConversion(PY_TYPE_ANY, PY_TYPE_LIST, "py_convert_any_to_list", 2);
    registerTypeConversion(PY_TYPE_LIST, PY_TYPE_ANY, "py_convert_to_any", 1);
    registerTypeConversion(PY_TYPE_ANY, PY_TYPE_DICT, "py_convert_any_to_dict", 2);
    registerTypeConversion(PY_TYPE_DICT, PY_TYPE_ANY, "py_convert_to_any", 1);

    // 针对参数透传增加特殊转换函数
    registerTypeConversion(PY_TYPE_ANY, PY_TYPE_ANY, "py_convert_any_preserve_type", 0);

    // 类型保留函数 - 用于返回值处理
    registerTypeConversion(PY_TYPE_INT, PY_TYPE_ANY, "py_convert_to_any", 1);
    registerTypeConversion(PY_TYPE_DOUBLE, PY_TYPE_ANY, "py_convert_to_any", 1);
    registerTypeConversion(PY_TYPE_BOOL, PY_TYPE_ANY, "py_convert_to_any", 1);
    registerTypeConversion(PY_TYPE_STRING, PY_TYPE_ANY, "py_convert_to_any", 1);
    registerTypeConversion(PY_TYPE_LIST, PY_TYPE_ANY, "py_convert_to_any", 1);
    registerTypeConversion(PY_TYPE_DICT, PY_TYPE_ANY, "py_convert_to_any", 1);

    // 容器基础类型与Any的转换
    registerTypeConversion(PY_TYPE_LIST_BASE, PY_TYPE_ANY, "py_convert_to_any", 1);
    registerTypeConversion(PY_TYPE_DICT_BASE, PY_TYPE_ANY, "py_convert_to_any", 1);
    registerTypeConversion(PY_TYPE_ANY, PY_TYPE_LIST_BASE, "py_convert_any_to_list_base", 2);
    registerTypeConversion(PY_TYPE_ANY, PY_TYPE_DICT_BASE, "py_convert_any_to_dict_base", 2);

    // 注册索引操作
    // 1. 列表索引操作
    registerTypeCompatibility(PY_TYPE_LIST, PY_TYPE_INT, true);       // 列表可以用整数索引
    registerTypeCompatibility(PY_TYPE_LIST_BASE, PY_TYPE_INT, true);  // 通用列表类型也可以用整数索引

    // 2. 字典索引操作 - 字典可以用各种类型作为键
    registerTypeCompatibility(PY_TYPE_DICT, PY_TYPE_INT, true);
    registerTypeCompatibility(PY_TYPE_DICT, PY_TYPE_STRING, true);
    registerTypeCompatibility(PY_TYPE_DICT, PY_TYPE_DOUBLE, true);  // Note: Using float as dict key is generally discouraged
    registerTypeCompatibility(PY_TYPE_DICT, PY_TYPE_BOOL, true);
    registerTypeCompatibility(PY_TYPE_DICT_BASE, PY_TYPE_ANY, true);

    // 3. 字符串索引操作
    registerTypeCompatibility(PY_TYPE_STRING, PY_TYPE_INT, true);  // 字符串可以用整数索引

    // 4. ANY类型索引操作 - ANY可以用任何类型索引，会在运行时处理
    registerTypeCompatibility(PY_TYPE_ANY, PY_TYPE_ANY, true);
    registerTypeCompatibility(PY_TYPE_ANY, PY_TYPE_INT, true);
    registerTypeCompatibility(PY_TYPE_ANY, PY_TYPE_STRING, true);
    registerTypeCompatibility(PY_TYPE_ANY, PY_TYPE_DOUBLE, true);
    registerTypeCompatibility(PY_TYPE_ANY, PY_TYPE_BOOL, true);

    // 5. 索引操作的类型转换 - 支持将各种类型转换为索引
    registerTypeConversion(PY_TYPE_ANY, PY_TYPE_INT, "py_extract_int_from_any", 1);
    registerTypeConversion(PY_TYPE_STRING, PY_TYPE_INT, "py_extract_int_from_any", 2);
    registerTypeConversion(PY_TYPE_DOUBLE, PY_TYPE_INT, "py_extract_int_from_any", 2);
    registerTypeConversion(PY_TYPE_BOOL, PY_TYPE_INT, "py_extract_int_from_any", 1);

    // 6. 列表元素类型兼容性
    for (int i = 1; i <= 10; i++)
    {
        // 允许任何类型作为列表元素
        registerTypeCompatibility(PY_TYPE_LIST, i, true);
        registerTypeCompatibility(PY_TYPE_LIST_BASE, i, true);
    }

    // 7. 索引操作特殊函数
    registerTypeConversion(PY_TYPE_INT, PY_TYPE_INT, "py_ensure_valid_index", 1);

    // 8.特殊注册 ANY 类型与所有基本类型的二元操作兼容性
    // 对每种基本类型
    std::vector<int> basicTypes = {
            PY_TYPE_INT, PY_TYPE_DOUBLE, PY_TYPE_BOOL,
            PY_TYPE_STRING, PY_TYPE_LIST, PY_TYPE_DICT};

    // 对每种操作符
    // 对每种操作符
    std::vector<PyTokenType> operators = {
            TOK_PLUS, TOK_MINUS, TOK_MUL, TOK_DIV, TOK_MOD,
            TOK_LT, TOK_GT, TOK_EQ, TOK_NEQ, TOK_LE, TOK_GE,
            TOK_POWER  // --- 添加 TOK_POW ---
                       // Add other relevant binary operators like TOK_AMPERSAND, TOK_VBAR, TOK_CIRCUMFLEX if needed
    };

    for (int typeId : basicTypes)
    {
        // 注册类型兼容性 (Already done above, but doesn't hurt to ensure)
        registerTypeCompatibility(PY_TYPE_ANY, typeId, true);

        for (PyTokenType op : operators)
        {
            bool isComparison = (op == TOK_LT || op == TOK_GT || op == TOK_EQ || op == TOK_NEQ || op == TOK_LE || op == TOK_GE);
            int resultType = isComparison ? PY_TYPE_BOOL : PY_TYPE_ANY;
            // Use a generic runtime function name; specific logic is in handleAnyTypeOperation
            std::string runtimeFuncName = isComparison ? "py_object_compare" : "py_object_binary_op";

            // ANY 作为左操作数
            registerBinaryOp(
                    op,
                    PY_TYPE_ANY,
                    typeId,
                    resultType,
                    runtimeFuncName);

            // ANY 作为右操作数
            registerBinaryOp(
                    op,
                    typeId,
                    PY_TYPE_ANY,
                    resultType,
                    runtimeFuncName);
        }
    }

    // ANY 与 ANY 的操作
    for (PyTokenType op : operators)
    {
        bool isComparison = (op == TOK_LT || op == TOK_GT || op == TOK_EQ || op == TOK_NEQ || op == TOK_LE || op == TOK_GE);
        int resultType = isComparison ? PY_TYPE_BOOL : PY_TYPE_ANY;
        std::string runtimeFuncName = isComparison ? "py_object_compare" : "py_object_binary_op";

        registerBinaryOp(
                op,
                PY_TYPE_ANY,
                PY_TYPE_ANY,
                resultType,
                runtimeFuncName);
    }

    // Register Unary Operations (Example)
    registerUnaryOp(TOK_MINUS, PY_TYPE_INT, PY_TYPE_INT, "py_object_negate", true);

    registerUnaryOp(TOK_MINUS, PY_TYPE_DOUBLE, PY_TYPE_DOUBLE, "py_object_negate", true);

    registerUnaryOp(TOK_NOT, PY_TYPE_BOOL, PY_TYPE_BOOL, "py_object_not", true);      // Assuming TOK_NOT for '!' or 'not'
    registerUnaryOp(TOK_MINUS, PY_TYPE_BOOL, PY_TYPE_INT, "py_object_negate", true);  // Negating bool results in int (-1 or 0)
    registerUnaryOp(TOK_MINUS, PY_TYPE_ANY, PY_TYPE_ANY, "py_object_negate", true);

    registerUnaryOp(TOK_NOT, PY_TYPE_INT, PY_TYPE_BOOL, "py_object_not", true);
    registerUnaryOp(TOK_NOT, PY_TYPE_DOUBLE, PY_TYPE_BOOL, "py_object_not", true);
    registerUnaryOp(TOK_NOT, PY_TYPE_STRING, PY_TYPE_BOOL, "py_object_not", true);
    registerUnaryOp(TOK_NOT, PY_TYPE_LIST, PY_TYPE_BOOL, "py_object_not", true);
    registerUnaryOp(TOK_NOT, PY_TYPE_DICT, PY_TYPE_BOOL, "py_object_not", true);
    registerUnaryOp(TOK_NOT, PY_TYPE_ANY, PY_TYPE_BOOL, "py_object_not", true);
    // Add TOK_TILDE if needed for bitwise invert
}

void TypeOperationRegistry::registerBinaryOp(
        PyTokenType op,
        int leftTypeId,
        int rightTypeId,
        int resultTypeId,
        const std::string& runtimeFunc,
        bool needsWrap,
        std::function<llvm::Value*(PyCodeGen&, llvm::Value*, llvm::Value*)> customImpl)
{
    BinaryOpDescriptor descriptor;
    descriptor.leftTypeId = leftTypeId;
    descriptor.rightTypeId = rightTypeId;
    descriptor.resultTypeId = resultTypeId;
    descriptor.runtimeFunction = runtimeFunc;
    descriptor.needsWrap = needsWrap;
    descriptor.customImpl = customImpl;

    auto key = std::make_pair(leftTypeId, rightTypeId);
    binaryOps[op][key] = descriptor;
}

void TypeOperationRegistry::registerUnaryOp(
        PyTokenType op,
        int operandTypeId,
        int resultTypeId,
        const std::string& runtimeFunc,
        bool needsWrap,
        std::function<llvm::Value*(PyCodeGen&, llvm::Value*)> customImpl)
{
    UnaryOpDescriptor descriptor;
    descriptor.operandTypeId = operandTypeId;
    descriptor.resultTypeId = resultTypeId;
    descriptor.runtimeFunction = runtimeFunc;
    descriptor.needsWrap = needsWrap;
    descriptor.customImpl = customImpl;

    unaryOps[op][operandTypeId] = descriptor;
}

void TypeOperationRegistry::registerTypeConversion(
        int sourceTypeId,
        int targetTypeId,
        const std::string& runtimeFunc,
        int conversionCost,
        std::function<llvm::Value*(PyCodeGen&, llvm::Value*)> customImpl)
{
    TypeConversionDescriptor descriptor;
    descriptor.sourceTypeId = sourceTypeId;
    descriptor.targetTypeId = targetTypeId;
    descriptor.runtimeFunction = runtimeFunc;
    descriptor.conversionCost = conversionCost;
    descriptor.customImpl = customImpl;

    typeConversions[sourceTypeId][targetTypeId] = descriptor;
}

void TypeOperationRegistry::registerTypeCompatibility(int typeIdA, int typeIdB, bool compatible)
{
    auto key = std::make_pair(typeIdA, typeIdB);
    typeCompatibility[key] = compatible;

    // 兼容性是对称的
    auto reverseKey = std::make_pair(typeIdB, typeIdA);
    typeCompatibility[reverseKey] = compatible;
}

void TypeOperationRegistry::registerTypePromotion(int typeIdA, int typeIdB, PyTokenType op, int resultTypeId)
{
    size_t hashValue = (static_cast<size_t>(typeIdA) << 16) | (static_cast<size_t>(typeIdB) << 8) | static_cast<size_t>(op);

    // 使用 std::make_tuple 创建元组并添加到 map 中，不再需要手动计算哈希值
    typePromotions[std::make_tuple(typeIdA, typeIdB, op)] = resultTypeId;

    // 操作数顺序通常不重要，所以也注册反向的组合
    size_t reverseHashValue = (static_cast<size_t>(typeIdB) << 16) | (static_cast<size_t>(typeIdA) << 8) | static_cast<size_t>(op);

    // 操作数顺序通常不重要，所以也注册反向的组合
    typePromotions[std::make_tuple(typeIdB, typeIdA, op)] = resultTypeId;
}

BinaryOpDescriptor* TypeOperationRegistry::getBinaryOpDescriptor(PyTokenType op, int leftTypeId, int rightTypeId)
{
    auto opIt = binaryOps.find(op);
    if (opIt != binaryOps.end())
    {
        auto key = std::make_pair(leftTypeId, rightTypeId);
        auto& descriptors = opIt->second;
        auto descIt = descriptors.find(key);

        if (descIt != descriptors.end())
        {
            return &descIt->second;
        }
    }

    return nullptr;
}

UnaryOpDescriptor* TypeOperationRegistry::getUnaryOpDescriptor(PyTokenType op, int operandTypeId)
{
    auto opIt = unaryOps.find(op);
    if (opIt != unaryOps.end())
    {
        auto& descriptors = opIt->second;
        auto descIt = descriptors.find(operandTypeId);

        if (descIt != descriptors.end())
        {
            return &descIt->second;
        }
    }

    return nullptr;
}

TypeConversionDescriptor* TypeOperationRegistry::getTypeConversionDescriptor(int sourceTypeId, int targetTypeId)
{
    auto sourceIt = typeConversions.find(sourceTypeId);
    if (sourceIt != typeConversions.end())
    {
        auto& targetMap = sourceIt->second;
        auto targetIt = targetMap.find(targetTypeId);

        if (targetIt != targetMap.end())
        {
            return &targetIt->second;
        }
    }

    return nullptr;
}

bool TypeOperationRegistry::areTypesCompatible(int typeIdA, int typeIdB)
{
    // 相同类型总是兼容的
    if (typeIdA == typeIdB)
    {
        return true;
    }

    // 检查注册的兼容性
    auto key = std::make_pair(typeIdA, typeIdB);
    auto it = typeCompatibility.find(key);

    if (it != typeCompatibility.end())
    {
        return it->second;
    }

    // 默认不兼容
    return false;
}

int TypeOperationRegistry::getResultTypeId(int leftTypeId, int rightTypeId, PyTokenType op)
{
    // 如果类型相同，通常结果也是该类型 (除非特殊操作)
    if (leftTypeId == rightTypeId)
    {
        // 检查是否有注册的操作规则
        BinaryOpDescriptor* desc = getBinaryOpDescriptor(op, leftTypeId, rightTypeId);
        if (desc)
        {
            return desc->resultTypeId;
        }

        // 否则默认返回相同类型
        return leftTypeId;
    }

    // 检查注册的类型提升规则
    auto result = typePromotions.find(std::make_tuple(leftTypeId, rightTypeId, op));
    if (result != typePromotions.end())
    {
        return result->second;
    }

    // 否则返回比较高级的类型 (简单启发式规则)
    return std::max(leftTypeId, rightTypeId);
}

TypeConversionDescriptor* TypeOperationRegistry::findBestConversion(int fromTypeId, int toTypeId)
{
    // 如果类型相同，不需要转换
    if (fromTypeId == toTypeId)
    {
        return nullptr;
    }

    // 尝试直接转换
    TypeConversionDescriptor* directConv = getTypeConversionDescriptor(fromTypeId, toTypeId);
    if (directConv)
    {
        return directConv;
    }

    // 寻找中间转换路径 (简单版本，只考虑一步中间转换)
    std::unordered_set<int> intermediateTypes = {
            PY_TYPE_INT, PY_TYPE_DOUBLE, PY_TYPE_BOOL, PY_TYPE_STRING};

    TypeConversionDescriptor* bestConv = nullptr;
    int bestCost = INT_MAX;

    for (int midType : intermediateTypes)
    {
        if (midType == fromTypeId || midType == toTypeId)
        {
            continue;
        }

        TypeConversionDescriptor* firstStep = getTypeConversionDescriptor(fromTypeId, midType);
        TypeConversionDescriptor* secondStep = getTypeConversionDescriptor(midType, toTypeId);

        if (firstStep && secondStep)
        {
            int totalCost = firstStep->conversionCost + secondStep->conversionCost;
            if (totalCost < bestCost)
            {
                bestCost = totalCost;
                bestConv = firstStep;  // 返回第一步转换
            }
        }
    }

    return bestConv;
}

std::pair<int, int> TypeOperationRegistry::findOperablePath(PyTokenType op, int leftTypeId, int rightTypeId)
{
    // 检查是否已有直接的操作路径
    if (getBinaryOpDescriptor(op, leftTypeId, rightTypeId))
    {
        return {leftTypeId, rightTypeId};
    }

    // 尝试将左操作数转换为右操作数类型
    if (areTypesCompatible(leftTypeId, rightTypeId))
    {
        if (getBinaryOpDescriptor(op, rightTypeId, rightTypeId))
        {
            return {rightTypeId, rightTypeId};
        }
    }

    // 尝试将右操作数转换为左操作数类型
    if (areTypesCompatible(rightTypeId, leftTypeId))
    {
        if (getBinaryOpDescriptor(op, leftTypeId, leftTypeId))
        {
            return {leftTypeId, leftTypeId};
        }
    }

    // 尝试将两个操作数都转换为一个公共类型
    std::vector<int> commonTypes = {PY_TYPE_DOUBLE, PY_TYPE_INT, PY_TYPE_STRING, PY_TYPE_BOOL};

    for (int commonType : commonTypes)
    {
        if (areTypesCompatible(leftTypeId, commonType) && areTypesCompatible(rightTypeId, commonType))
        {
            if (getBinaryOpDescriptor(op, commonType, commonType))
            {
                return {commonType, commonType};
            }
        }
    }

    // 没有找到可行路径，返回原始类型对
    return {leftTypeId, rightTypeId};
}

//===----------------------------------------------------------------------===//
// 操作代码生成器实现
//===----------------------------------------------------------------------===//
// filepath: [TypeOperations.cpp](http://_vscodecontentref_/1)

// 添加 handleAnyTypeOperation 的实现:

llvm::Value* OperationCodeGenerator::handleAnyTypeOperation(
        CodeGenBase& gen,
        PyTokenType op,  // Already PyTokenType
        llvm::Value* left,
        llvm::Value* right,
        bool anyTypeIsLeft,
        int otherTypeId)
{
    auto& builder = gen.getBuilder();
    llvm::LLVMContext& context = gen.getContext();

    // 对于比较操作，使用 py_object_compare 函数
    // Use PyTokenType constants for comparison
    if (op == TOK_LT || op == TOK_GT || op == TOK_EQ || op == TOK_NEQ || op == TOK_LE || op == TOK_GE)
    {
        // 确定比较运算符类型
        int compareOp;
        switch (op)
        {
            case TOK_LT:
                compareOp = 2;
                break;  // PY_CMP_LT
            case TOK_GT:
                compareOp = 4;
                break;  // PY_CMP_GT
            case TOK_EQ:
                compareOp = 0;
                break;  // PY_CMP_EQ
            case TOK_NEQ:
                compareOp = 1;
                break;  // PY_CMP_NE
            case TOK_LE:
                compareOp = 3;
                break;  // PY_CMP_LE (Assuming 3, adjust if needed)
            case TOK_GE:
                compareOp = 5;
                break;  // PY_CMP_GE (Assuming 5, adjust if needed)
            default:
                compareOp = 0;  // Should not happen if check above is correct
        }

        // ... (rest of the comparison logic remains the same) ...
        // 获取比较函数
        llvm::Function* compareFunc = gen.getOrCreateExternalFunction(
                "py_object_compare",
                llvm::PointerType::get(context, 0),
                {llvm::PointerType::get(context, 0),
                 llvm::PointerType::get(context, 0),
                 llvm::Type::getInt32Ty(context)},
                false);

        // 确保两个操作数都是指针类型（Python对象）
        if (!left->getType()->isPointerTy())
        {
            PyCodeGen* pyCodeGen = gen.asPyCodeGen();
            if (pyCodeGen)
            {
                left = createObject(*pyCodeGen, left, anyTypeIsLeft ? PY_TYPE_ANY : otherTypeId);
            }
        }

        if (!right->getType()->isPointerTy())
        {
            PyCodeGen* pyCodeGen = gen.asPyCodeGen();
            if (pyCodeGen)
            {
                right = createObject(*pyCodeGen, right, anyTypeIsLeft ? otherTypeId : PY_TYPE_ANY);
            }
        }

        // 创建常量比较运算符
        llvm::Value* opValue = llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), compareOp);

        // 调用比较函数
        llvm::Value* result = builder.CreateCall(compareFunc, {left, right, opValue}, "any_cmp_result");

        // 为比较结果添加类型元数据（布尔类型）
        gen.getRuntimeGen()->attachTypeMetadata(result, PY_TYPE_BOOL);

        return result;
    }

    // 对于其他二元操作，使用相应的运行时函数
    std::string opName;
    switch (op)  // Use PyTokenType constants
    {
        case TOK_PLUS:
            opName = "add";
            break;
        case TOK_MINUS:
            opName = "subtract";
            break;
        case TOK_MUL:
            opName = "multiply";
            break;
        case TOK_DIV:
            opName = "divide";
            break;
        case TOK_MOD:
            opName = "modulo";
            break;
        // Add other binary operators as needed (e.g., TOK_POWERER, TOK_FLOOR_DIV)
        default:
            // Handle unsupported or unexpected operators for ANY type
            std::cerr << "Unsupported binary operator " << op << " for ANY type in handleAnyTypeOperation" << std::endl;
            // Consider returning a specific error object or nullptr
            return nullptr;  // Or generate code to raise TypeError at runtime
    }

    // ... (rest of the binary operation logic remains the same) ...
    std::string funcName = "py_object_" + opName;
    llvm::Function* opFunc = gen.getOrCreateExternalFunction(
            funcName,
            llvm::PointerType::get(context, 0),
            {llvm::PointerType::get(context, 0),
             llvm::PointerType::get(context, 0)},
            false);

    // 确保两个操作数都是指针类型
    if (!left->getType()->isPointerTy())
    {
        PyCodeGen* pyCodeGen = gen.asPyCodeGen();
        if (pyCodeGen)
        {
            left = createObject(*pyCodeGen, left, anyTypeIsLeft ? PY_TYPE_ANY : otherTypeId);
        }
    }

    if (!right->getType()->isPointerTy())
    {
        PyCodeGen* pyCodeGen = gen.asPyCodeGen();
        if (pyCodeGen)
        {
            right = createObject(*pyCodeGen, right, anyTypeIsLeft ? otherTypeId : PY_TYPE_ANY);
        }
    }

    // 调用运算函数
    llvm::Value* result = builder.CreateCall(opFunc, {left, right}, "any_op_result");

    // ANY 类型的操作结果默认也是 ANY 类型
    gen.getRuntimeGen()->attachTypeMetadata(result, PY_TYPE_ANY);

    return result;
}

llvm::Value* OperationCodeGenerator::handleBinaryOp(
        CodeGenBase& gen,
        PyTokenType op,
        llvm::Value* left,
        llvm::Value* right,
        int leftTypeId,
        int rightTypeId)
{
    auto& builder = gen.getBuilder();
    llvm::LLVMContext& context = gen.getContext();
    auto& registry = TypeOperationRegistry::getInstance();

    // ANY 类型特殊处理
    if (leftTypeId == PY_TYPE_ANY || rightTypeId == PY_TYPE_ANY)
    {
        // 确定哪一个是ANY类型
        bool anyTypeIsLeft = (leftTypeId == PY_TYPE_ANY);
        int otherTypeId = anyTypeIsLeft ? rightTypeId : leftTypeId;

        // 使用专门的ANY类型处理函数
        return handleAnyTypeOperation(gen, op, left, right, anyTypeIsLeft, otherTypeId);
    }

    // 获取操作描述符
    BinaryOpDescriptor* descriptor = registry.getBinaryOpDescriptor(op, leftTypeId, rightTypeId);

    if (!descriptor)
    {
        std::cerr << "No binary operation " << op << " defined for types "
                  << leftTypeId << " and " << rightTypeId << std::endl;
        return nullptr;
    }

    // 如果有自定义实现，使用它
    if (descriptor->customImpl)
    {
        PyCodeGen* pyCodeGen = gen.asPyCodeGen();
        if (pyCodeGen)
        {
            return descriptor->customImpl(*pyCodeGen, left, right);
        }
    }

    // 直接使用运行时函数，不尝试提取整数
    llvm::Function* func = gen.getOrCreateExternalFunction(
            descriptor->runtimeFunction,
            llvm::PointerType::get(context, 0),
            {llvm::PointerType::get(context, 0),
             llvm::PointerType::get(context, 0)},
            false);

    // 确保操作数是指针类型
    if (!left->getType()->isPointerTy())
    {
        PyCodeGen* pyCodeGen = gen.asPyCodeGen();
        if (pyCodeGen)
        {
            left = createObject(*pyCodeGen, left, leftTypeId);
        }
    }

    if (!right->getType()->isPointerTy())
    {
        PyCodeGen* pyCodeGen = gen.asPyCodeGen();
        if (pyCodeGen)
        {
            right = createObject(*pyCodeGen, right, rightTypeId);
        }
    }

    // 调用运行时函数
    llvm::Value* result = builder.CreateCall(func, {left, right}, "binop_result");

    // 附加类型元数据
    int resultTypeId = descriptor->resultTypeId;
    gen.getRuntimeGen()->attachTypeMetadata(result, resultTypeId);

    return result;
}

llvm::Value* OperationCodeGenerator::handleUnaryOp(
        CodeGenBase& gen,
        PyTokenType op,
        llvm::Value* operand,
        int operandTypeId)
{
    auto& builder = gen.getBuilder();
    llvm::LLVMContext& context = gen.getContext();
    auto& registry = TypeOperationRegistry::getInstance();

    // 获取操作描述符
    UnaryOpDescriptor* descriptor = registry.getUnaryOpDescriptor(op, operandTypeId);

    if (!descriptor)
    {
        std::cerr << "No unary operation " << op << " defined for type " << operandTypeId << std::endl;
        return nullptr;
    }

    // 如果有自定义实现，使用它
    if (descriptor->customImpl)
    {
        PyCodeGen* pyCodeGen = gen.asPyCodeGen();
        return descriptor->customImpl(*pyCodeGen, operand);
    }

    // 否则使用运行时函数
    // 注意：这里修正了函数声明，确保只接受一个参数
    llvm::Function* func = gen.getOrCreateExternalFunction(
            descriptor->runtimeFunction,
            llvm::PointerType::get(context, 0),
            {llvm::PointerType::get(context, 0)},  // 只有一个参数
            false);

    // 如果操作数不是指针类型，需要包装为对象
    if (!operand->getType()->isPointerTy())
    {
        PyCodeGen* pyCodeGen = gen.asPyCodeGen();
        operand = createObject(*pyCodeGen, operand, operandTypeId);
    }

    // 调用运行时函数
    return builder.CreateCall(func, {operand}, "unaryop_result");
}

// prepareIndexValue处理索引值，确保其为整数？
llvm::Value* OperationCodeGenerator::prepareIndexValue(
        CodeGenBase& gen,
        llvm::Value* index,
        int indexTypeId)
{
    auto& builder = gen.getBuilder();

    // 如果索引已经是整数类型，直接返回
    if (indexTypeId == PY_TYPE_INT)
    {
        // 从Python整数对象提取C整数
        llvm::Function* extractIntFunc = gen.getOrCreateExternalFunction(
                "py_extract_int",
                llvm::Type::getInt32Ty(gen.getContext()),
                {llvm::PointerType::get(gen.getContext(), 0)});

        return builder.CreateCall(extractIntFunc, {index}, "idx");
    }

    // 如果索引是布尔类型，先转换为整数
    if (indexTypeId == PY_TYPE_BOOL)
    {
        // 从Python布尔对象提取C布尔值
        llvm::Function* extractBoolFunc = gen.getOrCreateExternalFunction(
                "py_extract_bool",
                llvm::Type::getInt1Ty(gen.getContext()),
                {llvm::PointerType::get(gen.getContext(), 0)});

        llvm::Value* boolVal = builder.CreateCall(extractBoolFunc, {index}, "bool_val");

        // 将布尔值转换为整数
        llvm::Value* intVal = builder.CreateZExt(boolVal, llvm::Type::getInt32Ty(gen.getContext()), "int_from_bool");

        return intVal;
    }

    // 对于其他类型，尝试从任意对象提取整数
    llvm::Function* extractIntFromAnyFunc = gen.getOrCreateExternalFunction(
            "py_extract_int_from_any",
            llvm::PointerType::get(gen.getContext(), 0),
            {llvm::PointerType::get(gen.getContext(), 0)});

    llvm::Value* intObj = builder.CreateCall(extractIntFromAnyFunc, {index}, "int_obj");

    // 从整数对象提取C整数
    llvm::Function* extractIntFunc = gen.getOrCreateExternalFunction(
            "py_extract_int",
            llvm::Type::getInt32Ty(gen.getContext()),
            {llvm::PointerType::get(gen.getContext(), 0)});

    llvm::Value* intVal = builder.CreateCall(extractIntFunc, {intObj}, "idx");

    // 释放临时对象
    llvm::Function* decRefFunc = gen.getOrCreateExternalFunction(
            "py_decref",
            llvm::Type::getVoidTy(gen.getContext()),
            {llvm::PointerType::get(gen.getContext(), 0)});

    builder.CreateCall(decRefFunc, {intObj});

    return intVal;
}

llvm::Value* OperationCodeGenerator::handleTypeConversion(
        CodeGenBase& gen,
        llvm::Value* value,
        int fromTypeId,
        int toTypeId)
{
    // 如果类型相同，不需要转换
    if (fromTypeId == toTypeId)
    {
        return value;
    }

    auto& builder = gen.getBuilder();
    llvm::LLVMContext& context = gen.getContext();
    auto& registry = TypeOperationRegistry::getInstance();

    // 获取类型转换描述符
    TypeConversionDescriptor* descriptor = registry.getTypeConversionDescriptor(fromTypeId, toTypeId);

    if (!descriptor)
    {
        std::cerr << "No conversion defined from type " << fromTypeId << " to " << toTypeId << std::endl;
        return value;  // 返回原值，可能导致后续错误
    }

    // 如果有自定义实现，使用它
    if (descriptor->customImpl)
    {
        PyCodeGen* pyCodeGen = gen.asPyCodeGen();
        return descriptor->customImpl(*pyCodeGen, value);
    }

    // 对于基本数值类型之间的转换，使用LLVM指令
    if (fromTypeId == PY_TYPE_INT && toTypeId == PY_TYPE_DOUBLE)
    {
        if (value->getType()->isIntegerTy())
        {
            return builder.CreateSIToFP(value, llvm::Type::getDoubleTy(context), "int_to_double");
        }
    }
    else if (fromTypeId == PY_TYPE_DOUBLE && toTypeId == PY_TYPE_INT)
    {
        if (value->getType()->isDoubleTy())
        {
            return builder.CreateFPToSI(value, llvm::Type::getInt32Ty(context), "double_to_int");
        }
    }

    // 否则使用运行时函数
    // 注意：这里修正了函数声明，确保只接受一个参数
    llvm::Function* func = gen.getOrCreateExternalFunction(
            descriptor->runtimeFunction,
            llvm::PointerType::get(context, 0),
            {llvm::PointerType::get(context, 0)},  // 只有一个参数
            false);

    // 如果值不是指针类型，需要包装为对象
    if (!value->getType()->isPointerTy())
    {
        PyCodeGen* pyCodeGen = gen.asPyCodeGen();
        value = createObject(*pyCodeGen, value, fromTypeId);
    }

    // 调用运行时函数
    return builder.CreateCall(func, {value}, "conversion_result");
}

int OperationCodeGenerator::getTypeId(ObjectType* type)
{
    if (!type) return PY_TYPE_NONE;

    return type->getTypeId();
}

llvm::Value* OperationCodeGenerator::createObject(PyCodeGen& gen, llvm::Value* value, int typeId)
{
    auto& builder = gen.getBuilder();
    llvm::LLVMContext& context = gen.getContext();

    // 根据类型ID创建不同的对象
    llvm::FunctionType* funcType = nullptr;
    llvm::Function* createFunc = nullptr;
    std::string funcName;

    switch (typeId)
    {
        case PY_TYPE_INT:
            funcName = "py_create_int";
            funcType = llvm::FunctionType::get(
                    llvm::PointerType::get(context, 0),
                    {llvm::Type::getInt32Ty(context)},
                    false);
            break;

        case PY_TYPE_DOUBLE:
            funcName = "py_create_double";
            funcType = llvm::FunctionType::get(
                    llvm::PointerType::get(context, 0),
                    {llvm::Type::getDoubleTy(context)},
                    false);
            break;

        case PY_TYPE_BOOL:
            funcName = "py_create_bool";
            funcType = llvm::FunctionType::get(
                    llvm::PointerType::get(context, 0),
                    {llvm::Type::getInt1Ty(context)},
                    false);
            break;

        case PY_TYPE_STRING:
            funcName = "py_create_string";
            funcType = llvm::FunctionType::get(
                    llvm::PointerType::get(context, 0),
                    {llvm::PointerType::get(context, 0)},
                    false);
            break;

        default:
            std::cerr << "Cannot create object of type " << typeId << std::endl;
            return value;
    }

    createFunc = gen.getOrCreateExternalFunction(
            funcName,
            llvm::PointerType::get(context, 0),
            // 根据不同类型使用适当的参数类型
            [&]() -> std::vector<llvm::Type*>
            {
                switch (typeId)
                {
                    case PY_TYPE_INT:
                        return {llvm::Type::getInt32Ty(context)};
                    case PY_TYPE_DOUBLE:
                        return {llvm::Type::getDoubleTy(context)};
                    case PY_TYPE_BOOL:
                        return {llvm::Type::getInt1Ty(context)};
                    case PY_TYPE_STRING:
                        return {llvm::PointerType::get(context, 0)};
                    default:
                        return {llvm::Type::getInt32Ty(context)};
                }
            }(),
            false);

    // 类型转换，如果需要的话
    if (typeId == PY_TYPE_INT && !value->getType()->isIntegerTy(32))
    {
        if (value->getType()->isIntegerTy())
        {
            value = builder.CreateIntCast(value, llvm::Type::getInt32Ty(context), true, "intcast");
        }
        else if (value->getType()->isDoubleTy())
        {
            value = builder.CreateFPToSI(value, llvm::Type::getInt32Ty(context), "double_to_int");
        }
    }
    else if (typeId == PY_TYPE_DOUBLE && !value->getType()->isDoubleTy())
    {
        if (value->getType()->isIntegerTy())
        {
            value = builder.CreateSIToFP(value, llvm::Type::getDoubleTy(context), "int_to_double");
        }
    }
    else if (typeId == PY_TYPE_BOOL && !value->getType()->isIntegerTy(1))
    {
        value = builder.CreateICmpNE(value, llvm::ConstantInt::get(value->getType(), 0), "to_bool");
    }

    // 调用创建函数
    return builder.CreateCall(createFunc, {value}, "obj_create");
}

//===----------------------------------------------------------------------===//
// 操作结果处理器实现
//===----------------------------------------------------------------------===//

llvm::Value* OperationResultHandler::adjustResult(
        CodeGenBase& gen,
        llvm::Value* result,
        int resultTypeId,
        int expectedTypeId)
{
    // 如果类型已经匹配，不需要调整
    if (resultTypeId == expectedTypeId)
    {
        return result;
    }

    auto& builder = gen.getBuilder();
    auto& registry = TypeOperationRegistry::getInstance();

    // 检查是否需要类型转换
    TypeConversionDescriptor* convDesc = registry.getTypeConversionDescriptor(resultTypeId, expectedTypeId);
    if (convDesc)
    {
        result = OperationCodeGenerator::handleTypeConversion(gen, result, resultTypeId, expectedTypeId);
        return result;
    }

    // 如果没有直接的转换，可能需要拆箱或装箱
    if (result->getType()->isPointerTy() && expectedTypeId == PY_TYPE_INT)
    {
        // 尝试从对象中提取整数值
        llvm::FunctionType* extractFuncType = llvm::FunctionType::get(
                llvm::Type::getInt32Ty(gen.getContext()),
                {llvm::PointerType::get(gen.getContext(), 0)},
                false);

        llvm::Function* extractFunc = gen.getOrCreateExternalFunction(
                "py_extract_int",
                llvm::Type::getInt32Ty(gen.getContext()),
                {llvm::PointerType::get(gen.getContext(), 0)},
                false);

        return builder.CreateCall(extractFunc, {result}, "extract_int");
    }
    else if (!result->getType()->isPointerTy() && (expectedTypeId == PY_TYPE_NONE || expectedTypeId >= PY_TYPE_LIST_BASE))
    {
        // 将基本类型值包装为对象
        PyCodeGen* pyCodeGen = gen.asPyCodeGen();
        return OperationCodeGenerator::createObject(*pyCodeGen, result, resultTypeId);
    }

    // 如果都不适用，返回原值
    return result;
}

llvm::Value* OperationResultHandler::handleFunctionReturn(
        PyCodeGen& gen,
        llvm::Value* result,
        int resultTypeId,
        bool isFunctionReturn)
{
    auto& builder = gen.getBuilder();
    llvm::LLVMContext& context = gen.getContext();

    // 如果不是函数返回，不需要特殊处理
    if (!isFunctionReturn)
    {
        return result;
    }

    // 对于引用类型，需要确保正确处理对象生命周期
    bool isResultReference = (resultTypeId == PY_TYPE_STRING || resultTypeId == PY_TYPE_LIST || resultTypeId == PY_TYPE_DICT || resultTypeId >= PY_TYPE_LIST_BASE);

    if (isResultReference && result->getType()->isPointerTy())
    {
        // 复制对象，并确保使用正确的类型ID
        llvm::FunctionType* copyFuncType = llvm::FunctionType::get(
                llvm::PointerType::get(context, 0),
                {llvm::PointerType::get(context, 0),
                 llvm::Type::getInt32Ty(context)},
                false);

        llvm::Function* copyFunc = gen.getOrCreateExternalFunction(
                "py_object_copy",
                llvm::PointerType::get(context, 0),
                {llvm::PointerType::get(context, 0), llvm::Type::getInt32Ty(context)},
                false);

        // 使用正确的类型ID - 这是修复"TypeError: Expected type 5, got 1"错误的关键部分
        llvm::Value* typeIdValue = llvm::ConstantInt::get(
                llvm::Type::getInt32Ty(context),
                resultTypeId);

        return builder.CreateCall(copyFunc, {result, typeIdValue}, "return_copy");
    }

    return result;
}

llvm::Value* OperationResultHandler::prepareArgument(
        PyCodeGen& gen,
        llvm::Value* value,
        int fromTypeId,
        int toTypeId)
{
    // 如果类型已经匹配，不需要调整
    if (fromTypeId == toTypeId)
    {
        return value;
    }

    auto& builder = gen.getBuilder();
    auto& registry = TypeOperationRegistry::getInstance();

    // 检查是否需要类型转换
    TypeConversionDescriptor* convDesc = registry.getTypeConversionDescriptor(fromTypeId, toTypeId);
    if (convDesc)
    {
        value = OperationCodeGenerator::handleTypeConversion(gen, value, fromTypeId, toTypeId);
        return value;
    }

    // 如果是传递基本类型到引用类型参数，包装为对象
    if (!value->getType()->isPointerTy() && (toTypeId == PY_TYPE_OBJECT || toTypeId >= PY_TYPE_LIST_BASE))
    {
        return OperationCodeGenerator::createObject(gen, value, fromTypeId);
    }

    // 如果都不适用，返回原值
    return value;
}

//===----------------------------------------------------------------------===//
// 类型推导器实现
//===----------------------------------------------------------------------===//

ObjectType* TypeInferencer::inferBinaryOpResultType(
        ObjectType* leftType,
        ObjectType* rightType,
        PyTokenType op)
{
    if (!leftType || !rightType)
    {
        return nullptr;
    }

    // 获取类型ID
    int leftTypeId = leftType->getTypeId();
    int rightTypeId = rightType->getTypeId();

    // 使用类型操作注册表获取结果类型ID
    int resultTypeId = TypeOperationRegistry::getInstance().getResultTypeId(leftTypeId, rightTypeId, op);

    // 基于结果类型ID创建ObjectType
    switch (resultTypeId)
    {
        case PY_TYPE_INT:
            return TypeRegistry::getInstance().getType("int");

        case PY_TYPE_DOUBLE:
            return TypeRegistry::getInstance().getType("double");

        case PY_TYPE_BOOL:
            return TypeRegistry::getInstance().getType("bool");

        case PY_TYPE_STRING:
            return TypeRegistry::getInstance().getType("string");

        case PY_TYPE_LIST:
            // 对于列表，需要保持元素类型
            if (const ListType* leftList = dynamic_cast<const ListType*>(leftType))
            {
                return TypeRegistry::getInstance().getListType(
                        const_cast<ObjectType*>(leftList->getElementType()));
            }
            else if (const ListType* rightList = dynamic_cast<const ListType*>(rightType))
            {
                return TypeRegistry::getInstance().getListType(
                        const_cast<ObjectType*>(rightList->getElementType()));
            }
            // 默认为Object元素类型
            return TypeRegistry::getInstance().getListType(
                    TypeRegistry::getInstance().getType("object"));  // Consider returning list[any]

        case PY_TYPE_DICT:
            // 对于字典，保持键值类型
            if (const DictType* leftDict = dynamic_cast<const DictType*>(leftType))
            {
                return TypeRegistry::getInstance().getDictType(
                        const_cast<ObjectType*>(leftDict->getKeyType()),
                        const_cast<ObjectType*>(leftDict->getValueType()));
            }
            else if (const DictType* rightDict = dynamic_cast<const DictType*>(rightType))  // Also check right operand
            {
                return TypeRegistry::getInstance().getDictType(
                        const_cast<ObjectType*>(rightDict->getKeyType()),
                        const_cast<ObjectType*>(rightDict->getValueType()));
            }
            // 默认键为string，值为object
            return TypeRegistry::getInstance().getDictType(
                    TypeRegistry::getInstance().getType("string"),   // Consider key type any
                    TypeRegistry::getInstance().getType("object"));  // Consider value type any

        // --- 添加 PY_TYPE_ANY 的处理 ---
        case PY_TYPE_ANY:
            return TypeRegistry::getInstance().getType("any");
            // --- 结束添加 ---

        default:
            // 对于未知或未处理的类型ID，返回 Any 类型更合适
            std::cerr << "Warning [inferBinaryOpResultType]: Unhandled resultTypeId " << resultTypeId
                      << " for op " << op << ". Defaulting to Any." << std::endl;
            return TypeRegistry::getInstance().getType("any");  // Changed from "object" to "any"
    }
}

/**
 * 推断索引操作的结果类型
 * @param containerType 容器类型
 * @param indexType 索引类型
 * @return 结果类型 ObjectType*，如果无法推断或操作无效则返回 nullptr 或 Any 类型
 */
ObjectType* TypeInferencer::inferIndexOpResultType(
        ObjectType* containerType,
        ObjectType* indexType)
{
    if (!containerType || !indexType)
    {
        // 输入类型无效，无法推断
        std::cerr << "Warning [inferIndexOpResultType]: Invalid input types (null)." << std::endl;
        return TypeRegistry::getInstance().getType("any");  // 返回 Any 作为安全默认值
    }

    int containerTypeId = containerType->getTypeId();
    int indexTypeId = indexType->getTypeId();
    auto& registry = TypeRegistry::getInstance();

    // 1. 处理 ANY 类型容器
    if (containerTypeId == PY_TYPE_ANY)
    {
        // 对 ANY 类型进行索引，结果通常也是 ANY
        return registry.getType("any");
    }

    // 2. 处理列表 (List)
    // 检查是否为 ListType 或基础 PY_TYPE_LIST
    const ListType* listType = dynamic_cast<const ListType*>(containerType);
    if (listType || containerTypeId == PY_TYPE_LIST)
    {
        // 列表索引通常需要整数或布尔值 (隐式转为 0/1)
        if (indexTypeId == PY_TYPE_INT || indexTypeId == PY_TYPE_BOOL || indexTypeId == PY_TYPE_ANY)
        {
            if (listType)
            {
                // 如果是具体的 ListType，返回其元素类型
                return const_cast<ObjectType*>(listType->getElementType());
            }
            else
            {
                // 如果是泛型 PY_TYPE_LIST，我们不知道元素类型，返回 Any
                std::cerr << "Warning [inferIndexOpResultType]: Indexing generic list (PY_TYPE_LIST), element type unknown. Returning Any." << std::endl;
                return registry.getType("any");
            }
        }
        else
        {
            // 无效的列表索引类型
            std::cerr << "Error [inferIndexOpResultType]: Invalid index type (" << indexType->getName() << ") for list." << std::endl;
            return nullptr;  // 表示类型错误
        }
    }

    // 3. 处理字典 (Dict)
    const DictType* dictType = dynamic_cast<const DictType*>(containerType);
    if (dictType || containerTypeId == PY_TYPE_DICT)
    {
        if (dictType)
        {
            // 字典索引需要键类型或兼容类型，或 ANY
            ObjectType* keyType = const_cast<ObjectType*>(dictType->getKeyType());
            if (indexTypeId == keyType->getTypeId() || indexTypeId == PY_TYPE_ANY || TypeOperationRegistry::getInstance().areTypesCompatible(indexTypeId, keyType->getTypeId()))
            {
                // 返回字典的值类型
                return const_cast<ObjectType*>(dictType->getValueType());
            }
            else
            {
                // 无效的字典键类型
                std::cerr << "Error [inferIndexOpResultType]: Invalid key type (" << indexType->getName() << ") for dictionary with key type (" << keyType->getName() << ")." << std::endl;
                return nullptr;  // 表示类型错误
            }
        }
        else
        {
            // 如果是泛型 PY_TYPE_DICT，我们不知道键值类型，返回 Any
            std::cerr << "Warning [inferIndexOpResultType]: Indexing generic dict (PY_TYPE_DICT), value type unknown. Returning Any." << std::endl;
            return registry.getType("any");
        }
    }

    // 4. 处理字符串 (String)
    if (containerTypeId == PY_TYPE_STRING)
    {
        // 字符串索引通常需要整数或布尔值
        if (indexTypeId == PY_TYPE_INT || indexTypeId == PY_TYPE_BOOL || indexTypeId == PY_TYPE_ANY)
        {
            // 字符串索引的结果仍然是字符串 (单个字符)
            return registry.getType("str");
        }
        else
        {
            // 无效的字符串索引类型
            std::cerr << "Error [inferIndexOpResultType]: Invalid index type (" << indexType->getName() << ") for string." << std::endl;
            return nullptr;  // 表示类型错误
        }
    }

    // 5. 处理其他可索引类型 (如果未来添加)
    // 可以检查 TypeFeatureChecker::isIndexable(containerType)
    // 但目前没有明确的通用索引结果类型，返回 Any

    // 6. 处理不可索引类型
    if (!TypeFeatureChecker::isIndexable(containerType))
    {
        std::cerr << "Error [inferIndexOpResultType]: Type (" << containerType->getName() << ") is not indexable." << std::endl;
        return nullptr;  // 表示类型错误
    }

    // 默认或未知情况：返回 Any
    std::cerr << "Warning [inferIndexOpResultType]: Could not determine specific result type for indexing " << containerType->getName() << " with " << indexType->getName() << ". Returning Any." << std::endl;
    return registry.getType("any");
}

/**
* 检查索引是否可用于容器
* @param containerType 容器类型
* @param indexType 索引类型
* @return 是否可用
*/
bool TypeInferencer::canIndexContainer(
        ObjectType* containerType,
        ObjectType* indexType)
{
    if (!containerType || !indexType) return false;

    int containerTypeId = containerType->getTypeId();
    int indexTypeId = indexType->getTypeId();

    // ANY 容器或 ANY 索引通常在运行时检查
    if (containerTypeId == PY_TYPE_ANY || indexTypeId == PY_TYPE_ANY) return true;

    // 列表/字符串索引
    if (containerTypeId == PY_TYPE_LIST || containerTypeId == PY_TYPE_STRING || dynamic_cast<const ListType*>(containerType))
    {
        return indexTypeId == PY_TYPE_INT || indexTypeId == PY_TYPE_BOOL;
    }

    // 字典索引
    const DictType* dictType = dynamic_cast<const DictType*>(containerType);
    if (dictType || containerTypeId == PY_TYPE_DICT)
    {
        if (dictType)
        {
            ObjectType* keyType = const_cast<ObjectType*>(dictType->getKeyType());
            return indexTypeId == keyType->getTypeId() || TypeOperationRegistry::getInstance().areTypesCompatible(indexTypeId, keyType->getTypeId());
        }
        else
        {
            // 泛型字典，允许任何非 None 索引（运行时检查）
            return indexTypeId != PY_TYPE_NONE;
        }
    }

    // 其他情况默认不允许静态检查通过
    return false;
}

ObjectType* TypeInferencer::inferUnaryOpResultType(
        ObjectType* operandType,
        PyTokenType op)  // Already PyTokenType
{
    if (!operandType)
    {
        return nullptr;
    }

    // 对于一元操作，结果类型通常与操作数类型相同，除了特殊情况
    switch (op)  // Use PyTokenType constants
    {
        case TOK_MINUS:  // Use TOK_MINUS instead of '-'
            // 数值取负，类型保持不变
            if (TypeFeatureChecker::isNumeric(operandType))
            {
                return operandType;
            }
            // 如果不是数值类型，尝试在运行时处理，静态推断为 ANY 或引发错误
            // For now, let's return ANY as a safe default, or consider error reporting
            std::cerr << "Warning [inferUnaryOpResultType]: Unary minus applied to non-numeric type "
                      << operandType->getName() << ". Inferring ANY." << std::endl;
            return TypeRegistry::getInstance().getType("any");  // Or return nullptr for error

        case TOK_NOT:  // Use TOK_NOT instead of '!' or '~'
            // Python's 'not' operator always returns bool
            return TypeRegistry::getInstance().getType("bool");

            // Add cases for other unary operators like TOK_PLUS (unary plus), TOK_TILDE (bitwise not) if needed
            // case TOK_PLUS:
            //     if (TypeFeatureChecker::isNumeric(operandType)) {
            //         return operandType;
            //     }
            //     // Handle non-numeric case
            //     return TypeRegistry::getInstance().getType("any");
            // case TOK_TILDE:
            //     if (operandType->getTypeId() == PY_TYPE_INT || operandType->getTypeId() == PY_TYPE_BOOL) {
            //         return TypeRegistry::getInstance().getType("int");
            //     }
            //     // Handle non-integer case
            //     return TypeRegistry::getInstance().getType("any");

        default:
            // 未知或不支持的一元操作
            std::cerr << "Warning [inferUnaryOpResultType]: Unknown or unsupported unary operator "
                      << op << " for type " << operandType->getName() << ". Inferring operand type." << std::endl;
            return operandType;  // Or return ANY or nullptr
    }
}

// 实现 TypeInferencer::getCommonSuperType 方法
ObjectType* TypeInferencer::getCommonSuperType(
        ObjectType* typeA,
        ObjectType* typeB)
{
    if (!typeA) return typeB;
    if (!typeB) return typeA;
    if (typeA == typeB) return typeA;

    // 获取类型ID
    int typeIdA = typeA->getTypeId();
    int typeIdB = typeB->getTypeId();

    // 如果是基本数值类型
    bool isANumeric = (typeIdA == PY_TYPE_INT || typeIdA == PY_TYPE_DOUBLE || typeIdA == PY_TYPE_BOOL);
    bool isBNumeric = (typeIdB == PY_TYPE_INT || typeIdB == PY_TYPE_DOUBLE || typeIdB == PY_TYPE_BOOL);

    if (isANumeric && isBNumeric)
    {
        // 数值类型提升规则: bool < int < double
        if (typeIdA == PY_TYPE_DOUBLE || typeIdB == PY_TYPE_DOUBLE)
        {
            return TypeRegistry::getInstance().getType("double");
        }
        else if (typeIdA == PY_TYPE_INT || typeIdB == PY_TYPE_INT)
        {
            return TypeRegistry::getInstance().getType("int");
        }
        else
        {
            return TypeRegistry::getInstance().getType("bool");
        }
    }

    // 如果是同类容器类型，尝试合并元素类型
    if (typeIdA == PY_TYPE_LIST && typeIdB == PY_TYPE_LIST)
    {
        const ListType* listA = dynamic_cast<const ListType*>(typeA);
        const ListType* listB = dynamic_cast<const ListType*>(typeB);
        if (listA && listB)
        {
            ObjectType* elemCommonType = getCommonSuperType(
                    const_cast<ObjectType*>(listA->getElementType()),
                    const_cast<ObjectType*>(listB->getElementType()));
            return TypeRegistry::getInstance().getListType(elemCommonType);
        }
    }

    if (typeIdA == PY_TYPE_DICT && typeIdB == PY_TYPE_DICT)
    {
        const DictType* dictA = dynamic_cast<const DictType*>(typeA);
        const DictType* dictB = dynamic_cast<const DictType*>(typeB);
        if (dictA && dictB)
        {
            ObjectType* keyCommonType = getCommonSuperType(
                    const_cast<ObjectType*>(dictA->getKeyType()),
                    const_cast<ObjectType*>(dictB->getKeyType()));
            ObjectType* valueCommonType = getCommonSuperType(
                    const_cast<ObjectType*>(dictA->getValueType()),
                    const_cast<ObjectType*>(dictB->getValueType()));
            return TypeRegistry::getInstance().getDictType(keyCommonType, valueCommonType);
        }
    }

    // 如果类型不兼容或者没有明确的共同类型，返回通用Object类型
    return TypeRegistry::getInstance().getType("object");
}

}  // namespace llvmpy
