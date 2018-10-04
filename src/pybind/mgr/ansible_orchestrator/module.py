"""
ceph-mgr Ansible orchestrator module

The external Orchestrator is the Ansible runner service (RESTful http service)
"""

# Python stuff
from threading import Event
import errno

# Ceph stuff
from mgr_module import MgrModule
import orchestrator

# Orchestrator stuff
# An agent is used to communicate with the Ansible Runner service
from ansible_runner_svc import Agent, ExecutionStatusCode

# Constants section

# Time to clean the completions list
WAIT_PERIOD = 10

class AnsibleOperation(object):
    """Execution of playbooks using Ansible runner service

    Each Ansible playboks will be executed in a separated thread using a
    ansible_runner_svc.Agent object
    """

    def __init__(self):

        # This class is designed to be used as parent class
        # in other multi-inheritance classes
        # Althougth this is a base class, this prevents to break the chain of
        #  initializations in child classes
        super(AnsibleOperation, self).__init__()

        self.callback_called = False

        # Used to provide the Code of Ansible playbook execution status
        self._status = ExecutionStatusCode.NOT_INIT

        # Used to know if the operation has failed
        self.error = False

        # Function assigned dinamically to process the result
        self.process_output = None

        # The usable result (string output) of the Ansible Playbook
        self.result = None

        # All the operations are executed using an Ansible Runner Agent
        self.ar_agent = Agent("localhost", "admin:admin")

    @property
    def status(self):
        """Return the status code of the operation
        """
        return self._status

    def execute_playbook(self, playbook_name, the_params):
        """Execute the playbook with the provided params.
        Use and Ansible runner service agent to launch th operation and provide
        the callback function that will be used when the playbook execution
        finishes

        @param string playbook_name: the playbook to execute
        @param list the_params     : The params needed to execute the playbook
        """

        # Launch the execution using Ansible Runner service
        self.ar_agent.launch(playbook_name, the_params, self.cb_playbook_finished)


    def cb_playbook_finished(self, pb_result):
        """Callback function used when a playbook has been executed.
        Update instance attributes with the playbook execution result obtained
        from the Ansible runner service though the Agent

        @param object pb_result:  ansible_runner_svc.PlayBookExecution instance
        """

        self.result = pb_result.result
        self.error = (pb_result.status != ExecutionStatusCode.ERROR)
        self._status = pb_result.status
        self.callback_called = True


class AnsibleReadOperation(AnsibleOperation, orchestrator.ReadCompletion):
    """ A read operation means to obtain information from the cluster.
    """

    def __init__(self):
        super(AnsibleReadOperation, self).__init__()
        self.error = False

    @property
    def is_read(self):
        return self.result != None

    @property
    def is_complete(self):
        return self.callback_called

    @property
    def is_errored(self):
        return self.error

    def get_result(self):
        """Output of the read operation

        The result of the playbook execution can be customized through the
        function provided as 'process_output' attribute
        @return string: Result of the operation formatted if it is possible
        """

        if self.process_output:
            formatted_result = self.process_output(self.result)
        else:
            formatted_result = self.result

        return formatted_result


class AnsibleChangeOperation(AnsibleOperation, orchestrator.WriteCompletion):
    """Operations that changes the "cluster" state

    Modifications/Changes (writes) are a two-phase thing, firstly execute
    the playbook that is going to change elements in the Ceph Cluster.
    When the playbook finishes execution (independently of the result),
    the modification/change operation has finished.
    """
    def __init__(self):
        super(AnsibleChangeOperation, self).__init__()

    @property
    def is_persistent(self):
        """
        Has the operation updated the orchestrator's configuration
        persistently?  Typically this would indicate that an update
        had been written to a manifest, but that the update
        had not necessarily been pushed out to the cluster.

        In the case of Ansible is always False.
        because a initiated playbook execution will need always to be
        relaunched if it fails.
        """

        return False

    @property
    def is_effective(self):
        """Has the operation taken effect on the cluster?
        For example, if we were adding a service, has it come up and appeared
        in Ceph's cluster maps?

        In the case of Ansible, this will be True if the playbooks has been
        executed succesfully.

        @return Boolean: if the playbook has been executed succesfully
        """

        return self.callback_called and self.status == "successful"

    @property
    def is_errored(self):
        return self.error

    @property
    def is_complete(self):
        return self.is_errored or (self.is_persistent and self.is_effective)


class Module(MgrModule, orchestrator.Orchestrator):
    """An Orchestrator that an external Ansible runner service to perform
    operations
    """

    COMMANDS = [
        {
            "cmd" : "inventory name=filter,type=CephString,req=false",
            "desc": "Show the nodes and devices in the cluster," \
                    "a filter string can be used to reduce output",
            "perm": "r"
        },
    ]


    def __init__(self, *args, **kwargs):
        """
        """
        super(Module, self).__init__(*args, **kwargs)

        self.run = False

        self.all_completions = []

        self.event = Event()

    def handle_command(self, inbuf, cmd):
        """Called by ceph-mgr to request the plugin to handle one
        of the commands that it declared in self.COMMANDS

        @param string inbuf: content of any "-i <file>" supplied to ceph cli
        @param dict command: from Ceph's cmdmap_t

        @return: 3-tuple of (int: status code,
                             str: output buffer <data results>,
                             str: output string <informative text>)
        """

        if cmd['prefix'] == 'inventory':
            the_filter = cmd['filter'] if 'filter' in cmd else ''
            operation = self.get_inventory(the_filter)
            result = (operation.status, operation.get_result(),
                      operation.result)
        else:
            result = (-errno.EINVAL, '',
                      "Command not found '{0}'".format(cmd['prefix']))

        return result

    def available(self):
        """ Check if Ansible Runner service is working
        """
        # TODO
        return (True, "Everything ready")

    def wait(self, completions=None):
        """Given a list of Completion instances, progress any which are
           incomplete.

           @param completions: list of Completion instances
           @Returns          : True if everything is done.
        """

        # TODO: Manage external completions

        self.all_completions = filter(lambda x: not x.is_complete,
                                      self.all_completions)

        return len(self.all_completions) == 0

    def serve(self):
        """ Mandatory for standby modules
        """
        self.log.info("Starting Ansible Orchestrator module ...")
        self.run = True

        while self.run:
            # Periodic clean of completed operations
            if self.wait():
                self.log.info("No pending operations")
            else:
                self.log.info("%s operations pending",
                              len(self.all_completions))

            self.event.wait(timeout=WAIT_PERIOD)

    def shutdown(self):
        self.log.info('Stopping Ansible orchestrator module')
        self.run = False
        self.event.set()

    def get_inventory(self, node_filter=None):
        """

        @param   :	node_filter instance
        @Return  :	A AnsibleReadOperation instance (Completion Object)
        """

        # Create a new read completion object
        ansible_operation = AnsibleReadOperation()

        # Assing the process_output function
        ansible_operation.process_output = process_inventary_json

        # Execute the playbook to obtain data
        ansible_operation.execute_playbook("get_inventory", {})

        self.all_completions.append(ansible_operation)

        return ansible_operation



# Auxiliary functions
#==============================================================================

def process_inventary_json(json_inventary):
    """ Adapt the output of the 'get_inventory' playbook
        to the Orchestrator rules

    @param json_inventary: Inventary as is returned by the 'get_inventory' pb
    @return              : list of InventoryNode
    """

    #TODO
    return "<<<{}>>>".format(json_inventary)
