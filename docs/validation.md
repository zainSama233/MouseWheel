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

2026-09-08：屏幕标注与应用级针对性回归通过，覆盖原生绘制后不抢前台、画笔／文字按钮与颜色／粗细下拉框点击、桌面穿透，以及中键轮盘启动标注。入口见上面的标注与应用测试。

2026-09-08：文字生命周期测试覆盖原生中文输入、确定、取消、Esc、切换桌面与退出时释放对话框；应用测试连续三轮执行桌面穿透点击和中键轮盘重入，核对标注像素保持、窗口数量与工具切换。复用会话时恢复最小化工具栏也有覆盖。

当前测试环境与未完成验收沿用本文对应章节。框选操作使用 Qt 测试事件；穿透与轮盘输入使用 Win32 SendInput。屏幕标注尚未完成混合 DPI 多屏、物理鼠标连续操作、HDR 和 macOS 验收。内置工具不承诺固定内存或延迟指标。
## 图标轮盘与应用／网页启动

2026-09-08：8 个受影响测试目标通过，Release 构建通过。便携包在移除开发工具 PATH 后完成 smoke-test，退出码为 0。三种形状与三种主题组合已生成预览并检查布局；图片导入后删除原文件，配置内图片仍可读取。动画中快速取消后不再重开窗口。

入口：[形状命中与间距取消](../tests/core_tests.cpp)、[图片和启动地址持久化](../tests/config_tests.cpp)、[外观与动画](../tests/ui_tests.cpp)、[原生动作完整分发](../tests/desktop_tests.cpp)、[实际应用与快捷方式启动](../tests/launcher_tests.cpp)。

应用测试运行带中文、空格及特殊字符路径的真实测试程序，并通过 Windows Shell 创建和启动 `.lnk`。网页测试验证向 QDesktopServices 传递完整 URL，使用其测试处理器截取请求；尚未逐个验收浏览器的实际加载结果。前文跨平台与混合 DPI 验收边界仍适用。

## 独立图标与完整动作体系

2026-09-08：11 个受影响测试目标通过，Release 构建通过；便携包移除开发工具 PATH 后 smoke-test 退出码为 0，运行库依赖已核对。测试范围为 actions、core、config、injection、ui、launcher、extended、desktop、app、screen_annotation、screenshot。

- [动作及外观持久化](../tests/action_tests.cpp)：所有动作类型、独立图片／矢量图标、类型错误拒绝；[配置测试](../tests/config_tests.cpp) 覆盖现有配置读取与当前格式保存。
- [设置与录入](../tests/ui_tests.cpp)：图标跨动作保留、主键搜索、Pause／Break、原生独占录入、Tab 保存、编辑器隐藏／关闭时取消捕获和配对释放、设置释放与主题预览。
- [真实执行](../tests/extended_tests.cpp)：程序及自定义浏览器参数、CMD／PowerShell 生成文件、临时目录打开与关闭、置顶切换、透明度、平铺；本地 OCR 实际识别图片文字。
- OCR 网络测试连接本机 HTTP 服务，验证 HTTP 与 AI 请求、结果提取、失败和取消；未将屏幕内容发送给外部 AI 服务。
- 原有中键触发、输入配对、截图贴图和独立屏幕标注的回归入口沿用前文。

当前机器的 WSL 没有可运行的发行版环境，因此未标记 WSL 实际命令执行通过。外部模型服务、各浏览器实际加载、提升权限后的普通用户启动、多显示器迁移、物理 Pause／Break 键及系统锁屏／虚拟桌面的逐项实机验收仍需对应环境。macOS 原生后端与发布验收尚未完成。
