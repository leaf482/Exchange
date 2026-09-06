"""Synthetic event generation and analysis for Mercury Exchange."""

from mercury_sim.events import (
    CancelEvent,
    Event,
    LimitEvent,
    MarketEvent,
    StopEvent,
    event_from_dict,
    event_to_dict,
    read_jsonl,
    write_jsonl,
)
from mercury_sim.sim import SimConfig, simulate_market

__all__ = [
    "CancelEvent",
    "Event",
    "LimitEvent",
    "MarketEvent",
    "StopEvent",
    "SimConfig",
    "event_from_dict",
    "event_to_dict",
    "read_jsonl",
    "simulate_market",
    "write_jsonl",
]
