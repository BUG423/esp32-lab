# 贡献指南

感谢你对 OpenPal 的兴趣！我们欢迎任何贡献。

## 🚀 快速开始

```bash
git clone https://github.com/<you>/openpal
cd openpal/server
python3 -m venv .venv && source .venv/bin/activate
pip install -r requirements.txt pytest
pytest -q
python -m pal_server.main
# 打开 http://localhost:8080
```

## 🧩 能做什么贡献

| 类型 | 例子 |
|---|---|
| 🧠 工具 | 新 MCP 工具（日历、音乐、智能家居…） |
| 🎭 人格 | 新情绪规则、说话风格、TTS voice |
| 🔊 音频 | Opus 编解码、回声消除、唤醒词 |
| 🧪 测试 | 覆盖率、fuzz、协议兼容性 |
| 📺 界面 | Dashboard 主题、语音可视化 |
| 🔩 固件 | 新板型支持、低功耗、LED 灯环 |
| 📝 文档 | 教程、翻译、演示 GIF |

## 📐 代码规范

- Python: `ruff` 兼容，类型注解友好
- C / ESP-IDF: 与现有 `main/` 风格一致（4 空格，`s_` 前缀静态变量）
- 提交信息: `feat: xxx` / `fix: xxx` / `docs: xxx`

## 🔌 添加一个新工具

1. 在 `pal_server/tools.py` 写实现函数 + schema：

```python
async def tool_my_tool(args: dict) -> dict:
    return {"ok": True}

TOOLS["my_tool"] = {
    "fn": tool_my_tool,
    "description": "我的工具",
    "schema": {
        "type": "function",
        "function": {
            "name": "my_tool",
            "description": "我的工具",
            "parameters": {"type": "object", "properties": {}, "required": []},
        },
    },
}
```

2. （可选）在 `toy_try_tool` 里加关键词路由，让无 Key demo 也能触发
3. 加测试到 `server/tests/`
4. 提 PR 🎉

## 🧪 测试

```bash
cd server && pytest -q
```

CI 会在 PR 上跑服务端测试 + ESP-IDF 固件编译。

## 📜 License

MIT — 贡献即表示同意以 MIT 协议发布你的代码。
