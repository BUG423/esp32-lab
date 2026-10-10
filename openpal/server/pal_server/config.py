"""OpenPal server configuration.

Everything is env-driven so a single `docker run -e ...` or `.env` works.
No cloud account is required — LLM/ASR can point at local endpoints.
"""

from __future__ import annotations

from pathlib import Path
from typing import Literal

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
    llm_temperature: float = 0.7
    llm_max_tokens: int = 512

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
    tts_voice: str = "zh-CN-XiaoxiaoNeural"   # edge-tts voice
    tts_rate: str = "+0%"
    tts_volume: str = "+0%"
    sample_rate_out: int = 24000

    # ---- personality ----
    persona_name: str = "小帕"
    persona_style: str = (
        "你是一个温暖、有趣、略带俏皮的桌面 AI 伙伴。"
        "回答简短自然，像朋友聊天，不要说教。"
        "偶尔用一点可爱的语气词，但不要过度。"
    )
    history_turns: int = 12

    # ---- audio / device ----
    sample_rate_in: int = 16000
    vad_energy_threshold: float = 0.012
    silence_ms: int = 700
    max_utterance_ms: int = 15000

    @property
    def llm_ready(self) -> bool:
        return bool(self.llm_api_key)

    @property
    def asr_ready(self) -> bool:
        return self.asr_provider != "disabled" and bool(self.asr_api_key or self.asr_provider == "whisper_local")


settings = Settings()
