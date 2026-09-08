# 鼠标快捷强化

Windows 鼠标快捷键轮盘。首次运行打开设置，改动即时保存，关闭设置后在系统托盘常驻。

- 默认按住 **鼠标中键** 呼出，移动到动作后松开中键执行。中键点击由轮盘占用，滚轮照常使用；暂停后恢复中键原功能。
- 已有配置可在设置中选择「无修饰键」切换到单独中键；也可选择修饰键与鼠标键组合。
- 主中心默认取消，可在设置中启用独立动作；子轮盘中心返回。空槽位、轮盘外松开或 Esc 取消。
- 托盘提供设置、暂停／恢复、重新连接输入和退出。
- 设置自动保存到程序旁的 `config.json`；设置窗口打开期间暂停触发。
- 快捷键支持字母、数字、F1–F24、空格、Tab、Enter、Esc、方向键及导航编辑键、Pause／Break；支持主键搜索、组合键拼装与独占录入。
- 普通权限进程无法向更高权限应用发送快捷键；失败会显示托盘提示。

## 运行

Windows 推荐下载 `MouseWheel-Portable.exe`，放入可写目录后直接双击，无需安装。首次启动将内置依赖释放到同目录隐藏的 `.mousewheel` 文件夹；配置和图标仍保存在 EXE 旁，搬移时一起保留 `config.json` 与 `icons`。关闭程序后删除这些文件即可移除。

也可使用 ZIP 目录版：解压整个便携包后运行 `MouseWheel.exe`，保留随包的 DLL 与 `platforms` 目录。

## 轮盘与启动

在设置中选择配置方案，点击右侧画布或全览列表编辑槽位；滚轮缩放、中键平移、复位按钮恢复视角，拖拽交换位置。应用选择与场景暂停见 [配置交互](docs/product-design.md#配置交互)。共享图标库支持 SVG／PNG／ICO／JPG，导入资源保存在配置旁的 `icons` 目录；搬移便携版时请一并保留。动作分类与功能见 [产品设计](docs/product-design.md#动作体系)，远程 OCR 配置见 [接口说明](docs/ocr-http.md)。

## 内置工具

从托盘或轮盘槽位选择「截图贴图」或「屏幕标注」。功能范围与交互见 [产品设计](docs/product-design.md#内置工具)。新配置的默认槽位见 [defaultConfig](src/core/model.cpp)，已有槽位可在设置中更换动作。

## 开发

Windows 10 1809+／Windows 11 x64，Python 3.11+。依赖版本见 [toolchain.json](toolchain.json)，Qt SDK 安装到系统盘的 Qt 目录。

```powershell
./scripts/bootstrap.ps1
./scripts/build.ps1
./scripts/test.ps1 -Pattern '^(actions|core|config|annotation|injection)$'
./scripts/test.ps1 -Pattern '^(ui|desktop|screenshot|screen_annotation|app|launcher|extended)$'
./scripts/build.ps1 -Package
./tests/portable_tests.ps1
```

桌面测试会暂时打开测试编辑器并操作鼠标、剪贴板；运行时请勿操作键鼠。测试结束恢复鼠标位置与原剪贴板内容。

便携输出：`dist/MouseWheel/`。设置截图：`build/artifacts/`。

## 文档

- [产品设计](docs/product-design.md)
- [技术方案与代码入口](docs/technical-plan.md)
- [验证记录与交付边界](docs/validation.md)
