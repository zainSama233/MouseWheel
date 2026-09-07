# 验证记录

## 验证环境

2026-09-08：Windows 10 22H2（19045），AMD Ryzen 9 5900HX，x64 Release。MSVC 19.44.35228，Windows SDK 10.0.26100.0；依赖版本见 [toolchain.json](../toolchain.json)。

## 已执行

| 检查 | 结果与证据入口 |
| --- | --- |
| 状态机与几何 | 通过：[core_tests.cpp](../tests/core_tests.cpp) |
| 配置事务与损坏保护 | 通过：[config_tests.cpp](../tests/config_tests.cpp)，使用真实临时文件 |
| 注入计划与部分失败清理 | 通过：[injection_tests.cpp](../tests/injection_tests.cpp) |
| 设置、持久化、主题截图、窗口释放 | 通过：[ui_tests.cpp](../tests/ui_tests.cpp) |
| 原生输入和真实应用 | 通过：[desktop_tests.cpp](../tests/desktop_tests.cpp)，使用实际 Win32 钩子、SendInput、Qt 编辑器和 Windows 记事本 |
| 便携依赖 | 已在移除开发工具 PATH 的环境启动，退出码 0；Qt、qwindows 和 VC 运行库均从包内加载 |
| 代码审查 | 已检查模块边界、配置写入口、输入配对、原生资源生命周期；无超过 2000 行的源文件 |

测试命令见 [README](../README.md#开发)。CTest 记录在 `build/Testing/Temporary/`；生成的截图和性能测量在 `build/artifacts/`，不纳入源代码提交。

桌面测试通过外部标识的系统合成输入驱动真实钩子，检查实际剪贴板结果和前台窗口。系统恢复测试只向本程序的状态窗口发送通知，没有让机器实际锁屏或睡眠。注入部分失败由纯计划测试覆盖，未在真实高权限目标上故意制造失败。

## 性能样本

同一机器的一次 10 秒空闲观测：工作集约 20.8 MiB，私有内存约 4.0 MiB；进程 CPU 时间增量为 0 ms，受系统计时精度限制，不代表任何场景均为零 CPU。

呼出测量从输入线程处理触发开始，到轮盘首个 QPainter 绘制结束；结果见 `build/artifacts/latency.json`。这是绘制完成时延，不包含桌面合成器与显示器扫描输出，不能直接当作可见首帧延迟承诺。设置窗口释放通过对象生命周期测试验证，尚未做长时间反复开关的内存压力测试。

## 尚未完成的验收

- Windows 11、Windows 10 最低版本，以及未安装开发工具的独立干净机器。
- 浏览器、Win 修饰键的系统菜单副作用、不同键盘布局和硬件连续输入。
- 多显示器混合 DPI、真实显示器热插拔、真实锁屏／睡眠恢复。
- 高权限目标、监听被系统静默移除、系统层部分注入失败。
- macOS 原生后端、真实 Mac 权限与焦点验证、独立应用打包、签名和公证。

当前交付是可运行的 Windows MVP；上述项目不标记为通过，也不代表跨平台发布验收完成。
