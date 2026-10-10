"""OpenPal server — FastAPI app + WebSocket voice protocol.

Endpoints
---------
GET  /                 web console (dashboard + browser voice mode)
GET  /api/status       JSON status / config (no secrets)
GET  /api/history      recent conversation
POST /api/text         text chat (no audio needed)
WS   /ws               device / browser voice channel
"""

from __future__ import annotations

import asyncio
import base64
import json
import logging
import time
from collections import deque
from pathlib import Path
from typing import Any

from fastapi import FastAPI, WebSocket, WebSocketDisconnect
from fastapi.responses import FileResponse, JSONResponse
from fastapi.staticfiles import StaticFiles
from pydantic import BaseModel

from . import asr, llm, tts
from .config import settings
from .personality import PersonalityEngine

logging.basicConfig(level=logging.INFO, format="%(asctime)s %(name)s %(levelname)s %(message)s")
log = logging.getLogger("pal.server")

app = FastAPI(title="OpenPal", version="0.1.0")

persona = PersonalityEngine(settings.persona_name, settings.persona_style)

# conversation history (shared, simple)
history: deque[dict] = deque(maxlen=200)
# connected clients
clients: set[WebSocket] = set()


# ---------------------------------------------------------------------------
# helpers
# ---------------------------------------------------------------------------

async def ws_send(ws: WebSocket, obj: dict) -> None:
    try:
        await ws.send_text(json.dumps(obj, ensure_ascii=False))
    except Exception:
        pass


async def broadcast(obj: dict) -> None:
    dead = []
    for ws in list(clients):
        try:
            await ws.send_text(json.dumps(obj, ensure_ascii=False))
        except Exception:
            dead.append(ws)
    for ws in dead:
        clients.discard(ws)


def push_history(role: str, text: str) -> None:
    history.append({"role": role, "text": text, "ts": time.time()})


def llm_messages() -> list[dict]:
    msgs = []
    for h in list(history)[-settings.history_turns:]:
        msgs.append({"role": h["role"], "content": h["text"]})
    return msgs


async def run_turn(user_text: str, ws: WebSocket | None = None) -> dict:
    """Full pipeline: user text -> LLM -> TTS -> payload for clients."""
    user_text = user_text.strip()
    if not user_text:
        return {}

    push_history("user", user_text)
    persona.observe_user(user_text)
    await broadcast({"type": "transcript", "role": "user", "text": user_text})
    await broadcast({"type": "emotion", **persona.device_payload()})

    # thinking
    persona.set_state("thinking")
    await broadcast({"type": "status", "state": "thinking", **persona.device_payload()})

    system = persona.system_prompt()
    reply = await llm.chat(llm_messages(), system)

    push_history("assistant", reply)
    persona.observe_assistant(reply)

    # speaking
    persona.set_state("speaking")
    hints = persona.tts_hints()
    await broadcast({"type": "status", "state": "speaking", **persona.device_payload()})
    await broadcast({"type": "transcript", "role": "assistant", "text": reply})

    pcm = await tts.synthesize(reply, rate=hints.get("rate"))
    payload = {
        "type": "reply",
        "text": reply,
        "emotion": persona.device_payload(),
        "tts_rate": hints.get("rate"),
        "audio_b64": base64.b64encode(pcm).decode("ascii") if pcm else "",
        "audio_format": "pcm16",
        "sample_rate": settings.sample_rate_out,
    }
    await broadcast(payload)

    persona.set_state("listening")
    await broadcast({"type": "status", "state": "listening", **persona.device_payload()})
    return payload


# ---------------------------------------------------------------------------
# HTTP routes
# ---------------------------------------------------------------------------

@app.get("/api/status")
async def api_status() -> JSONResponse:
    return JSONResponse({
        "name": settings.persona_name,
        "llm_ready": settings.llm_ready,
        "llm_model": settings.llm_model if settings.llm_ready else "toy-brain (demo)",
        "asr_provider": settings.asr_provider,
        "tts_provider": settings.tts_provider,
        "emotion": persona.device_payload(),
        "clients": len(clients),
        "history_len": len(history),
    })


