"""Deployment safety tests, no serial device or ESP-IDF needed."""
import importlib.util
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

spec = importlib.util.spec_from_file_location('fw', Path(__file__).resolve().parents[1] / 'tools/fw.py')
fw = importlib.util.module_from_spec(spec)
spec.loader.exec_module(fw)


class PipelineTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.prod = self.root / 'build'
        self.bridge = self.prod / 'ota-bridge'
        for directory in (self.prod, self.bridge):
            (directory / 'partition_table').mkdir(parents=True)
            (directory / 'partition_table/partition-table.bin').write_bytes(b'partitions')
            (directory / 'bldsp_control.bin').write_bytes(b'normal-firmware')
        self.patchers = [patch.object(fw, 'ROOT', self.root),
                        patch.object(fw, 'build', side_effect=lambda target, bridge=False: self.bridge if bridge else self.prod),
                        patch.object(fw, 'release_monitor'), patch.object(fw, 'flash'), patch.object(fw, 'monitor'), patch.object(fw, 'require_port')]
        self.mocks = [p.start() for p in self.patchers]
        for p in self.patchers:
            self.addCleanup(p.stop)
        self.flash = self.mocks[3]
        self.monitor = self.mocks[4]

    def assert_restored(self):
        self.assertEqual(self.flash.call_count, 2)
        self.assertEqual(self.flash.call_args_list[0].args, ('s3', 'COM7', self.bridge))
        self.assertEqual(self.flash.call_args_list[1].args, ('s3', 'COM7', self.prod))

    def test_success_restores(self):
        fw.ota('COM7')
        self.assert_restored()

    def test_prepare_never_touches_device(self):
        fw.ota("COM7", prepare_only=True)
        self.flash.assert_not_called()
        self.monitor.assert_not_called()

    def test_timeout_restores_and_still_fails(self):
        self.monitor.side_effect = RuntimeError('timeout')
        with self.assertRaisesRegex(RuntimeError, 'timeout'):
            fw.ota('COM7')
        self.assert_restored()

    def test_interrupt_restores(self):
        self.monitor.side_effect = KeyboardInterrupt()
        with self.assertRaises(KeyboardInterrupt):
            fw.ota('COM7')
        self.assert_restored()

    def test_first_flash_error_attempts_restore(self):
        self.flash.side_effect = [RuntimeError('serial error'), None]
        with self.assertRaisesRegex(RuntimeError, 'serial error'):
            fw.ota('COM7')
        self.assert_restored()

    def test_changed_partitions_refused_before_flash(self):
        (self.bridge / 'partition_table/partition-table.bin').write_bytes(b'other')
        with self.assertRaisesRegex(RuntimeError, 'partition'):
            fw.ota('COM7')
        self.flash.assert_not_called()

    def test_bridge_in_production_refused(self):
        (self.prod / 'bldsp_control.bin').write_bytes(b'ota_smoke')
        with self.assertRaisesRegex(RuntimeError, 'Production'):
            fw.ota('COM7')
        self.flash.assert_not_called()


if __name__ == '__main__':
    unittest.main()
