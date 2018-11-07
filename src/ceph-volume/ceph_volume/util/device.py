# -*- coding: utf-8 -*-

import os
from functools import total_ordering
from ceph_volume import sys_info
from ceph_volume.api import lvm
from ceph_volume.util import disk

report_template = """
{dev:<25} {size:<12} {rot!s:<7} {valid!s:<7} {model}"""


class Devices(object):
    """
    A container for Device instances with reporting
    """

    def __init__(self, devices=None):
        if not sys_info.devices:
            sys_info.devices = disk.get_devices()
        self.devices = [Device(k) for k in
                            sys_info.devices.keys()]

    def pretty_report(self, all=True):
        output = [
            report_template.format(
                dev='Device Path',
                size='Size',
                rot='rotates',
                model='Model name',
                valid='valid',
            )]
        for device in sorted(self.devices):
            output.append(device.report())
        return ''.join(output)

    def json_report(self):
        output = []
        for device in sorted(self.devices):
            output.append(device.json_report())
        return output

@total_ordering
class Device(object):

    pretty_template = """
     {attr:<25} {value}"""

    report_fields = [
        '_rejected_reasons',
        '_valid',
        'path',
        'sys_api',
    ]
    pretty_report_sys_fields = [
        'human_readable_size',
        'model',
        'removable',
        'ro',
        'rotational',
        'sas_address',
        'scheduler_mode',
        'vendor',
    ]

    def __init__(self, path):
        self.path = path
        # LVs can have a vg/lv path, while disks will have /dev/sda
        self.abspath = path
        self.lv_api = None
        self.lvs = []
        self.vg_name = None
        self.pvs_api = []
        self.disk_api = {}
        self.blkid_api = {}
        self.sys_api = {}
        self._exists = None
        self._is_lvm_member = None
        self._valid = False
        self._rejected_reasons = []
        self._parse()
        self.is_valid

    def __lt__(self, other):
        '''
        Implementing this method and __eq__ allows the @total_ordering
        decorator to turn the Device class into a totally ordered type.
        This can slower then implementing all comparison operations.
        This sorting should put valid devices before invalid devices and sort
        on the path otherwise (str sorting).
        '''
        if self._valid == other._valid:
            return self.path < other.path
        return self._valid and not other._valid

    def __eq__(self, other):
        return self.path == other.path

    def _parse(self):
        if not sys_info.devices:
            sys_info.devices = disk.get_devices()
        self.sys_api = sys_info.devices.get(self.abspath, {})

        # start with lvm since it can use an absolute or relative path
        lv = lvm.get_lv_from_argument(self.path)
        if lv:
            self.lv_api = lv
            self.lvs = [lv]
            self.abspath = lv.lv_path
            self.vg_name = lv.vg_name
        else:
            dev = disk.lsblk(self.path)
            self.blkid_api = disk.blkid(self.path)
            self.disk_api = dev
            device_type = dev.get('TYPE', '')
            # always check is this is an lvm member
            if device_type in ['part', 'disk']:
                self._set_lvm_membership()

        self.ceph_disk = CephDiskDevice(self)

    def __repr__(self):
        prefix = 'Unknown'
        if self.is_lv:
            prefix = 'LV'
        elif self.is_partition:
            prefix = 'Partition'
        elif self.is_device:
            prefix = 'Raw Device'
        return '<%s: %s>' % (prefix, self.abspath)

    def pretty_report(self):
        output = ['\n====== Device report {} ======\n'.format(self.path)]
        output.extend(
            [self.pretty_template.format(
                attr=k.strip('_').replace('_', ' '),
                value=v) for k, v in vars(self).items() if k in
                self.report_fields and k != 'disk_api' and k != 'sys_api'] )
        output.extend(
            [self.pretty_template.format(
                attr=k,
                value=v) for k, v in self.sys_api.items() if k in
                self.pretty_report_sys_fields])
        return ''.join(output)

    def report(self):
        return report_template.format(
            dev=self.abspath,
            size=self.size_human,
            rot=self.rotational,
            valid=self.is_valid,
            model=self.model,
        )

    def json_report(self):
        output = {k.strip('_'): v for k, v in vars(self).items() if k in
                  self.report_fields}
        return output

    def _set_lvm_membership(self):
        if self._is_lvm_member is None:
            for path in self._get_pv_paths():
                # check if there was a pv created with the
                # name of device
                pvs = lvm.PVolumes()
                pvs.filter(pv_name=path)
                if not pvs:
                    self._is_lvm_member = False
                    return self._is_lvm_member
                has_vgs = [pv.vg_name for pv in pvs if pv.vg_name]
                if has_vgs:
                    # a pv can only be in one vg, so this should be safe
                    self.vg_name = has_vgs[0]
                    self._is_lvm_member = True
                    self.pvs_api = pvs
                    for pv in pvs:
                        if pv.vg_name and pv.lv_uuid:
                            lv = lvm.get_lv(vg_name=pv.vg_name, lv_uuid=pv.lv_uuid)
                            if lv:
                                self.lvs.append(lv)
            else:
                # this is contentious, if a PV is recognized by LVM but has no
                # VGs, should we consider it as part of LVM? We choose not to
                # here, because most likely, we need to use VGs from this PV.
                self._is_lvm_member = False

        return self._is_lvm_member

    def _get_pv_paths(self):
        """
        For block devices LVM can reside on the raw block device or on a
        partition. Return a list of paths to be checked for a pv.
        """
        paths = [self.abspath]
        path_comp = self.abspath.split('/')
        for part in self.sys_api.get('partitions', {}).keys():
            path_comp[-1] = part
            paths.append('/'.join(path_comp))
        return paths

    @property
    def exists(self):
        return os.path.exists(self.abspath)

    @property
    def rotational(self):
        return self.sys_api['rotational'] == '1'

    @property
    def model(self):
        return self.sys_api['model']

    @property
    def size_human(self):
        return self.sys_api['human_readable_size']

    @property
    def size(self):
            return self.sys_api['size']

    @property
    def is_lvm_member(self):
        if self._is_lvm_member is None:
            self._set_lvm_membership()
        return self._is_lvm_member

    @property
    def is_ceph_disk_member(self):
        return self.ceph_disk.is_member

    @property
    def is_mapper(self):
        return self.path.startswith('/dev/mapper')

    @property
    def is_lv(self):
        return self.lv_api is not None

    @property
    def is_partition(self):
        if self.disk_api:
            return self.disk_api['TYPE'] == 'part'
        return False

    @property
    def is_device(self):
        if self.disk_api:
            return self.disk_api['TYPE'] == 'device'
        return False

    @property
    def used_by_ceph(self):
        # only filter out data devices as journals could potentially be reused
        osd_ids = [lv.tags.get("ceph.osd_id") is not None for lv in self.lvs
                   if lv.tags.get("ceph.type") in ["data", "block"]]
        return any(osd_ids)


    @property
    def is_valid(self):
        def reject_device(item, value, reason):
            try:
                if self.sys_api[item] == value:
                    self._rejected_reasons.append(reason)
            except KeyError:
                pass
        reject_device('removable', 1, 'removable')
        reject_device('ro', 1, 'read-only')
        reject_device('locked', 1, 'locked')

        self._valid = len(self._rejected_reasons) == 0
        return self._valid


class CephDiskDevice(object):
    """
    Detect devices that have been created by ceph-disk, report their type
    (journal, data, etc..). Requires a ``Device`` object as input.
    """

    def __init__(self, device):
        self.device = device
        self._is_ceph_disk_member = None

    @property
    def partlabel(self):
        """
        In containers, the 'PARTLABEL' attribute might not be detected
        correctly via ``lsblk``, so we poke at the value with ``lsblk`` first,
        falling back to ``blkid`` (which works correclty in containers).
        """
        lsblk_partlabel = self.device.disk_api.get('PARTLABEL')
        if lsblk_partlabel:
            return lsblk_partlabel
        return self.device.blkid_api.get('PARTLABEL', '')

    @property
    def is_member(self):
        if self._is_ceph_disk_member is None:
            if 'ceph' in self.partlabel:
                self._is_ceph_disk_member = True
                return True
            return False
        return self._is_ceph_disk_member

    @property
    def type(self):
        types = [
            'data', 'wal', 'db', 'lockbox', 'journal',
            # ceph-disk uses 'ceph block' when placing data in bluestore, but
            # keeps the regular OSD files in 'ceph data' :( :( :( :(
            'block',
        ]
        for t in types:
            if t in self.partlabel:
                return t
        return 'unknown'
