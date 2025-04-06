#ifndef TYPE_OPERATIONS_H
#define TYPE_OPERATIONS_H

#include "TypeIDs.h"
#include "lexer.h"
#include "ObjectType.h"
#include "CodeGen/CodeGenBase.h"
#include "ObjectType.h"
#include <functional>
#include <unordered_map>
#include <functional>  // 确保 std::hash 可用
#include <tuple>       // 确保 std::tuple 和 std::apply 可用
#include <string>
#include <vector>
#include <memory>
#include <llvm/IR/Value.h>

namespace llvmpy {

class PyCodeGen;

// 哈希元组的辅助类
struct TupleHash {
    template <typename... T>
    std::size_t operator()(const std::tuple<T...>& t) const {
        return std::apply([](const T&... args) {
            std::size_t seed = 0;
            (hash_combine(seed, args), ...);
            return seed;
        }, t);
    }
    
private:
    // 组合哈希值的辅助函数
    template <typename T>
    static void hash_combine(std::size_t& seed, const T& val) {
        seed ^= std::hash<T>{}(val) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    }
};

/**
 * 操作类型枚举 - 描述支持的操作种类
 */
enum class OperationType {
    BINARY,     // 二元操作 (+, -, *, / 等)
    UNARY,      // 一元操作 (-, not, ~ 等)
    COMPARISON, // 比较操作 (==, !=, <, > 等)
    CONVERSION, // 类型转换 (int(), float() 等)
    INDEX,      // 索引操作 (list[i], dict[key] 等)
    CALL        // 函数调用
};

/**
 * 二元操作描述符 - 描述两个类型之间的操作规则
 */
struct BinaryOpDescriptor {
    int leftTypeId;               // 左操作数类型ID
    int rightTypeId;              // 右操作数类型ID
    int resultTypeId;             // 结果类型ID
    std::string runtimeFunction;  // 对应的运行时函数名
    bool needsWrap;               // 是否需要包装结果
    
    // 自定义操作实现 (可选)
    std::function<llvm::Value*(PyCodeGen&, llvm::Value*, llvm::Value*)> customImpl;
};

/**
 * 一元操作描述符 - 描述单个类型的一元操作规则
 */
struct UnaryOpDescriptor {
    int operandTypeId;            // 操作数类型ID
    int resultTypeId;             // 结果类型ID
    std::string runtimeFunction;  // 对应的运行时函数名
    bool needsWrap;               // 是否需要包装结果
    
    // 自定义操作实现 (可选)
    std::function<llvm::Value*(PyCodeGen&, llvm::Value*)> customImpl;
};

/**
 * 类型转换描述符 - 描述类型转换规则
 */
struct TypeConversionDescriptor {
    int sourceTypeId;             // 源类型ID
    int targetTypeId;             // 目标类型ID
    std::string runtimeFunction;  // 对应的运行时函数名
    int conversionCost;           // 转换成本 (数值越小优先级越高)
    
    // 自定义转换实现 (可选)
    std::function<llvm::Value*(PyCodeGen&, llvm::Value*)> customImpl;
};

/**
 * 索引操作描述符 - 描述容器和索引类型之间的规则 (新增)
 */
struct IndexOpDescriptor {
    int containerTypeId;          // 容器类型ID
    int indexTypeId;              // 索引类型ID
    int resultTypeId;             // 结果类型ID
    std::string runtimeFunction;  // 对应的运行时函数名
    
    // 自定义索引实现 (可选)
    std::function<llvm::Value*(PyCodeGen&, llvm::Value*, llvm::Value*)> customImpl;
};

/**
 * 哈希函数，用于std::pair<int, int>
 */
struct PairHash {
    template <class T1, class T2>
    std::size_t operator() (const std::pair<T1, T2>& pair) const {
        return std::hash<T1>()(pair.first) ^ std::hash<T2>()(pair.second);
    }
};

/**
 * 类型操作注册表 - 管理所有类型操作的中央注册处
 */
class TypeOperationRegistry {
private:
    // 二元操作映射: 操作符 -> (左类型ID, 右类型ID) -> 操作描述符
    std::unordered_map<
        char, 
        std::unordered_map<
            std::pair<int, int>, 
            BinaryOpDescriptor, 
            PairHash
        >
    > binaryOps;
    
