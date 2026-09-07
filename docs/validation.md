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

## 中键单键触发

2026-09-08：核心、配置、UI 与桌面测试共 4 个受影响测试目标通过；Release 构建及便携包 smoke-test 退出码为 0。默认配置验证为无修饰键＋中键，设置截图已检查。

自动化入口：[核心输入配对](../tests/core_tests.cpp)、[配置保存](../tests/config_tests.cpp)、[设置切换](../tests/ui_tests.cpp)、[原生中键输入](../tests/desktop_tests.cpp)。触发行为与占用范围见 [产品设计](product-design.md#主要功能)。

真实 Windows 钩子测试覆盖中键松开后复制到剪贴板、保持原窗口焦点、中心与 Esc 取消，以及暂停后中键点击恢复。输入由 SendInput 驱动，尚未替代物理鼠标连续操作和浏览器专项验收。

新建配置采用默认触发方式，已有配置按其保存值加载，可在设置中选择无修饰键切换。


## 独立截图贴图与屏幕标注

2026-09-08：8 个受影响测试目标通过；新增原生工具栏点击、贴图期间中键呼出、多贴图场景的针对性复测通过。Release 构建通过，三种主题贴图与浮动工具栏已检查。便携包在移除开发工具 PATH 的环境启动设置，smoke-test 退出码为 0。

验证入口：[透明绘制、橡皮与撤销历史](../tests/annotation_tests.cpp)、[真实屏幕框选、原始像素、贴图缩放与 PNG](../tests/screenshot_tests.cpp)、[实时桌面与原生鼠标穿透](../tests/screen_annotation_tests.cpp)、[工具和设置并存](../tests/app_tests.cpp)、[轮盘隐藏后分发](../tests/desktop_tests.cpp)、[工具槽位保存](../tests/ui_tests.cpp)。

当前测试环境与未完成验收沿用本文对应章节。框选操作使用 Qt 测试事件；穿透与轮盘输入使用 Win32 SendInput。屏幕标注尚未完成混合 DPI 多屏、物理鼠标连续操作、HDR 和 macOS 验收。内置工具不承诺固定内存或延迟指标。
