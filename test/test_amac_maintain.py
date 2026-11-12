"""User-visible charging diagnostics must not guess full/USB state."""
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from amac_maintain import battery_summary


class ChargingDiagnosticsTests(unittest.TestCase):
    def test_old_firmware_and_invalid_samples(self):
        self.assertIn('更新固件', battery_summary({'charging': 0}))
        state = dict(raw_charging=-1, raw_samples=0, raw_valid=0, raw_mv=0, raw_age_ms=0)
        self.assertIn('等待电池采样', battery_summary(state))
        state.update(raw_samples=1, raw_age_ms=5001)
        self.assertIn('过期', battery_summary(state))
        state.update(raw_age_ms=0)
        self.assertIn('电压无效', battery_summary(state))

    def test_physical_state_overrides_guest_service(self):
        state = dict(charging=0, battery=0, raw_charging=1, raw_samples=2,
                     raw_valid=1, raw_mv=4200, raw_age_ms=10)
        self.assertIn('4.200 V；正在充电', battery_summary(state))
        state['raw_charging'] = 0
        self.assertIn('未在充电', battery_summary(state))
        self.assertIn('无法单独判断 USB', battery_summary(state))
        state['raw_charging'] = -1
        self.assertIn('等待充电状态稳定', battery_summary(state))
