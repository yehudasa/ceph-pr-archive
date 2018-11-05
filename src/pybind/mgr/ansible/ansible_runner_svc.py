"""
Tool module to interact with the Ansible Runner Service
"""
import requests
import json
import re

# Ansible Runner events finished


# Ansible Runner service API endpoints
API_URL = "api"
LOGIN_URL = "api/v1/login"
PLAYBOOK_EXEC_URL = "api/v1/playbooks"
PLAYBOOK_EVENTS = "api/v1/jobs/%s/events"
EVENT_DATA_URL = "api/v1/jobs/%s/events/%s"

class ExecutionStatusCode(object):
    """Execution status of playbooks ( 'msg' field in playbook status request)
    """

    SUCCESS = 0   # Playbook has been executed succesfully" msg = successful
    ERROR = 1     # Playbook has finished with error        msg = failed
    ON_GOING = 2  # Playbook is being executed              msg = running
    NOT_LAUNCHED = 3  # Not initialized


class PlayBookExecution(object):
    """Object to provide all the results of a Playbook execution
    """

    def __init__(self, rest_client, playbook, logger, result_pattern="", the_params={}):

        self.rest_client = rest_client

        # Identifier of the playbook execution
        self.play_uuid = "-"

        # Pattern used to extract the result from the events
        self.result_task_pattern = result_pattern

        # Playbook name
        self.playbook = playbook

        # Params used in the playbook
        self.params = the_params

        # Logger
        self.log = logger

    def launch(self):
        """ Launch the playbook execution
        """

        endpoint = "%s/%s" % (PLAYBOOK_EXEC_URL, self.playbook)

        response = self.rest_client.http_post(endpoint, self.params)

        if response:
            self.play_uuid = json.loads(response.text)["data"]["play_uuid"]
            self.log.info("Playbook execution launched succesfuly")
        else:
            # An error launching the execution implies play_uuid empty
            self.play_uuid = ""
            self.log.error("Playbook launch error. \
                            Check <endpoint> request result")

    def get_status(self):
        """ Return the status of the execution

        In the msg field of the respons we can find:
            "msg": "successful"
            "msg": "running"
            "msg": "failed"
        """

        status_value = ExecutionStatusCode.NOT_LAUNCHED

        if self.play_uuid == '-': # Initialized
            status_value = ExecutionStatusCode.NOT_LAUNCHED
        elif self.play_uuid == '': # Error launching playbook
            status_value = ExecutionStatusCode.ERROR
        else:
            endpoint = "%s/%s" % (PLAYBOOK_EXEC_URL, self.play_uuid)
            response = self.rest_client.http_get(endpoint)

            if response:
                the_status = json.loads(response.text)["msg"]
                if the_status == 'successful':
                    status_value = ExecutionStatusCode.SUCCESS
                elif the_status == 'failed':
                    status_value = ExecutionStatusCode.ERROR
                else:
                    status_value = ExecutionStatusCode.ON_GOING
            else:
                status_value = ExecutionStatusCode.ERROR

        self.log.info("Requested playbook execution status is: %s", status_value)
        return status_value

    def get_result(self, event_filter=""):
        """Get the data of the events filtered by a task pattern and
        a event filter

        @returns: the events that matches with the patterns provided
        """

        if not self.result_task_pattern or not self.play_uuid:
            result_events = {}

        response = self.rest_client.http_get(PLAYBOOK_EVENTS % self.play_uuid)

        if not response:
            result_events = {}
        else:
            events = json.loads(response.text)["data"]["events"]
            result_events = {event:data for event,data in events.items()
                            if "task" in data and
                            re.match(self.result_task_pattern, data["task"])}
            if event_filter:
                result_events = {event:data for event,data in result_events.items()
                                if re.match(event_filter, data['event'])}

        self.log.info("Requested playbook result is: %s", json.dumps(result_events))
        return result_events

class Client(object):
    """An utility object that allows to connect with the Ansible runner service
    and execute easily playbooks
    """

    def __init__(self, server, port, user, password, certificate, logger):
        """Get the Ansible runner service url and user credentials to be used in
        all the requests.

        @param ar_service_url: The URL of the Ansible runner service
        @param user: A tuple with (user_name, password), the user authorized to
                    connect with the Ansible Runner service
        """
        self.server = server
        self.port = port
        self.user = user
        self.password = password
        self.log = logger
        self.auth = (self.user, self.password)

        # Used to verify or not https server identity
        if not certificate:
            self.certificate = False
        else:
            self.certificate = certificate

        # Once authenticated this token will be used in all the requests
        self.token = ""

        self.server_url = "https://{0}:{1}".format(self.server, self.port)

        # Log in the server and get a token
        self.login()

    def login(self):
        """ Login with user credentials to obtain a valid token
        """

        response = None

        the_url = "%s/%s" % (self.server_url, LOGIN_URL)
        response = requests.get(the_url,
                                auth = self.auth,
                                verify = self.certificate)

        if response.status_code != requests.codes.ok:
            self.log.error("login error <<%s>> (%s):%s",
                            the_url, response.status_code, response.text)
        else:
            self.log.info("login succesful <<%s>> (%s):%s",
                            the_url, response.status_code, response.text)

        if response:
            self.token = json.loads(response.text)["data"]["token"]
            self.log.info("Connection with Ansible Runner Service is operative")


    def is_operative(self):
        """Indicates if the connection with the Ansible runner Server is ok
        """

        # No Token... this means we haven't used yet the service.
        if not self.token:
            return False

        # Check the service
        response = self.http_get(API_URL)

        if response:
            return response.status_code == requests.codes.ok
        else:
            return False

    def http_get(self, endpoint):
        """Execute an http get request

        @param endpoint: Ansible Runner service RESTful API endpoint

        @returns: A requests object
        """

        response = None

        try:
            the_url = "%s/%s" % (self.server_url, endpoint)
            r = requests.get(the_url,
                             verify = self.certificate,
                             headers = {"Authorization": self.token})

            if r.status_code != requests.codes.ok:
                self.log.error("http GET %s <--> (%s - %s)\n%s",
                               the_url, r.status_code, r.reason, r.text)
            else:
                self.log.info("http GET %s <--> (%s - %s)",
                              the_url, r.status_code, r.text)

            response = r

        except Exception as ex:
            self.log.exception("Ansible runner service(GET %s)", the_url)

        return response

    def http_post(self, endpoint, payload):
        """Execute an http post request

        @param endpoint: Ansible Runner service RESTful API endpoint
        @param payload: Dictionary with the data used in the post request

        @returns: A requests object
        """

        response = None

        try:
            the_url = "%s/%s" % (self.server_url, endpoint)
            r = requests.post(the_url,
                              verify = self.certificate,
                              headers = {"Authorization": self.token,
                                         "Content-type": "application/json"},
                              data = payload)

            if r.status_code != requests.codes.ok:
                self.log.error("http POST %s [%s] <--> (%s - %s)\n%s",
                              the_url, payload, r.status_code, r.reason, r.text)
            else:
                self.log.info("http POST %s <--> (%s - %s)",
                              the_url, r.status_code, r.text)
            response = r

        except Exception as ex:
            self.log.exception("Ansible runner service(POST %s)", the_url)

        return response

    def http_put(self, endpoint, payload):
        """Execute an http put request

        @param endpoint: Ansible Runner service RESTful API endpoint
        @param payload: Dictionary with the data used in the put request

        @returns: A requests object
        """
        # TODO
        raise NotImplementedError("TODO")
