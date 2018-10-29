# -*- coding: utf-8 -*-
from __future__ import absolute_import

from .helper import DashboardTestCase


class LogsTest(DashboardTestCase):
    CEPHFS = True

    def test_health(self):
        data = self._get("/api/logs/all")
        self.assertStatus(200)
        self.assertIn('clog', data)
        self.assertIn('audit_log', data)
        self.assertIsNotNone(data['clog'])
        self.assertIsNotNone(data['audit_log'])
