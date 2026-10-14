"""LLM adapter — OpenAI-compatible chat + tool calling, with a toy-brain fallback.

The fallback is important: OpenPal must demo with **zero API keys**.
"""

from __future__ import annotations

import json
import logging
import random

import httpx

from . import tools as tools_mod
from .config import settings

log = logging.getLogger("pal.llm")


async def chat(messages: list[dict], system: str) -> str:
    if settings.llm_ready:
        return await _chat_openai(messages, system)
    return await _chat_toy(messages, system)


# ---------------------------------------------------------------------------
# OpenAI-compatible with function calling
# ---------------------------------------------------------------------------

async def _chat_openai(messages: list[dict], system: str) -> str:
    url = settings.llm_base_url.rstrip("/") + "/chat/completions"
    headers = {"Authorization": f"Bearer {settings.llm_api_key}"}

    payload = {
        "model": settings.llm_model,
        "temperature": settings.llm_temperature,
        "max_tokens": settings.llm_max_tokens,
        "messages": [{"role": "system", "content": system}, *messages],
        "tools": tools_mod.openai_tool_schemas(),
    }

    try:
        async with httpx.AsyncClient(timeout=settings.llm_timeout_seconds) as client:
            # first pass — maybe the model wants a tool
            resp = await client.post(url, headers=headers, json=payload)
            resp.raise_for_status()
            data = resp.json()
            msg = data["choices"][0]["message"]

            # tool call loop (max 3 rounds)
            for _ in range(3):
                if not msg.get("tool_calls"):
                    return (msg.get("content") or "").strip()

                # execute each tool call, append results
                messages = [*messages, msg]
                for tc in msg["tool_calls"]:
                    fn = tc.get("function") or {}
                    name = fn.get("name", "")
                    try:
                        args = json.loads(fn.get("arguments") or "{}")
                    except json.JSONDecodeError:
                        args = {}
                    result = await tools_mod.call_tool(name, args)
                    messages.append({
                        "role": "tool",
                        "tool_call_id": tc.get("id", name),
                        "name": name,
                        "content": json.dumps(result, ensure_ascii=False),
                    })
                    log.info("tool %s -> %s", name, result)

                payload["messages"] = [{"role": "system", "content": system}, *messages]
                resp = await client.post(url, headers=headers, json=payload)
                resp.raise_for_status()
                data = resp.json()
                msg = data["choices"][0]["message"]

            return (msg.get("content") or "（工具调用轮次过多）").strip()

    except Exception as e:
        log.warning("LLM call failed (%s); falling back to toy brain", e)
        return await _chat_toy(messages, system)


# ---------------------------------------------------------------------------
# Toy brain — keeps the demo alive with no API key
# ---------------------------------------------------------------------------

_TOY_REPLIES = [
    "嗯嗯，我在听呢。",
    "这个想法挺有意思的，再多说一点？",
    "哈哈，我记下来了。",
    "收到！要不要我帮你想想下一步？",
    "嗯……让我琢磨琢磨。",
    "好呀，这个交给我。",
]


async def _chat_toy(messages: list[dict], system: str) -> str:
    """Very small rule-based responder for offline demos (with tools)."""
    last = ""
    for m in reversed(messages):
        if m.get("role") == "user":
            last = (m.get("content") or "").strip()
            break

    # try a tool first
    tool_hit = await tools_mod.toy_try_tool(last)
    if tool_hit:
        name = tool_hit["tool"]
        result = tool_hit["result"]
        return _render_tool_reply(name, result)

    low = last.lower()
    if any(k in low for k in ("你好", "hello", "hi", "嗨", "在吗")):
        return "你好呀！我是小帕，你的桌面搭子～ 今天想聊点什么？"
    if any(k in low for k in ("名字", "你是谁", "who are you")):
        return "我是小帕（OpenPal），一个可以完全跑在你自己电脑上的 AI 语音伙伴。"
    if any(k in low for k in ("你能做什么", "功能", "help", "会什么")):
        return "我能聊天、查时间天气、讲笑话、记笔记、控制灯效。接上真正的 LLM 后会更聪明！"
    if any(k in low for k in ("谢谢", "thank")):
        return "不客气！有事随时叫我～"
    if any(k in low for k in ("再见", "拜拜", "bye")):
        return "拜拜，下次再聊！"
    if last.endswith("？") or last.endswith("?") or any(k in low for k in ("为什么", "怎么", "如何", "what", "why", "how")):
        return "好问题！接上真正的 LLM API 后，我会回答得更靠谱。现在先当我在认真思考中～"
    return random.choice(_TOY_REPLIES)


def _render_tool_reply(name: str, result: dict) -> str:
    if "error" in result:
        return f"哎呀，{result['error']}"

    if name == "get_time":
        return f"现在是 {result['now']}，{result['weekday']}。"
    if name == "get_joke":
        return result.get("joke", "……今天笑话库休假了。")
    if name == "get_weather":
        return (
            f"{result.get('city')} 现在 {result.get('temp_c')}°C（体感 {result.get('feels_like_c')}°C），"
            f"{result.get('desc')}，湿度 {result.get('humidity')}%。"
        )
    if name == "control_led":
        if not result.get("ok"):
            return "当前会话没有可控的灯。"
        return ("灯已经打开啦 💡" if result.get("led_on") else "灯关掉了 🌙") + f" 颜色 {result.get('color')}"
    if name == "remember":
        return f"记住啦：{result.get('saved')}（一共 {result.get('total')} 条）"
    if name == "recall":
        notes = result.get("notes") or []
        if not notes:
            return "我还没记过什么笔记呢。"
        return "最近记下的：\n" + "\n".join(f"· {n}" for n in notes)
    if name == "compute":
        return f"{result.get('expression')} = {result.get('result')}"
    return json.dumps(result, ensure_ascii=False)
