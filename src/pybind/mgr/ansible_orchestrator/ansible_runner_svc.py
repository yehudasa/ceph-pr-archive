"""
Tool module to interact with the Ansible Runner Service
"""

class ExecutionStatusCode(object):
    """Execution status of playbooks
    """

    SUCCESS = 0   # Playbook has been executed succesfully"
    ERROR = 1     # Playbook has finished with error
    ON_GOING = 2  # Playbook is being executed
    NOT_INIT = 3  # Not initialized


class PlayBookExecution(object):
    """Object to provide all the results of a Plybook execution
    """
    # A string with the log of the execution
    result = ""

    # The current status of the playbook execution
    status = ExecutionStatusCode.NOT_INIT


class Agent(object):
    """An utility object that allows to connect with the Ansible runner service
    and execute easily playbooks
    """

    def __init__(self, ar_service_url, user):
        """Get the Ansible runner service url and user credentials to be used in
        all the requests.

        @param ar_service_url: The URL of the Ansible runner service
        @param user: A tuple with (user_name, password), the user authorized to
                    connect with the Ansible Runner service
        """

        # TODO
        pass

    def launch(self, playbook_name, the_params, callback_fn):
        """ Launch the playbook in a new thread and execute the callback when
            finished

        @param playbook_name: The playbook to execute
        @param the_params   : The parameters to use in the playbook invocation
        @param callback_fn  : The function to call when the playbook execution
                              has finished
        """

        pb_result = PlayBookExecution()

        #TODO: Threaded request to REST ansible runner service
        #-----------------------------------------------------
        # Poll playbook execution state

        # Update result
        pb_result.result = "Textual result of the playbook execution"
        pb_result.status = ExecutionStatusCode.SUCCESS

        callback_fn(pb_result)
        #-----------------------------------------------------
