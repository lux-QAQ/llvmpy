将 py_type_objects.cpp 和 py_type_objects.h 的功能拆分到更专门的文件中是一个很好的重构思路，这可以提高代码的模块化和可维护性。关于您提到的公共函数以及整体结构，这里有一些建议：

**处理公共/基础对象函数：**

这些函数通常作为 `PyBaseObject_Type` 的实现，或者作为许多其他类型的默认实现。

1.  **`object_new`, `object_init`, `object_dealloc`, `object_repr`, `object_str`**:
    *   这些函数是 `PyBaseObject_Type` 的核心槽实现。
    *   **建议**: 将这些函数的定义以及 `PyBaseObject_Type` 结构本身的定义移动到 py_object.cpp 中。
    *   在 py_object.h 中声明 `extern PyTypeObject_t PyBaseObject_Type;` 以及这些函数的原型（如果它们需要被其他模块直接调用，而不仅仅是通过类型槽）。
    *   它们在 py_type_objects.cpp 文件顶部的 `static` 前向声明也应随之移动到 py_object.cpp 的顶部。

2.  **`default_tp_getattro`, `default_tp_setattro`**:
    *   这些是通用的属性获取和设置处理器，被许多类型用作默认的 `tp_getattro` 和 `tp_setattro` 实现。
    *   **建议**: 将它们的定义移动到 py_attributes.cpp（如果此文件用于属性相关逻辑）或者如果 py_object.cpp 更合适也可以放在那里。
    *   在 py_attributes.h (或相应的头文件) 中声明这些函数的原型，以便其他文件可以将它们赋值给类型对象的槽。
    *   它们在 py_type_objects.cpp 中的 `static` 前向声明也应移动。

**拆分其他类型对象及其槽函数：**

对于 `PyBaseObject_Type` 之外的其他类型 (如 `PyString_Type`, `PyList_Type`, `PyFunction_Type`, `PyBoundMethod_Type` 以及所有异常类型):

1.  **为每个主要类型或类型组创建专门的 `.h` 和 `.cpp` 文件**：
    *   **不建议使用**像 `object_method` 这样过于通用的目录名。直接在 RunTime 和 RunTime 下按类型组织文件通常更清晰。
    *   例如，对于字符串类型：
        *   **`include/RunTime/py_string.h`**:
            *   声明 `extern PyTypeObject_t PyString_Type;`
            *   声明字符串特有的槽函数原型，如 `PyObject* string_repr(PyObject* self);`，`void string_dealloc_specific(PyObject* self);` 等。
        *   **`src/RunTime/py_string.cpp`**:
            *   包含 `RunTime/py_string.h` 和其他必要的头文件。
            *   定义 `PyString_Type` 结构实例，并将其槽指向对应的实现函数。
            *   实现 `string_repr`, `string_dealloc_specific` 等函数。
    *   对 `PyList_Type`, `PyDict_Type`, `PyTuple_Type`, `PyFunction_Type`, `PyBoundMethod_Type` 等执行类似操作。
    *   对于异常类型，可以将它们组合到 `include/RunTime/py_exceptions.h` 和 `src/RunTime/py_exceptions.cpp` 中。

2.  **删除 py_type_objects.cpp 和 py_type_objects.h**:
    *   在所有类型定义和相关函数都迁移到新的专用文件后，这两个文件就可以安全地删除了。

3.  **处理 `initialize_type_protocols()`**:
    *   此函数（在 py_type_objects.cpp 中）用于初始化某些内置类型的协议槽（如 `tp_as_number`, `tp_as_sequence`）。
    *   在拆分后，此函数需要能访问到它所初始化的类型对象（如 `PyInt_Type`, `PyFloat_Type` 等）。
    *   **建议**: 可以将其移动到一个更通用的运行时初始化文件，例如 `src/RunTime/runtime_init.cpp`（如果还没有，可以创建一个）。这个初始化文件将在所有相关的类型对象定义链接后被调用。它需要包含所有相关类型的头文件（例如 `py_int.h`, `py_float.h`, `py_string.h`, `py_list.h`）。

**代码示例 (以 `default_tp_setattro` 和 `PyExc_MemoryError` 为例):**

