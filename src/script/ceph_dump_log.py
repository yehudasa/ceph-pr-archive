# Copyright (C) 2018 Red Hat Inc.
#
# Authors: Sergio Lopez Pascual <slopezpa@redhat.com>
#          Brad Hubbard <bhubbard@redhat.com>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU Library Public License as published by
# the Free Software Foundation; either version 2, or (at your option)
# any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Library Public License for more details.
#

# By default ceph daemons and clients maintain a list of log_max_recent (default
# 500) log entries at a high debug level. This script will attempt to dump out
# that log from a ceph::log::Log* passed to the ceph-dump-log function. This may
# be obtained via the _log member of a CephContext object (i.e. *cct->_log) from
# any thread that contains such a CephContext. Normally, you will find a thread
# waiting in ceph::logging::Log::entry and the 'this' pointer from such a frame
# can also be passed to ceph-dump-log.

import gdb
from datetime import datetime

class CephDumpLog(gdb.Command):
    def __init__(self):
        super(CephDumpLog, self).__init__(
            'ceph-dump-log',
            gdb.COMMAND_DATA, gdb.COMPLETE_SYMBOL, False)

    def invoke(self, args, from_tty):
        arg_list = gdb.string_to_argv(args)
        if len(arg_list) < 1:
            gdb.write("usage: ceph-dump-log ceph::log::Log*\n")
            return
        expr = arg_list[0] + '->m_recent->m_head'
        entry = gdb.parse_and_eval(expr).dereference()
        while (entry):
            ts = datetime.fromtimestamp(float(str(entry['m_stamp']['tv']['tv_sec'])))
            ts_usec = entry['m_stamp']['tv']['tv_nsec'] / 1000
            gdb.write('%s.%d: ' % (ts, ts_usec))
            gdb.write(entry['m_streambuf']['_M_out_beg'].string("ascii", errors='ignore'))
            output = entry['m_streambuf']['m_buf'].string("ascii", errors='ignore')
            gdb.write('\n')
            if entry['m_next'] != 0:
                entry = entry['m_next'].dereference()
            else:
                return

CephDumpLog()
