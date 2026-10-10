"""Tool registry — OpenPal's lightweight MCP-style function calling.

Each tool exposes a JSON schema so OpenAI-compatible LLMs can call it via
`tools=[...]`. The toy-brain fallback can also "call" a few tools by keyword
so the demo stays alive without an API key.
"""

from __future__ import annotations

import ast
import datetime as dt
import logging
import operator
import random
from typing import Any, Awaitable, Callable

import httpx

log = logging.getLogger("pal.tools")

# ---------------------------------------------------------------------------
# tool implementations
# ---------------------------------------------------------------------------

_NOTES: list[str] = []


async def tool_get_time(args: dict) -> dict:
    now = dt.datetime.now()
    return {
        "now": now.strftime("%Y-%m-%d %H:%M:%S"),
        "weekday": ["周一", "周二", "周三", "周四", "周五", "周六", "周日"][now.weekday()],
        "unix": int(now.timestamp()),
    }


async def tool_get_joke(args: dict) -> dict:
    jokes = [
        "为什么程序员总分不清万圣节和圣诞节？因为 Oct 31 == Dec 25。",
        "Bug 是什么？就是代码里隐藏的功能。",
        "我写的代码没有 bug，只有未文档化的特性。",
        "键盘上最贵的键是回车——因为一按下去就上线了。",
        "为什么 WiFi 信号不好？因为它在找自己。",
    ]
    return {"joke": random.choice(jokes)}


async def tool_get_weather(args: dict) -> dict:
    """Use wttr.in (no API key). Falls back gracefully."""
    city = (args.get("city") or "Beijing").strip()
    try:
        async with httpx.AsyncClient(timeout=8) as client:
            r = await client.get(
                f"https://wttr.in/{city}",
                params={"format": "j1"},
                headers={"User-Agent": "OpenPal"},
            )
            r.raise_for_status()
            data = r.json()
            cur = data.get("current_condition", [{}])[0]
            area = data.get("nearest_area", [{}])[0]
            return {
                "city": area.get("areaName", [{}])[0].get("value", city),
                "temp_c": cur.get("temp_C"),
                "feels_like_c": cur.get("FeelsLikeC"),
                "desc": (cur.get("weatherDesc") or [{}])[0].get("value", ""),
                "humidity": cur.get("humidity"),
                "wind_kmph": cur.get("windspeedKmph"),
            }
    except Exception as e:
        log.warning("weather failed: %s", e)
        return {"error": f"天气查询失败: {e}", "city": city}


async def tool_control_led(args: dict) -> dict:
    """Broadcast a LED command to connected devices (ESP32 / browser)."""
    from . import main as main_mod

    color = str(args.get("color") or "#2ee6a8")
    on = bool(args.get("on", True))
    payload = {
        "type": "device_cmd",
        "cmd": "led",
        "on": on,
        "color": color,
    }
    await main_mod.broadcast(payload)
    return {"ok": True, "led_on": on, "color": color}


async def tool_remember(args: dict) -> dict:
    note = str(args.get("note") or "").strip()
    if not note:
        return {"ok": False, "error": "空笔记"}
    _NOTES.append(note)
    return {"ok": True, "saved": note, "total": len(_NOTES)}


async def tool_recall(args: dict) -> dict:
    return {"notes": _NOTES[-10:], "total": len(_NOTES)}


# safe arithmetic for the compute tool
_OPS = {
    ast.Add: operator.add,
    ast.Sub: operator.sub,
    ast.Mult: operator.mul,
    ast.Div: operator.truediv,
    ast.Pow: operator.pow,
    ast.USub: operator.neg,
}


def _eval_node(node: ast.AST) -> float:
    if isinstance(node, ast.Expression):
        return _eval_node(node.body)
    if isinstance(node, ast.Constant) and isinstance(node.value, (int, float)):
        return node.value
    if isinstance(node, ast.BinOp) and type(node.op) in _OPS:
        return _OPS[type(node.op)](_eval_node(node.left), _eval_node(node.right))
    if isinstance(node, ast.UnaryOp) and type(node.op) in _OPS:
        return _OPS[type(node.op)](_eval_node(node.operand))
    raise ValueError("unsupported expression")


async def tool_compute(args: dict) -> dict:
    expr = str(args.get("expression") or "").strip()
    try:
        tree = ast.parse(expr, mode="eval")
        value = _eval_node(tree)
        return {"expression": expr, "result": value}
    except Exception as e:
        return {"error": f"计算失败: {e}", "expression": expr}


# ---------------------------------------------------------------------------
# registry
# ---------------------------------------------------------------------------

ToolFn = Callable[[dict], Awaitable[dict]]