*   **py_attributes.h (新增或修改)**
    ````cpp
    // filepath: include/RunTime/py_attributes.h
    #ifndef PY_ATTRIBUTES_H
    #define PY_ATTRIBUTES_H

    #include "runtime_common.h" // For PyObject, PyTypeObject_t

    // Forward declaration for the function pointer type if needed, or rely on PyTypeObject definition
    // typedef int (*setattrofunc)(PyObject *, const char *, PyObject *);

    PyObject* default_tp_getattro(PyObject* obj, const char* name_c);
    int default_tp_setattro(PyObject* obj, const char* name_c, PyObject* value);

    #endif // PY_ATTRIBUTES_H
    ````

*   **py_attributes.cpp (新增或修改)**
    ````cpp
    // filepath: src/RunTime/py_attributes.cpp
    #include "RunTime/py_attributes.h"
    #include "RunTime/py_object.h"       // For py_create_string, PyInstanceObject_t, etc.
    #include "RunTime/py_type.h"         // For PyType_Ready, PyType_IsSubtype
    #include "RunTime/py_type_objects.h" // Temporarily, until all types are moved. Then include specific type headers.
                                       // For PyDict_GetItem, PyDict_SetItem (might move to py_dict.h/cpp)
                                       // For PyExc_TypeError, PyExc_AttributeError, PyExc_SystemError (might move to py_exceptions.h/cpp)
    #include "RunTime/py_log.h"
    #include "RunTime/py_error.h"        // For PyErr_Format, PyErr_SetString

    // ... (default_tp_getattro implementation) ...

    int default_tp_setattro(PyObject* obj, const char* name_c, PyObject* value) {
        // ... (current implementation of default_tp_setattro from py_type_objects.cpp)
        // filepath: /home/ljs/code/llvmpy/src/RunTime/py_type_objects.cpp
        // ...existing code...
        LOG_DEBUG("default_tp_setattro: obj=%p (type %s), attr_name='%s', value=%p",
                  (void*)obj, obj && obj->ob_type ? obj->ob_type->tp_name : "NULL_OBJ_OR_TYPE", name_c, (void*)value);

        if (!obj || !name_c)
        {
            PyErr_SetString(&PyExc_TypeError, "setattr called with NULL object or name");
            return -1;
        }
        if (!obj->ob_type)
        {
            PyErr_Format(&PyExc_SystemError, "object %p has NULL ob_type in setattr", (void*)obj);
            return -1;
        }

        // TODO: Implement full attribute setting logic, including:
        // 1. Check for data descriptors on the type (MRO search). If found, call its __set__.
        // 2. If no data descriptor, set in instance_dict.
        // This is a simplified version that only sets in instance_dict.

        PyTypeObject_t* type = obj->ob_type;
        if ((type->tp_flags & Py_TPFLAGS_HEAPTYPE) && type->tp_basicsize >= sizeof(PyInstanceObject_t))
        {
            PyInstanceObject_t* inst = (PyInstanceObject_t*)obj;
            if (!inst->instance_dict)
            {  // Should have been created by object_new
                LOG_ERROR("default_tp_setattro: Instance %p of type '%s' has no instance_dict.", (void*)obj, type->tp_name);
                PyErr_Format(&PyExc_SystemError, "instance of %s has no instance_dict", type->tp_name);
                return -1;
            }
            PyObject* attr_name_obj = py_create_string(name_c);
            if (!attr_name_obj) return -1;

            int status = PyDict_SetItem((PyObject*)inst->instance_dict, attr_name_obj, value);
            py_decref(attr_name_obj);
            if (status < 0)
            {
                LOG_ERROR("default_tp_setattro: PyDict_SetItem failed for instance_dict of %p.", (void*)obj);
                return -1;
            }
            LOG_DEBUG("default_tp_setattro: Set '%s' in instance_dict of %p.", name_c, (void*)obj);
            return 0;
        }

        LOG_ERROR("AttributeError: cannot set attribute '%s' on '%s' object (not a heap instance or no instance_dict support).",
                  name_c, type->tp_name);
        PyErr_Format(&PyExc_AttributeError, "'%s' object can't set attributes", type->tp_name);
        return -1;
    }
    ````

