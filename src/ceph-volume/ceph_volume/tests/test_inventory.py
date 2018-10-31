# -*- coding: utf-8 -*-

import pytest
from ceph_volume.util.device import Devices
from ceph_volume import sys_info

class TestInventory(object):

    def test_json_inventory(self):
        # populate sys_info with something
        sys_info.devices = {
            # example output of disk.get_devices()
            '/dev/sdb': {'human_readable_size': '1.82 TB',
                         'locked': 0,
                         'model': 'PERC H700',
                         'nr_requests': '128',
                         'partitions': {},
                         'path': '/dev/sdb',
                         'removable': '0',
                         'rev': '2.10',
                         'ro': '0',
                         'rotational': '1',
                         'sas_address': '',
                         'sas_device_handle': '',
                         'scheduler_mode': 'cfq',
                         'sectors': 0,
                         'sectorsize': '512',
                         'size': 1999844147200.0,
                         'support_discard': '',
                         'vendor': 'DELL'}
        }
        expected_keys = [
            'path',
            'rejected_reasons',
            'sys_api',
            'valid',
        ]
        expected_sys_api_keys = [
            'human_readable_size',
            'locked',
            'model',
            'nr_requests',
            'partitions',
            'path',
            'removable',
            'rev',
            'ro',
            'rotational',
            'sas_address',
            'sas_device_handle',
            'scheduler_mode',
            'sectors',
            'sectorsize',
            'size',
            'support_discard',
            'vendor',
        ]
        report = Devices().json_report()[0]
        report_keys = list(report.keys())
        for k in report_keys:
            assert k in expected_keys, "unexpected key {} in report".format(k)
        for k in expected_keys:
            assert k in report_keys, "expected key {} in report".format(k)
        sys_api_keys = list(report['sys_api'].keys())
        for k in sys_api_keys:
            assert k in expected_sys_api_keys, "unexpected key {} in sys_api field".format(k)
        for k in expected_sys_api_keys:
            assert k in sys_api_keys, "expected key {} in sys_api field".format(k)

