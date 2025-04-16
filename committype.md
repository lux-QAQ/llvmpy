在 C++ 项目（或其他语言项目）的 GitHub 提交中，良好地组织提交类型可以帮助团队更清晰地理解变更内容。下面是一些常见的 **提交类型**（commit types），它们适用于大多数 C++ 项目，特别是配合如 Conventional Commits、semantic versioning、自动生成 changelog 等实践时非常有用：

---

### 🔧 常见的 Git 提交类型

| 提交类型        | 说明 |
|-----------------|------|
| `feat`          | 添加新功能（Feature） |
| `fix`           | 修复 Bug |
| `refactor`      | 重构代码、删除冗余代码、结构代码 |
| `perf`          | 性能优化 |
| `docs`          | 仅修改文档，如 README、注释等 |
| `style`         | 不影响代码逻辑的格式修改，如空格、缩进 |
| `test`          | 添加或修改测试代码 |
| `build`         | 构建系统或外部依赖相关修改，如 CMake、Makefile 等 |
| `ci`            | 持续集成配置修改，如 GitHub Actions、Travis、Jenkins |
| `chore`         | 其他杂项修改，如更新脚本、版本号、包管理配置等 |
| `revert`        | 回滚某个提交 |

---

### 📦 可选的扩展类型（视项目而定）

| 扩展类型        | 说明 |
|-----------------|------|
| `version`       | 发布新版本（可自动生成 changelog） |
| `deps`          | 依赖项变更（添加、更新或删除第三方库） |
| `merge`         | 合并分支（可由 Git 自动生成） |
| `security`      | 与安全漏洞相关的修复 |
| `api`           | 变更了对外公开的 API 接口 |
| `config`        | 配置相关，如 `.clang-format`、`.editorconfig` 变更等 |
| `infra`         | 与项目基础设施（如部署、脚本）相关的改动 |

---

### ✅ 示例提交信息（遵循 Conventional Commits）

```bash
feat: 添加对象生命周期跟踪功能
fix: 修复 LLVM IR 输出中变量重复定义的问题
refactor: 重构 CodeGen 模块以支持多目标平台
docs: 更新 README 添加用法说明
chore: bump 版本号到 v0.2.0
test: 添加 parser 的错误恢复测试用例
```

---

### 🚀 建议组合使用

- 使用 `type(scope): message` 格式会更清晰，例如：
  - `feat(parser): 支持函数参数默认值`
  - `fix(runtime): 修复 divide by zero 问题`
  - `refactor(codegen): 分离指令生成逻辑`

---

需要我帮你创建一个 Git commit message 模板或自动化 changelog 生成工具（比如 `cz-conventional-changelog` 或 `git-chglog`）的配置吗？这对维护大型 C++ 项目非常有帮助。