*   **`include/RunTime/py_exceptions.h` (新增)**
    ````cpp
    // filepath: include/RunTime/py_exceptions.h
    #ifndef PY_EXCEPTIONS_H
    #define PY_EXCEPTIONS_H

    #include "runtime_common.h" // For PyTypeObject_t

    extern PyTypeObject_t PyExc_BaseException;
    extern PyTypeObject_t PyExc_Exception;
    extern PyTypeObject_t PyExc_TypeError;
    extern PyTypeObject_t PyExc_StopIteration;
    extern PyTypeObject_t PyExc_ValueError;
    extern PyTypeObject_t PyExc_AttributeError;
    extern PyTypeObject_t PyExc_IndexError;
    extern PyTypeObject_t PyExc_KeyError;
    extern PyTypeObject_t PyExc_MemoryError;
    extern PyTypeObject_t PyExc_OverflowError;
    extern PyTypeObject_t PyExc_ZeroDivisionError;
    extern PyTypeObject_t PyExc_SystemError;

    // Potentially declare common dealloc/repr/str if they are truly specific to exceptions
    // or rely on those from py_object.h if PyBaseObject_Type's methods are used.

    #endif // PY_EXCEPTIONS_H
    ````

*   **`src/RunTime/py_exceptions.cpp` (新增)**
    ````cpp
    // filepath: src/RunTime/py_exceptions.cpp
    #include "RunTime/py_exceptions.h"
    #include "RunTime/py_object.h"     // For object_dealloc, object_repr, object_str, object_new, object_init, PyBaseObject_Type, PyType_Type
    #include "RunTime/py_attributes.h" // For default_tp_getattro, default_tp_setattro
    #include <climits>                 // For INT_MAX

    // Definition of PyExc_MemoryError (example)
    PyTypeObject_t PyExc_MemoryError = {
            {INT_MAX, &PyType_Type},
            "MemoryError",
            sizeof(PyObject),
            0,
            object_dealloc,        // From py_object.cpp
            object_repr,         // From py_object.cpp
            object_str,          // From py_object.cpp
            NULL,
            default_tp_getattro, // From py_attributes.cpp
            default_tp_setattro, // From py_attributes.cpp
            &PyExc_Exception,    // Assumes PyExc_Exception is also defined in this file or included
            NULL,
            object_new,          // From py_object.cpp
            object_init,         // From py_object.cpp
            NULL,
            NULL,
            Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
            NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL
    };

    // ... (definitions for all other exception types, ensuring PyType_Type and base exceptions are correctly referenced) ...
    // Make sure PyExc_BaseException and PyExc_Exception are defined before types that inherit from them.
    ````

**重要提示：**

*   **逐步重构**：这是一个较大的改动，建议逐步进行，每次迁移一个或一组相关的类型，并进行测试。
*   **更新构建系统**：确保所有新的 `.cpp` 文件都已添加到您的 CMake 或其他构建系统中。
*   **Include 路径和依赖**：仔细管理 `#include` 指令，确保文件只包含它们实际需要的头文件，以避免不必要的耦合和编译时间增加。
*   **`PyType_Ready()`**: 确保在运行时早期为所有类型对象调用 `PyType_Ready()`，这通常在所有类型定义完成后，但在它们被实际使用之前。

这个重构方向将使您的代码库更加结构化。将 py_type_objects.cpp 和 py_type_objects.h 的功能拆分到更专门的文件中是一个很好的重构思路，这可以提高代码的模块化和可维护性。关于您提到的公共函数以及整体结构，这里有一些建议：

**处理公共/基础对象函数：**

这些函数通常作为 `PyBaseObject_Type` 的实现，或者作为许多其他类型的默认实现。

1.  **`object_new`, `object_init`, `object_dealloc`, `object_repr`, `object_str`**:
    *   这些函数是 `PyBaseObject_Type` 的核心槽实现。
    *   **建议**: 将这些函数的定义以及 `PyBaseObject_Type` 结构本身的定义移动到 py_object.cpp 中。
    *   在 py_object.h 中声明 `extern PyTypeObject_t PyBaseObject_Type;` 以及这些函数的原型（如果它们需要被其他模块直接调用，而不仅仅是通过类型槽）。
    *   它们在 py_type_objects.cpp 文件顶部的 `static` 前向声明也应随之移动到 py_object.cpp 的顶部。

2.  **`default_tp_getattro`, `default_tp_setattro`**:
    *   这些是通用的属性获取和设置处理器，被许多类型用作默认的 `tp_getattro` 和 `tp_setattro` 实现。
    *   **建议**: 将它们的定义移动到 py_attributes.cpp（如果此文件用于属性相关逻辑）或者如果 py_object.cpp 更合适也可以放在那里。
    *   在 py_attributes.h (或相应的头文件) 中声明这些函数的原型，以便其他文件可以将它们赋值给类型对象的槽。
    *   它们在 py_type_objects.cpp 中的 `static` 前向声明也应移动。

