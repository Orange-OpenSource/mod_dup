Overview
========
This repository contain two modules apache, mod_dup and mod_compare.

mod_dup
=======
mod_dup duplicates Apache requests (POST, GET or both).
Only requests which match specified filters are duplicated.
Before duplication, all defined substitutions are applied to the incoming request.
To minimize resource usage, mod_dup adapts to the amount of incoming by adjusting its number of threads.
If maximum thresholds are reached, requests are dropped.
In other words, mod_dup is built to guarantee a low system impact by sacrifizing the reliability of duplications.
However, by using a high number of maximum threads, request dropping can be avoided and system impact raised.
mod_dup periodically emits log messages containing metrics such as the number of dropped requests.

mod_compare
===========
mod_compare allows to compare the response header and body of HTTP Requests between two web services.
mod_compare receives an http request which contains the response header and body of a Web Service that will be compared to the response of the web service installed in the same server of mod_compare.
In order to work fine, the input request must contain the following header:
 * Duplication Type: Response
and must respect the "dup format":
 * URL --> the URL of orifinal request 
 * BODY --> XXXXXXXX{request_body}XXXXXXXX{response_header}XXXXXXXX{response_body} 
The first 8 characters indicate the size of the request body. Then the request body, 8 characters for the size of the response header, the response header, 8 characters for the size of the response body and the response body.
Two operating modes are possible: 
 * Response Comparison
 * No Comparison


Configuration mod_dup
=====================

For an example configuration file, see conf/tee.conf.example.

Basic
-----


* `DupDestination <host>:<port>`

  Sets the destination for the duplicated requests

* `DupQueue <min> <max>`

  Sets the minimum and maximum size of the internal request queue of each thread.
  Once the maximum size is reached, a new thread will be spawned.
  If the size falls below the minimum a thread is destroyed.

* `DupThreads <n>`

  Sets the minimum and maximum number of threads per Apache process.
  If the maximum number of threads is reached, and all queues are full, new requests will get dropped.

* `DupTimeout <ms>`

  The timeout for outgoing requests in milliseconds.

* `DupName <name>`

  A name which gets displayed on the periodic logs.

* `DupPayload <True|False>`

  If set to True, mod_dup will read and duplicate the body of incoming requests. False improves performance.

Filters
-------

Incoming requests can be filtered. If filters are specified at least one of them needs to match for a request to be duplicated.

* `DupFilter <HEADER|BODY|ALL> <param> <regexp>`

  Filters the content of GET or POST params using a reg exp which needs to match.
  The first argument specifies if the reg exp should be applied to params in the HEADER (GET requests), the BODY (POST requests) or ALL.

  Example:

    `DupFilter HEADER "INFOS" "(A|B|C)"`
   
    matches ?param1=x&infos=A&other=none

* `DupRawFilter <HEADER|BODY|ALL> <regexp>`

  Filters the content of the whole HEADER, BODY or ALL using a reg exp which needs to match.
  Useful for matching more complex queries which don''t use HTTP params

  Example:
    DupRawFilter BODY "Some secret sentence"

Substitutions
-------------

Once mod_dup decides to duplicate a request it will apply all substitutions in their defined order.

* `DupSubstitute <param> <regexp> <replace>`

  Applies regexp on specified param. Each match will be replaced by the last argument.

  E.g.:
    `DupSubstitute "param3" "(.+)" "new_value"`

* DupRawSubstitute <HEADER|BODY|ALL> <regexp> <replace>

  Same as DupSubstitute but applies to the whole HEADER, BODY or ALL.
 
  E.g.:
    `DupRawSubstitute BODY "(.*) wrong words (.*)" "\1 fixed stuff \2. FTFY"`

Configuration mod_compare
=========================
### Configuration independent from the location ###

  * `FilePath "{path}"`
    Sets the path of the file where to log the differences or the the two responses depending on the activated mode "Response Comparison" and "No Comparison", respectively.

### Location dependent directives ###

The directives that follow are only accessible in an Apache location.

* `Compare`
  If present, mod_compare is active for the current location.

* `HeaderList <param> <header> <reg_ex>` 

  List of reg_ex to apply to the header for the comparison. Two params are possible: **IGNORE** or **STOP**. 
  If the arg is IGNORE, mod_compare will ignore for the comparison the indicated header if it matches the reg_ex.
  If the arg is STOP, mod_compare will stop the comparison as soon as it finds that the indicated header matches the reg_ex.

  Example:
    HeaderList "IGNORE" "Content-Length" "."
    HeaderList "STOP" "Content-Type" "application/x-www-form-urlencoded"


* `BodyList <param> <reg_ex>`

  List of reg_ex to apply to the body for the comparison. Two params are possible: **IGNORE** or **STOP**.

  If the arg is IGNORE, mod_compare will ignore in the comparison the body line which matches the reg_ex.

  If the arg is STOP, mod_compare will stop the comparison as soon as it finds that a body line matches the reg_ex.

  Example:
    BodyList "STOP" "<Code>604</Code>"
    BodyList "IGNORE" "Date"

* `DisableLibwsdiff <param>`

  Enables or disables the comparison. If the parameter is **true** the comparison is disabled and it prints a raw serialization of the responses in the log file. If the parameter is **false** the comparison is activated.
  If missing, the comparison is activated by default.


Logging and monitoring
======================

// TODO
