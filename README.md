# 鼠标快捷强化

Windows 鼠标快捷键轮盘。首次运行打开设置，点击「完成」后在系统托盘常驻。

- 默认按住 **鼠标中键** 呼出，移动到动作后松开中键执行。中键点击由轮盘占用，滚轮照常使用；暂停后恢复中键原功能。
- 已有配置可在设置中选择「无修饰键」切换到单独中键；也可选择修饰键与鼠标键组合。
- 中心、空槽位或 Esc 取消。轮盘外松开同样取消。
- 托盘提供设置、暂停／恢复、重新连接输入和退出。
- 设置自动保存到程序旁的 `config.json`；设置窗口打开期间暂停触发。
- 快捷键支持字母、数字、F1–F24、空格、Tab、Enter、Esc、方向键及导航编辑键。使用主键盘录制。
- 普通权限进程无法向更高权限应用发送快捷键；失败会显示托盘提示。

## 运行

解压整个便携包后运行 `MouseWheel.exe`。保留随包的 DLL 与 `platforms` 目录；程序目录需要可写。

## 截图

从托盘选择「区域截图」，或在设置中将任一槽位的动作设为「区域截图」。鼠标框选后进入标注编辑器，支持画笔、矩形、箭头、文字、马赛克、撤销／重做、复制与保存 PNG。取消按钮或 Esc 退出。单次选区位于一个显示器内。

截图期间轮盘暂停，退出后恢复。现有配置保留，新配置的默认动作见 [defaultConfig](src/core/model.cpp)。

## 开发

Windows 10 1809+／Windows 11 x64，Python 3.11+。依赖版本见 [toolchain.json](toolchain.json)，Qt SDK 安装到系统盘的 Qt 目录。

```powershell
./scripts/bootstrap.ps1
./scripts/build.ps1
./scripts/test.ps1 -Pattern '^(core|config|annotation|injection)$'
./scripts/test.ps1 -Pattern '^(ui|desktop|screenshot)$'
./scripts/build.ps1 -Package
```

桌面测试会暂时打开测试编辑器并操作鼠标、剪贴板；运行时请勿操作键鼠。测试结束恢复鼠标位置与原剪贴板内容。

便携输出：`dist/MouseWheel/`。设置截图：`build/artifacts/`。

## 文档

- [产品设计](docs/product-design.md)
- [技术方案与代码入口](docs/technical-plan.md)
- [验证记录与交付边界](docs/validation.md)