**拆分其他类型对象及其槽函数：**

对于 `PyBaseObject_Type` 之外的其他类型 (如 `PyString_Type`, `PyList_Type`, `PyFunction_Type`, `PyBoundMethod_Type` 以及所有异常类型):

1.  **为每个主要类型或类型组创建专门的 `.h` 和 `.cpp` 文件**：
    *   **不建议使用**像 `object_method` 这样过于通用的目录名。直接在 RunTime 和 RunTime 下按类型组织文件通常更清晰。
    *   例如，对于字符串类型：
        *   **`include/RunTime/py_string.h`**:
            *   声明 `extern PyTypeObject_t PyString_Type;`
            *   声明字符串特有的槽函数原型，如 `PyObject* string_repr(PyObject* self);`，`void string_dealloc_specific(PyObject* self);` 等。
        *   **`src/RunTime/py_string.cpp`**:
            *   包含 `RunTime/py_string.h` 和其他必要的头文件。
            *   定义 `PyString_Type` 结构实例，并将其槽指向对应的实现函数。
            *   实现 `string_repr`, `string_dealloc_specific` 等函数。
    *   对 `PyList_Type`, `PyDict_Type`, `PyTuple_Type`, `PyFunction_Type`, `PyBoundMethod_Type` 等执行类似操作。
    *   对于异常类型，可以将它们组合到 `include/RunTime/py_exceptions.h` 和 `src/RunTime/py_exceptions.cpp` 中。

2.  **删除 py_type_objects.cpp 和 py_type_objects.h**:
    *   在所有类型定义和相关函数都迁移到新的专用文件后，这两个文件就可以安全地删除了。

3.  **处理 `initialize_type_protocols()`**:
    *   此函数（在 py_type_objects.cpp 中）用于初始化某些内置类型的协议槽（如 `tp_as_number`, `tp_as_sequence`）。
    *   在拆分后，此函数需要能访问到它所初始化的类型对象（如 `PyInt_Type`, `PyFloat_Type` 等）。
    *   **建议**: 可以将其移动到一个更通用的运行时初始化文件，例如 `src/RunTime/runtime_init.cpp`（如果还没有，可以创建一个）。这个初始化文件将在所有相关的类型对象定义链接后被调用。它需要包含所有相关类型的头文件（例如 `py_int.h`, `py_float.h`, `py_string.h`, `py_list.h`）。

**代码示例 (以 `default_tp_setattro` 和 `PyExc_MemoryError` 为例):**

*   **py_attributes.h (新增或修改)**
    ````cpp
    // filepath: include/RunTime/py_attributes.h
    #ifndef PY_ATTRIBUTES_H
    #define PY_ATTRIBUTES_H

    #include "runtime_common.h" // For PyObject, PyTypeObject_t

    // Forward declaration for the function pointer type if needed, or rely on PyTypeObject definition
    // typedef int (*setattrofunc)(PyObject *, const char *, PyObject *);

    PyObject* default_tp_getattro(PyObject* obj, const char* name_c);
    int default_tp_setattro(PyObject* obj, const char* name_c, PyObject* value);

    #endif // PY_ATTRIBUTES_H
    ````

