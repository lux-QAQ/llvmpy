#ifndef PY_TYPE_DISPATCH_H
#define PY_TYPE_DISPATCH_H

#include "runtime_common.h"

#ifdef __cplusplus
extern "C" {
#endif

// Forward declaration
struct PyTypeMethods;

// Function pointer types for type-specific methods
typedef PyObject* (*py_binary_op_func)(PyObject* left, PyObject* right);
typedef PyObject* (*py_unary_op_func)(PyObject* operand);
typedef PyObject* (*py_compare_op_func)(PyObject* left, PyObject* right, PyCompareOp op);
typedef PyObject* (*py_index_get_func)(PyObject* container, PyObject* index);
typedef void      (*py_index_set_func)(PyObject* container, PyObject* index, PyObject* value);
typedef int       (*py_len_func)(PyObject* obj);





typedef PyObject* (*py_getattr_func)(PyObject* obj, const char* attr_name); // 获取属性
typedef int       (*py_setattr_func)(PyObject* obj, const char* attr_name, PyObject* value); // 设置属性 (返回 0 表示成功, -1 表示失败)




// ---  Hash 和 Equals 函数指针类型 ---
typedef unsigned int (*py_hash_func)(PyObject* obj); // 返回哈希值
typedef PyObject*    (*py_eq_func)(PyObject* self, PyObject* other); // 返回 PyBoolObject (True/False)


// Add more function pointer types as needed (e.g., for iteration, attribute access)

// Structure to hold methods for a specific type
typedef struct PyTypeMethods {
    py_index_get_func index_get; // Handler for obj[index]
    py_index_set_func index_set; // Handler for obj[index] = value
    py_len_func       len;       // Handler for len(obj)
    py_getattr_func   getattr;   // Handler for obj.attr
    py_setattr_func   setattr;   // Handler for obj.attr = value

    // ---  Hash 和 Equals 槽位 ---
    py_hash_func      hash;      // Handler for hash(obj)
    py_eq_func        equals;    // Handler for obj == other


    // Add slots for other operations like binary_add, compare_eq, etc.
    // py_binary_op_func binary_add;
    // py_compare_op_func compare_op; // Maybe rename this if equals handles ==/!=
} PyTypeMethods;

/**
 * @brief Registers a set of methods for a given type ID.
 *
 * @param typeId The type ID to register methods for.
 * @param methods Pointer to the PyTypeMethods structure. Must remain valid.
 * @return True on success, false if typeId is already registered or invalid.
 */
bool py_register_type_methods(int typeId, const PyTypeMethods* methods);

/**
 * @brief Retrieves the methods registered for a given type ID.
 *
 * @param typeId The type ID.
 * @return Pointer to the PyTypeMethods structure, or NULL if not found.
 */
const PyTypeMethods* py_get_type_methods(int typeId);

/**
 * @brief Initializes the built-in type methods (call this during runtime startup).
 */
void py_initialize_builtin_type_methods();




// --- 属性访问处理器前置声明 ---
static PyObject* py_list_index_get_handler(PyObject* container, PyObject* index);
static void      py_list_index_set_handler(PyObject* container, PyObject* index, PyObject* value);
static int       py_list_len_handler(PyObject* obj);

static PyObject* py_dict_index_get_handler(PyObject* container, PyObject* index);
static void      py_dict_index_set_handler(PyObject* container, PyObject* index, PyObject* value);
static int       py_dict_len_handler(PyObject* obj);


static PyObject* py_instance_getattr_handler(PyObject* obj, const char* attr_name);
static int       py_instance_setattr_handler(PyObject* obj, const char* attr_name, PyObject* value);
static PyObject* py_class_getattr_handler(PyObject* obj, const char* attr_name);
static int       py_class_setattr_handler(PyObject* obj, const char* attr_name, PyObject* value);

static PyObject* _py_string_index_get_handler(PyObject* container, PyObject* index);
static unsigned int _py_int_hash(PyObject* obj);
static unsigned int _py_float_hash(PyObject* obj);
static unsigned int _py_string_hash(PyObject* obj);
static unsigned int _py_bool_hash(PyObject* obj);
static unsigned int _py_none_hash(PyObject* obj);
static PyObject* _py_numeric_eq(PyObject* self, PyObject* other);
static PyObject* _py_string_eq(PyObject* self, PyObject* other);
static PyObject* _py_bool_eq(PyObject* self, PyObject* other);
static PyObject* _py_none_eq(PyObject* self, PyObject* other);


#ifdef __cplusplus
} // extern "C"
#endif

#endif // PY_TYPE_DISPATCH_H