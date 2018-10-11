
.. _ansible-orchestrator-module:

====================
Ansible Orchestrator
====================

This module is a :ref:`Ceph orchestrator <orchestrator-modules>` module that uses `Ansible Runner Service <https://github.com/pcuzner/ansible-runner-service>`_ (a RESTful API server) to execute Ansible playbooks in order to satisfy the different operations supported.

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

Disable the module

::

    # ceph mgr module disable ansible-orchestrator


Enable the Ansible orchestrator module and use it with the :ref:`CLI <orchestrator-cli-module>`:

::

    ceph mgr module enable orchestrator_cli
    ceph mgr module enable ansible-orchestrator
    ceph orchestrator set backend ansible-orchestrator
