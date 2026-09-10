from __future__ import annotations

import json
from dataclasses import asdict, dataclass
from typing import Iterable, Literal, Optional, Union


@dataclass(frozen=True)
class LimitEvent:
    id: int
    side: Literal["buy", "sell"]
    price: int
    quantity: int
    account: int = 0
    tif: Literal["gtc", "ioc", "fok"] = "gtc"
    symbol: int = 0
    post_only: bool = False
    reduce_only: bool = False
    type: Literal["limit"] = "limit"


@dataclass(frozen=True)
class MarketEvent:
    id: int
    side: Literal["buy", "sell"]
    quantity: int
    account: int = 0
    symbol: int = 0
    reduce_only: bool = False
    type: Literal["market"] = "market"


@dataclass(frozen=True)
class CancelEvent:
    id: int
    type: Literal["cancel"] = "cancel"


@dataclass(frozen=True)
class ReplaceEvent:
    id: int
    price: int
    quantity: int
    type: Literal["replace"] = "replace"


@dataclass(frozen=True)
class MassCancelEvent:
    account: Optional[int] = None
    symbol: Optional[int] = None
    side: Optional[Literal["buy", "sell"]] = None
    type: Literal["mass_cancel"] = "mass_cancel"


@dataclass(frozen=True)
class StopEvent:
    id: int
    side: Literal["buy", "sell"]
    stop_price: int
    quantity: int
    account: int = 0
    limit_price: Optional[int] = None
    tif: Literal["gtc", "ioc", "fok"] = "gtc"
    symbol: int = 0
    type: Literal["stop"] = "stop"


Event = Union[LimitEvent, MarketEvent, CancelEvent, ReplaceEvent, MassCancelEvent, StopEvent]


def event_to_dict(event: Event) -> dict:
    data = asdict(event)
    if isinstance(event, StopEvent) and event.limit_price is None:
        del data["limit_price"]
    if isinstance(event, LimitEvent):
        if not event.post_only:
            del data["post_only"]
        if not event.reduce_only:
            del data["reduce_only"]
    if isinstance(event, MarketEvent) and not event.reduce_only:
        del data["reduce_only"]
    if isinstance(event, MassCancelEvent):
        data = {k: v for k, v in data.items() if v is not None or k == "type"}
    return data


def event_from_dict(data: dict) -> Event:
    kind = data["type"]
    if kind == "limit":
        return LimitEvent(
            id=data["id"],
            side=data["side"],
            price=data["price"],
            quantity=data["quantity"],
            account=data.get("account", 0),
            tif=data.get("tif", "gtc"),
            symbol=data.get("symbol", 0),
            post_only=bool(data.get("post_only", False)),
            reduce_only=bool(data.get("reduce_only", False)),
        )
    if kind == "market":
        return MarketEvent(
            id=data["id"],
            side=data["side"],
            quantity=data["quantity"],
            account=data.get("account", 0),
            symbol=data.get("symbol", 0),
            reduce_only=bool(data.get("reduce_only", False)),
        )
    if kind == "cancel":
        return CancelEvent(id=data["id"])
    if kind == "replace":
        return ReplaceEvent(
            id=data["id"],
            price=data["price"],
            quantity=data["quantity"],
        )
    if kind == "mass_cancel":
        return MassCancelEvent(
            account=data.get("account"),
            symbol=data.get("symbol"),
            side=data.get("side"),
        )
    if kind == "stop":
        return StopEvent(
            id=data["id"],
            side=data["side"],
            stop_price=data["stop_price"],
            quantity=data["quantity"],
            account=data.get("account", 0),
            limit_price=data.get("limit_price"),
            tif=data.get("tif", "gtc"),
            symbol=data.get("symbol", 0),
        )
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
