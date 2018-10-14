import logging
import os
import re
import stat
from ceph_volume import process
from ceph_volume_zfs.api import zfs
from ceph_volume_zfs.util.system import get_file_contents


logger = logging.getLogger(__name__)


def _stat_is_device(stat_obj):
    """
    Helper function that will interpret ``os.stat`` output directly, so that other
    functions can call ``os.stat`` once and interpret that result several times
    """
    return stat.S_ISBLK(stat_obj)


def is_device(dev):
    """
    Boolean to determine if a given device is a block device (**not**
    a partition!)

    For example: /dev/sda would return True, but not /dev/sdc1
    """
    if not os.path.exists(dev):
        return False
    return True


def is_partition(dev):
    """
    Boolean to determine if a given device is a partition, like /dev/sda1
    """
    if not os.path.exists(dev):
        return False

    # fallback to stat
    stat_obj = os.stat(dev)
    if _stat_is_device(stat_obj.st_mode):
        return False

    return False


def _map_dev_paths(_path, include_abspath=False, include_realpath=False):
    """
    Go through all the items in ``_path`` and map them to their absolute path::

        {'sda': '/dev/sda'}

    If ``include_abspath`` is set, then a reverse mapping is set as well::

        {'sda': '/dev/sda', '/dev/sda': 'sda'}

    If ``include_realpath`` is set then the same operation is done for any
    links found when listing, these are *not* reversed to avoid clashing on
    existing keys, but both abspath and basename can be included. For example::

        {
            'ceph-data': '/dev/mapper/ceph-data',
            '/dev/mapper/ceph-data': 'ceph-data',
            '/dev/dm-0': '/dev/mapper/ceph-data',
            'dm-0': '/dev/mapper/ceph-data'
        }


    In case of possible exceptions the mapping is returned empty, and the
    exception is logged.
    """
    mapping = {}
    try:
        dev_names = os.listdir(_path)
    except (OSError, IOError):
        logger.exception('unable to list block devices from: %s' % _path)
        return {}

    for dev_name in dev_names:
        mapping[dev_name] = os.path.join(_path, dev_name)

    if include_abspath:
        for k, v in list(mapping.items()):
            mapping[v] = k

    if include_realpath:
        for abspath in list(mapping.values()):
            if not os.path.islink(abspath):
                continue

            realpath = os.path.realpath(abspath)
            basename = os.path.basename(realpath)
            mapping[basename] = abspath
            if include_abspath:
                mapping[realpath] = abspath

    return mapping


def human_readable_size(size):
    """
    Take a size in bytes, and transform it into a human readable size with up
    to two decimals of precision.
    """
    suffixes = ['B', 'KB', 'MB', 'GB', 'TB']
    suffix_index = 0
    while size > 1024:
        suffix_index += 1
        size = size / 1024.0
    return "{size:.2f} {suffix}".format(
        size=size,
        suffix=suffixes[suffix_index])

