"""OpenPal server configuration.

Everything is env-driven so a single `docker run -e ...` or `.env` works.
No cloud account is required — LLM/ASR can point at local endpoints.
"""

from __future__ import annotations

from pathlib import Path
from typing import Literal

from pydantic import Field
from pydantic_settings import BaseSettings, SettingsConfigDict


class Settings(BaseSettings):
    model_config = SettingsConfigDict(
        env_file=".env", env_prefix="PAL_", extra="ignore"
    )

    # ---- server ----
    host: str = "0.0.0.0"
    port: int = 8080
    static_dir: Path = Path(__file__).resolve().parent.parent / "static"

    # ---- LLM (any OpenAI-compatible endpoint) ----
    llm_base_url: str = "https://api.deepseek.com/v1"
    llm_api_key: str = ""                 # empty => toy-brain fallback (demo mode)
    llm_model: str = "deepseek-chat"
    llm_temperature: float = Field(default=0.7, ge=0.0, le=2.0)
    llm_max_tokens: int = Field(default=512, ge=32, le=4096)
    llm_timeout_seconds: float = Field(default=60.0, ge=1.0, le=300.0)

    # ---- ASR ----
    asr_provider: Literal["openai", "whisper_local", "disabled"] = "disabled"
    asr_base_url: str = "https://api.openai.com/v1"
    asr_api_key: str = ""
    asr_model: str = "whisper-1"
    asr_language: str = "zh"

    # ---- TTS ----
    tts_provider: Literal["edge", "openai", "silent"] = "edge"
    tts_base_url: str = "https://api.openai.com/v1"
    tts_api_key: str = ""
    tts_model: str = "tts-1"
    tts_openai_voice: str = "alloy"
    tts_voice: str = "zh-CN-XiaoxiaoNeural"   # edge-tts voice
    tts_rate: str = "+0%"
    tts_volume: str = "+0%"
    sample_rate_out: int = Field(default=24000, ge=8000, le=48000)

    # ---- personality ----
    persona_name: str = "小帕"
    persona_style: str = (
        "你是一个温暖、有趣、略带俏皮的桌面 AI 伙伴。"
        "回答简短自然，像朋友聊天，不要说教。"
        "偶尔用一点可爱的语气词，但不要过度。"
    )
    history_turns: int = Field(default=12, ge=2, le=100)

    # ---- audio / device ----
    sample_rate_in: int = Field(default=16000, ge=8000, le=48000)
    vad_energy_threshold: float = Field(default=0.012, ge=0.0, le=1.0)
    min_utterance_ms: int = Field(default=250, ge=100, le=5000)
    max_utterance_ms: int = Field(default=15000, ge=1000, le=120000)
    audio_chunk_bytes: int = Field(default=4096, ge=512, le=32768)
    max_clients: int = Field(default=16, ge=1, le=256)
    expose_history: bool = False

    @property
    def llm_ready(self) -> bool:
        return bool(self.llm_api_key)

    @property
    def asr_ready(self) -> bool:
        return self.asr_provider != "disabled" and bool(self.asr_api_key or self.asr_provider == "whisper_local")


settings = Settings()
