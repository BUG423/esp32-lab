"""TTS adapters — edge-tts (free), OpenAI TTS, or silent.

Returns PCM16 mono bytes at `settings.sample_rate_out`.
"""

from __future__ import annotations

import asyncio
import io
import logging
import struct
import wave

import httpx

from .config import settings

log = logging.getLogger("pal.tts")


async def synthesize(text: str, rate: str | None = None) -> bytes:
    if not text:
        return b""
    if settings.tts_provider == "silent":
        return b""
    if settings.tts_provider == "openai":
        return await _tts_openai(text)
    return await _tts_edge(text, rate or settings.tts_rate)


# ---------------------------------------------------------------------------
# edge-tts (free, no key). Streams MP3 — we decode via a tiny built-in path
# only if needed; for the wire we actually send MP3 and let clients handle it,
# BUT ESP32 is happier with PCM. So we request raw PCM via edge's audio format
# when possible, otherwise convert with a minimal WAV passthrough.
#
# edge-tts gives us MP3. To keep the firmware simple we convert MP3 -> PCM16
# using `ffmpeg` if available, else we fall back to a short silence so the
# conversation loop still works in silent environments.
# ---------------------------------------------------------------------------

async def _tts_edge(text: str, rate: str) -> bytes:
    try:
        import edge_tts
    except ImportError:
        log.warning("edge-tts not installed")
        return b""

    communicate = edge_tts.Communicate(
        text,
        voice=settings.tts_voice,
        rate=rate or settings.tts_rate,
        volume=settings.tts_volume,
    )

    chunks: list[bytes] = []
    try:
        async for chunk in communicate.stream():
            if chunk["type"] == "audio":
                chunks.append(chunk["data"])
    except Exception as e:
        log.warning("edge-tts failed: %s", e)
        return b""

    mp3 = b"".join(chunks)
    if not mp3:
        return b""
    return await _mp3_to_pcm16(mp3, settings.sample_rate_out)


async def _tts_openai(text: str) -> bytes:
    url = settings.tts_base_url.rstrip("/") + "/audio/speech"
    headers = {"Authorization": f"Bearer {settings.tts_api_key}"}
    payload = {
        "model": settings.tts_model,
        "voice": "alloy",
        "input": text,
        "response_format": "pcm",
    }
    try:
        async with httpx.AsyncClient(timeout=60) as client:
            resp = await client.post(url, headers=headers, json=payload)
            resp.raise_for_status()
            pcm = resp.content
            # OpenAI PCM is 24 kHz 16-bit mono already
            return pcm
    except Exception as e:
        log.warning("OpenAI TTS failed: %s", e)
        return b""


async def _mp3_to_pcm16(mp3: bytes, rate: int) -> bytes:
    """Convert MP3 to PCM16 using ffmpeg if present; else return silence."""
    import shutil
    import subprocess

    if not shutil.which("ffmpeg"):
        log.warning("ffmpeg not found; sending silence instead of voice")
        return _silence(rate, 0.4)

    proc = await asyncio.create_subprocess_exec(
        "ffmpeg", "-hide_banner", "-loglevel", "error",
        "-i", "pipe:0",
        "-f", "s16le", "-acodec", "pcm_s16le",
        "-ac", "1", "-ar", str(rate),
        "pipe:1",
        stdin=asyncio.subprocess.PIPE,
        stdout=asyncio.subprocess.PIPE,
        stderr=asyncio.subprocess.PIPE,
    )
    out, _err = await proc.communicate(mp3)
    return out or _silence(rate, 0.3)


def _silence(rate: int, seconds: float) -> bytes:
    n = int(rate * seconds)
    return b"\x00\x00" * n


def pcm_duration_ms(pcm: bytes, rate: int) -> int:
    return int(len(pcm) / 2 / rate * 1000)
