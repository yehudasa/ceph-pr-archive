import os


class ConfigurationError(Exception):

    def __init__(self, cluster_name='ceph', path='/etc/ceph', abspath=None):
        self.cluster_name = cluster_name
        self.path = path
        self.abspath = abspath or "%s.conf" % os.path.join(self.path, self.cluster_name)

    def __str__(self):
        return 'Unable to load expected Ceph config at: %s' % self.abspath


class ConfigurationSectionError(Exception):

    def __init__(self, section):
        self.section = section

    def __str__(self):
        return 'Unable to find expected configuration section: "%s"' % self.section


class ConfigurationKeyError(Exception):

    def __init__(self, section, key):
        self.section = section
        self.key = key

    def __str__(self):
        return 'Unable to find expected configuration key: "%s" from section "%s"' % (
            self.key,
            self.section
        )


class SuffixParsingError(Exception):

    def __init__(self, suffix, part=None):
        self.suffix = suffix
        self.part = part

    def __str__(self):
        return 'Unable to parse the %s from systemd suffix: %s' % (self.part, self.suffix)


class SuperUserError(Exception):

    def __str__(self):
        return 'This command needs to be executed with sudo or as root'


class MultipleZPoolsError(Exception):

    def __init__(self, zp_name):
        self.pv_name = zp_name

    def __str__(self):
        msg = "Got more than 1 result looking for zpool: %s" % self.zp_name
        return msg


class MultipleZFSError(Exception):

    def __init__(self, zv_name, zv_path):
        self.zv_name = zv_name
        self.zv_path = zv_path

    def __str__(self):
        msg = "Got more than 1 result looking for %s with path: %s" % (self.zv_name, self.zv_path)
        return msg


class MultipleVGsError(Exception):

    def __init__(self, vg_name):
        self.vg_name = vg_name

    def __str__(self):
        msg = "Got more than 1 result looking for volume group: %s" % self.vg_name
        return msg


class SizeAllocationError(Exception):

    def __init__(self, requested, available):
        self.requested = requested
        self.available = available

    def __str__(self):
        msg = 'Unable to allocate size (%s), not enough free space (%s)' % (
            self.requested, self.available
        )
        return msg
