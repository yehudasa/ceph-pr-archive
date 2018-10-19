

.. _orchestrator-modules:

.. py:currentmodule:: orchestrator

ceph-mgr orchestrator modules
=============================

.. warning::

    This is developer documentation, describing Ceph internals that
    are only relevant to people writing ceph-mgr orchestrator modules.

In this context, *orchestrator* refers to some external service that
provides the ability to discover devices and create Ceph services.  This
includes external projects such as ceph-ansible, DeepSea, and Rook.

An *orchestrator module* is a ceph-mgr module (:ref:`mgr-module-dev`)
which implements common management operations using a particular
orchestrator.

Orchestrator modules subclass the ``Orchestrator`` class: this class is
an interface, it only provides method definitions to be implemented
by subclasses.  The purpose of defining this common interface
for different orchestrators is to enable common UI code, such as
the dashboard, to work with various different backends.

Behind all the abstraction, the purpose of orchestrator modules is simple:
enable Ceph to do things like discover available hardware, create and
destroy OSDs, and run MDS and RGW services.

A tutorial is not included here: for full and concrete examples, see
the existing implemented orchestrator modules in the Ceph source tree.

Glossary
--------

Stateful service
  a daemon that uses local storage, such as OSD or mon.

Stateless service
  a daemon that doesn't use any local storage, such
  as an MDS, RGW, nfs-ganesha, iSCSI gateway.

Label
  arbitrary string tags that may be applied by administrators
  to nodes.  Typically administrators use labels to indicate
  which nodes should run which kinds of service.  Labels are
  advisory (from human input) and do not guarantee that nodes
  have particular physical capabilities.

Drive group
  collection of block devices with common/shared OSD
  formatting (typically one or more SSDs acting as
  journals/dbs for a group of HDDs).

Placement
  choice of which node is used to run a service.

Key Concepts
------------

The underlying orchestrator remains the source of truth for information
about whether a service is running, what is running where, which
nodes are available, etc.  Orchestrator modules should avoid taking
any internal copies of this information, and read it directly from
the orchestrator backend as much as possible.

Bootstrapping nodes and adding them to the underlying orchestration
system is outside the scope of Ceph's orchestrator interface.  Ceph
can only work on nodes when the orchestrator is already aware of them.

Calls to orchestrator modules are all asynchronous, and return *completion*
objects (see below) rather than returning values immediately.

Where possible, placement of stateless services should be left up to the
orchestrator.

Completions and batching
------------------------

All methods that read or modify the state of the system can potentially
be long running.  To handle that, all such methods return a *completion*
object (a *ReadCompletion* or a *WriteCompletion*).  Orchestrator modules
must implement the *wait* method: this takes a list of completions, and
is responsible for checking if they're finished, and advancing the underlying
operations as needed.

Each orchestrator module implements its own underlying mechanisms
for completions.  This might involve running the underlying operations
in threads, or batching the operations up before later executing
in one go in the background.  If implementing such a batching pattern, the
module would do no work on any operation until it appeared in a list
of completions passed into *wait*.

*WriteCompletion* objects have a two-stage execution.  First they become
*persistent*, meaning that the write has made it to the orchestrator
itself, and been persisted there (e.g. a manifest file has been updated).
If ceph-mgr crashed at this point, the operation would still eventually take
effect.  Second, the completion becomes *effective*, meaning that the operation has really happened (e.g. a service has actually been started).

.. automethod:: Orchestrator.wait

.. autoclass:: ReadCompletion
.. autoclass:: WriteCompletion

Placement
---------

In general, stateless services do not require any specific placement
rules, as they can run anywhere that sufficient system resources
are available.  However, some orchestrators may not include the
functionality to choose a location in this way, so we can optionally
specify a location when creating a stateless service.

OSD services generally require a specific placement choice, as this
will determine which storage devices are used.

Excluded functionality
----------------------

- Ceph's orchestrator interface is not a general purpose framework for
  managing linux servers -- it is deliberately constrained to manage
  the Ceph cluster's services only.
- Multipathed storage is not handled (multipathing is unnecessary for
  Ceph clusters).  Each drive is assumed to be visible only on
  a single node.

Inventory and status
--------------------

.. automethod:: Orchestrator.get_inventory
.. autoclass:: InventoryFilter
.. autoclass:: InventoryNode
.. autoclass:: InventoryDevice

.. automethod:: Orchestrator.describe_service
.. autoclass:: ServiceDescription
.. autoclass:: ServiceLocation

OSD management
--------------

.. automethod:: Orchestrator.create_osds
.. automethod:: Orchestrator.replace_osds
.. automethod:: Orchestrator.remove_osds
.. autoclass:: OsdCreationSpec
.. autoclass:: DriveGroupSpec

Upgrades
--------

.. automethod:: Orchestrator.upgrade_available
.. automethod:: Orchestrator.upgrade_start
.. automethod:: Orchestrator.upgrade_status
.. autoclass:: UpgradeSpec
.. autoclass:: UpgradeStatusSpec

Utility
-------

.. automethod:: Orchestrator.available

