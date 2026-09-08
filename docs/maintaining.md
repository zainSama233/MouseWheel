# 维护与发布

## 合并前

审查模块职责和配置写入口，确认受影响测试通过；参考 [贡献指南](../CONTRIBUTING.md)。平台实测结论只记录在 [验证记录](validation.md)，不要把尚未完成的验收写成已支持。

## 发布

1. 在 [项目版本](../CMakeLists.txt) 中确定版本，使用 [README 构建入口](../README.md#从源码构建) 生成对应产物。
2. Windows 运行便携隔离测试；macOS 使用 [构建工作流](../.github/workflows/macos.yml)，确认构建、专项测试、部署和签名校验结果。
3. 检查归档内容，确保不包含个人配置、图标库、密钥和测试日志；配置测试使用临时目录。
4. 对将公开的 Git 历史及实际发布文件运行密钥扫描，例如 Gitleaks；扫描失败不能当作“没有发现”。若确认历史有敏感数据，先撤销相关凭据，再清理历史及受影响的标签和发布资产，并通知协作者重新同步。
5. 发布到 [GitHub Releases](https://github.com/zainSama233/MouseWheel/releases)，实验版本标记为预发布，写明平台边界。更新 [README 下载入口](../README.md#下载与上手)。

不要上传本地构建缓存。源代码通过 Git 管理，用户需要的可执行文件通过 Release 分发。
