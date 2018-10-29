# -*- coding: utf-8 -*-
from __future__ import absolute_import

from .helper import DashboardTestCase


class HealthTest(DashboardTestCase):
    CEPHFS = True

    def test_minimal_health(self):
        data = self._get("/api/health/minimal")
        self.assertStatus(200)

        self.assertNotIn('audit_log', data)
        self.assertIn('client_perf', data)
        self.assertSortedEqual(
            data['client_perf'].keys(),
            ['read_bytes_sec', 'read_op_per_sec',
             'recovering_bytes_per_sec', 'write_bytes_sec',
             'write_op_per_sec']
        )
        self.assertNotIn('clog', data)
        self.assertIn('df', data)
        self.assertEqual(data['df'].keys(), ['stats'])
        self.assertSortedEqual(
            data['df']['stats'].keys(),
            ['total_avail_bytes', 'total_bytes', 'total_objects',
             'total_used_bytes']
        )
        self.assertIn('fs_map', data)
        self.assertSortedEqual(
            data['fs_map'].keys(), ['filesystems', 'standbys'])
        for fs in data['fs_map']['filesystems']:
            self.assertEqual(fs.keys(), ['mdsmap'])
            self.assertEqual(fs['mdsmap'].keys(), ['info'])
            for item in fs['mdsmap']['info'].values():
                self.assertEqual(item.keys(), ['state'])
        self.assertIn('health', data)
        self.assertSortedEqual(data['health'].keys(), ['checks', 'status'])
        self.assertIn('hosts', data)

    def test_full_health(self):
        data = self._get("/api/health/full")
        self.assertStatus(200)

        self.assertIn('health', data)
        self.assertIn('mon_status', data)
        self.assertIn('fs_map', data)
        self.assertIn('osd_map', data)
        self.assertNotIn('clog', data)
        self.assertNotIn('audit_log', data)
        self.assertIn('pools', data)
        self.assertIn('mgr_map', data)
        self.assertIn('df', data)
        self.assertIn('scrub_status', data)
        self.assertIn('pg_info', data)
        self.assertIn('client_perf', data)
        self.assertIn('hosts', data)
        self.assertIn('rgw', data)
        self.assertIn('iscsi_daemons', data)
        self.assertIsNotNone(data['health'])
        self.assertIsNotNone(data['mon_status'])
        self.assertIsNotNone(data['fs_map'])
        self.assertIsNotNone(data['osd_map'])
        self.assertIsNotNone(data['pools'])
        self.assertIsNotNone(data['scrub_status'])
        self.assertIsNotNone(data['pg_info'])
        self.assertIsNotNone(data['client_perf'])
        self.assertIsNotNone(data['hosts'])
        self.assertIsNotNone(data['rgw'])
        self.assertIsNotNone(data['iscsi_daemons'])

        cluster_pools = self.ceph_cluster.mon_manager.list_pools()
        self.assertEqual(len(cluster_pools), len(data['pools']))
        for pool in data['pools']:
            self.assertIn(pool['pool_name'], cluster_pools)

        self.assertIsNotNone(data['mgr_map'])
        self.assertIsNotNone(data['df'])

    @DashboardTestCase.RunAs('test', 'test', ['pool-manager'])
    def test_health_permissions(self):
        data = self._get("/api/health/full")
        self.assertStatus(200)

        self.assertIn('health', data)
        self.assertNotIn('mon_status', data)
        self.assertNotIn('fs_map', data)
        self.assertNotIn('osd_map', data)
        self.assertNotIn('clog', data)
        self.assertNotIn('audit_log', data)
        self.assertIn('pools', data)
        self.assertNotIn('mgr_map', data)
        self.assertIn('df', data)
        self.assertNotIn('scrub_status', data)
        self.assertNotIn('pg_info', data)
        self.assertIn('client_perf', data)
        self.assertNotIn('hosts', data)
        self.assertNotIn('rgw', data)
        self.assertNotIn('iscsi_daemons', data)
        self.assertIsNotNone(data['health'])
        self.assertIsNotNone(data['pools'])
        self.assertIsNotNone(data['client_perf'])

        cluster_pools = self.ceph_cluster.mon_manager.list_pools()
        self.assertEqual(len(cluster_pools), len(data['pools']))
        for pool in data['pools']:
            self.assertIn(pool['pool_name'], cluster_pools)

        self.assertIsNotNone(data['df'])