TOOLS: dict[str, dict] = {
    "get_time": {
        "fn": tool_get_time,
        "description": "获取当前日期、时间、星期几",
        "schema": {
            "type": "function",
            "function": {
                "name": "get_time",
                "description": "获取当前日期、时间、星期几",
                "parameters": {"type": "object", "properties": {}, "required": []},
            },
        },
    },
    "get_joke": {
        "fn": tool_get_joke,
        "description": "讲一个简短的中文笑话",
        "schema": {
            "type": "function",
            "function": {
                "name": "get_joke",
                "description": "讲一个简短的中文笑话",
                "parameters": {"type": "object", "properties": {}, "required": []},
            },
        },
    },
    "get_weather": {
        "fn": tool_get_weather,
        "description": "查询指定城市的当前天气（温度、体感、描述、湿度、风速）",
        "schema": {
            "type": "function",
            "function": {
                "name": "get_weather",
                "description": "查询指定城市的当前天气",
                "parameters": {
                    "type": "object",
                    "properties": {
                        "city": {"type": "string", "description": "城市名，如 北京 / Shanghai"},
                    },
                    "required": ["city"],
                },
            },
        },
    },
    "control_led": {
        "fn": tool_control_led,
        "description": "控制设备 LED 灯环的颜色和开关",
        "schema": {
            "type": "function",
            "function": {
                "name": "control_led",
                "description": "控制设备 LED 灯环的颜色和开关",
                "parameters": {
                    "type": "object",
                    "properties": {
                        "on": {"type": "boolean", "description": "开或关"},
                        "color": {"type": "string", "description": "颜色，如 #2ee6a8 或 green"},
                    },
                    "required": [],
                },
            },
        },
    },
    "remember": {
        "fn": tool_remember,
        "description": "记一条笔记供以后回忆",
        "schema": {
            "type": "function",
            "function": {
                "name": "remember",
                "description": "记一条笔记供以后回忆",
                "parameters": {
                    "type": "object",
                    "properties": {"note": {"type": "string", "description": "要记住的内容"}},
                    "required": ["note"],
                },
            },
        },
    },
    "recall": {
        "fn": tool_recall,
        "description": "回忆最近记下的笔记",
        "schema": {
            "type": "function",
            "function": {
                "name": "recall",
                "description": "回忆最近记下的笔记",
                "parameters": {"type": "object", "properties": {}, "required": []},
            },
        },
    },
    "compute": {
        "fn": tool_compute,
        "description": "计算一个数学表达式，如 2+3*4",
        "schema": {
            "type": "function",
            "function": {
                "name": "compute",
                "description": "计算一个数学表达式",
                "parameters": {
                    "type": "object",
                    "properties": {
                        "expression": {"type": "string", "description": "数学表达式"}
                    },
                    "required": ["expression"],
                },
            },
        },
    },
}


def openai_tool_schemas() -> list[dict]:
    return [t["schema"] for t in TOOLS.values()]


async def call_tool(name: str, arguments: dict) -> dict:
    entry = TOOLS.get(name)
    if not entry:
        return {"error": f"unknown tool: {name}"}
    try:
        result = await entry["fn"](arguments or {})
    except Exception as e:
        log.exception("tool %s failed", name)
        result = {"error": str(e)}

    # notify all clients so the dashboard can show tool activity
    try:
        from . import main as main_mod
        await main_mod.broadcast({
            "type": "tool",
            "name": name,
            "args": arguments or {},
            "result": result,
        })
    except Exception:
        pass
    return result


# ---------------------------------------------------------------------------
# toy-brain tool routing (no API key needed)
# ---------------------------------------------------------------------------

async def toy_try_tool(user_text: str) -> dict | None:
    """Return a tool result dict if the text clearly wants a tool, else None."""
    low = user_text.lower()

    if any(k in low for k in ("几点", "时间", "日期", "星期", "礼拜", "what time", "today", "weekday")):
        return {"tool": "get_time", "result": await tool_get_time({})}
    if any(k in low for k in ("笑话", "joke", "讲个")):
        return {"tool": "get_joke", "result": await tool_get_joke({})}
    if any(k in low for k in ("天气", "weather", "气温")):
        # crude city extraction
        city = "Beijing"
        for c in ("北京", "上海", "深圳", "广州", "杭州", "成都", "Tokyo", "London", "New York"):
            if c.lower() in low or c in user_text:
                city = c
                break
        return {"tool": "get_weather", "result": await tool_get_weather({"city": city})}
    if any(k in low for k in ("开灯", "关灯", "灯", "led", "light")):
        on = "关" not in user_text and "off" not in low
        return {"tool": "control_led", "result": await tool_control_led({"on": on, "color": "#2ee6a8"})}
    if "记住" in user_text or "remember" in low:
        note = user_text.replace("记住", "").replace("remember", "").strip(" ：:，,")
        if note:
            return {"tool": "remember", "result": await tool_remember({"note": note})}
    if any(k in low for k in ("记了什么", "笔记", "recall", "notes")):
        return {"tool": "recall", "result": await tool_recall({})}

    # simple arithmetic like "2+3*4" or "算一下 12*13"
    expr = user_text
    for prefix in ("算一下", "计算", "compute", "calc"):
        expr = expr.replace(prefix, "")
    expr = expr.strip(" ？?！!。.")
    if expr and any(ch.isdigit() for ch in expr) and any(op in expr for op in "+-*/"):
        import re
        m = re.fullmatch(r"[\d\s+\-*/().]+", expr)
        if m:
            return {"tool": "compute", "result": await tool_compute({"expression": expr})}

    return None