    // 一元操作映射: 操作符 -> 类型ID -> 操作描述符
    std::unordered_map<
        char,
        std::unordered_map<int, UnaryOpDescriptor>
    > unaryOps;
    
    // 类型转换映射: 源类型ID -> 目标类型ID -> 转换描述符
    std::unordered_map<
        int,
        std::unordered_map<int, TypeConversionDescriptor>
    > typeConversions;
    
    // 索引操作映射: 容器类型ID -> 索引类型ID -> 索引描述符 (新增)
    std::unordered_map<
        int,
        std::unordered_map<int, IndexOpDescriptor, PairHash>
    > indexOps;
    
    // 类型兼容性映射: (类型A, 类型B) -> 是否兼容
    std::unordered_map<
        std::pair<int, int>,
        bool,
        PairHash
    > typeCompatibility;
    
    // 类型提升规则: (类型A, 类型B, 操作符) -> 结果类型
    std::unordered_map<
        std::tuple<int, int, char>,
        int,
        TupleHash
    > typePromotions;
    
    // 私有构造函数 (单例模式)
    TypeOperationRegistry();
    
    // 初始化内置操作
    void initializeBuiltinOperations();
    
public:
    // 单例访问
    static TypeOperationRegistry& getInstance();
    
    /**
     * 注册二元操作
     * @param op 操作符 (如 '+', '-' 等)
     * @param leftTypeId 左操作数类型ID
     * @param rightTypeId 右操作数类型ID
     * @param resultTypeId 结果类型ID
     * @param runtimeFunc 运行时函数名
     * @param needsWrap 结果是否需要包装
     * @param customImpl 自定义实现函数 (可选)
     */
    void registerBinaryOp(
        char op, 
        int leftTypeId, 
        int rightTypeId, 
        int resultTypeId, 
        const std::string& runtimeFunc, 
        bool needsWrap = true,
        std::function<llvm::Value*(PyCodeGen&, llvm::Value*, llvm::Value*)> customImpl = nullptr
    );
    
    /**
     * 注册一元操作
     * @param op 操作符 (如 '-', '~' 等)
     * @param operandTypeId 操作数类型ID
     * @param resultTypeId 结果类型ID
     * @param runtimeFunc 运行时函数名
     * @param needsWrap 结果是否需要包装
     * @param customImpl 自定义实现函数 (可选)
     */
    void registerUnaryOp(
        char op,
        int operandTypeId,
        int resultTypeId,
        const std::string& runtimeFunc,
        bool needsWrap = true,
        std::function<llvm::Value*(PyCodeGen&, llvm::Value*)> customImpl = nullptr
    );
    
    /**
     * 注册类型转换
     * @param sourceTypeId 源类型ID
     * @param targetTypeId 目标类型ID
     * @param runtimeFunc 运行时函数名
     * @param conversionCost 转换成本
     * @param customImpl 自定义实现函数 (可选)
     */
    void registerTypeConversion(
        int sourceTypeId,
        int targetTypeId,
        const std::string& runtimeFunc,
        int conversionCost = 1,
        std::function<llvm::Value*(PyCodeGen&, llvm::Value*)> customImpl = nullptr
    );
    
    /**
     * 注册索引操作 (新增)
     * @param containerTypeId 容器类型ID
     * @param indexTypeId 索引类型ID
     * @param resultTypeId 结果类型ID
     * @param runtimeFunc 运行时函数名
     * @param customImpl 自定义实现函数 (可选)
     */
    void registerIndexOp(
        int containerTypeId,
        int indexTypeId,
        int resultTypeId,
        const std::string& runtimeFunc,
        std::function<llvm::Value*(PyCodeGen&, llvm::Value*, llvm::Value*)> customImpl = nullptr
    );
    
