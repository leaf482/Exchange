import tempfile
import unittest
from pathlib import Path

from mercury_sim.compare import compare_files, find_jsonl_replay
from mercury_sim.events import write_jsonl
from mercury_sim.generate import generate_events

REPLAY_BIN = find_jsonl_replay()


@unittest.skipUnless(REPLAY_BIN is not None, "jsonl_replay binary not built")
class ParityTests(unittest.TestCase):
    def test_python_and_cpp_trades_match(self) -> None:
        events = generate_events(120, seed=42)
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "events.jsonl"
            write_jsonl(str(path), events)
            py_trades, cxx_trades = compare_files(path, REPLAY_BIN)

        self.assertEqual(py_trades, cxx_trades)
        self.assertGreater(len(py_trades), 0)


if __name__ == "__main__":
    unittest.main()
