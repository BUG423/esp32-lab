"""ASR adapters.

Providers
---------
- openai      : Whisper-compatible HTTP API (OpenAI, Groq, local whisper-server)
- whisper_local : optional faster-whisper if installed (fully offline)
- disabled    : text-only mode (browser typing / device text frame)
"""

from __future__ import annotations

import asyncio
import array
import importlib.util
import io
import logging
import tempfile
import wave
from pathlib import Path

import httpx

from .config import settings

log = logging.getLogger("pal.asr")


def capability() -> tuple[bool, str]:
    """Return configured runtime readiness and a short diagnostic."""
    if settings.asr_provider == "disabled":
        return False, "disabled"
    if settings.asr_provider == "openai":
        return bool(settings.asr_api_key), "openai" if settings.asr_api_key else "API key missing"
    available = importlib.util.find_spec("faster_whisper") is not None
    return available, "whisper_local" if available else "faster-whisper missing"


def pcm16_to_wav(pcm: bytes, sample_rate: int, channels: int = 1) -> bytes:
    buf = io.BytesIO()
    with wave.open(buf, "wb") as w:
        w.setnchannels(channels)
        w.setsampwidth(2)
        w.setframerate(sample_rate)
        w.writeframes(pcm)
    return buf.getvalue()


async def transcribe(pcm: bytes, sample_rate: int) -> str:
    """Transcribe PCM16 mono audio to text."""
    if settings.asr_provider == "disabled":
        return ""
    if not pcm:
        return ""

    if settings.asr_provider == "whisper_local":
        # Model loading and inference are CPU/blocking work. Keep them off the
        # FastAPI event loop so pings, interrupts, and other clients stay live.
        return await asyncio.to_thread(_transcribe_local, pcm, sample_rate)

    return await _transcribe_openai(pcm, sample_rate)


async def _transcribe_openai(pcm: bytes, sample_rate: int) -> str:
    wav = pcm16_to_wav(pcm, sample_rate)
    url = settings.asr_base_url.rstrip("/") + "/audio/transcriptions"
    headers = {}
    if settings.asr_api_key:
        headers["Authorization"] = f"Bearer {settings.asr_api_key}"

    try:
        async with httpx.AsyncClient(timeout=settings.llm_timeout_seconds) as client:
            resp = await client.post(
                url,
                headers=headers,
                data={"model": settings.asr_model, "language": settings.asr_language},
                files={"file": ("audio.wav", wav, "audio/wav")},
            )
            resp.raise_for_status()
            data = resp.json()
            return (data.get("text") or "").strip()
    except Exception as e:
        log.warning("ASR failed: %s", e)
        return ""


def _transcribe_local(pcm: bytes, sample_rate: int) -> str:
    try:
        from faster_whisper import WhisperModel  # type: ignore
    except ImportError:
        log.warning("faster-whisper not installed; pip install faster-whisper")
        return ""

    model = getattr(_transcribe_local, "_model", None)
    if model is None:
        model = WhisperModel("base", device="cpu", compute_type="int8")
        _transcribe_local._model = model

    with tempfile.NamedTemporaryFile(suffix=".wav", delete=False) as f:
        f.write(pcm16_to_wav(pcm, sample_rate))
        path = Path(f.name)

    try:
        segments, _info = model.transcribe(str(path), language=settings.asr_language)
        return " ".join(s.text.strip() for s in segments).strip()
    finally:
        path.unlink(missing_ok=True)


def pcm16_metrics(pcm: bytes, sample_rate: int) -> tuple[int, float]:
    """Return duration in milliseconds and normalized RMS for little-endian PCM16."""
    if not pcm or sample_rate <= 0:
        return 0, 0.0
    usable = pcm[: len(pcm) & ~1]
    samples = array.array("h")
    samples.frombytes(usable)
    if not samples:
        return 0, 0.0
    # ESP32 and browsers send little-endian PCM. byteswap on big-endian hosts.
    import sys
    if sys.byteorder != "little":
        samples.byteswap()
    square_sum = sum(int(sample) * int(sample) for sample in samples)
    rms = (square_sum / len(samples)) ** 0.5 / 32768.0
    duration_ms = int(len(samples) * 1000 / sample_rate)
    return duration_ms, rms
