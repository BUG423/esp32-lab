"""TTS adapters — edge-tts (free), OpenAI TTS, or silent.

Returns PCM16 mono bytes at `output_sample_rate()`.
"""

from __future__ import annotations

import asyncio
import logging
import shutil

import httpx

from .config import settings

log = logging.getLogger("pal.tts")


def output_sample_rate() -> int:
    """OpenAI raw PCM is fixed at 24 kHz; Edge is resampled by ffmpeg."""
    return 24000 if settings.tts_provider == "openai" else settings.sample_rate_out


async def synthesize(text: str, rate: str | None = None) -> bytes:
    if not text:
        return b""
    if settings.tts_provider == "silent":
        return b""
    if settings.tts_provider == "openai":
        if not settings.tts_api_key:
            log.warning("OpenAI TTS selected but PAL_TTS_API_KEY is empty")
            return b""
        return await _tts_openai(text)
    return await _tts_edge(text, rate or settings.tts_rate)


# ---------------------------------------------------------------------------
# edge-tts (free, no key) streams MP3. To keep both browser and firmware
# clients simple, ffmpeg converts it to the protocol's PCM16 output format.
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
    return await _mp3_to_pcm16(mp3, output_sample_rate())


async def _tts_openai(text: str) -> bytes:
    url = settings.tts_base_url.rstrip("/") + "/audio/speech"
    headers = {"Authorization": f"Bearer {settings.tts_api_key}"}
    payload = {
        "model": settings.tts_model,
        "voice": settings.tts_openai_voice,
        "input": text,
        "response_format": "pcm",
    }
    try:
        async with httpx.AsyncClient(timeout=settings.llm_timeout_seconds) as client:
            resp = await client.post(url, headers=headers, json=payload)
            resp.raise_for_status()
            pcm = resp.content
            # OpenAI PCM is 24 kHz 16-bit mono already
            return pcm
    except Exception as e:
        log.warning("OpenAI TTS failed: %s", e)
        return b""


async def _mp3_to_pcm16(mp3: bytes, rate: int) -> bytes:
    """Convert MP3 to PCM16 using ffmpeg."""

    if not shutil.which("ffmpeg"):
        log.warning("ffmpeg not found; TTS audio is unavailable")
        return b""

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
    try:
        out, err = await proc.communicate(mp3)
    except asyncio.CancelledError:
        proc.kill()
        await proc.wait()
        raise
    if not out:
        log.warning("ffmpeg produced no PCM: %s", err.decode(errors="replace"))
    return out


def pcm_duration_ms(pcm: bytes, rate: int) -> int:
    return int(len(pcm) / 2 / rate * 1000)


def capability() -> tuple[bool, str]:
    """Report runtime readiness without claiming silent fallback is speech."""
    if settings.tts_provider == "silent":
        return False, "silent"
    if settings.tts_provider == "openai":
        return bool(settings.tts_api_key), "openai"
    try:
        import edge_tts  # noqa: F401
    except ImportError:
        return False, "edge-tts missing"
    if not shutil.which("ffmpeg"):
        return False, "ffmpeg missing"
    return True, "edge"
