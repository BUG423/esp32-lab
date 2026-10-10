"""Emotion / personality engine — one of OpenPal's differentiators.

XiaoZhi is a capable assistant; OpenPal is a *companion*. The mood model
changes:
  - system prompt tone
  - LED colour sent to the device
  - TTS speaking rate / pitch hints
  - emoji shown on device screens
"""

from __future__ import annotations

import random
import time
from dataclasses import dataclass, field


EMOTIONS = {
    "neutral":  {"valence": 0.5, "arousal": 0.4, "color": "#5b8cff", "emoji": "🙂"},
    "happy":    {"valence": 0.9, "arousal": 0.7, "color": "#2ee6a8", "emoji": "😄"},
    "excited":  {"valence": 0.95, "arousal": 0.95, "color": "#ffc857", "emoji": "🤩"},
    "curious":  {"valence": 0.7, "arousal": 0.6, "color": "#9b5bff", "emoji": "🤔"},
    "calm":     {"valence": 0.6, "arousal": 0.2, "color": "#4cc9f0", "emoji": "😌"},
    "sad":      {"valence": 0.2, "arousal": 0.3, "color": "#6c8cff", "emoji": "🥺"},
    "thinking": {"valence": 0.5, "arousal": 0.5, "color": "#b388ff", "emoji": "💭"},
    "listening": {"valence": 0.5, "arousal": 0.5, "color": "#2ee6a8", "emoji": "🎧"},
    "speaking": {"valence": 0.7, "arousal": 0.6, "color": "#5b8cff", "emoji": "🗣️"},
}

# crude keyword hints for Chinese/English
_MOOD_HINTS = {
    "happy": ["开心", "高兴", "太棒", "哈哈", "喜欢", "谢谢", "amazing", "great", "love", "yay"],
    "sad": ["难过", "伤心", "哭", "唉", "失败", "sorry", "sad", "tired", "累"],
    "excited": ["哇", "牛", "厉害", "酷", "wow", "awesome", "!!"],
    "curious": ["为什么", "怎么", "如何", "吗", "why", "how", "what"],
    "calm": ["晚安", "休息", "安静", "sleep", "relax", "quiet"],
}


@dataclass
class MoodState:
    emotion: str = "neutral"
    valence: float = 0.5
    arousal: float = 0.4
    updated_at: float = field(default_factory=time.time)

    def as_dict(self) -> dict:
        meta = EMOTIONS.get(self.emotion, EMOTIONS["neutral"])
        return {
            "emotion": self.emotion,
            "valence": round(self.valence, 3),
            "arousal": round(self.arousal, 3),
            "color": meta["color"],
            "emoji": meta["emoji"],
        }


class PersonalityEngine:
    def __init__(self, name: str, style: str):
        self.name = name
        self.style = style
        self.mood = MoodState()
        self.turn_count = 0

    # ---- public API ----

    def observe_user(self, text: str) -> None:
        """Update mood from user utterance."""
        self.turn_count += 1
        scored = self._score_text(text)
        # EMA toward the new signal so mood doesn't snap wildly
        self.mood.valence = 0.7 * self.mood.valence + 0.3 * scored["valence"]
        self.mood.arousal = 0.7 * self.mood.arousal + 0.3 * scored["arousal"]
        self.mood.emotion = scored["emotion"]
        self.mood.updated_at = time.time()

    def observe_assistant(self, text: str) -> None:
        """Assistant output slightly stabilises mood."""
        if self.mood.arousal > 0.8:
            self.mood.arousal *= 0.9

    def set_state(self, state: str) -> None:
        """listening / thinking / speaking override for device UI."""
        if state in ("listening", "thinking", "speaking"):
            self.mood.emotion = state
            self.mood.updated_at = time.time()

    def system_prompt(self) -> str:
        m = self.mood.as_dict()
        extra = {
            "happy": "你现在心情不错，回答可以更活泼一点。",
            "excited": "你现在很兴奋，可以用更有能量的语气。",
            "sad": "用户可能情绪低落，请温柔体贴、简短陪伴。",
            "calm": "现在氛围安静，回答尽量轻柔简洁。",
            "curious": "用户在提问，请清晰有条理地解释。",
            "thinking": "保持专注，给出条理清晰的回答。",
        }.get(m["emotion"], "")
        return (
            f"你是{self.name}，一个桌面 AI 伙伴。\n"
            f"风格：{self.style}\n"
            f"当前情绪：{m['emoji']} {m['emotion']}。\n"
            f"{extra}\n"
            "回答请控制在 2-4 句以内，口语化。"
        )

    def tts_hints(self) -> dict:
        m = self.mood.as_dict()
        # map arousal/valence to speaking rate percent for edge-tts
        rate = int((self.mood.arousal - 0.5) * 20)  # -10 .. +10 %
        rate = max(-15, min(25, rate))
        return {
            "rate": f"{rate:+d}%",
            "emotion": m["emotion"],
            "emoji": m["emoji"],
        }

    def device_payload(self) -> dict:
        return self.mood.as_dict()

    # ---- internals ----

    def _score_text(self, text: str) -> dict:
        low = text.lower()
        hits = {k: 0 for k in _MOOD_HINTS}
        for emo, words in _MOOD_HINTS.items():
            for w in words:
                if w in low:
                    hits[emo] += 1

        best = max(hits, key=hits.get) if max(hits.values()) > 0 else "neutral"
        meta = EMOTIONS[best]
        return {
            "emotion": best,
            "valence": meta["valence"],
            "arousal": meta["arousal"],
        }
