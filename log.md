## 修改说明

1. 将所有 `Type::getInt8PtrTy(gen.getContext())` 调用替换为 `PointerType::get(Type::getInt8Ty(gen.getContext()), 0)`

2. 这是因为新版本的LLVM移除了 `getInt8PtrTy()` 方法，我们需要使用 `PointerType::get()` 来创建指针类型

3. 参数 `0` 表示默认地址空间（在大多数情况下是正确的）

这个修改应该能解决您遇到的编译错误，同时保持原有代码的功能不变。如果项目中还有其他地方使用了 `getInt8PtrTy`，也需要以同样的方式进行修改。