@app.get("/api/history")
async def api_history() -> JSONResponse:
    return JSONResponse({"items": list(history)})


class TextIn(BaseModel):
    text: str


@app.post("/api/text")
async def api_text(body: TextIn) -> JSONResponse:
    result = await run_turn(body.text)
    return JSONResponse({"ok": True, "reply": result.get("text", ""), "emotion": result.get("emotion", {})})


@app.get("/")
async def index() -> FileResponse:
    return FileResponse(settings.static_dir / "index.html")


# ---------------------------------------------------------------------------
# WebSocket
# ---------------------------------------------------------------------------

@app.websocket("/ws")
async def ws_endpoint(ws: WebSocket) -> None:
    await ws.accept()
    clients.add(ws)
    log.info("client connected (%d)", len(clients))
    await ws_send(ws, {
        "type": "hello",
        "name": settings.persona_name,
        "sample_rate_in": settings.sample_rate_in,
        "sample_rate_out": settings.sample_rate_out,
        "emotion": persona.device_payload(),
        "asr_ready": settings.asr_ready,
        "llm_ready": settings.llm_ready,
        "tts_provider": settings.tts_provider,
    })
    await ws_send(ws, {"type": "status", "state": "listening", **persona.device_payload()})

    # audio accumulation for one utterance
    pcm_buf = bytearray()
    last_audio_ts = time.time()
    utterance_open = False

    try:
        while True:
            msg = await ws.receive()

            if msg.get("type") == "websocket.disconnect":
                break

            # ---- binary frame: raw PCM16 ----
            if "bytes" in msg and msg["bytes"] is not None:
                pcm_buf.extend(msg["bytes"])
                last_audio_ts = time.time()
                utterance_open = True
                continue

            text = msg.get("text")
            if not text:
                continue

            # ---- text frame: JSON control ----
            try:
                data = json.loads(text)
            except json.JSONDecodeError:
                continue

            mtype = data.get("type")

            if mtype == "audio_chunk":
                chunk = base64.b64decode(data.get("data", ""))
                pcm_buf.extend(chunk)
                last_audio_ts = time.time()
                utterance_open = True

            elif mtype == "audio_end" or (mtype == "text" and data.get("text")):
                if mtype == "text":
                    user_text = data["text"]
                    pcm_buf.clear()
                    utterance_open = False
                else:
                    user_text = await asr.transcribe(bytes(pcm_buf), settings.sample_rate_in)
                    pcm_buf.clear()
                    utterance_open = False
                    if not user_text:
                        await ws_send(ws, {"type": "transcript", "role": "user", "text": "(未识别到语音)"})
                        continue
                await run_turn(user_text, ws)

            elif mtype == "interrupt":
                pcm_buf.clear()
                utterance_open = False
                await broadcast({"type": "interrupted"})
                await broadcast({"type": "status", "state": "listening", **persona.device_payload()})

            elif mtype == "ping":
                await ws_send(ws, {"type": "pong", "ts": time.time()})

            elif mtype == "recalibrate_mood":
                persona.mood.emotion = "neutral"
                await broadcast({"type": "emotion", **persona.device_payload()})

            # end of stream from device after playback
            elif mtype == "playback_done":
                await broadcast({"type": "status", "state": "listening", **persona.device_payload()})

    except WebSocketDisconnect:
        pass
    finally:
        clients.discard(ws)
        log.info("client disconnected (%d)", len(clients))


# ---------------------------------------------------------------------------
# static
# ---------------------------------------------------------------------------

if settings.static_dir.exists():
    app.mount("/static", StaticFiles(directory=settings.static_dir), name="static")


def main() -> None:
    import uvicorn
    log.info("OpenPal server starting on http://%s:%d", settings.host, settings.port)
    log.info("LLM: %s | ASR: %s | TTS: %s",
             "ready" if settings.llm_ready else "toy-brain demo",
             settings.asr_provider, settings.tts_provider)
    uvicorn.run(app, host=settings.host, port=settings.port, log_level="info")


if __name__ == "__main__":
    main()
