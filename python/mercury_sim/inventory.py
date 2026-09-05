from __future__ import annotations

from dataclasses import dataclass, field


@dataclass
class InventoryBook:
    """Signed quantity per account (buy +, sell -)."""

    _qty: dict[int, int] = field(default_factory=dict)

    def quantity(self, account: int) -> int:
        return self._qty.get(account, 0)

    def apply_trade(
        self,
        maker_account: int,
        taker_account: int,
        taker_side: str,
        quantity: int,
    ) -> None:
        if taker_side == "buy":
            self._qty[taker_account] = self.quantity(taker_account) + quantity
            self._qty[maker_account] = self.quantity(maker_account) - quantity
        else:
            self._qty[taker_account] = self.quantity(taker_account) - quantity
            self._qty[maker_account] = self.quantity(maker_account) + quantity
