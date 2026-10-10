"""OpenPal test suite — run with `pytest -q` (no pytest-asyncio needed)."""

from __future__ import annotations

import asyncio

from pal_server import llm, tools
from pal_server.personality import PersonalityEngine


def run(coro):
    return asyncio.run(coro)


# ---------------------------------------------------------------------------
# personality
# ---------------------------------------------------------------------------

def test_personality_mood_moves_toward_happy():
    p = PersonalityEngine("小帕", "test")
    assert p.mood.emotion == "neutral"
    p.observe_user("今天太开心了！哈哈")
    assert p.mood.emotion == "happy"
    # first EMA step already lifts valence above neutral
    assert p.mood.valence > 0.55
    # and keeps rising with more happy input
    p.observe_user("真的好开心啊！")
    assert p.mood.valence > 0.7


def test_personality_system_prompt_contains_name():
    p = PersonalityEngine("小帕", "测试风格")
    sp = p.system_prompt()
    assert "小帕" in sp
    assert "测试风格" in sp


def test_tts_hints_rate_is_string():
    p = PersonalityEngine("小帕", "x")
    h = p.tts_hints()
    assert isinstance(h["rate"], str) and h["rate"].endswith("%")


# ---------------------------------------------------------------------------
# tools
# ---------------------------------------------------------------------------

def test_tool_get_time():
    r = run(tools.call_tool("get_time", {}))
    assert "now" in r and "weekday" in r


def test_tool_get_joke():
    r = run(tools.call_tool("get_joke", {}))
    assert r.get("joke")


def test_tool_compute():
    r = run(tools.call_tool("compute", {"expression": "2+3*4"}))
    assert r["result"] == 14


def test_tool_compute_rejects_unsafe():
    r = run(tools.call_tool("compute", {"expression": "__import__('os')"}))
    assert "error" in r


def test_tool_remember_recall():
    run(tools.call_tool("remember", {"note": "买牛奶"}))
    r = run(tools.call_tool("recall", {}))
    assert "买牛奶" in r["notes"]


def test_toy_try_tool_time():
    hit = run(tools.toy_try_tool("现在几点了？"))
    assert hit and hit["tool"] == "get_time"


def test_toy_try_tool_compute():
    hit = run(tools.toy_try_tool("12*13"))
    assert hit and hit["tool"] == "compute"
    assert hit["result"]["result"] == 156


# ---------------------------------------------------------------------------
# toy LLM
# ---------------------------------------------------------------------------

def test_toy_llm_greeting():
    reply = run(llm.chat([{"role": "user", "content": "你好"}], "sys"))
    assert "小帕" in reply or "你好" in reply


def test_toy_llm_uses_tool_for_time():
    reply = run(llm.chat([{"role": "user", "content": "今天星期几？"}], "sys"))
    assert "星期" in reply or "周" in reply