    /**
     * 注册类型兼容性
     * @param typeIdA 类型A ID
     * @param typeIdB 类型B ID
     * @param compatible 是否兼容
     */
    void registerTypeCompatibility(int typeIdA, int typeIdB, bool compatible);
    
    /**
     * 注册类型提升规则
     * @param typeIdA 类型A ID
     * @param typeIdB 类型B ID
     * @param op 操作符
     * @param resultTypeId 结果类型ID
     */
    void registerTypePromotion(int typeIdA, int typeIdB, char op, int resultTypeId);
    
    /**
     * 获取二元操作描述符
     * @param op 操作符
     * @param leftTypeId 左操作数类型ID
     * @param rightTypeId 右操作数类型ID
     * @return 操作描述符指针，如果不存在则返回nullptr
     */
    BinaryOpDescriptor* getBinaryOpDescriptor(char op, int leftTypeId, int rightTypeId);
    
    /**
     * 获取一元操作描述符
     * @param op 操作符
     * @param operandTypeId 操作数类型ID
     * @return 操作描述符指针，如果不存在则返回nullptr
     */
    UnaryOpDescriptor* getUnaryOpDescriptor(char op, int operandTypeId);
    
    /**
     * 获取类型转换描述符
     * @param sourceTypeId 源类型ID
     * @param targetTypeId 目标类型ID
     * @return 转换描述符指针，如果不存在则返回nullptr
     */
    TypeConversionDescriptor* getTypeConversionDescriptor(int sourceTypeId, int targetTypeId);
    
    /**
     * 获取索引操作描述符 (新增)
     * @param containerTypeId 容器类型ID
     * @param indexTypeId 索引类型ID
     * @return 索引描述符指针，如果不存在则返回nullptr
     */
    IndexOpDescriptor* getIndexOpDescriptor(int containerTypeId, int indexTypeId);
    
    /**
     * 判断两种类型是否兼容
     * @param typeIdA 类型A ID
     * @param typeIdB 类型B ID
     * @return 是否兼容
     */
    bool areTypesCompatible(int typeIdA, int typeIdB);
    
    /**
     * 判断索引类型是否与容器兼容 (新增)
     * @param containerTypeId 容器类型ID
     * @param indexTypeId 索引类型ID
     * @return 是否兼容
     */
    bool isIndexCompatible(int containerTypeId, int indexTypeId);
    
    /**
     * 获取操作的结果类型ID
     * @param leftTypeId 左操作数类型ID
     * @param rightTypeId 右操作数类型ID
     * @param op 操作符
     * @return 结果类型ID
     */
    int getResultTypeId(int leftTypeId, int rightTypeId, char op);
    
    /**
     * 获取索引操作的结果类型ID (新增)
     * @param containerTypeId 容器类型ID
     * @param indexTypeId 索引类型ID
     * @return 结果类型ID
     */
    int getIndexResultTypeId(int containerTypeId, int indexTypeId);
    
    /**
     * 为操作数自动选择正确的类型转换
     * @param fromTypeId 源类型ID
     * @param toTypeId 目标类型ID
     * @return 转换描述符指针，如果不需要转换则返回nullptr
     */
    TypeConversionDescriptor* findBestConversion(int fromTypeId, int toTypeId);
    
    /**
     * 为索引自动选择正确的类型转换 (新增)
     * @param indexTypeId 索引类型ID
     * @return 转换描述符指针，如果不需要转换则返回nullptr
     */
    TypeConversionDescriptor* findBestIndexConversion(int indexTypeId);
    
    /**
     * 查找可行的二元操作路径
     * @param op 操作符
     * @param leftTypeId 左操作数类型ID
     * @param rightTypeId 右操作数类型ID
     * @return 转换后的操作数类型对 (如果不需要转换则返回原始类型对)
     */
    std::pair<int, int> findOperablePath(char op, int leftTypeId, int rightTypeId);
    
