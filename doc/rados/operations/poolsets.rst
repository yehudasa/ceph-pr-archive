
======================================
Automated PG management using poolsets
======================================

What is a poolset?
==================

Poolsets are simply groups of pools, used for a shared purpose (such as a
CephFS filesystem, or an RGW Zone).  They simplify administration by
enabling you to create your pools in a single command, and they enable
the Ceph to do a better job of configuring placement groups, by using
this knowledge of how grouped pools related to one another.

When you upgrade your Ceph cluster to **nautilus**, or create pools
outside of a poolset, Ceph will automatically create poolsets for you,
grouping existing pools according to their usage (e.g. pools already used
together in a CephFS filesystem).

Where pools don't form part of a logical group (e.g. typical RBD usage),
they simply form poolsets of size one.

Usage
=====

The ``poolset create`` command automates the process of creating a number
of pools, and selecting approprate placement group counts.

::
    
    ceph poolset create <application> <name> <size>

The *application* parameter is one of ``cephfs``, ``rgw``, ``rbd`` or
``rados``.  The ``rados`` operation is used for any general purpose
pools, such as those consumed directly via librados by a third party
application.

The *name* parameter is a name of your choice: this will be used as
the start of the names of the poolset within your poolset.

The *size* parameter may be either a size in bytes (units are permitted,
such as "10GB"), or a percentage (such as "10%").  This is used
as an estimate of how much of the cluster's capacity will be used by this
poolset.  This is *not* a quota or limit on the capacity of the pools --
when pools use more space than their initial *size* setting, their
placement group settings are simply adjusted upwards accordingly.

**todo** add and document way to select replication factor, ec rule etc
**todo** explain how SSD pools are auto selected
**todo** explain that size ratio is specific to the crush rule
    
**todo** other commands

Automatic adjustment
====================

**todo** explain rules

All poolsets are initially created with adjustment policy `auto`, i.e.
their placement group configuration will be automatically adjusted
according to usage and available capacity.  

Configuration
=============

Placement group counts
----------------------

The automated selection/adjustment of placement groups is primarily
driven by the ``mon_target_pg_per_osd`` setting.  This is a new
setting in the *nautilus* release of Ceph.

The ``mon_max_pg_per_osd`` setting controls a health check that will
warn the user if the number of PGs is significantly higher than
the target -- this setting has existed in Ceph since before
automated PG management was added, and continues to operate
as a check on the pg_nums chosen by the automated process.


Toggling automatic adjustment
-----------------------------

All poolsets have a 'policy' attribute that controls how Ceph controls
the placement group configuration of pools.  There are three possible
settings:

- ``autoscale``: Automatically adjust placement group counts if outside the
  limits controlled by ``mon_target_pg_per_osd`` and ``mon_max_pg_per_osd``.
- ``warn``: Raise health warnings if the placement group configuration
  is outside acceptable limits and should be adjusted.
- ``silent``: No action or warning on placement group counts.

Poolset attributes may be adjusted using the ``set`` command:

::

    ceph poolset set <poolset name> <property> <value>

For example, to enable auto adjustment on a poolset ``myset``:

::

    ceph poolset set myset policy autoscale

Upgrades
========

When upgrading to Ceph 14.x *Nautilus*, or when any individual pools are created
directly, they will be automatically added to poolsets.  Poolsets are constructed
according to metadata: e.g. pools that are part of a CephFS filesystem are
automatically added to a poolset for the filesystem.

Poolsets that are automatically created will have the ``warn`` adjustment
policy.  They can be set to ``autoscale`` to enable automatic adjustment
of placement group configuration.


