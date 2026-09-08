# 验证记录

## 环境与证据

2026-09-08：Windows 10 22H2（19045），AMD Ryzen 9 5900HX，x64 Release；MSVC 19.44.35228，Windows SDK 10.0.26100.0。固定依赖见 [toolchain.json](../toolchain.json)。

测试命令见 [README](../README.md#开发)，执行记录位于 `build/Testing/Temporary/`，截图与性能样本位于 `build/artifacts/`。

## 自动化与桌面验证

| 范围 | 结果与入口 |
| --- | --- |
| 动作模型与图标 | 通过：[动作测试](../tests/action_tests.cpp)，所有动作类型、独立图标、严格解码 |
| 触发状态与几何 | 通过：[核心测试](../tests/core_tests.cpp)，四种布局与 4／8／12 槽位、负坐标屏幕与缩放、分组展开／返回、安全释放、配置快照与配对释放 |
| 配置事务 | 通过：[配置测试](../tests/config_tests.cpp) 与 [工作区测试](../tests/workspace_tests.cpp)，方案／样式读写、重复绑定拒绝、旧配置读取、损坏保护、四种图标格式、原文件保留、迁移去重、引用删除与写盘失败回滚 |
| 分组配置与画布 | 通过：[迭代测试](../tests/iteration_tests.cpp)，方案隔离、中心与跨级换位限制、缩放平移命中、SVG 高 DPI 渲染、样式继承、无效颜色草稿保留、语言切换保留输入与布局截图 |
| 程序图标 | 通过：[发现测试](../tests/discovery_tests.cpp)，真实 EXE 图标与解析快捷方式后图标一致 |
| 网站图标 | 通过：[图标测试](../tests/icon_tests.cpp)，相对链接、HTML 实体、favicon、PNG／ICO／SVG、缓存持久化、失败及取消；显式开启在线测试后 Python 官网实测通过 |
| 应用发现 | 通过：[发现测试](../tests/discovery_tests.cpp)，真实 Shell 快捷方式、运行中的记事本、窗口路径与全屏判定 |
| 设置界面 | 通过：[UI 测试](../tests/ui_tests.cpp)，输入后即时保存、未完成槽位不阻塞其他改动、关闭窗口无需确认、应用搜索与选择、回车确认、可选程序图标、暂停应用增删与保存、形态切换与整槽交换、失败时草稿保留、选择器关闭与主题截图 |
| 输入与场景暂停 | 通过：[桌面测试](../tests/desktop_tests.cpp)，真实 Win32 钩子和 SendInput；应用排除、全屏进入／退出、按住期间规则变化、原始中键成对放行及恢复触发 |
| 注入与启动 | 通过：[注入测试](../tests/injection_tests.cpp)、[启动测试](../tests/launcher_tests.cpp)，修饰键计划、失败清理、中文及空格路径、真实程序与快捷方式启动 |
| 工具回归 | 通过：[应用测试](../tests/app_tests.cpp)、[截图测试](../tests/screenshot_tests.cpp)、[屏幕标注测试](../tests/screen_annotation_tests.cpp)，设置、轮盘、贴图及独立标注的生命周期与输入交互 |

设置工作区运行 iteration、ui 与 app 受影响目标；覆盖任务页导航、草稿保留、画布返回动作编辑、全览展开、默认隐藏高级设置、自动保存及四语切换。900×640 紧凑窗口与各页截图位于 `build/artifacts/settings-*.png`。

工作区功能验收运行 workspace、actions、core、config、iteration、icons、ui、desktop、app、screenshot、screen_annotation、extended 共 12 个受影响目标，全部通过，未执行全量测试。程序发现与注入专项沿用既有验证。迭代及桌面详细结果位于 `build/iteration-results.txt`、`build/desktop-results.txt`，布局与设置截图位于 `build/artifacts/iteration-*.png`。

[桌面测试](../tests/desktop_tests.cpp) 使用 SendInput 注入移动与中键输入，覆盖前台方案匹配、全局回退、中心动作快照、排除优先级、真实桌面采样材质及四种形态下的分组悬停、刚展开松键取消及移动后执行；不能替代物理鼠标长期使用验收。

[扩展测试](../tests/extended_tests.cpp) 覆盖 CMD／PowerShell、目录打开、窗口置顶／透明度／平铺与本地 OCR 图片识别；HTTP 与 AI 使用本机服务验证结果、失败及取消，未向外部模型发送屏幕内容。

[截图测试](../tests/screenshot_tests.cpp) 验证真实屏幕单像素取色、取消与资源释放；采样前等待测试窗口完成显示动画。四语工作区与毛玻璃截图位于 `build/artifacts/workspace-japanese.png` 和 `build/artifacts/frosted-desktop.png`。

## 构建与便携运行

Release 构建通过。便携启动验证移除开发工具 PATH，仅使用随包 Qt、平台插件与 VC 运行库。smoke-test 退出码为 0；打包与启动测试前后用户配置 SHA-256 一致。

单 EXE 隔离测试见 [portable_tests.ps1](../tests/portable_tests.ps1)：仅复制 EXE 到中文及空格目录、移除开发工具 PATH，首次解包与重复启动通过，配置保存在外部且哈希不变。封装实现见 [Windows launcher](../packaging/windows/launcher.cpp)。

## 代码审查

已检查配置唯一写入口、界面与平台职责、输入配对、异步扫描取消及对象销毁。应用发现和暂停设置复用同一应用条目类型；分组复用动作模型，配置解码拒绝更深层嵌套；预览与运行轮盘共享层级显示及几何规则。共享资源与配置保持唯一写入口，未完成颜色输入保留最近有效值；位图按目标像素尺寸读取，SVG 保留原文件并按目标分辨率渲染。源文件没有超过 2000 行。

## 性能口径

空闲资源测量记录在 `build/artifacts/idle.json`，为本机 10 秒样本；CPU 时间增量受系统计时精度影响，不代表所有场景均为零占用。

`build/artifacts/latency.json` 记录触发处理开始至首个 QPainter 绘制结束的 P50／P95，不包含桌面合成和显示扫描。输入线程通过系统事件更新场景信息，应用发现只在打开选择器或刷新时执行；空闲时没有应用扫描或轮询任务。

## 尚未完成的验收

- Windows 11、Windows 10 最低版本、无开发工具的独立干净机器及长时间压力测试。
- 多显示器混合 DPI、显示器热插拔、真实锁屏／睡眠恢复；状态通知测试没有让机器真正锁屏或休眠。
- 独占全屏游戏、所有桌面程序与打包应用的发现覆盖率；当前应用搜索范围见 [配置交互](product-design.md#配置交互)。
- 高权限目标、提升权限后通过普通用户启动、系统静默移除钩子、系统层注入失败、物理 Pause／Break 与不同键盘布局。
- WSL 实际命令执行（当前机器没有可运行的发行版）、外部模型服务、各浏览器实际加载、全部系统控制动作与多屏窗口迁移。
- macOS 真实鼠标、系统权限与焦点、多屏及全屏应用验收，Developer ID 签名与公证。

平台接口重构后，Windows 的 actions、discovery、desktop、screenshot、config、workspace、iteration、ui、app 通过；screen_annotation 首次出现两项桌面输入失败，隔离复测全部通过，尚未稳定复现。

## macOS 适配

原生实现与双架构打包入口见 [技术方案](technical-plan.md)。2026-09-09：[macOS 验证运行](https://github.com/zainSama233/MouseWheel/actions/runs/34248176304) 通过 Intel / Apple 芯片双架构编译、macos 与 annotation 专项测试、依赖部署和临时签名校验；原生 Cocoa 浮层创建与输入穿透切换、应用图标与发现、配置及设置窗口测试见 [macOS 测试](../tests/macos_tests.cpp)。尚未验证真实鼠标触发、辅助功能授权、屏幕录制、多屏及全屏应用。macOS 包为实验性产物，未完成 Developer ID 签名与公证。

## 仓库发布检查

2026-09-09：Gitleaks 8.30.1 对公开准备前的全部 26 个历史提交及实际 Release ZIP 解包内容扫描，均未发现密钥；另检查全部 462 个历史文件对象，未发现私人配置、环境文件、无关媒体或个人路径。提交元数据中存在作者个人邮箱，历史作者与提交者邮箱统一替换为 GitHub noreply 地址，分支及发布标签同步重写。发布 EXE 与已验证本地产物 SHA-256 一致；后续发布仍需重新检查，扫描不能保证排除所有形式的敏感信息。

仓库文件约束与本地文档链接由 [检查脚本](../scripts/check_repository.py) 验证，[对应测试](../tests/repository_tests.py) 覆盖禁止上传的文件、必要资源和缺失文档资源；[工作流](../.github/workflows/repository.yml) 在提交及 PR 时执行。README 截图来自使用临时配置的界面测试，不含用户配置。
