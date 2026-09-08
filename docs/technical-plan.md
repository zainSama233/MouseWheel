# 鼠标快捷强化 · 技术方案

## 文档职责

产品范围、交互规则与验收目标以 [产品设计](product-design.md) 为唯一来源。本文记录技术选型、模块边界和验证策略。类型、配置字段和算法以代码与测试为准；已验证能力与发布边界以验证记录为准。

## 实现入口

- [交互模型与几何](../src/core/model.h)、[状态机](../src/core/interaction.h)
- [动作编解码](../src/config/action_codec.h)、[逐槽位编辑](../src/ui/slot_editor.h)、[快捷键编辑](../src/ui/shortcut_editor.h)、[独占捕获](../src/platform/shortcut_capture.h)
- [共享屏幕框选](../src/tools/region_capture.h)、[OCR 会话](../src/tools/ocr_session.h)、[本地 OCR](../src/tools/local_ocr.h)、[窗口与系统操作](../src/platform/desktop_actions.h)
- [配置存储](../src/config/config_store.h)、[共享图片导入与解码](../src/core/image_asset.h)
- [平台接口](../src/platform/input_service.h)、[Windows 输入线程](../src/platform/windows_input.cpp)、[注入计划](../src/platform/windows_injection.cpp)
- [按需应用发现](../src/platform/application_catalog.h)、[窗口上下文](../src/platform/window_context.h)、[应用选择器](../src/ui/application_picker.h)、[暂停规则编辑](../src/ui/trigger_rules_editor.h)
- [轮盘](../src/ui/wheel_window.h)、[设置](../src/ui/settings_window.h)、[共享主题](../src/ui/theme.h)
- [标注模型与撤销](../src/tools/annotation_document.h)、[独立屏幕标注](../src/tools/screen_annotation_session.h)、[贴图窗口](../src/tools/pinned_image.h)、[截图会话](../src/tools/screenshot_session.h)
- [程序图标与快捷方式解析](../src/platform/program_icon.h)、[网站图标请求](../src/tools/website_icon.h)
- [启动目标执行](../src/tools/launcher.h)、[动作图标](../src/ui/action_icons.h)、[图标来源](../src/ui/icons/SOURCE.md)、[系统动作图标映射](../src/core/model.cpp)、[图标渲染与自动选择测试](../tests/icon_tests.cpp)
- [应用装配](../src/app.cpp)、[启动入口](../src/main.cpp)
- [固定依赖版本](../toolchain.json)、[构建与打包](../scripts/build.ps1)、[测试入口](../scripts/test.ps1)
- [实测结果与尚未验证的项目](validation.md)


## 技术选型

| 领域 | 方案 |
| --- | --- |
| 主语言 | C++20，使用 RAII 管理原生资源 |
| 界面 | Qt 6 Widgets；轮盘使用 QWidget + QPainter 自绘，Qt SVG 渲染图标 |
| Windows | Win32 低级输入钩子、非激活窗口、SendInput |
| macOS | Objective-C++ 桥接 AppKit / Core Graphics，使用非激活面板和事件接口 |
| 配置 | Qt JSON + QSaveFile，本地单文件保存 |
| 构建与测试 | CMake、CTest、Qt Test；Windows 使用 MSVC，macOS 使用 Apple Clang |

Qt 提供界面与通用基础设施，原生接口处理系统输入和窗口特性。首版采用单进程，不引入 WebView、脚本运行时、数据库、后台服务或插件系统。

