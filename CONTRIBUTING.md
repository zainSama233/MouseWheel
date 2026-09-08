# 参与 MouseWheel

欢迎反馈问题、改进文档，也欢迎提交代码。较大的功能请先开 Issue 说明使用场景，避免和现有方向重复。

## 开始开发

构建步骤见 [README](README.md#从源码构建)，模块入口见 [技术方案](docs/technical-plan.md)。从 `main` 建立分支，让每个 PR 聚焦一件事。

改动前先读现有实现，复用共享动作模型、配置写入口和预览组件；配置、交互规则和平台代码保持各自职责。精细功能应放进高级设置，避免日常界面越来越复杂。

## 验证改动

先为变化编写能复现问题的测试，再修改实现。只运行受影响的测试；桌面测试会操作鼠标、窗口和剪贴板，运行时请暂停其他操作。

Windows 示例：

```powershell
./scripts/build.ps1
./scripts/test.ps1 -Pattern '^(core|config)$'
```

按实际改动选择测试目标。便携包测试和 macOS 构建入口见 [README](README.md#从源码构建)，各测试覆盖范围见 [验证记录](docs/validation.md)。

文档和仓库文件检查（Python 3.11+）：

```bash
python tests/repository_tests.py
python scripts/check_repository.py
```

检查以 Git 暂存区中的文件清单为准，新增文档需先 `git add`。

## 提交 PR

说明解决了什么问题、最终行为和验证结果；界面变化附上不含私人信息的截图。文档只保留一份事实来源，其他页面通过链接引用。

只提交源码、必要测试、构建脚本、许可证和文档资源。不要提交个人配置、密钥、运行日志、依赖目录、打包产物或无关媒体；可执行文件通过 Release 分发。敏感问题请遵循 [安全反馈](SECURITY.md)。
