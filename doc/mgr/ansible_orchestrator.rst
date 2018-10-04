
.. _ansible-orchestrator-module:

====================
Ansible Orchestrator
====================

This module is a `Ceph orchestrator <http://docs.ceph.com/docs/master/mgr/orchestrator_modules/>`_ module that uses `Ansible Runner Service <https://github.com/jmolmo/ansible-runner-service>`_ (a RESTful API server) to execute Ansible playbooks in order to satisfy the different operations supported.

These operations basically (and for the moment) are:

- Get and inventory of the Ceph cluster nodes and the devices present in each node
- Create new OSD's
- Replace existant OSD's



Configuration
=============

TODO

Usage
=====

Enable the module:

::

    # ceph mgr module enable ansible-orchestrator


Get inventory of the cluster (nodes and storage devices):

::

    # ceph inventory


Get the inventory of the cluster filtered:

::

    # ceph inventory filter=osd_node_*
