# -*- coding: utf-8 -*-
from __future__ import absolute_import

from .helper import DashboardTestCase, JLeaf, JList, JObj, JAny


class HealthTest(DashboardTestCase):
    CEPHFS = True

    def test_minimal_health(self):
        data = self._get('/api/health/minimal')
        self.assertStatus(200)
        spec = JObj({
            'client_perf': JObj({
                'read_bytes_sec': int,
                'recovering_bytes_per_sec': int,
                'write_op_per_sec': int,
                'write_bytes_sec': int,
                'read_op_per_sec': int,
            }),
            'df': JObj({
                'stats': JObj({
                    'total_objects': int,
                    'total_used_bytes': int,
                    'total_bytes': int,
                    'total_avail_bytes': int,
                })
            }),
            'osd_map': JObj({
                'osds': JList(
                    JObj({
                        'in': int,
                        'up': int,
                    })),
            }),
            'rgw': int,
            'fs_map': JObj({
                'standbys': JList(JObj({})),
                'filesystems': JList(
                    JObj({
                        # TODO: Find a way to verify the structure of
                        # mdsmap['info']; it doesn't appear there is a way to
                        # specify that a dict can have any string as a *key*.
                        'mdsmap': JAny(none=False)
                    }),
                )
            }),
            'pg_info': JObj({
                'pgs_per_osd': int,
                # TODO: Find a way to verify that each key under 'statuses' has
                # an integer value
                'statuses': JObj({}, allow_unknown=True)
            }),
            'mon_status': JObj({
                'monmap': JObj({
                    'mons': JList(JLeaf(dict)),
                }),
                'quorum': JList(int)
            }),
            'health': JObj({
                'status': str,
                'checks': JList(str)
            }),
            'scrub_status': str,
            'pools': JList(JLeaf(dict)),
            'hosts': int,
            'iscsi_daemons': int,
            'mgr_map': JObj({
                'standbys': JList(JLeaf(dict)),
                'active_name': str
            })
        })
        self.assertSchema(data, spec)

    def test_full_health(self):
        data = self._get('/api/health/full')
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
        data = self._get('/api/health/full')
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
