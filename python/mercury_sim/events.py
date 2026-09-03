from __future__ import annotations

import json
from dataclasses import asdict, dataclass
from typing import Iterable, Literal, Union


@dataclass(frozen=True)
class LimitEvent:
    id: int
    side: Literal["buy", "sell"]
    price: int
    quantity: int
    account: int = 0
    type: Literal["limit"] = "limit"


@dataclass(frozen=True)
class MarketEvent:
    id: int
    side: Literal["buy", "sell"]
    quantity: int
    account: int = 0
    type: Literal["market"] = "market"


@dataclass(frozen=True)
class CancelEvent:
    id: int
    type: Literal["cancel"] = "cancel"


Event = Union[LimitEvent, MarketEvent, CancelEvent]


def event_to_dict(event: Event) -> dict:
    return asdict(event)


def event_from_dict(data: dict) -> Event:
    kind = data["type"]
    if kind == "limit":
        return LimitEvent(
            id=data["id"],
            side=data["side"],
            price=data["price"],
            quantity=data["quantity"],
            account=data.get("account", 0),
        )
    if kind == "market":
        return MarketEvent(
            id=data["id"],
            side=data["side"],
            quantity=data["quantity"],
            account=data.get("account", 0),
        )
    if kind == "cancel":
        return CancelEvent(id=data["id"])
    raise ValueError(f"unknown event type: {kind}")


def write_jsonl(path: str, events: Iterable[Event]) -> None:
    with open(path, "w", encoding="utf-8") as out:
        for event in events:
            out.write(json.dumps(event_to_dict(event), separators=(",", ":")))
            out.write("\n")


def read_jsonl(path: str) -> list[Event]:
    events: list[Event] = []
    with open(path, encoding="utf-8") as inp:
        for line in inp:
            line = line.strip()
            if line:
                events.append(event_from_dict(json.loads(line)))
    return events