*   **py_attributes.cpp (新增或修改)**
    ````cpp
    // filepath: src/RunTime/py_attributes.cpp
    #include "RunTime/py_attributes.h"
    #include "RunTime/py_object.h"       // For py_create_string, PyInstanceObject_t, etc.
    #include "RunTime/py_type.h"         // For PyType_Ready, PyType_IsSubtype
    #include "RunTime/py_type_objects.h" // Temporarily, until all types are moved. Then include specific type headers.
                                       // For PyDict_GetItem, PyDict_SetItem (might move to py_dict.h/cpp)
                                       // For PyExc_TypeError, PyExc_AttributeError, PyExc_SystemError (might move to py_exceptions.h/cpp)
    #include "RunTime/py_log.h"
    #include "RunTime/py_error.h"        // For PyErr_Format, PyErr_SetString

    // ... (default_tp_getattro implementation) ...

    int default_tp_setattro(PyObject* obj, const char* name_c, PyObject* value) {
        // ... (current implementation of default_tp_setattro from py_type_objects.cpp)
        // filepath: /home/ljs/code/llvmpy/src/RunTime/py_type_objects.cpp
        // ...existing code...
        LOG_DEBUG("default_tp_setattro: obj=%p (type %s), attr_name='%s', value=%p",
                  (void*)obj, obj && obj->ob_type ? obj->ob_type->tp_name : "NULL_OBJ_OR_TYPE", name_c, (void*)value);

        if (!obj || !name_c)
        {
            PyErr_SetString(&PyExc_TypeError, "setattr called with NULL object or name");
            return -1;
        }
        if (!obj->ob_type)
        {
            PyErr_Format(&PyExc_SystemError, "object %p has NULL ob_type in setattr", (void*)obj);
            return -1;
        }

        // TODO: Implement full attribute setting logic, including:
        // 1. Check for data descriptors on the type (MRO search). If found, call its __set__.
        // 2. If no data descriptor, set in instance_dict.
        // This is a simplified version that only sets in instance_dict.

        PyTypeObject_t* type = obj->ob_type;
        if ((type->tp_flags & Py_TPFLAGS_HEAPTYPE) && type->tp_basicsize >= sizeof(PyInstanceObject_t))
        {
            PyInstanceObject_t* inst = (PyInstanceObject_t*)obj;
            if (!inst->instance_dict)
            {  // Should have been created by object_new
                LOG_ERROR("default_tp_setattro: Instance %p of type '%s' has no instance_dict.", (void*)obj, type->tp_name);
                PyErr_Format(&PyExc_SystemError, "instance of %s has no instance_dict", type->tp_name);
                return -1;
            }
            PyObject* attr_name_obj = py_create_string(name_c);
            if (!attr_name_obj) return -1;

            int status = PyDict_SetItem((PyObject*)inst->instance_dict, attr_name_obj, value);
            py_decref(attr_name_obj);
            if (status < 0)
            {
                LOG_ERROR("default_tp_setattro: PyDict_SetItem failed for instance_dict of %p.", (void*)obj);
                return -1;
            }
            LOG_DEBUG("default_tp_setattro: Set '%s' in instance_dict of %p.", name_c, (void*)obj);
            return 0;
        }

        LOG_ERROR("AttributeError: cannot set attribute '%s' on '%s' object (not a heap instance or no instance_dict support).",
                  name_c, type->tp_name);
        PyErr_Format(&PyExc_AttributeError, "'%s' object can't set attributes", type->tp_name);
        return -1;
    }
    ````

*   **`include/RunTime/py_exceptions.h` (新增)**
    ````cpp
    // filepath: include/RunTime/py_exceptions.h
    #ifndef PY_EXCEPTIONS_H
    #define PY_EXCEPTIONS_H

    #include "runtime_common.h" // For PyTypeObject_t

    extern PyTypeObject_t PyExc_BaseException;
    extern PyTypeObject_t PyExc_Exception;
    extern PyTypeObject_t PyExc_TypeError;
    extern PyTypeObject_t PyExc_StopIteration;
    extern PyTypeObject_t PyExc_ValueError;
    extern PyTypeObject_t PyExc_AttributeError;
    extern PyTypeObject_t PyExc_IndexError;
    extern PyTypeObject_t PyExc_KeyError;
    extern PyTypeObject_t PyExc_MemoryError;
    extern PyTypeObject_t PyExc_OverflowError;
    extern PyTypeObject_t PyExc_ZeroDivisionError;
    extern PyTypeObject_t PyExc_SystemError;

    // Potentially declare common dealloc/repr/str if they are truly specific to exceptions
    // or rely on those from py_object.h if PyBaseObject_Type's methods are used.

    #endif // PY_EXCEPTIONS_H
    ````