依赖版本由 [toolchain.json](../toolchain.json) 统一管理，构建入口见 [CMakeLists.txt](../CMakeLists.txt)。Windows 目标为 10 1809+／11 x64，当前实测基线见 [验证记录](validation.md)。Qt 平台约束参考 [Qt 6.8 Windows 支持](https://doc.qt.io/qt-6.8/windows.html)。macOS 的平台设计保留在本文，原生实现与发布验收尚未完成。

## 模块边界

初期按职责组织同一工程，不为目录边界额外拆进程或动态库。

| 模块 | 职责 |
| --- | --- |
| 应用装配 | 启动、退出、托盘／菜单栏和组件生命周期 |
| 交互核心 | 唯一交互状态，触发、选择、取消与执行决策，共用强类型 |
| 平台适配 | 捕获与消费输入，目标窗口、非激活显示、权限和快捷键发送 |
| 界面 | 展示轮盘和设置，提交配置修改，不直接操作钩子或发送输入 |
| 配置管理 | 唯一配置写入口，统一校验、持久化和发布有效配置 |

交互核心不依赖 QWidget、Win32 或 AppKit。两端复用事件与动作类型，原生键码只在平台边界转换。独占捕获封装在平台层，录入组件关闭后仍配对消费已拦截按键的释放，再释放钩子。槽位使用强类型动作变体，动作参数不与图标混合。配置只写当前格式，旧配置在读取边界转换为同一模型。槽位动作类型由交互模型统一定义；内置工具在轮盘隐藏确认后转交 UI 层，不经过键盘注入。快捷键使用结构化按键与修饰键表示，显示文本由其派生，不通过显示字符串驱动执行。

轮盘和设置样式从同一主题定义派生。设置草稿通过配置管理提交；活动轮盘持有触发时的配置快照，避免操作中途改变含义。

## 输入处理与执行

采用事件驱动。原生回调必须同步、快速地决定放行或消费，不等待 UI、文件保存或耗时工作。交互状态由一个串行输入执行上下文持有；UI 通过带会话标识的通知更新，避免过期通知在取消后重新显示轮盘。

Windows 在带消息循环的专用线程安装钩子；macOS 使用承载事件 tap 的运行循环。UI 留在主线程，原生窗口操作遵循平台线程要求，通过完成通知协调隐藏与执行。

必须保持以下约束：

- 未命中触发条件时原样放行；消费鼠标按下后，必须配对消费对应松开。取消、暂停和设置变更也必须完成配对，不能向原应用泄漏孤立松开事件。
- Esc 取消时配对处理被消费的按下、重复和松开，不吞掉无关输入。
- 输入上下文根据最新鼠标位置计算选择，松开时使用最终位置；命中与绘制共享同一份几何结果，不依赖滞后的 UI 高亮。
- 记录触发时的前台窗口，执行前确认其仍有效且仍为前台。目标改变则取消，不强行抢回焦点，也不向新应用发送动作。
- 标识自身注入事件，防止递归触发；不屏蔽其他来源的所有合成事件。

修饰键处理封装在平台输入组件中，分别跟踪物理状态和自身注入状态。执行时识别仍按住但不属于动作的修饰键，按平台语义临时中和、发送动作并恢复仍实际按住的状态；保留左右键身份，不能仅依赖触发时快照。禁止释放所有按键或使用固定延迟掩盖状态问题。

该策略须先用真实应用验证，重点覆盖 Alt／Win／Command 的系统副作用和执行期间的物理释放。若平台不能可靠满足立即执行语义，应回到产品层明确约束，不静默改为等待所有按键松开。Windows SendInput 不会自动重置现有键盘状态，且受目标进程完整性级别限制；注入成功也不等于应用已执行动作，见 [SendInput 文档](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-sendinput)。

部分注入失败时仅清理自身负责的未配对按键，不盲目重试完整动作。暂停、锁屏、睡眠和权限丢失统一终止活动会话；恢复时重新核对输入状态与监听有效性，不重放旧操作。空闲时不持续轮询。前台与窗口位置事件更新场景快照；低级鼠标钩子只读取快照，不扫描应用或读取进程路径。规则只决定新的按下是否触发，既有会话仍由状态机负责释放配对。

## 窗口与显示适配

轮盘使用透明、无边框、非激活窗口。Windows 对接非激活样式与显示方式；macOS 验证 NSPanel 非激活模式，不仅依赖跨平台窗口标志。设置窗口正常获取焦点，录制快捷键期间暂停轮盘触发。

平台边界统一转换系统坐标与 Qt 逻辑坐标，保留显示器原点和缩放信息。绘制、命中和边缘布局共享几何结果。越界时将轮盘调整到可用屏幕内，中心取消区随实际绘制中心移动，不移动系统鼠标；此边缘交互须通过原型确认可用性。

macOS 检查监听与发送所需权限并提供授权入口；缺少权限时保持设置可用，明确显示不可用能力。事件 tap 被禁用时处理系统通知并核对恢复条件。Windows 普通权限下对高权限应用的限制作为真实验收边界，不自动提权。

## 配置与资源管理

Windows 便携版在程序目录保存配置；目录不可写时明确报错，不悄悄改写其他位置。macOS 使用 QStandardPaths 定位应用配置目录，不写入应用包。配置位置在启动时确定，由配置管理统一持有。

配置修改先校验并通过 QSaveFile 提交，成功后发布运行配置；失败时保留原有效配置并提示。禁止开启直接覆盖的降级写入。损坏或不支持的配置保留原文件，要求用户明确重置，不自动覆盖。保存机制见 [QSaveFile 文档](https://doc.qt.io/qt-6/qsavefile.html)。

轮盘呼出采用 [QVariantAnimation](https://doc.qt.io/qt-6/qvariantanimation.html)，隐藏时立即终止。其余时间仅在内容或选择变化时重绘。轮盘可保留窗口以降低再次呼出的延迟，隐藏时停止绘制活动；设置窗口关闭后释放界面资源。原生句柄统一由 RAII 对象释放。诊断只记录错误、状态转换和性能数据，不记录全局输入内容。

## 实施与验证

### 第一阶段：两端技术原型

先用固定动作完成触发、非激活显示、取消、松开执行和修饰键恢复。Windows 为主要验证平台，同时在真实 Mac 上验证权限、面板和输入发送。通过后将有效代码整理进正式模块，删除临时入口。

原型确定系统基线和性能预算。使用 Release 构建测量空闲 CPU、常驻内存、触发至首帧的 P50／P95 延迟，以及设置窗口关闭后的资源变化；记录硬件、系统与测量方式，不预设未经验证的性能结论。

### 第二阶段：完成 MVP

在已验证链路上接入配置、设置、主题和常驻管理。按模块采用 TDD：先覆盖交互状态与输入配对，再实现功能。测试包含下一层真实依赖，仅替换更深层系统边界；核心测试不依赖桌面会话，配置测试使用真实临时文件。

| 验证层 | 覆盖重点 |
| --- | --- |
| 核心测试 | 触发、取消、空槽、最终位置选择、重复事件、配置快照、暂停后的输入配对 |
| 集成测试 | 配置读写与失败保护、平台事件归一化、注入标识和失败清理 |
| 桌面验证 | 真实编辑器和浏览器、左右修饰键、不同动作修饰键、焦点变化和快速操作 |
| 平台验证 | 混合缩放、屏幕边缘、锁屏／睡眠恢复、显示器切换、权限变化和监听失效 |

产品级验收直接使用 [产品设计的体验与质量要求](product-design.md#体验与质量要求)。使用受控测试窗口观察输入配对，并在真实应用确认动作结果；模拟测试不能替代桌面验证。

### 第三阶段：打包与交付

Windows 随包携带所需 Qt 插件和运行库，在未安装开发工具的目标系统验证解压运行。macOS 构建独立应用包，在目标架构验证签名、公证和首次授权。构建配置固定依赖版本与部署组件，不依赖开发机全局环境。

开发期间运行受影响模块测试及相关构建、桌面验证；发布前执行全量自动化测试和目标平台验收矩阵。记录区分通过、失败和未验证，未执行的检查不能标记通过。交付前审查模块职责、重复状态、资源生命周期与无效代码。

## 原生接口参考

- Windows：[低级鼠标钩子](https://learn.microsoft.com/en-us/windows/win32/winmsg/lowlevelmouseproc)、[扩展窗口样式](https://learn.microsoft.com/en-us/windows/win32/winmsg/extended-window-styles)。
- macOS：[CGEvent](https://developer.apple.com/documentation/coregraphics/cgevent)、[NSPanel](https://developer.apple.com/documentation/appkit/nspanel)。


## 内置工具依赖

标注命令使用 [QUndoStack](https://doc.qt.io/qt-6/qundostack.html)；裁剪保留屏幕抓取结果的原始像素，参见 [QScreen::grabWindow](https://doc.qt.io/qt-6/qscreen.html#grabWindow)。Windows 捕获前使用 [DwmFlush](https://learn.microsoft.com/en-us/windows/win32/api/dwmapi/nf-dwmapi-dwmflush) 同步本程序待提交画面；不使用固定延时。显示器坐标与捕获像素的映射以截图会话代码为准。PNG 经 QImageWriter 编码并由 QSaveFile 提交。

屏幕标注使用实时透明覆盖层，鼠标穿透遵循 [Windows layered window hit testing](https://learn.microsoft.com/en-us/windows/win32/winmsg/window-features#layered-windows)。窗口焦点策略参考 [Windows 无激活窗口](https://devblogs.microsoft.com/oldnewthing/20240919-00/?p=110283)。文字输入使用 [QDialog 异步对话框](https://doc.qt.io/qt-6/qdialog.html#open)。浮动工具栏独立于覆盖层；状态与资源生命周期以 [屏幕标注会话](../src/tools/screen_annotation_session.cpp) 为准。产品交互参考 [MarkerOn](https://github.com/ifer47/markeron)。

应用与网页启动遵循 [QProcess::startDetached](https://doc.qt.io/qt-6/qprocess.html#startDetached) 和 [QDesktopServices](https://doc.qt.io/qt-6/qdesktopservices.html)，参数与路径不拼接为 shell 命令。动作与完整目标在轮盘隐藏后统一分发；执行入口见上方索引。

## 平台能力依据

- 普通用户启动使用桌面 Shell，参考 [Microsoft 的桌面进程启动说明](https://devblogs.microsoft.com/oldnewthing/20131118-00/?p=2643)。
- 本地识别使用 [Windows OCR](https://learn.microsoft.com/en-us/uwp/api/windows.media.ocr.ocrengine)，异步任务在工作线程执行。
- 隐藏终端通过 [QProcess 原生创建参数](https://doc.qt.io/qt-6/qprocess-createprocessarguments.html) 设置；CMD 原生参数与普通程序参数分别遵循对应的解析规则。

## 交互参考

应用选择、轮盘预览编辑及场景暂停的产品交互参考 [StarPie](https://github.com/SoftBlack42/StarPie)。平台实现使用 [QueryFullProcessImageNameW](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-queryfullprocessimagenamew)、[SetWinEventHook](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setwineventhook) 和 Qt 的异步任务接口；可取消的应用扫描不访问界面对象。
