"""OpenPal server — FastAPI app and protocol-v2 voice WebSocket.

The WebSocket deliberately separates small JSON control messages from raw
PCM16 binary frames. Each connection owns its conversation, mood, recording
buffer, and active turn so one device cannot hear or interrupt another.
"""

from __future__ import annotations

import asyncio
import contextlib
import json
import logging
import time
import uuid
from collections import deque
from dataclasses import dataclass, field
from typing import Any, Coroutine

from fastapi import FastAPI, WebSocket, WebSocketDisconnect
from fastapi.responses import FileResponse, JSONResponse
from fastapi.staticfiles import StaticFiles
from pydantic import BaseModel, Field

from . import asr, llm, tools, tts
from .config import settings
from .personality import PersonalityEngine

logging.basicConfig(level=logging.INFO, format="%(asctime)s %(name)s %(levelname)s %(message)s")
log = logging.getLogger("pal.server")

app = FastAPI(title="OpenPal", version="0.2.0")

# Aggregate history is optional audit data. It is never fed to another
# client's LLM context, and its API is disabled by default.
history: deque[dict[str, Any]] = deque(maxlen=200)


@dataclass(eq=False)
class ClientSession:
    websocket: WebSocket | None = None
    session_id: str = field(default_factory=lambda: uuid.uuid4().hex[:12])
    persona: PersonalityEngine = field(
        default_factory=lambda: PersonalityEngine(settings.persona_name, settings.persona_style)
    )
    conversation: deque[dict[str, str]] = field(
        default_factory=lambda: deque(maxlen=settings.history_turns * 2)
    )
    pcm: bytearray = field(default_factory=bytearray)
    sample_rate: int = settings.sample_rate_in
    utterance_open: bool = False
    discarding_audio: bool = False
    active_task: asyncio.Task[Any] | None = None
    send_lock: asyncio.Lock = field(default_factory=asyncio.Lock)


clients: dict[str, ClientSession] = {}
http_session = ClientSession(session_id="http")
http_lock = asyncio.Lock()


def _json(obj: dict[str, Any]) -> str:
    return json.dumps(obj, ensure_ascii=False, separators=(",", ":"))


def push_history(session: ClientSession, role: str, text: str) -> None:
    item = {"role": role, "content": text}
    session.conversation.append(item)
    history.append({
        "session_id": session.session_id,
        "role": role,
        "text": text,
        "ts": time.time(),
    })


async def send_json(session: ClientSession, obj: dict[str, Any]) -> bool:
    if session.websocket is None:
        return False
    try:
        async with session.send_lock:
            await session.websocket.send_text(_json(obj))
        return True
    except Exception:
        return False


async def send_status(session: ClientSession, state: str) -> None:
    session.persona.set_state(state)
    await send_json(session, {"type": "status", "state": state, **session.persona.device_payload()})


async def send_error(
    session: ClientSession,
    code: str,
    message: str,
    *,
    recoverable: bool = True,
) -> None:
    await send_json(session, {
        "type": "error",
        "code": code,
        "message": message,
        "recoverable": recoverable,
    })


async def send_to_session(session_id: str, obj: dict[str, Any]) -> bool:
    session = clients.get(session_id)
    return await send_json(session, obj) if session else False


async def _send_reply_audio(
    session: ClientSession,
    reply: str,
    pcm: bytes,
    turn_id: str,
) -> None:
    """Send one uninterrupted reply envelope followed by binary PCM chunks."""
    if session.websocket is None:
        return
    start = {
        "type": "reply_start",
        "turn_id": turn_id,
        "text": reply,
        "audio_format": "pcm_s16le",
        "sample_rate": tts.output_sample_rate(),
        "audio_bytes": len(pcm),
        "emotion": session.persona.device_payload(),
    }
    try:
        async with session.send_lock:
            await session.websocket.send_text(_json(start))
            for offset in range(0, len(pcm), settings.audio_chunk_bytes):
                await session.websocket.send_bytes(pcm[offset:offset + settings.audio_chunk_bytes])
                # Give the receive loop a chance to process barge-in.
                await asyncio.sleep(0)
            await session.websocket.send_text(_json({
                "type": "reply_end",
                "turn_id": turn_id,
                "audio_bytes": len(pcm),
            }))
    except asyncio.CancelledError:
        raise
    except Exception as exc:
        log.info("reply stream failed for %s: %s", session.session_id, exc)