*   **`src/RunTime/py_exceptions.cpp` (新增)**
    ````cpp
    // filepath: src/RunTime/py_exceptions.cpp
    #include "RunTime/py_exceptions.h"
    #include "RunTime/py_object.h"     // For object_dealloc, object_repr, object_str, object_new, object_init, PyBaseObject_Type, PyType_Type
    #include "RunTime/py_attributes.h" // For default_tp_getattro, default_tp_setattro
    #include <climits>                 // For INT_MAX

    // Definition of PyExc_MemoryError (example)
    PyTypeObject_t PyExc_MemoryError = {
            {INT_MAX, &PyType_Type},
            "MemoryError",
            sizeof(PyObject),
            0,
            object_dealloc,        // From py_object.cpp
            object_repr,         // From py_object.cpp
            object_str,          // From py_object.cpp
            NULL,
            default_tp_getattro, // From py_attributes.cpp
            default_tp_setattro, // From py_attributes.cpp
            &PyExc_Exception,    // Assumes PyExc_Exception is also defined in this file or included
            NULL,
            object_new,          // From py_object.cpp
            object_init,         // From py_object.cpp
            NULL,
            NULL,
            Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
            NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL
    };

    // ... (definitions for all other exception types, ensuring PyType_Type and base exceptions are correctly referenced) ...
    // Make sure PyExc_BaseException and PyExc_Exception are defined before types that inherit from them.
    ````

**重要提示：**

*   **逐步重构**：这是一个较大的改动，建议逐步进行，每次迁移一个或一组相关的类型，并进行测试。
*   **更新构建系统**：确保所有新的 `.cpp` 文件都已添加到您的 CMake 或其他构建系统中。
*   **Include 路径和依赖**：仔细管理 `#include` 指令，确保文件只包含它们实际需要的头文件，以避免不必要的耦合和编译时间增加。
*   **`PyType_Ready()`**: 确保在运行时早期为所有类型对象调用 `PyType_Ready()`，这通常在所有类型定义完成后，但在它们被实际使用之前。

这个重构方向将使您的代码库更加结构化。将 py_type_objects.cpp 和 py_type_objects.h 的功能拆分到更专门的文件中是一个很好的重构思路，这可以提高代码的模块化和可维护性。关于您提到的公共函数以及整体结构，这里有一些建议：

**处理公共/基础对象函数：**

这些函数通常作为 `PyBaseObject_Type` 的实现，或者作为许多其他类型的默认实现。

1.  **`object_new`, `object_init`, `object_dealloc`, `object_repr`, `object_str`**:
    *   这些函数是 `PyBaseObject_Type` 的核心槽实现。
    *   **建议**: 将这些函数的定义以及 `PyBaseObject_Type` 结构本身的定义移动到 py_object.cpp 中。
    *   在 py_object.h 中声明 `extern PyTypeObject_t PyBaseObject_Type;` 以及这些函数的原型（如果它们需要被其他模块直接调用，而不仅仅是通过类型槽）。
    *   它们在 py_type_objects.cpp 文件顶部的 `static` 前向声明也应随之移动到 py_object.cpp 的顶部。

2.  **`default_tp_getattro`, `default_tp_setattro`**:
    *   这些是通用的属性获取和设置处理器，被许多类型用作默认的 `tp_getattro` 和 `tp_setattro` 实现。
    *   **建议**: 将它们的定义移动到 py_attributes.cpp（如果此文件用于属性相关逻辑）或者如果 py_object.cpp 更合适也可以放在那里。
    *   在 py_attributes.h (或相应的头文件) 中声明这些函数的原型，以便其他文件可以将它们赋值给类型对象的槽。
    *   它们在 py_type_objects.cpp 中的 `static` 前向声明也应移动。

**拆分其他类型对象及其槽函数：**

对于 `PyBaseObject_Type` 之外的其他类型 (如 `PyString_Type`, `PyList_Type`, `PyFunction_Type`, `PyBoundMethod_Type` 以及所有异常类型):

1.  **为每个主要类型或类型组创建专门的 `.h` 和 `.cpp` 文件**：
    *   **不建议使用**像 `object_method` 这样过于通用的目录名。直接在 RunTime 和 RunTime 下按类型组织文件通常更清晰。
    *   例如，对于字符串类型：
        *   **`include/RunTime/py_string.h`**:
            *   声明 `extern PyTypeObject_t PyString_Type;`
            *   声明字符串特有的槽函数原型，如 `PyObject* string_repr(PyObject* self);`，`void string_dealloc_specific(PyObject* self);` 等。
        *   **`src/RunTime/py_string.cpp`**:
            *   包含 `RunTime/py_string.h` 和其他必要的头文件。
            *   定义 `PyString_Type` 结构实例，并将其槽指向对应的实现函数。
            *   实现 `string_repr`, `string_dealloc_specific` 等函数。
    *   对 `PyList_Type`, `PyDict_Type`, `PyTuple_Type`, `PyFunction_Type`, `PyBoundMethod_Type` 等执行类似操作。
    *   对于异常类型，可以将它们组合到 `include/RunTime/py_exceptions.h` 和 `src/RunTime/py_exceptions.cpp` 中。

