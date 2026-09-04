from __future__ import annotations

import argparse
import json
import subprocess
from pathlib import Path

from mercury_sim.events import read_jsonl, write_jsonl
from mercury_sim.generate import generate_events
from mercury_sim.replay import replay


def find_jsonl_replay(repo_root: Path | None = None) -> Path | None:
    root = repo_root or Path(__file__).resolve().parents[2]
    candidates = [
        root / "build" / "apps" / "jsonl_replay.exe",
        root / "build" / "apps" / "jsonl_replay",
        root / "build-release" / "apps" / "jsonl_replay.exe",
        root / "build-release" / "apps" / "jsonl_replay",
    ]
    for path in candidates:
        if path.is_file():
            return path
    return None


def load_trade_dicts(text: str) -> list[dict]:
    trades: list[dict] = []
    for line in text.splitlines():
        line = line.strip()
        if line:
            trades.append(json.loads(line))
    return trades


def cpp_trades(events_path: Path, replay_bin: Path) -> list[dict]:
    result = subprocess.run(
        [str(replay_bin), str(events_path)],
        check=True,
        capture_output=True,
        text=True,
    )
    return load_trade_dicts(result.stdout)


def python_trades(events_path: Path) -> list[dict]:
    trades, _ = replay(read_jsonl(str(events_path)))
    return [trade.to_dict() for trade in trades]


def compare_files(events_path: Path, replay_bin: Path) -> tuple[list[dict], list[dict]]:
    return python_trades(events_path), cpp_trades(events_path, replay_bin)


def main(argv: list[str] | None = None) -> None:
    parser = argparse.ArgumentParser(description="Compare Python and C++ JSONL replays")
    parser.add_argument("events", nargs="?", help="events.jsonl (generated if omitted)")
    parser.add_argument("--seed", type=int, default=42)
    parser.add_argument("--count", type=int, default=100)
    parser.add_argument("--replay-bin", type=Path, default=None)
    args = parser.parse_args(argv)

    replay_bin = args.replay_bin or find_jsonl_replay()
    if replay_bin is None:
        raise SystemExit("jsonl_replay binary not found; build the C++ app first")

    if args.events:
        events_path = Path(args.events)
    else:
        events_path = Path("events.jsonl")
        write_jsonl(str(events_path), generate_events(args.count, seed=args.seed))

    py_trades, cxx_trades = compare_files(events_path, replay_bin)
    if py_trades != cxx_trades:
        raise SystemExit(
            f"mismatch: python={len(py_trades)} trades, cpp={len(cxx_trades)} trades"
        )
    print(f"ok: {len(py_trades)} matching trades")


if __name__ == "__main__":
    main()
