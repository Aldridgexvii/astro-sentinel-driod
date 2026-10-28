"""Small pure helpers for Astro-Sentinel; no board-specific imports."""
from __future__ import annotations

import re
import unicodedata

ALLOWED_MOODS = {"HAPPY", "CURIOUS", "CONFUSED"}


def sanitize_vlm_label(raw: object, max_words: int = 8) -> str:
    """Turn VLM prose into a short, safe, single-line object label.

    The prompt asks the local VLM to ignore people. This second line of defense
    drops person-centred outputs and limits text before it can reach the cloud.
    """
    text = unicodedata.normalize("NFKC", str(raw or "")).strip()
    if not text:
        return "unknown"

    # Use only the first non-empty line and strip common label prefixes.
    lines = [line.strip() for line in text.splitlines() if line.strip()]
    text = lines[0] if lines else ""
    text = text.strip(" `\"'“”‘’*#")
    text = re.sub(r"^(?:label|object|caption|answer)\s*[:=-]\s*", "", text, flags=re.I)
    text = re.sub(r"^(?:the main object is|i can see|i see|this is|it is)\s+", "", text, flags=re.I)
    # A VLM may answer with a short sentence despite the prompt; keep the first sentence.
    text = re.split(r"[.!?](?:\s|$)", text, maxsplit=1)[0].strip()
    text = text.strip(" `\"'“”‘’*#")

    if not text:
        return "unknown"

    lowered = text.casefold()
    if lowered in {
        "unknown", "none", "nothing", "uncertain", "not sure",
        "cannot tell", "can't tell", "no identifiable object", "no object",
    }:
        return "unknown"

    # Do not pass person labels or potentially sensitive document/card categories
    # to the cloud reasoning model. The VLM is instructed not to read text, but this
    # is an additional conservative guardrail in case it emits such a label anyway.
    person_terms = {"person", "people", "human", "man", "woman", "child", "boy", "girl", "baby"}
    words = re.findall(r"[\w'-]+", lowered, flags=re.UNICODE)
    if any(word in person_terms for word in words):
        return "unknown"
    sensitive_phrases = (
        "passport", "identity card", "id card", "driver's license", "driving licence",
        "credit card", "debit card", "bank card", "bank statement", "medical record",
        "prescription", "utility bill", "boarding pass",
    )
    if any(phrase in lowered for phrase in sensitive_phrases):
        return "unknown"

    # Keep only ordinary descriptive label characters; no multi-line/control payloads.
    text = re.sub(r"[^\w\s,'’\-]", "", text, flags=re.UNICODE)
    text = re.sub(r"\s+", " ", text).strip(" ,-'’")
    if not text:
        return "unknown"

    pieces = text.split()
    if len(pieces) > max(1, max_words):
        text = " ".join(pieces[:max(1, max_words)])
    return text[:96].strip() or "unknown"


def parse_mood(raw: object) -> str | None:
    """Extract one allowed token from a model response, tolerating wrappers."""
    match = re.search(r"\b(HAPPY|CURIOUS|CONFUSED)\b", str(raw or "").upper())
    return match.group(1) if match else None