2.  **删除 py_type_objects.cpp 和 py_type_objects.h**:
    *   在所有类型定义和相关函数都迁移到新的专用文件后，这两个文件就可以安全地删除了。

3.  **处理 `initialize_type_protocols()`**:
    *   此函数（在 py_type_objects.cpp 中）用于初始化某些内置类型的协议槽（如 `tp_as_number`, `tp_as_sequence`）。
    *   在拆分后，此函数需要能访问到它所初始化的类型对象（如 `PyInt_Type`, `PyFloat_Type` 等）。
    *   **建议**: 可以将其移动到一个更通用的运行时初始化文件，例如 `src/RunTime/runtime_init.cpp`（如果还没有，可以创建一个）。这个初始化文件将在所有相关的类型对象定义链接后被调用。它需要包含所有相关类型的头文件（例如 `py_int.h`, `py_float.h`, `py_string.h`, `py_list.h`）。

**代码示例 (以 `default_tp_setattro` 和 `PyExc_MemoryError` 为例):**

*   **py_attributes.h (新增或修改)**
    ````cpp
    // filepath: include/RunTime/py_attributes.h
    #ifndef PY_ATTRIBUTES_H
    #define PY_ATTRIBUTES_H

    #include "runtime_common.h" // For PyObject, PyTypeObject_t

    // Forward declaration for the function pointer type if needed, or rely on PyTypeObject definition
    // typedef int (*setattrofunc)(PyObject *, const char *, PyObject *);

    PyObject* default_tp_getattro(PyObject* obj, const char* name_c);
    int default_tp_setattro(PyObject* obj, const char* name_c, PyObject* value);

    #endif // PY_ATTRIBUTES_H
    ````

*   **py_attributes.cpp (新增或修改)**
    ````cpp
    // filepath: src/RunTime/py_attributes.cpp
    #include "RunTime/py_attributes.h"
    #include "RunTime/py_object.h"       // For py_create_string, PyInstanceObject_t, etc.
    #include "RunTime/py_type.h"         // For PyType_Ready, PyType_IsSubtype
    #include "RunTime/py_type_objects.h" // Temporarily, until all types are moved. Then include specific type headers.
                                       // For PyDict_GetItem, PyDict_SetItem (might move to py_dict.h/cpp)
                                       // For PyExc_TypeError, PyExc_AttributeError, PyExc_SystemError (might move to py_exceptions.h/cpp)
    #include "RunTime/py_log.h"
    #include "RunTime/py_error.h"        // For PyErr_Format, PyErr_SetString

    // ... (default_tp_getattro implementation) ...

    int default_tp_setattro(PyObject* obj, const char* name_c, PyObject* value) {
        // ... (current implementation of default_tp_setattro from py_type_objects.cpp)
        // filepath: /home/ljs/code/llvmpy/src/RunTime/py_type_objects.cpp
        // ...existing code...
        LOG_DEBUG("default_tp_setattro: obj=%p (type %s), attr_name='%s', value=%p",
                  (void*)obj, obj && obj->ob_type ? obj->ob_type->tp_name : "NULL_OBJ_OR_TYPE", name_c, (void*)value);

        if (!obj || !name_c)
        {
            PyErr_SetString(&PyExc_TypeError, "setattr called with NULL object or name");
            return -1;
        }
        if (!obj->ob_type)
        {
            PyErr_Format(&PyExc_SystemError, "object %p has NULL ob_type in setattr", (void*)obj);
            return -1;
        }

        // TODO: Implement full attribute setting logic, including:
        // 1. Check for data descriptors on the type (MRO search). If found, call its __set__.
        // 2. If no data descriptor, set in instance_dict.
        // This is a simplified version that only sets in instance_dict.

        PyTypeObject_t* type = obj->ob_type;
        if ((type->tp_flags & Py_TPFLAGS_HEAPTYPE) && type->tp_basicsize >= sizeof(PyInstanceObject_t))
        {
            PyInstanceObject_t* inst = (PyInstanceObject_t*)obj;
            if (!inst->instance_dict)
            {  // Should have been created by object_new
                LOG_ERROR("default_tp_setattro: Instance %p of type '%s' has no instance_dict.", (void*)obj, type->tp_name);
                PyErr_Format(&PyExc_SystemError, "instance of %s has no instance_dict", type->tp_name);
                return -1;
            }
            PyObject* attr_name_obj = py_create_string(name_c);
            if (!attr_name_obj) return -1;

            int status = PyDict_SetItem((PyObject*)inst->instance_dict, attr_name_obj, value);
            py_decref(attr_name_obj);
            if (status < 0)
            {
                LOG_ERROR("default_tp_setattro: PyDict_SetItem failed for instance_dict of %p.", (void*)obj);
                return -1;
            }
            LOG_DEBUG("default_tp_setattro: Set '%s' in instance_dict of %p.", name_c, (void*)obj);
            return 0;
        }

        LOG_ERROR("AttributeError: cannot set attribute '%s' on '%s' object (not a heap instance or no instance_dict support).",
                  name_c, type->tp_name);
        PyErr_Format(&PyExc_AttributeError, "'%s' object can't set attributes", type->tp_name);
        return -1;
    }
    ````