async def run_turn(
    session: ClientSession,
    user_text: str,
    *,
    synthesize_audio: bool = True,
) -> dict[str, Any]:
    """Run a turn for exactly one client session."""
    user_text = user_text.strip()
    if not user_text:
        return {}

    push_history(session, "user", user_text)
    session.persona.observe_user(user_text)
    await send_json(session, {"type": "transcript", "role": "user", "text": user_text})
    await send_json(session, {"type": "emotion", **session.persona.device_payload()})
    await send_status(session, "thinking")

    with tools.session_scope(session.session_id):
        reply = await llm.chat(list(session.conversation), session.persona.system_prompt())
    if not reply:
        reply = "我刚才没组织好语言，可以再说一次吗？"

    push_history(session, "assistant", reply)
    session.persona.observe_assistant(reply)
    hints = session.persona.tts_hints()
    await send_status(session, "speaking")
    await send_json(session, {"type": "transcript", "role": "assistant", "text": reply})

    pcm = await tts.synthesize(reply, rate=hints.get("rate")) if synthesize_audio else b""
    turn_id = uuid.uuid4().hex[:12]
    if session.websocket is not None:
        await _send_reply_audio(session, reply, pcm, turn_id)
        # With audio, the client reports playback_done. With no audio there is
        # nothing to wait for, so immediately return to listening.
        if not pcm:
            await send_status(session, "listening")
    else:
        session.persona.set_state("listening")

    return {
        "text": reply,
        "emotion": session.persona.device_payload(),
        "audio_bytes": len(pcm),
        "sample_rate": tts.output_sample_rate(),
        "turn_id": turn_id,
    }


async def _cancel_active(session: ClientSession) -> None:
    task = session.active_task
    if task is None or task.done() or task is asyncio.current_task():
        return
    task.cancel()
    with contextlib.suppress(asyncio.CancelledError):
        await task
    session.active_task = None


def _start_task(session: ClientSession, coro: Coroutine[Any, Any, Any]) -> None:
    task = asyncio.create_task(coro)
    session.active_task = task

    def done(completed: asyncio.Task[Any]) -> None:
        if session.active_task is completed:
            session.active_task = None
        if completed.cancelled():
            return
        exc = completed.exception()
        if exc:
            log.error(
                "session %s task failed",
                session.session_id,
                exc_info=(type(exc), exc, exc.__traceback__),
            )

    task.add_done_callback(done)


async def _process_audio(session: ClientSession, pcm: bytes, sample_rate: int) -> None:
    await send_status(session, "transcribing")
    text = await asr.transcribe(pcm, sample_rate)
    if not text:
        await send_error(session, "asr_empty", "没有识别到有效语音，请靠近麦克风再试一次。")
        await send_status(session, "listening")
        return
    await run_turn(session, text)


# ---------------------------------------------------------------------------
# HTTP routes
# ---------------------------------------------------------------------------

@app.get("/api/status")
async def api_status() -> JSONResponse:
    asr_ready, asr_detail = asr.capability()
    tts_ready, tts_detail = tts.capability()
    return JSONResponse({
        "name": settings.persona_name,
        "protocol": 2,
        "llm_ready": settings.llm_ready,
        "llm_model": settings.llm_model if settings.llm_ready else "toy-brain (demo)",
        "asr_provider": settings.asr_provider,
        "asr_ready": asr_ready,
        "asr_detail": asr_detail,
        "tts_provider": settings.tts_provider,
        "tts_ready": tts_ready,
        "tts_detail": tts_detail,
        "emotion": http_session.persona.device_payload(),
        "clients": len(clients),
        "history_len": len(history),
    })


@app.get("/api/history")
async def api_history() -> JSONResponse:
    if not settings.expose_history:
        return JSONResponse(
            {"error": "history API is disabled; set PAL_EXPOSE_HISTORY=true to enable it"},
            status_code=403,
        )
    return JSONResponse({"items": list(history)})


class TextIn(BaseModel):
    text: str = Field(min_length=1, max_length=4000)


@app.post("/api/text")
async def api_text(body: TextIn) -> JSONResponse:
    # Keep the shared HTTP demo coherent and avoid overlapping mutations.
    async with http_lock:
        result = await run_turn(http_session, body.text, synthesize_audio=False)
    return JSONResponse({"ok": True, "reply": result.get("text", ""), **result})


@app.get("/")
async def index() -> FileResponse:
    return FileResponse(settings.static_dir / "index.html")


# ---------------------------------------------------------------------------
# WebSocket protocol v2
# ---------------------------------------------------------------------------

