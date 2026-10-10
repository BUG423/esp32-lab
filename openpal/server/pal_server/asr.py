"""ASR adapters.

Providers
---------
- openai      : Whisper-compatible HTTP API (OpenAI, Groq, local whisper-server)
- whisper_local : optional faster-whisper if installed (fully offline)
- disabled    : text-only mode (browser typing / device text frame)
"""

from __future__ import annotations

import io
import logging
import tempfile
import wave
from pathlib import Path

import httpx

from .config import settings

log = logging.getLogger("pal.asr")


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
        return _transcribe_local(pcm, sample_rate)

    return await _transcribe_openai(pcm, sample_rate)


async def _transcribe_openai(pcm: bytes, sample_rate: int) -> str:
    wav = pcm16_to_wav(pcm, sample_rate)
    url = settings.asr_base_url.rstrip("/") + "/audio/transcriptions"
    headers = {}
    if settings.asr_api_key:
        headers["Authorization"] = f"Bearer {settings.asr_api_key}"

    try:
        async with httpx.AsyncClient(timeout=60) as client:
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