*   **`include/RunTime/py_exceptions.h` (新增)**
    ````cpp
    // filepath: include/RunTime/py_exceptions.h
    #ifndef PY_EXCEPTIONS_H
    #define PY_EXCEPTIONS_H

    #include "runtime_common.h" // For PyTypeObject_t

    extern PyTypeObject_t PyExc_BaseException;
    extern PyTypeObject_t PyExc_Exception;
    extern PyTypeObject_t PyExc_TypeError;
    extern PyTypeObject_t PyExc_StopIteration;
    extern PyTypeObject_t PyExc_ValueError;
    extern PyTypeObject_t PyExc_AttributeError;
    extern PyTypeObject_t PyExc_IndexError;
    extern PyTypeObject_t PyExc_KeyError;
    extern PyTypeObject_t PyExc_MemoryError;
    extern PyTypeObject_t PyExc_OverflowError;
    extern PyTypeObject_t PyExc_ZeroDivisionError;
    extern PyTypeObject_t PyExc_SystemError;

    // Potentially declare common dealloc/repr/str if they are truly specific to exceptions
    // or rely on those from py_object.h if PyBaseObject_Type's methods are used.

    #endif // PY_EXCEPTIONS_H
    ````

*   **`src/RunTime/py_exceptions.cpp` (新增)**
    ````cpp
    // filepath: src/RunTime/py_exceptions.cpp
    #include "RunTime/py_exceptions.h"
    #include "RunTime/py_object.h"     // For object_dealloc, object_repr, object_str, object_new, object_init, PyBaseObject_Type, PyType_Type
    #include "RunTime/py_attributes.h" // For default_tp_getattro, default_tp_setattro
    #include <climits>                 // For INT_MAX

    // Definition of PyExc_MemoryError (example)
    PyTypeObject_t PyExc_MemoryError = {
            {INT_MAX, &PyType_Type},
            "MemoryError",
            sizeof(PyObject),
            0,
            object_dealloc,        // From py_object.cpp
            object_repr,         // From py_object.cpp
            object_str,          // From py_object.cpp
            NULL,
            default_tp_getattro, // From py_attributes.cpp
            default_tp_setattro, // From py_attributes.cpp
            &PyExc_Exception,    // Assumes PyExc_Exception is also defined in this file or included
            NULL,
            object_new,          // From py_object.cpp
            object_init,         // From py_object.cpp
            NULL,
            NULL,
            Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
            NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL
    };

    // ... (definitions for all other exception types, ensuring PyType_Type and base exceptions are correctly referenced) ...
    // Make sure PyExc_BaseException and PyExc_Exception are defined before types that inherit from them.
    ````

**重要提示：**

*   **逐步重构**：这是一个较大的改动，建议逐步进行，每次迁移一个或一组相关的类型，并进行测试。
*   **更新构建系统**：确保所有新的 `.cpp` 文件都已添加到您的 CMake 或其他构建系统中。
*   **Include 路径和依赖**：仔细管理 `#include` 指令，确保文件只包含它们实际需要的头文件，以避免不必要的耦合和编译时间增加。
*   **`PyType_Ready()`**: 确保在运行时早期为所有类型对象调用 `PyType_Ready()`，这通常在所有类型定义完成后，但在它们被实际使用之前。

这个重构方向将使您的代码库更加结构化。