# OCR 接口

自定义 HTTP 动作向完整接口地址发送 JSON POST。`image` 为 PNG 的 Base64，`mimeType` 为 `image/png`。填写 API Key 时使用 `Authorization: Bearer <key>`。

响应必须是 JSON，通过设置中的结果字段读取字符串。点号分隔对象字段和数组下标，例如 `text` 或 `results.0.text`。

AI 模式使用兼容 Chat Completions 的多模态请求；填写完整接口地址及支持图片输入的模型。请求和响应处理的唯一实现见 [OcrSession](../src/tools/ocr_session.cpp)。不自动发现提供商或改写接口地址。

凭据随本地配置保存；分享配置前请移除凭据。取消后不采用迟到结果。
