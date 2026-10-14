"""OpenPal test suite — run with `pytest -q` (no pytest-asyncio needed)."""

from __future__ import annotations

import asyncio
import json

from fastapi.testclient import TestClient

from pal_server import asr, llm, tools
from pal_server import main as main_mod
from pal_server.main import ClientSession, run_turn
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


def test_activity_does_not_overwrite_mood():
    p = PersonalityEngine("小帕", "x")
    p.observe_user("今天太开心了")
    p.set_state("thinking")
    assert p.mood.emotion == "happy"
    assert p.device_payload()["activity"] == "thinking"


def test_personality_reset_is_complete():
    p = PersonalityEngine("小帕", "x")
    p.observe_user("太棒了！")
    p.set_state("speaking")
    p.reset()
    assert p.mood.emotion == "neutral"
    assert p.mood.valence == 0.5
    assert p.activity_state == "idle"
    assert p.turn_count == 0


# ---------------------------------------------------------------------------
# audio validation / session isolation
# ---------------------------------------------------------------------------

def test_pcm16_metrics_silence_and_duration():
    duration, rms = asr.pcm16_metrics(b"\x00\x00" * 16000, 16000)
    assert duration == 1000
    assert rms == 0.0


def test_pcm16_metrics_signal():
    sample = (16384).to_bytes(2, "little", signed=True)
    duration, rms = asr.pcm16_metrics(sample * 8000, 16000)
    assert duration == 500
    assert 0.49 < rms < 0.51


def test_sessions_keep_separate_history(monkeypatch):
    async def fake_chat(messages, system):
        return f"echo:{messages[-1]['content']}"

    monkeypatch.setattr(llm, "chat", fake_chat)
    left = ClientSession(session_id="left")
    right = ClientSession(session_id="right")
    run(run_turn(left, "苹果", synthesize_audio=False))
    run(run_turn(right, "香蕉", synthesize_audio=False))

    assert [item["content"] for item in left.conversation] == ["苹果", "echo:苹果"]
    assert [item["content"] for item in right.conversation] == ["香蕉", "echo:香蕉"]


def test_websocket_v2_streams_binary_reply(monkeypatch):
    async def fake_chat(messages, system):
        return "测试回复"

    async def fake_tts(text, rate=None):
        return b"\x01\x00" * 32

    monkeypatch.setattr(main_mod.llm, "chat", fake_chat)
    monkeypatch.setattr(main_mod.tts, "synthesize", fake_tts)

    with TestClient(main_mod.app).websocket_connect("/ws") as websocket:
        hello = websocket.receive_json()
        initial = websocket.receive_json()
        assert hello["protocol"] == 2
        assert initial["state"] == "listening"

        websocket.send_json({"type": "text", "text": "测试"})
        controls = []
        binary = b""
        while not any(item.get("type") == "reply_end" for item in controls):
            message = websocket.receive()
            if message.get("bytes") is not None:
                binary += message["bytes"]
            elif message.get("text"):
                controls.append(json.loads(message["text"]))

        start = next(item for item in controls if item.get("type") == "reply_start")
        assert start["audio_format"] == "pcm_s16le"
        assert start["audio_bytes"] == len(binary) == 64
        assert "audio_b64" not in start


def test_history_endpoint_is_private_by_default():
    response = TestClient(main_mod.app).get("/api/history")
    assert response.status_code == 403


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


def test_tool_compute_rejects_resource_abuse():
    r = run(tools.call_tool("compute", {"expression": "9**999999999"}))
    assert "error" in r


def test_tool_remember_recall():
    run(tools.call_tool("remember", {"note": "买牛奶"}))
    r = run(tools.call_tool("recall", {}))
    assert "买牛奶" in r["notes"]


def test_notes_are_isolated_by_session():
    with tools.session_scope("session-a"):
        run(tools.call_tool("remember", {"note": "A 的秘密"}))
    with tools.session_scope("session-b"):
        result = run(tools.call_tool("recall", {}))
    assert "A 的秘密" not in result["notes"]


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