@app.websocket("/ws")
async def ws_endpoint(ws: WebSocket) -> None:
    await ws.accept()
    if len(clients) >= settings.max_clients:
        await ws.send_text(_json({"type": "error", "code": "server_busy", "recoverable": True}))
        await ws.close(code=1013, reason="server busy")
        return

    session = ClientSession(websocket=ws)
    clients[session.session_id] = session
    asr_ready, asr_detail = asr.capability()
    tts_ready, tts_detail = tts.capability()
    log.info("client %s connected (%d)", session.session_id, len(clients))
    await send_json(session, {
        "type": "hello",
        "protocol": 2,
        "session_id": session.session_id,
        "name": settings.persona_name,
        "audio_up": {"format": "pcm_s16le", "sample_rate": settings.sample_rate_in},
        "audio_down": {"format": "pcm_s16le", "sample_rate": tts.output_sample_rate()},
        "max_utterance_ms": settings.max_utterance_ms,
        "emotion": session.persona.device_payload(),
        "asr_ready": asr_ready,
        "asr_detail": asr_detail,
        "llm_ready": settings.llm_ready,
        "tts_ready": tts_ready,
        "tts_detail": tts_detail,
    })
    await send_status(session, "listening")

    try:
        while True:
            msg = await ws.receive()
            if msg.get("type") == "websocket.disconnect":
                break

            chunk = msg.get("bytes")
            if chunk is not None:
                if not session.utterance_open:
                    if session.discarding_audio:
                        continue
                    await send_error(session, "audio_not_started", "请先发送 audio_start。")
                    continue
                limit = session.sample_rate * 2 * settings.max_utterance_ms // 1000
                if len(session.pcm) + len(chunk) > limit:
                    session.pcm.clear()
                    session.utterance_open = False
                    session.discarding_audio = True
                    await send_error(session, "audio_too_long", "录音超过时长限制，已丢弃。")
                    await send_status(session, "listening")
                    continue
                session.pcm.extend(chunk)
                continue

            raw = msg.get("text")
            if not raw:
                continue
            if len(raw) > 8192:
                await send_error(session, "control_too_large", "JSON 控制消息不能超过 8192 字符。")
                continue
            try:
                data = json.loads(raw)
            except json.JSONDecodeError:
                await send_error(session, "bad_json", "控制消息必须是 JSON。")
                continue
            if not isinstance(data, dict):
                await send_error(session, "bad_json", "控制消息必须是 JSON object。")
                continue

            mtype = data.get("type")
            if mtype == "audio_start":
                await _cancel_active(session)
                sample_rate = data.get("sample_rate", settings.sample_rate_in)
                audio_format = data.get("format", "pcm_s16le")
                if audio_format != "pcm_s16le" or not isinstance(sample_rate, int) or not 8000 <= sample_rate <= 48000:
                    await send_error(session, "unsupported_audio", "仅支持 8–48 kHz 单声道 PCM16LE。")
                    continue
                session.pcm.clear()
                session.sample_rate = sample_rate
                session.utterance_open = True
                session.discarding_audio = False
                await send_status(session, "listening")

            elif mtype == "audio_end":
                if session.discarding_audio:
                    session.discarding_audio = False
                    continue
                if not session.utterance_open:
                    await send_error(session, "audio_not_started", "当前没有正在录制的语音。")
                    continue
                pcm = bytes(session.pcm)
                session.pcm.clear()
                session.utterance_open = False
                duration_ms, rms = asr.pcm16_metrics(pcm, session.sample_rate)
                if not asr.capability()[0]:
                    await send_error(session, "asr_unavailable", "服务端未配置语音识别；仍可使用文字输入。")
                elif duration_ms < settings.min_utterance_ms:
                    await send_error(session, "audio_too_short", "录音太短，请按住说完再松开。")
                elif rms < settings.vad_energy_threshold:
                    await send_error(session, "audio_too_quiet", "录音音量过低，请检查麦克风接线。")
                else:
                    _start_task(session, _process_audio(session, pcm, session.sample_rate))

            elif mtype == "text" and isinstance(data.get("text"), str):
                text = data["text"].strip()
                if len(text) > 4000:
                    await send_error(session, "text_too_large", "文字输入不能超过 4000 字符。")
                elif text:
                    await _cancel_active(session)
                    session.pcm.clear()
                    session.utterance_open = False
                    session.discarding_audio = False
                    _start_task(session, run_turn(session, text))

            elif mtype == "interrupt":
                await _cancel_active(session)
                session.pcm.clear()
                session.utterance_open = False
                session.discarding_audio = False
                await send_json(session, {"type": "interrupted"})
                await send_status(session, "listening")

            elif mtype == "playback_done":
                await send_status(session, "listening")

            elif mtype == "recalibrate_mood":
                session.persona.reset()
                session.persona.set_state("listening")
                await send_json(session, {"type": "emotion", **session.persona.device_payload()})

            elif mtype == "ping":
                await send_json(session, {"type": "pong", "ts": time.time()})

            elif mtype in ("hello", "pong"):
                continue

            else:
                await send_error(session, "unknown_message", f"不支持的消息类型：{mtype!r}")

    except WebSocketDisconnect:
        pass
    finally:
        await _cancel_active(session)
        clients.pop(session.session_id, None)
        tools.drop_session(session.session_id)
        log.info("client %s disconnected (%d)", session.session_id, len(clients))


if settings.static_dir.exists():
    app.mount("/static", StaticFiles(directory=settings.static_dir), name="static")


def main() -> None:
    import uvicorn

    log.info("OpenPal server starting on http://%s:%d", settings.host, settings.port)
    log.info(
        "LLM: %s | ASR: %s | TTS: %s",
        "ready" if settings.llm_ready else "toy-brain demo",
        settings.asr_provider,
        settings.tts_provider,
    )
    uvicorn.run(app, host=settings.host, port=settings.port, log_level="info")


if __name__ == "__main__":
    main()