    /**
     * 查找可行的索引操作路径 (新增)
     * @param containerTypeId 容器类型ID
     * @param indexTypeId 索引类型ID
     * @return 转换后的索引类型 (如果不需要转换则返回原始索引类型)
     */
    int findIndexablePath(int containerTypeId, int indexTypeId);
};

/**
 * 操作码映射 - 将字符操作符映射到字符串名称
 */
class OperatorMapper {
public:
    // 获取二元操作符对应的函数名后缀
    static std::string getBinaryOpName(char op);
    
    // 获取一元操作符对应的函数名后缀
    static std::string getUnaryOpName(char op);
    
    // 获取比较操作符对应的函数名后缀
    static std::string getComparisonOpName(char op);
    
    // 生成完整的运行时函数名
    static std::string getRuntimeFunctionName(const std::string& base, const std::string& opName);
};

/**
 * 操作代码生成器 - 生成类型操作的LLVM IR代码
 */
class OperationCodeGenerator {
public:
    /**
     * 处理二元操作
     * @param gen 代码生成器
     * @param op 操作符
     * @param left 左操作数
     * @param right 右操作数
     * @param leftTypeId 左操作数类型ID
     * @param rightTypeId 右操作数类型ID
     * @return 操作结果值
     */
    static llvm::Value* handleBinaryOp(
        CodeGenBase& gen,
        char op,
        llvm::Value* left,
        llvm::Value* right,
        int leftTypeId,
        int rightTypeId
    );
    
    /**
     * 处理一元操作
     * @param gen 代码生成器
     * @param op 操作符
     * @param operand 操作数
     * @param operandTypeId 操作数类型ID
     * @return 操作结果值
     */
    static llvm::Value* handleUnaryOp(
        CodeGenBase& gen,
        char op,
        llvm::Value* operand,
        int operandTypeId
    );
    
    /**
     * 处理索引操作 (新增)
     * @param gen 代码生成器
     * @param container 容器值
     * @param index 索引值
     * @param containerTypeId 容器类型ID
     * @param indexTypeId 索引类型ID
     * @return 索引操作结果值
     */
    static llvm::Value* handleIndexOp(
        CodeGenBase& gen,
        llvm::Value* container,
        llvm::Value* index,
        int containerTypeId,
        int indexTypeId
    );
    
    /**
     * 处理类型转换
     * @param gen 代码生成器
     * @param value 要转换的值
     * @param fromTypeId 源类型ID
     * @param toTypeId 目标类型ID
     * @return 转换后的值
     */
    static llvm::Value* handleTypeConversion(
        CodeGenBase& gen,
        llvm::Value* value,
        int fromTypeId,
        int toTypeId
    );
    
    /**
     * 准备索引值 - 确保索引值可用于容器访问 (新增)
     * @param gen 代码生成器
     * @param index 索引值
     * @param indexTypeId 索引类型ID
     * @return 准备好的索引值 (通常是整数)
     */
    static llvm::Value* prepareIndexValue(
        CodeGenBase& gen,
        llvm::Value* index,
        int indexTypeId
    );
    
    /**
     * 从任何类型值中提取整数 (新增)
     * @param gen 代码生成器
     * @param value 值
     * @param typeId 值的类型ID
     * @return 提取的整数值
     */
    static llvm::Value* extractIntFromValue(
        CodeGenBase& gen,
        llvm::Value* value,
        int typeId
    );
    
    /**
     * 从对象类型获取运行时类型ID
     * @param type 对象类型
     * @return 运行时类型ID
     */
    static int getTypeId(ObjectType* type);
    
    /**
     * 根据值和类型创建合适的对象
     * @param gen 代码生成器
     * @param value 基本值
     * @param typeId 类型ID
     * @return 创建的对象
     */
    static llvm::Value* createObject(PyCodeGen& gen, llvm::Value* value, int typeId);
    
    /**
     * 对ANY类型执行索引操作 (新增)
     * @param gen 代码生成器
     * @param container ANY类型的容器
     * @param index 索引值
     * @param indexTypeId 索引类型ID
     * @return 索引操作结果
     */
    static llvm::Value* handleAnyTypeIndexing(
        CodeGenBase& gen,
        llvm::Value* container,
        llvm::Value* index,
        int indexTypeId
    );
};

/**
 * 操作结果处理器 - 处理操作结果的生命周期和类型转换
 */
class OperationResultHandler {
public:
    /**
     * 调整操作结果，确保类型正确
     * @param gen 代码生成器
     * @param result 操作结果
     * @param resultTypeId 结果类型ID
     * @param expectedTypeId 期望类型ID
     * @return 调整后的结果
     */
    static llvm::Value* adjustResult(
        CodeGenBase& gen,
        llvm::Value* result,
        int resultTypeId,
        int expectedTypeId
    );
    
    /**
     * 处理函数返回值
     * @param gen 代码生成器
     * @param result 操作结果
     * @param resultTypeId 结果类型ID
     * @param isFunctionReturn 是否为函数返回值
     * @return 处理后的结果
     */
    static llvm::Value* handleFunctionReturn(
        PyCodeGen& gen,
        llvm::Value* result,
        int resultTypeId,
        bool isFunctionReturn
    );
    
    /**
     * 准备作为参数传递的值
     * @param gen 代码生成器
     * @param value 值
     * @param fromTypeId 源类型ID
     * @param toTypeId 目标参数类型ID
     * @return 准备好的参数值
     */
    static llvm::Value* prepareArgument(
        PyCodeGen& gen,
        llvm::Value* value,
        int fromTypeId,
        int toTypeId
    );
    
    /**
     * 处理索引操作结果 (新增)
     * @param gen 代码生成器
     * @param result 索引操作结果
     * @param containerTypeId 容器类型ID
     * @param indexTypeId 索引类型ID
     * @return 处理后的结果
     */
    static llvm::Value* handleIndexResult(
        PyCodeGen& gen,
        llvm::Value* result,
        int containerTypeId,
        int indexTypeId
    );
};

/**
 * 类型推导器 - 根据操作数类型推导结果类型
 */
class TypeInferencer {
public:
    /**
     * 推断二元操作的结果类型
     * @param leftType 左操作数类型
     * @param rightType 右操作数类型
     * @param op 操作符
     * @return 结果类型
     */
    static ObjectType* inferBinaryOpResultType(
        ObjectType* leftType,
        ObjectType* rightType,
        char op
    );
    
    /**
     * 推断一元操作的结果类型
     * @param operandType 操作数类型
     * @param op 操作符
     * @return 结果类型
     */
    static ObjectType* inferUnaryOpResultType(
        ObjectType* operandType,
        char op
    );
    
    /**
     * 推断二元操作的结果类型 (Token版本)
     * @param leftType 左操作数类型
     * @param rightType 右操作数类型
     * @param opToken 操作符Token
     * @return 结果类型
     */
    static ObjectType* inferBinaryOpResultType(
        ObjectType* leftType,
        ObjectType* rightType,
        PyTokenType opToken
    );
    
    /**
     * 推断一元操作的结果类型 (Token版本)
     * @param operandType 操作数类型
     * @param opToken 操作符Token
     * @return 结果类型
     */
    static ObjectType* inferUnaryOpResultType(
        ObjectType* operandType,
        PyTokenType opToken
    );
    
    /**
     * 推断索引操作的结果类型 (新增)
     * @param containerType 容器类型
     * @param indexType 索引类型
     * @return 结果类型
     */
    static ObjectType* inferIndexOpResultType(
        ObjectType* containerType,
        ObjectType* indexType
    );
    
    /**
     * 获取两个类型的公共超类型
     * @param typeA 类型A
     * @param typeB 类型B
     * @return 公共超类型
     */
    static ObjectType* getCommonSuperType(
        ObjectType* typeA,
        ObjectType* typeB
    );
    
    /**
     * 检查索引是否可用于容器 (新增)
     * @param containerType 容器类型
     * @param indexType 索引类型
     * @return 是否可用
     */
    static bool canIndexContainer(
        ObjectType* containerType,
        ObjectType* indexType
    );
};


} // namespace llvmpy

#endif // TYPE_OPERATIONS_H